/*
 * Code to deal with internal libopenspotify playlist retrieving
 *
 * Program flow:
 *
 * + network_thread()
 * +--+ playlist_process(REQ_TYPE_PLAYLIST_LOAD_CONTAINER)
 * |  +--+ playlist_send_playlist_container_request()
 * |  |  +--+ cmd_getplaylist()
 * |  |     +--+ channel_register() with callback playlist_container_callback()
 * |  +--- Update request->next_timeout
 * .  .
 * .  .
 * +--+ packet_read_and_process()
 * |   +--+ handle_channel()
 * |      +--+ channel_process()
 * |         +--+ playlist_container_callback()
 * |            +--- CHANNEL_DATA: Buffer XML-data
 * |            +--+ CHANNEL_END:
 * |               +--- playlist_parse_container_xml()
 * |               +--+ playlist_post_playlist_requests() (REQ_TYPE_PLAYLIST_LOAD_PLAYLIST)
 * |               |  +--- request_post(REQ_TYPE_PLAYLIST_LOAD_PLAYLIST)
 * |               +-- request_post_set_result(REQ_TYPE_PLAYLIST_LOAD_CONTAINER)
 * .
 * .
 * +--+ playlist_process(REQ_TYPE_PLAYLIST_LOAD_PLAYLIST)
 * |  +--+ playlist_send_playlist_request()
 * |  |  +--+ cmd_getplaylist()
 * |  |     +--+ channel_register() with callback playlist_container_callback()
 * |  +--- Update request->next_timeout
 * |
 * .  .
 * .  .
 * +--+ packet_read_and_process() 
 * |   +--+ handle_channel()
 * |      +--+ channel_process()
 * |         +--+ playlist_callback()
 * |            +--- CHANNEL_DATA: Buffer XML-data
 * |            +--+ CHANNEL_END:
 * |               +--- playlist_parse_playlist_xml()
 * |               +--+ playlist_post_track_requests()
 * |               |  +--- request_post(REQ_TYPE_BROWSE_TRACK)
 * |               +--- request_post_set_result(REQ_TYPE_PLAYLIST_LOAD_PLAYLIST)
 * .  .
 * .  .
 * +--- DONE
 * |
 *     
 */

#include <string.h>
#include <zlib.h>

#include <spotify/api.h>

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


static int playlist_send_playlist_container_request(sp_session *session, struct request *req);
static int playlist_container_callback(CHANNEL *ch, unsigned char *payload, unsigned short len);
static int playlist_parse_container_xml(sp_session *session);

static void playlist_post_playlist_requests(sp_session *session);
static int playlist_send_playlist_request(sp_session *session, struct request *req);
static int playlist_callback(CHANNEL *ch, unsigned char *payload, unsigned short len);
static int playlist_parse_playlist_xml(sp_session *session, sp_playlist *playlist);

static void playlist_post_track_request(sp_session *session, sp_playlist *);

unsigned long playlist_checksum(sp_playlist *playlist);
unsigned long playlistcontainer_checksum(sp_playlistcontainer *container);


/* For giving the channel handler access to both the session and the request */
struct callback_ctx {
	sp_session *session;
	struct request *req;
};


/* Initialize a playlist context, called by sp_session_init() */
struct playlist_ctx *playlist_create(void) {
	struct playlist_ctx *playlist_ctx;

	playlist_ctx = malloc(sizeof(struct playlist_ctx));
	if(playlist_ctx == NULL)
		return NULL;

	playlist_ctx->buf = NULL;

	playlist_ctx->container = malloc(sizeof(sp_playlistcontainer));
	playlist_ctx->container->playlists = NULL;

	/* FIXME: Should be an array of callbacks and userdatas */
	playlist_ctx->container->userdata = NULL;
	playlist_ctx->container->callbacks = malloc(sizeof(sp_playlistcontainer_callbacks));
	memset(playlist_ctx->container->callbacks, 0, sizeof(sp_playlistcontainer_callbacks));

