/*
 * Code to deal with internal libopenspotify playlist retrieving
 *
 * Program flow:
 *
 * + network_thread()
 * +--+ playlist_process(REQ_TYPE_PC_LOAD)
 * |  +--+ playlistcontainer_send_request()
 * |  |  +--+ cmd_getplaylist()
 * |  |     +--+ channel_register() with callback playlistcontainer_callback()
 * |  +--- Update request->next_timeout
 * .  .
 * .  .
 * +--+ packet_read_and_process()
 * |   +--+ handle_channel()
 * |      +--+ channel_process()
 * |         +--+ playlistcontainer_callback()
 * |            +--- CHANNEL_DATA: Buffer XML-data
 * |            +--+ CHANNEL_END:
 * |               +--- playlistcontainer_parse_xml()
 * |               +--+ playlistcontainer_request_playlists()
 * |               |  +--- request_post(REQ_TYPE_PLAYLIST_LOAD)
 * |               +-- request_post_set_result(REQ_TYPE_PC_LOAD)
 * .
 * .
 * +--+ playlist_process(REQ_TYPE_PLAYLIST_LOAD)
 * |  +--+ playlist_send_request()
 * |  |  +--+ cmd_getplaylist()
 * |  |     +--+ channel_register() with callback playlistcontainer_callback()
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
 * |               +--- playlist_parse_xml()
 * |               +--+ osfy_playlist_browse()
 * |               |  +--- request_post(REQ_TYPE_BROWSE_PLAYLIST_TRACKS)
 * |               +--- request_post_set_result(REQ_TYPE_PLAYLIST_LOAD)
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
#include "browse.h"
#include "channel.h"
#include "commands.h"
#include "debug.h"
#include "ezxml.h"
#include "playlist.h"
#include "request.h"
#include "sp_opaque.h"
#include "track.h"
#include "util.h"


static int playlistcontainer_send_request(sp_session *session, struct request *req);
static int playlistcontainer_callback(CHANNEL *ch, unsigned char *payload, unsigned short len);
static int playlistcontainer_parse_xml(sp_session *session);
static void playlistcontainer_request_playlists(sp_session *session);

static int playlist_send_request(sp_session *session, struct request *req);
static int playlist_callback(CHANNEL *ch, unsigned char *payload, unsigned short len);
static int playlist_parse_xml(sp_session *session, sp_playlist *playlist);

static int osfy_playlist_browse(sp_session *session, sp_playlist *playlist);
static int osfy_playlist_browse_callback(struct browse_callback_ctx *brctx);


/* For giving the channel handler access to both the session and the request */
struct callback_ctx {
	sp_session *session;
	struct request *req;
};


/* Handle playlist loading event, called by the network thread */
int playlist_process(sp_session *session, struct request *req) {
	int ret;
	
	if(req->state == REQ_STATE_NEW)
		req->state = REQ_STATE_RUNNING;
	
	if(req->type == REQ_TYPE_PC_LOAD) {
		req->next_timeout = get_millisecs() + PLAYLIST_RETRY_TIMEOUT*1000;
		
		/* Send request (CMD_GETPLAYLIST) to load playlist container */
		ret = playlistcontainer_send_request(session, req);
	}
	else if(req->type == REQ_TYPE_PLAYLIST_LOAD) {
		req->next_timeout = get_millisecs() + PLAYLIST_RETRY_TIMEOUT*1000;
		
		/* Send request (CMD_GETPLAYLIST) to load playlist */
		ret = playlist_send_request(session, req);
	}
	
	return ret;
}


/* Initialize a playlist context, called once by sp_session_init() */
void playlistcontainer_create(sp_session *session) {

	session->playlistcontainer = malloc(sizeof(sp_playlistcontainer));
	if(session->playlistcontainer == NULL)
		return;

	session->playlistcontainer->buf = NULL;
	
	session->playlistcontainer->num_playlists = 0;
	session->playlistcontainer->playlists = NULL;

	session->playlistcontainer->num_callbacks = 0;
	session->playlistcontainer->callbacks = NULL;
	session->playlistcontainer->userdata = NULL;
}


/* Add a playlist to the playlist container and notify the main thread */
void playlistcontainer_add_playlist(sp_session *session, sp_playlist *playlist) {
	session->playlistcontainer->playlists = realloc(session->playlistcontainer->playlists, 
					     sizeof(sp_playlist *) * (1 + session->playlistcontainer->num_playlists));
	session->playlistcontainer->playlists[session->playlistcontainer->num_playlists] = playlist;

	/* Set position */
	playlist->position = session->playlistcontainer->num_playlists;
	session->playlistcontainer->num_playlists++;

	/* Notify the main thread we added a playlist */
	request_post_result(session, REQ_TYPE_PC_PLAYLIST_ADD, SP_ERROR_OK, &playlist->position);
}


