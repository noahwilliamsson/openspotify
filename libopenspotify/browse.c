/*
 * Code to deal with internal libopenspotify playlist retrieving
 *
 * Program flow:
 *
 * + network_thread()
 * +--+ browse_process(REQ_TYPE_PLAYLIST_LOAD_CONTAINER)
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

#include "buf.h"
#include "channel.h"
#include "commands.h"
#include "debug.h"
#include "ezxml.h"
#include "playlist.h"
#include "request.h"
#include "sp_opaque.h"
#include "util.h"



static int browse_send_browsetrack_request(sp_session *session, sp_request *req, int offset);
static int browse_callback(CHANNEL *ch, unsigned char *payload, unsigned short len);
static int browse_parse_compressed_xml(sp_session *session, sp_playlist *playlist);


/* For giving the channel handler access to things it need to know */
#define MAX_TRACKS_PER_REQUEST 244
struct callback_ctx {
	sp_session *session;
	sp_request *req;
	sp_playlist *playlist;
	int browse_kind;
	int next_offset;
};


int browse_process(sp_session *session, sp_request *req) {
	int ret;

	if(req->type == REQ_TYPE_BROWSE_TRACK) {
		req->next_timeout = get_millisecs() + PLAYLIST_RETRY_TIMEOUT*1000;

		/* Send request (CMD_BROWSE) to load track data. */
		ret = browse_send_browsetrack_request(session, req, 0);
	}


	return ret;
}


/* Send BrowseTrack request to get information about track as compressed XML */
static int browse_send_browsetrack_request(sp_session *session, sp_request *req, int offset) {
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


static int browse_parse_compressed_xml(sp_session *session, sp_playlist *playlist) {
	const char *id;
	unsigned char track_id[16];
	struct buf *uncompressed;
	ezxml_t root, track_node, node;
	sp_track *track;
	int i;

	uncompressed = despotify_inflate(playlist->buf->ptr, playlist->buf->len);
	if(uncompressed == NULL)
		return -1;

	root = ezxml_parse_str((char *)uncompressed->ptr, uncompressed->len);

	for(track_node = ezxml_get(root, "tracks", 0, "track", -1); track_node; track_node = track_node->next) {
		node = ezxml_get(track_node, "id", -1);
		if(node == NULL)
			continue;

		hex_ascii_to_bytes(node->txt, track_id, sizeof(track_id));
		for(i = 0; i < playlist->num_tracks; i++) {
			track = playlist->tracks[i];

			if(memcmp(playlist->tracks[i]->id, track_id, sizeof(track_id)))
				continue;

			node = ezxml_get(track_node, "title", -1);
			if(node)
				track->title = strdup(node->txt);

			node = ezxml_get(track_node, "album", -1);
			if(node)
				track->album = strdup(node->txt);


			node = ezxml_get(track_node, "album-id", -1);
			if(node)
				hex_ascii_to_bytes(node->txt, track->album_id, sizeof(track->album_id));

			node = ezxml_get(track_node, "cover", -1);
			if(node)
				hex_ascii_to_bytes(node->txt, track->cover_id, sizeof(track->cover_id));

			node = ezxml_get(track_node, "files", 0, "file", -1);
			if(node && (id = ezxml_attr(node, "id")) != NULL) {
				track->playable = 1;
				hex_ascii_to_bytes(id, track->file_id, sizeof(track->file_id));
			}

			node = ezxml_get(track_node, "length", -1);
			if(node)
				track->duration = atoi(node->txt);

			/* FIXME: Handle all the other elements and attribuets */
		}
	}


	ezxml_free(root);

	return 0;
}