	return playlist_ctx;
}


/* Release resources held by the playlist context, called by sp_session_release() */
void playlist_release(struct playlist_ctx *playlist_ctx) {
	sp_playlist *playlist, *next_playlist;
	int i;

	if(playlist_ctx->container) {
		for(playlist = playlist_ctx->container->playlists;
			playlist; playlist = next_playlist) {

			if(playlist->buf)
				buf_free(playlist->buf);

			if(playlist->name)
				free(playlist->name);

			if(playlist->owner) {
				/* FIXME: Free sp_user */
			}

			for(i = 0; i < playlist->num_tracks; i++)
				sp_track_release(playlist->tracks[i]);

			if(playlist->num_tracks)
				free(playlist->tracks);

			if(playlist->callbacks)
				free(playlist->callbacks);

			next_playlist = playlist->next;
			free(playlist);
		}

		free(playlist_ctx->container->callbacks);
		free(playlist_ctx->container);
	}

	if(playlist_ctx->buf)
		buf_free(playlist_ctx->buf);

	free(playlist_ctx);
}


/* Playlist FSM */
int playlist_process(sp_session *session, struct request *req) {
	int ret;

	if(req->state == REQ_STATE_NEW)
		req->state = REQ_STATE_RUNNING;

	if(req->type == REQ_TYPE_PLAYLIST_LOAD_CONTAINER) {
		req->next_timeout = get_millisecs() + PLAYLIST_RETRY_TIMEOUT*1000;

		/* Send request (CMD_GETPLAYLIST) to load playlist container */
		ret = playlist_send_playlist_container_request(session, req);
	}
	else if(req->type == REQ_TYPE_PLAYLIST_LOAD_PLAYLIST) {
		req->next_timeout = get_millisecs() + PLAYLIST_RETRY_TIMEOUT*1000;

		/* Send request (CMD_GETPLAYLIST) to load playlist */
		ret = playlist_send_playlist_request(session, req);
	}

	return ret;
}


/* Request playlist container */
static int playlist_send_playlist_container_request(sp_session *session, struct request *req) {
	unsigned char container_id[17];
	static const char* decl_and_root =
		"<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n<playlist>\n";

	struct callback_ctx *callback_ctx;


	DSFYDEBUG("Requesting playlist container\n");

	/* Free'd by the callback */
	callback_ctx = malloc(sizeof(struct callback_ctx));
	callback_ctx->session = session;
	callback_ctx->req = req;

	session->playlist_ctx->buf = buf_new();
	buf_append_data(session->playlist_ctx->buf, (char*)decl_and_root, strlen(decl_and_root));

	memset(container_id, 0, 17);
	return cmd_getplaylist(session, container_id, ~0, 
				playlist_container_callback, callback_ctx);
}


/* Callback for playlist container buffering */
static int playlist_container_callback(CHANNEL *ch, unsigned char *payload, unsigned short len) {
	struct callback_ctx *callback_ctx = (struct callback_ctx *)ch->private;
	struct playlist_ctx *playlist_ctx = callback_ctx->session->playlist_ctx;

	switch(ch->state) {
	case CHANNEL_DATA:
		buf_append_data(playlist_ctx->buf, payload, len);
		break;

	case CHANNEL_ERROR:
		buf_free(playlist_ctx->buf);
		playlist_ctx->buf = NULL;

		/* Don't set error on request. It will be retried. */
		free(callback_ctx);

		DSFYDEBUG("Got a channel ERROR\n");
		break;

	case CHANNEL_END:
		/* Parse returned XML and request each listed playlist */
		if(playlist_parse_container_xml(callback_ctx->session) == 0) {

			/* Create new requests for each playlist found */
			playlist_post_playlist_requests(callback_ctx->session);

			/* Note we're done loading the playlist container */
			request_set_result(callback_ctx->session, callback_ctx->req, SP_ERROR_OK, NULL);
		}

		free(callback_ctx);

		buf_free(playlist_ctx->buf);
		playlist_ctx->buf = NULL;

		break;

	default:
		break;
	}

	return 0;
}