/* Free resources held by the playlist container, called once by sp_session_release() */
void playlistcontainer_release(sp_session *session) {
	int i;

	for(i = 0; i < session->playlistcontainer->num_playlists; i++)
		playlist_release(session, session->playlistcontainer->playlists[i]);
	
	if(session->playlistcontainer->num_playlists)
		free(session->playlistcontainer->playlists);

	if(session->playlistcontainer->buf)
		buf_free(session->playlistcontainer->buf);

	if(session->playlistcontainer->callbacks)
		free(session->playlistcontainer->callbacks);

	free(session->playlistcontainer);
	session->playlistcontainer = NULL;
}


/* Request playlist container */
static int playlistcontainer_send_request(sp_session *session, struct request *req) {
	unsigned char container_id[17];
	static const char* decl_and_root =
		"<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n<playlist>\n";

	struct callback_ctx *callback_ctx;

	DSFYDEBUG("Requesting playlist container\n");

	/* Free'd by the callback */
	callback_ctx = malloc(sizeof(struct callback_ctx));
	callback_ctx->session = session;
	callback_ctx->req = req;

	session->playlistcontainer->buf = buf_new();
	buf_append_data(session->playlistcontainer->buf, (char*)decl_and_root, strlen(decl_and_root));

	memset(container_id, 0, 17);
	return cmd_getplaylist(session, container_id, ~0, 
				playlistcontainer_callback, callback_ctx);
}


/* Callback for playlist container buffering */
static int playlistcontainer_callback(CHANNEL *ch, unsigned char *payload, unsigned short len) {
	struct callback_ctx *callback_ctx = (struct callback_ctx *)ch->private;

	switch(ch->state) {
	case CHANNEL_DATA:
		buf_append_data(callback_ctx->session->playlistcontainer->buf, payload, len);
		break;

	case CHANNEL_ERROR:
		buf_free(callback_ctx->session->playlistcontainer->buf);
		callback_ctx->session->playlistcontainer->buf = NULL;
		free(callback_ctx);

		DSFYDEBUG("Error on channel '%s', will retry request in %dms\n",
			ch->name, callback_ctx->req->next_timeout - get_millisecs());
		break;

	case CHANNEL_END:
		/* Parse returned XML and request each listed playlist */
		if(playlistcontainer_parse_xml(callback_ctx->session) == 0) {

			/* Create new requests for each playlist found */
			playlistcontainer_request_playlists(callback_ctx->session);

			/* Note we're done loading the playlist container */
			request_set_result(callback_ctx->session, callback_ctx->req,
				SP_ERROR_OK, callback_ctx->session->playlistcontainer);
		}

		buf_free(callback_ctx->session->playlistcontainer->buf);
		callback_ctx->session->playlistcontainer->buf = NULL;
		free(callback_ctx);

		break;

	default:
		break;
	}

	return 0;
}


static int playlistcontainer_parse_xml(sp_session *session) {
	static char *end_element = "</playlist>";
	char *id_list, *idstr;
	unsigned char id[17];
	ezxml_t root, node;
	sp_playlist *playlist;
	sp_playlistcontainer *pc = session->playlistcontainer;

	
	buf_append_data(pc->buf, end_element, strlen(end_element));
	buf_append_u8(pc->buf, 0);
	{
		FILE *fd;
		fd = fopen("playlistcontainer.xml", "w");
		if(fd) {
			fwrite(pc->buf->ptr, pc->buf->len, 1, fd);
			fclose(fd);
		}
	}
	
	root = ezxml_parse_str((char *)pc->buf->ptr, pc->buf->len);
	node = ezxml_get(root, "next-change", 0, "change", 0, "ops", 0, "add", 0, "items", -1);
	id_list = node->txt;

	for(idstr = strtok(id_list, ",\n"); idstr; idstr = strtok(NULL, ",\n")) {
		DSFYDEBUG("Playlist ID '%s'\n", idstr);
	
		hex_ascii_to_bytes(idstr, id, 17);
		playlist = playlist_create(session, id);

		playlistcontainer_add_playlist(session, playlist);
	}


	ezxml_free(root);

	return 0;
}


