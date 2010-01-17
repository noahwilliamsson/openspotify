/*
 * Code to deal with internal libopenspotify playlist retrieving
 *
 * Program flow:
 *
 * + iothread()
 * +--+ browse_process(REQ_TYPE_BROWSE_TRACK)
 * |  +--+ browse_send_browsetrack_request()
 * |  |  +--+ cmd_getplaylist()
 * |  |     +--+ channel_register() with callback browse_callback()
 * |  +--- Update request->next_timeout
 * .  .
 * .  .
 * +--+ packet_read_and_process()
 * |   +--+ handle_channel()
 * |      +--+ channel_process()
 * |         +--+ browse_callback()
 * |            +--- CHANNEL_DATA: Buffer XML-data
 * |            +--+ CHANNEL_END:
 * |               +--- browse_parse_compressed_xml()
 * |               +--+ browse_send_browsetrack_request()
 * |                  +-- Will do request_post_set_result(REQ_TYPE_BROWSE_TRACKS) when done
 * .
 * .
 * +--- DONE
 * |
 *     
 */

#include <assert.h>
#include <string.h>

#include "album.h"
#include "browse.h"
#include "buf.h"
#include "channel.h"
#include "commands.h"
#include "debug.h"
#include "ezxml.h"
#include "playlist.h"
#include "request.h"
#include "sp_opaque.h"
#include "track.h"
#include "util.h"



static int browse_send_generic_request(sp_session *session, struct request *req);
static int browse_generic_callback(CHANNEL *ch, unsigned char *payload, unsigned short len);


/* For giving the channel handler access to things it need to know */
#define MAX_TRACKS_PER_REQUEST 244
struct callback_ctx {
	sp_session *session;
	struct request *req;
	sp_playlist *playlist;
	int browse_kind;
	int next_offset;
};


int browse_process(sp_session *session, struct request *req) {
	int ret;

	
	req->next_timeout = get_millisecs() + BROWSE_RETRY_TIMEOUT*1000;

	switch(req->type) {
		case REQ_TYPE_BROWSE_TRACK:
		case REQ_TYPE_BROWSE_ALBUM:
		case REQ_TYPE_BROWSE_ARTIST:
		case REQ_TYPE_BROWSE_PLAYLIST_TRACKS:
		case REQ_TYPE_ALBUMBROWSE:
		case REQ_TYPE_ARTISTBROWSE:
			DSFYDEBUG("Browsing for <type %s, state %s, input %p>\n", 
				  REQUEST_TYPE_STR(req->type),
				  REQUEST_STATE_STR(req->state), req->input);
			ret = browse_send_generic_request(session, req);
			break;

		default:
			DSFYDEBUG("BUG: <type %s> is not yet implemented\n", REQUEST_TYPE_STR(req->type));
			ret = request_set_result(session, req, SP_ERROR_OTHER_PERMANENT, NULL);
			break;
	}
	
	return ret;
}