static int playlist_parse_container_xml(sp_session *session) {
	static char *end_element = "</playlist>";
	char *id_list, *id;
	char idstr[35];
	int position;
	ezxml_t root, node;
	sp_playlist *playlist;
	sp_playlistcontainer *container;

	buf_append_data(session->playlist_ctx->buf, end_element, strlen(end_element));
	buf_append_u8(session->playlist_ctx->buf, 0);

	root = ezxml_parse_str((char *)session->playlist_ctx->buf->ptr, session->playlist_ctx->buf->len);
	node = ezxml_get(root, "next-change", 0, "change", 0, "ops", 0, "add", 0, "items", -1);
	id_list = node->txt;

	container = session->playlist_ctx->container;
	for(id = strtok(id_list, ",\n"); id; id = strtok(NULL, ",\n")) {
		hex_bytes_to_ascii((unsigned char *)id, idstr, 17);
		DSFYDEBUG("Playlist ID '%s'\n", idstr);
	
		position = 0;
		if((playlist = container->playlists) == NULL) {
			container->playlists = malloc(sizeof(sp_playlist));
			playlist = container->playlists;
		}
		else {
			for(position = 0; playlist->next; position++)
				playlist = playlist->next;

			playlist->next = malloc(sizeof(sp_playlist));
			playlist = playlist->next;
		}

		hex_ascii_to_bytes(id, playlist->id, sizeof(playlist->id));
		playlist->num_tracks = 0;
		playlist->tracks = NULL;

		/* FIXME: Pull this info from XML ? */
		playlist->name = NULL;
		playlist->owner = NULL;

		playlist->lastrequest = 0;
		playlist->state = PLAYLIST_STATE_ADDED;

		playlist->callbacks = NULL;
		playlist->userdata = NULL;

		playlist->buf = NULL;
		playlist->next = NULL;

		/* FIXME: Probably shouldn't carry around playlist positions in the playlist itself! */
		playlist->position = position;
	}
	

	ezxml_free(root);

	return 0;
}


/* Create new requests for each playlist in the container */
static void playlist_post_playlist_requests(sp_session *session) {
	sp_playlist *playlist, **ptr;
	int i;

	i = 0;
	for(playlist = session->playlist_ctx->container->playlists;
		playlist; playlist = playlist->next) {

		ptr = (sp_playlist **)malloc(sizeof(sp_playlist *));
		*ptr = playlist;
		request_post(session, REQ_TYPE_PLAYLIST_LOAD_PLAYLIST, ptr);

		i++;
	}

	DSFYDEBUG("Created %d requests to retrieve playlists\n", i);
}


/* Request a playlist from Spotify */
static int playlist_send_playlist_request(sp_session *session, struct request *req) {
	int ret;
	char idstr[35];
	struct callback_ctx *callback_ctx;
	sp_playlist *playlist = *(sp_playlist **)req->input;
	static const char* decl_and_root =
		"<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n<playlist>\n";

	callback_ctx = malloc(sizeof(struct callback_ctx));
	callback_ctx->session = session;
	callback_ctx->req = req;

	playlist->buf = buf_new();
	buf_append_data(playlist->buf, (char*)decl_and_root, strlen(decl_and_root));
	
	hex_bytes_to_ascii((unsigned char *)playlist->id, idstr, 17);

	ret =  cmd_getplaylist(session, playlist->id, ~0, 
			playlist_callback, callback_ctx);

	DSFYDEBUG("Sent request for playlist with ID '%s' at time %d\n", idstr, get_millisecs());
	return ret;
}