sp_playlist *playlist_create(sp_session *session, unsigned char id[17]) {
	sp_playlist *playlist;

	
	playlist = (sp_playlist *)malloc(sizeof(sp_playlist));
	if(playlist == NULL)
		return playlist;
	
	memcpy(playlist->id, id, sizeof(playlist->id));

	memset(playlist->name, 0, sizeof(playlist->name));
	playlist->owner = NULL;
	playlist->position = 0;
	playlist->shared = 0;

	playlist->num_tracks = 0;
	playlist->tracks = NULL;
	
	playlist->state = PLAYLIST_STATE_ADDED;
	
	playlist->num_callbacks = 0;
	playlist->callbacks = NULL;
	playlist->userdata = NULL;
	
	playlist->buf = NULL;
	
	return playlist;
}


/* Release resources held by a playlist */
void playlist_release(sp_session *session, sp_playlist *playlist) {
	int i;
	
	if(playlist->buf)
		buf_free(playlist->buf);
	
	if(playlist->owner) {
		/* FIXME: Free sp_user */
	}
	
	for(i = 0; i < playlist->num_tracks; i++)
		sp_track_release(playlist->tracks[i]);
	
	if(playlist->num_tracks)
		free(playlist->tracks);
	
	if(playlist->callbacks)
		free(playlist->callbacks);
	
	free(playlist);
}


/* Set name of playlist and notify main thread */
void playlist_set_name(sp_session *session, sp_playlist *playlist, char *name) {
	strncpy(playlist->name, name, sizeof(playlist->name) - 1);
	playlist->name[sizeof(playlist->name) - 1] = 0;
	
	request_post_result(session, REQ_TYPE_PLAYLIST_RENAME, SP_ERROR_OK, playlist);
}


/* Create new requests for each playlist in the container */
static void playlistcontainer_request_playlists(sp_session *session) {
	sp_playlist **ptr;
	int i;

	for(i = 0; i < session->playlistcontainer->num_playlists; i++) {

		ptr = (sp_playlist **)malloc(sizeof(sp_playlist *));
		*ptr = session->playlistcontainer->playlists[i];

		request_post(session, REQ_TYPE_PLAYLIST_LOAD, ptr);
	}

	DSFYDEBUG("Created %d requests to retrieve playlists\n", i);
}


