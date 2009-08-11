/*
 * Code to deal with internal libopenspotify playlist retrieving
 *
 * Program flow:
 *
 * + network_thread()
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
static int browse_send_browsetrack_request(sp_session *session, struct request *req, int offset);
static int browse_callback(CHANNEL *ch, unsigned char *payload, unsigned short len);
static int browse_parse_compressed_xml(sp_session *session, sp_playlist *playlist);


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
			/* Send request (CMD_BROWSE) to load track data. */
			ret = browse_send_browsetrack_request(session, req, 0);
			break;

		case REQ_TYPE_BROWSE_ALBUM:
		case REQ_TYPE_BROWSE_ARTIST:
		case REQ_TYPE_ALBUMBROWSE:
		case REQ_TYPE_ARTISTBROWSE:
			DSFYDEBUG("Browsing for <type %s, state %s, input %p>\n", 
				  REQUEST_TYPE_STR(req->type),
				  REQUEST_STATE_STR(req->state), req->input);
			ret = browse_send_generic_request(session, req);
			break;

		default:
			DSFYDEBUG("BUG: <type %s> is not yet implemented\n", REQUEST_TYPE_STR(req->type));
			ret = request_set_result(session, req, SP_ERROR_OTHER_PERMAMENT, NULL);
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
	brctx->req = req;
	req->state = REQ_STATE_RUNNING;

	
	/* Are we done yet? */
	if(brctx->num_browsed == brctx->num_total) {
		DSFYDEBUG("Offset reached total count of %d, returning results for <type %s>!\n", brctx->num_total, REQUEST_TYPE_STR(req->type));
		switch(brctx->type) {
			case REQ_TYPE_ALBUMBROWSE:
				ret = request_set_result(session, req, SP_ERROR_OK, brctx->data.albumbrowses[0]);
			case REQ_TYPE_ARTISTBROWSE:
				ret = request_set_result(session, req, SP_ERROR_OK, brctx->data.artistbrowses[0]);
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
	if(brctx->num_in_request > MAX_TRACKS_PER_REQUEST)
		brctx->num_in_request = MAX_TRACKS_PER_REQUEST;


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
			
		case REQ_TYPE_BROWSE_TRACK:
			browse_type = BROWSE_TRACK;
			for(i = 0; i < brctx->num_in_request; i++)
				memcpy(idlist + i*16, brctx->data.tracks[brctx->num_browsed + i]->id, 16);
			break;

		default:
			DSFYDEBUG("BUG: Can't handle <type %s>\n", REQUEST_TYPE_STR(req->type));
			break;
	}

	
	/* Buffer to hold data (gzip'd XML) retrieved */
	assert(brctx->buf == NULL);
	brctx->buf = buf_new();

	
	DSFYDEBUG("Sending BROWSE for %d items (at offset %d) on behalf of <type %s>\n",
		  brctx->num_in_request, brctx->num_browsed, REQUEST_TYPE_STR(req->type));

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


/* Send BrowseTrack request to get information about track as compressed XML */
static int browse_send_browsetrack_request(sp_session *session, struct request *req, int offset) {
	sp_playlist *playlist = *(sp_playlist **)req->input;
	struct callback_ctx *callback_ctx;
	int i, count, ret;
	unsigned char *tracklist;


	if(offset == playlist->num_tracks) {
		request_set_result(session, req, SP_ERROR_OK, playlist);

		return 0;
	}


	count = playlist->num_tracks - offset;
	if(count > MAX_TRACKS_PER_REQUEST)
		count = MAX_TRACKS_PER_REQUEST;

	/* List with track IDs, needed for the request */
	tracklist = malloc(16 * count);
	for(i = 0; i < count; i++)
		memcpy(tracklist + i * 16, playlist->tracks[i]->id, 16);


	/* This is where the gzip'd XML will be stored */
	assert(playlist->buf == NULL);
	playlist->buf = buf_new();

	/* Free'd by the callback */
	callback_ctx = malloc(sizeof(struct callback_ctx));
	callback_ctx->session = session;
	callback_ctx->req = req;
	callback_ctx->playlist = playlist;
	callback_ctx->browse_kind = BROWSE_TRACK;
	callback_ctx->next_offset = offset + count;

	/* The channel callback will call this function again with offset + count as the new offset */
	DSFYDEBUG("Requesting BrowseTrack for %d tracks (at offset %d)\n", count, offset);
	ret = cmd_browse(session, BROWSE_TRACK, tracklist, count, browse_callback, callback_ctx);

	free(tracklist);

	return ret;
}


/* Callback for browse requests */
static int browse_callback(CHANNEL *ch, unsigned char *payload, unsigned short len) {
	struct callback_ctx *callback_ctx = (struct callback_ctx *)ch->private;
	sp_playlist *playlist = callback_ctx->playlist;
	int ret, skip_len;

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


		buf_append_data(playlist->buf, payload, len);
		break;

	case CHANNEL_ERROR:
		buf_free(playlist->buf);
		playlist->buf = NULL;

		request_set_result(callback_ctx->session, callback_ctx->req, SP_ERROR_OTHER_TRANSIENT, NULL);
		free(callback_ctx);

		DSFYDEBUG("Got a channel ERROR\n");
		break;

	case CHANNEL_END:
		switch(callback_ctx->browse_kind) {
		case BROWSE_TRACK:
			ret = browse_parse_compressed_xml(callback_ctx->session,
							callback_ctx->playlist);
			if(ret == 0)
				ret = browse_send_browsetrack_request(callback_ctx->session,
									callback_ctx->req,
									callback_ctx->next_offset);

			break;

		case BROWSE_ALBUM:
			ret = -1;
			break;

		case BROWSE_ARTIST:
			ret = -1;
			break;
		}


		free(callback_ctx);

		buf_free(playlist->buf);
		playlist->buf = NULL;

		break;

	default:
		break;
	}

	return 0;
}