/* Callback for playlist container buffering */
static int playlist_callback(CHANNEL *ch, unsigned char *payload, unsigned short len) {
	struct callback_ctx *callback_ctx = (struct callback_ctx *)ch->private;
	sp_playlist *playlist = *(sp_playlist **)callback_ctx->req->input;

	switch(ch->state) {
	case CHANNEL_DATA:
		buf_append_data(playlist->buf, payload, len);
		break;

	case CHANNEL_ERROR:
		buf_free(playlist->buf);
		playlist->buf = NULL;

		/* Don't set error on request. It will be retried. */
		free(callback_ctx);

		DSFYDEBUG("Got a channel ERROR\n");
		break;

	case CHANNEL_END:
		/* Parse returned XML and request tracks */
		if(playlist_parse_playlist_xml(callback_ctx->session, playlist) == 0) {
			playlist->state = PLAYLIST_STATE_LISTED;

			/* Create new request for loading tracks */
			playlist_post_track_request(callback_ctx->session, playlist);

			/* Note we're done loading this playlist */
			request_set_result(callback_ctx->session, callback_ctx->req, SP_ERROR_OK, NULL);

			{
				char idstr[35];
				hex_bytes_to_ascii((unsigned char *)playlist->id, idstr, 17);
				DSFYDEBUG("Successfully loaded playlist '%s'\n", idstr);
			}
		}

		buf_free(playlist->buf);
		playlist->buf = NULL;

		free(callback_ctx);
		break;

	default:
		break;
	}

	return 0;
}


static int playlist_parse_playlist_xml(sp_session *session, sp_playlist *playlist) {
	static char *end_element = "</playlist>";
	char *id_list, *idstr;
	unsigned char track_id[16];
	ezxml_t root, node;
	sp_track *track;

	/* NULL-terminate XML */
	buf_append_data(playlist->buf, end_element, strlen(end_element));
	buf_append_u8(playlist->buf, 0);

	root = ezxml_parse_str((char *)playlist->buf->ptr, playlist->buf->len);
	node = ezxml_get(root, "next-change", 0, "change", 0, "ops", 0, "add", 0, "items", -1);
	id_list = node->txt;
	
	for(idstr = strtok(id_list, ",\n"); idstr; idstr = strtok(NULL, ",\n")) {
		hex_ascii_to_bytes(idstr, track_id, sizeof(track_id));
		track = osfy_track_add(session, track_id);

		playlist->tracks = (sp_track **)realloc(playlist->tracks, (playlist->num_tracks + 1) * sizeof(sp_track *));
		playlist->tracks[playlist->num_tracks] = track;
		playlist->num_tracks++;

		sp_track_add_ref(track);
	}
	
	ezxml_free(root);

	return 0;
}


/* Create new track request for this playlist */
void playlist_post_track_request(sp_session *session, sp_playlist *playlist) {
	sp_playlist **ptr;

	ptr = (sp_playlist **)malloc(sizeof(sp_playlist *));
	*ptr = playlist;

	request_post(session, REQ_TYPE_BROWSE_TRACK, ptr);
}


/* Calculate a playlist checksum. */
unsigned long playlist_checksum(sp_playlist *playlist) {
	unsigned long checksum = 1L;
	int i;

	if(playlist == NULL)
		return 1L;

	/* Loop over all tracks (make sure the last byte is 0x01). */
	for(i = 0; i < playlist->num_tracks; i++){
		playlist->tracks[i]->id[16] = 0x01;

		checksum = adler32(checksum, playlist->tracks[i]->id, 17);
	}

	return checksum;
}


/* Calculate a playlists container checksum. */
unsigned long playlistcontainer_checksum(sp_playlistcontainer *container) {
	unsigned long checksum = 1L;
	sp_playlist *playlist;

	if(container == NULL)
		return 1L;

	/* Loop over all playlists (make sure the last byte is 0x02). */
	for(playlist = container->playlists; playlist != NULL; playlist = playlist->next){
		playlist->id[16] = 0x02;

		checksum = adler32(checksum, playlist->id, 17);
	}

	return checksum;
}