/* Request a playlist from Spotify */
static int playlist_send_request(sp_session *session, struct request *req) {
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
		if(playlist_parse_xml(callback_ctx->session, playlist) == 0) {
			playlist->state = PLAYLIST_STATE_LISTED;

			/* Create new request for loading tracks */
			osfy_playlist_browse(callback_ctx->session, playlist);
			
			/* Note we're done loading this playlist */
			request_set_result(callback_ctx->session, callback_ctx->req, SP_ERROR_OK, playlist);

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


static int playlist_parse_xml(sp_session *session, sp_playlist *playlist) {
	static char *end_element = "</playlist>";
	char *id_list, *idstr;
	unsigned char track_id[16];
	ezxml_t root, node;
	sp_track *track;

	/* NULL-terminate XML */
	buf_append_data(playlist->buf, end_element, strlen(end_element));
	buf_append_u8(playlist->buf, 0);

	{
		FILE *fd;
		char buf[9 + 35];
		sprintf(buf, "playlist-");
		hex_bytes_to_ascii(playlist->id, buf + 9, 17);
		fd = fopen(buf, "w");
		if(fd) {
			fwrite(playlist->buf->ptr, playlist->buf->len, 1, fd);
			fclose(fd);
		}
	}

	root = ezxml_parse_str((char *)playlist->buf->ptr, playlist->buf->len);

	/* Set playlist name and notify main thread */
	node = ezxml_get(root, "next-change", 0, "change", 0, "ops", 0, "name", -1);
	if(node)
		playlist_set_name(session, playlist, node->txt);

	/* Collaborative playlist? */
	playlist->shared = 0;
        node = ezxml_get(root, "next-change", 0, "change", 0, "ops", 0, "pub", -1);
        if(node && strcmp(node->txt, "1"))
                playlist->shared = 1;


	/* Loop over each track in the playlist and add it */
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


/*
 * Initiate track browsing of a single playlist
 *
 */
int osfy_playlist_browse(sp_session *session, sp_playlist *playlist) {
	int i;
	void **container;
	struct browse_callback_ctx *brctx;
	
	/*
	 * Temporarily increase ref count for the artist so it's not free'd
	 * accidentily. It will be decreaed by the chanel callback.
	 *
	 */
	for(i = 0; i < playlist->num_tracks; i++)
		sp_track_add_ref(playlist->tracks[i]);

	
	/* The playlist callback context */
	brctx = (struct browse_callback_ctx *)malloc(sizeof(struct browse_callback_ctx));
	
	brctx->session = session;
	brctx->req = NULL; /* Filled in by the request processor */
	brctx->buf = NULL; /* Filled in by the request processor */
	
	brctx->type = REQ_TYPE_BROWSE_PLAYLIST_TRACKS;
	brctx->data.playlist = playlist;
	brctx->num_total = playlist->num_tracks;
	brctx->num_browsed = 0;
	brctx->num_in_request = 0;
	
	
	/* Our gzip'd XML parser */
	brctx->browse_parser = osfy_playlist_browse_callback;
	
	/* Request input container. Will be free'd when the request is finished. */
	container = (void **)malloc(sizeof(void *));
	*container = brctx;
	
	return request_post(session, brctx->type, container);
}


static int osfy_playlist_browse_callback(struct browse_callback_ctx *brctx) {
	int i;
	struct buf *xml;
	unsigned char id[16];
	ezxml_t root, track_node, node;
	sp_track *track;
	
	
	/* Decompress the XML returned by track browsing */
	xml = despotify_inflate(brctx->buf->ptr, brctx->buf->len);
	{
		FILE *fd;
		char buf[35];
		hex_bytes_to_ascii(brctx->data.playlist->id, buf, 17);
		DSFYDEBUG("Decompresed %d bytes data for playlist '%s', xml=%p, saving raw XML to browse-playlist.xml\n",
			  brctx->buf->len, buf, xml);
		fd = fopen("browse-playlists.xml", "w");
		if(fd) {
			fwrite(xml->ptr, xml->len, 1, fd);
			fclose(fd);
		}
	}
	

	/* Load XML */
	root = ezxml_parse_str((char *) xml->ptr, xml->len);
	if(root == NULL) {
		DSFYDEBUG("Failed to parse XML\n");
		buf_free(xml);
		return -1;
	}
	

	/* Loop over each track in the list */
	for(i = 1, track_node = ezxml_get(root, "tracks", 0, "track", -1);
	    track_node;
	    track_node = track_node->next, i++) {
		
		/* Get ID of track */
		node = ezxml_get(track_node, "id", -1);
		hex_ascii_to_bytes(node->txt, id, 16);
		
		/* We'll simply use ofsy_track_add() to find a track by its ID */
		track = osfy_track_add(brctx->session, id);
		{
			char buf[33];
			hex_bytes_to_ascii(track->id, buf, 16);
			DSFYDEBUG("osfy_track_add(%s) gave track with ID '%s'\n", node->txt, buf);
		}
		
		
		
		/* Skip loading of already loaded tracks */
		if(sp_track_is_loaded(track)) {
			DSFYDEBUG("Track '%s' (%d in playlist) is already loaded\n", node->txt, i);
			continue;
		}
		
		{
		char buf[35];
		hex_bytes_to_ascii(brctx->data.playlist->id, buf, 17);
		DSFYDEBUG("Loading track '%s' (ref_count %d) in playlist '%s' (track number %d)\n", node->txt, track->ref_count, buf, i);
		
		}

		/* Load the track from XML */
		osfy_track_load_from_xml(brctx->session, track, track_node);
	}

	/* Free XML structures and buffer */
	ezxml_free(root);
	buf_free(xml);

	
	/* Release references made in osfy_playlist_browse() */
	for(i = 0; i < brctx->num_in_request; i++)
		sp_track_release(brctx->data.playlist->tracks[brctx->num_browsed + i]);
	
	
	return 0;
}


/* Calculate a playlist checksum. */
unsigned int playlist_checksum(sp_playlist *playlist) {
	unsigned int checksum = 1L;
	unsigned char id[17];
	int i;

	if(playlist == NULL)
		return checksum;

	/* Loop over all tracks (make sure the last byte is 0x01). */
	for(i = 0; i < playlist->num_tracks; i++) {
		memcpy(id, playlist->tracks[i], 16);
		id[16] = 0x01;

		checksum = adler32(checksum, id, 17);
	}

	return checksum;
}


/* Calculate a playlists container checksum. */
unsigned int playlistcontainer_checksum(sp_playlistcontainer *pc) {
	unsigned int checksum = 1L;
	unsigned char id[17];
	int i;

	if(pc == NULL)
		return checksum;

	/* Loop over all playlists (make sure the last byte is 0x02). */
	for(i = 0; i < pc->num_playlists; i++) {
		memcpy(id, pc->playlists[i]->id, 16);

		id[16] = 0x02;
		checksum = adler32(checksum, id, 17);
	}

	return checksum;
}