static int browse_send_generic_request(sp_session *session, struct request *req) {
	int ret;
	struct browse_callback_ctx *brctx;
	int i;
	unsigned char *idlist;
	int browse_type;
	
	brctx = *(struct browse_callback_ctx **)req->input;
	
	/* FIXME: Should probably move this one? */
	req->state = REQ_STATE_RUNNING;
	brctx->req = req;

	
	/* Are we done yet? */
	if(brctx->num_browsed == brctx->num_total) {
		DSFYDEBUG("Offset reached total count of %d, returning results for <type %s>!\n", brctx->num_total, REQUEST_TYPE_STR(req->type));
		switch(brctx->type) {
			case REQ_TYPE_ALBUMBROWSE:
				ret = request_set_result(session, req, brctx->data.albumbrowses[0]->error, brctx->data.albumbrowses[0]);
				break;

			case REQ_TYPE_ARTISTBROWSE:
				ret = request_set_result(session, req, brctx->data.artistbrowses[0]->error, brctx->data.artistbrowses[0]);
				break;

			case REQ_TYPE_BROWSE_PLAYLIST_TRACKS:
				brctx->data.playlist->state = PLAYLIST_STATE_LOADED;
				ret = request_set_result(session, req, SP_ERROR_OK, brctx->data.playlist);
				break;
				
			default:
				ret = request_set_result(session, req, SP_ERROR_OK, NULL);
				break;
		}
		
		/* Free browse callback context */
		free(brctx);
		
		return ret;
	}


	/* Don't send too many browse requests at once */
	brctx->num_in_request = brctx->num_total - brctx->num_browsed;
	switch(brctx->type) {
		case REQ_TYPE_ALBUMBROWSE:
		case REQ_TYPE_ARTISTBROWSE:
		case REQ_TYPE_BROWSE_ALBUM:
		case REQ_TYPE_BROWSE_ARTIST:
			if(brctx->num_in_request > 1)
				brctx->num_in_request = 1;
			break;

		case REQ_TYPE_BROWSE_TRACK:
		case REQ_TYPE_BROWSE_PLAYLIST_TRACKS:
			if(brctx->num_in_request > MAX_TRACKS_PER_REQUEST)
				brctx->num_in_request = MAX_TRACKS_PER_REQUEST;
			break;
		default:
			DSFYDEBUG("Unsupported request!\n");
			break;
	}


	/* Create list of album/artist/track IDs */
	idlist = (unsigned char *)malloc(16 * brctx->num_in_request);
	switch(brctx->type) {
		case REQ_TYPE_ALBUMBROWSE:
			browse_type = BROWSE_ALBUM;
			for(i = 0; i < brctx->num_in_request; i++)
				memcpy(idlist + i*16, brctx->data.albumbrowses[brctx->num_browsed + i]->album->id, 16);
			break;

		case REQ_TYPE_ARTISTBROWSE:
			browse_type = BROWSE_ARTIST;
			for(i = 0; i < brctx->num_in_request; i++)
				memcpy(idlist + i*16, brctx->data.artistbrowses[brctx->num_browsed + i]->artist->id, 16);
			break;
			
		case REQ_TYPE_BROWSE_ALBUM:
			browse_type = BROWSE_ALBUM;
			for(i = 0; i < brctx->num_in_request; i++)
				memcpy(idlist + i*16, brctx->data.albums[brctx->num_browsed + i]->id, 16);
			break;

		case REQ_TYPE_BROWSE_ARTIST:
			browse_type = BROWSE_ARTIST;
			for(i = 0; i < brctx->num_in_request; i++)
				memcpy(idlist + i*16, brctx->data.artists[brctx->num_browsed + i]->id, 16);
			break;

		case REQ_TYPE_BROWSE_PLAYLIST_TRACKS:
			browse_type = BROWSE_TRACK;
			for(i = 0; i < brctx->num_in_request; i++)
				memcpy(idlist + i*16, brctx->data.playlist->tracks[brctx->num_browsed + i]->id, 16);
			break;

		case REQ_TYPE_BROWSE_TRACK:
			browse_type = BROWSE_TRACK;
			for(i = 0; i < brctx->num_in_request; i++)
				memcpy(idlist + i*16, brctx->data.tracks[brctx->num_browsed + i]->id, 16);
			break;

		default:
			browse_type = 0;
			DSFYDEBUG("BUG: Can't handle <type %s>\n", REQUEST_TYPE_STR(req->type));
			break;
	}

	/* Need to have a valid browse type */
	assert(browse_type != 0);
	
	/* Buffer to hold data (gzip'd XML) retrieved */
	assert(brctx->buf == NULL);
	brctx->buf = buf_new();

	
	DSFYDEBUG("Sending BROWSE for %d items (from offset %d) on behalf of <type %s, state %s, input %p>\n",
		  brctx->num_in_request, brctx->num_browsed, REQUEST_TYPE_STR(req->type),
		  REQUEST_STATE_STR(req->state), req->input);

	ret = cmd_browse(session, browse_type, idlist, brctx->num_in_request, browse_generic_callback, brctx);
	
	free(idlist);
	
	return ret;
}


/* Callback for browse requests */
static int browse_generic_callback(CHANNEL *ch, unsigned char *payload, unsigned short len) {
	int skip_len;
	struct browse_callback_ctx *brctx;
	brctx = (struct browse_callback_ctx *)ch->private;

	switch(ch->state) {
		case CHANNEL_DATA:
			/* Skip a minimal gzip header */
			if (ch->total_data_len < 10) {
				skip_len = 10 - ch->total_data_len;
				while(skip_len && len) {
					skip_len--;
					len--;
					payload++;
				}
				
				if (len == 0)
					break;
			}
			
			buf_append_data(brctx->buf, payload, len);
			break;
			
		case CHANNEL_ERROR:
			DSFYDEBUG("Got a channel ERROR, freeing buffer and retrying within %d seconds\n", BROWSE_RETRY_TIMEOUT);
			buf_free(brctx->buf);
			brctx->buf = NULL;
			
			/* The request will be retried as soon as req->next_timeout expires */
			break;
			
		case CHANNEL_END:
			DSFYDEBUG("Got all data, calling parser\n");
			brctx->browse_parser(brctx);
			buf_free(brctx->buf);
			brctx->buf = NULL;
			
			/* Increase number of items processed */
			brctx->num_browsed += brctx->num_in_request;
			
			/* Force the next browse request to happen immediately */
			brctx->req->next_timeout = 0;
			
			break;
			
		default:
			break;
	}
	
	return 0;
}