/* FIXME: This should be moved into sp_track.c */
static int browse_parse_compressed_xml(sp_session *session, sp_playlist *playlist) {
	const char *idstr;
	unsigned char id[20];
	struct buf *uncompressed;
	ezxml_t root, track_node, node;
	sp_album *album;
	sp_track *track;


	uncompressed = despotify_inflate(playlist->buf->ptr, playlist->buf->len);
	if(uncompressed == NULL)
		return -1;

	root = ezxml_parse_str((char *)uncompressed->ptr, uncompressed->len);
	if(root == NULL)
		return -1;

	for(track_node = ezxml_get(root, "tracks", 0, "track", -1); track_node; track_node = track_node->next) {
		node = ezxml_get(track_node, "id", -1);
		if(node == NULL)
			continue;

		hex_ascii_to_bytes(node->txt, id, 16);
		track = osfy_track_add(session, id);

		if((node = ezxml_get(track_node, "title", -1)) != NULL)
			osfy_track_name_set(track, node->txt);

		if((node = ezxml_get(track_node, "album-id", -1)) != NULL) {
			hex_ascii_to_bytes(node->txt, id, 16);
			album = sp_album_add(session, id);
			osfy_track_album_set(track, album);
			sp_album_add_ref(album);
		}

		node = ezxml_get(track_node, "files", 0, "file", -1);
		if(node && (idstr = ezxml_attr(node, "id")) != NULL) {
			hex_ascii_to_bytes(idstr, id, 20);
			osfy_track_file_id_set(track, id);

			/* FIXME: Also check country restrictions */
			osfy_track_playable_set(track, 1);
		}

		if((node = ezxml_get(track_node, "length", -1)) != NULL)
			osfy_track_duration_set(track, atoi(node->txt));

		if((node = ezxml_get(track_node, "disc", -1)) != NULL)
			osfy_track_disc_set(track, atoi(node->txt));

		osfy_track_loaded_set(track, 1);
	}

	ezxml_free(root);

	return 0;
}
