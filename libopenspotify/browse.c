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

	if(req->type == REQ_TYPE_BROWSE_TRACK) {
		req->next_timeout = get_millisecs() + PLAYLIST_RETRY_TIMEOUT*1000;

		/* Send request (CMD_BROWSE) to load track data. */
		ret = browse_send_browsetrack_request(session, req, 0);
	}
	else if(req->type == REQ_TYPE_BROWSE_ALBUM) {
		req->next_timeout = get_millisecs() + PLAYLIST_RETRY_TIMEOUT*1000;
		
		DSFYDEBUG("REQ_TYPE_BROWSE_ALBUM is not yet implemented\n");
		return request_set_result(session, req, SP_ERROR_OTHER_PERMAMENT, NULL);
	}
	else if(req->type == REQ_TYPE_BROWSE_ARTIST) {
		req->next_timeout = get_millisecs() + PLAYLIST_RETRY_TIMEOUT*1000;
		
		DSFYDEBUG("REQ_TYPE_BROWSE_ARTIST is not yet implemented\n");
		return request_set_result(session, req, SP_ERROR_OTHER_PERMAMENT, NULL);
	}


	return ret;
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
	sp_image *image;


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
			osfy_track_title_set(track, node->txt);

		if((node = ezxml_get(track_node, "album", -1)) != NULL)
			osfy_track_album_name_set(track, node->txt);


		if((node = ezxml_get(track_node, "album-id", -1)) != NULL) {
			hex_ascii_to_bytes(node->txt, id, 16);
			album = sp_album_add(session, id);
			osfy_track_album_set(track, album);
			sp_album_add_ref(album);
		}

		if((node = ezxml_get(track_node, "cover", -1)) != NULL) {
			hex_ascii_to_bytes(node->txt, id, 20);
			image = sp_image_create(session, id);
			osfy_track_image_set(track, image);
			sp_image_add_ref(image);
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
