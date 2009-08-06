/*
 * Code to deal with internal libopenspotify playlist retrieving
 *
 * Program flow:
 *
 * + network_thread()
 * +--+ playlist_process() with state = 0
 * |  +--+ playlist_request_container()
 * |  |  +--+ cmd_getplaylist()
 * |  |     +--+ channel_register() with callback playlist_container_callback()
 * |  +--- Set playlist_ctx->state = 1
 * .  .
 * .  .
 * +--+ packet_read_and_process()
 * |   +--+ handle_channel()
 * |      +--+ channel_process()
 * |         +--+ playlist_container_callback()
 * |            +--- Buffer XML-data
 * |            +--- Set playlist_ctx->state = 2
 * .
 * .
 * +--+ playlist_process() with state = 2
 * |  +--+ playlist_parse_container_xml()
 * |     +--+ Set playlist_ctx->state = 3
 * |
 * +--+ playlist_process() with state = 3
 * |  +--+ playlist_load_playlists()
 * |  |  +--+ cmd_getplaylist()
 * |  |  |  +--+ channel_register() with callback playlist_callback()
 * |  |  |
 * |  |  +--+ cmd_browsetrack()
 * |  |     +--+ channel_register() with callback playlist_browsetrack_callback()
 * |  |
 * .  .
 * .  .
 * +--+ packet_read_and_process() 
 * |   +--+ handle_channel()
 * |      +--+ channel_process()
 * |         +--+ playlist_callback()
 * |            +--- Buffer XML-data
 * |            +--+ At channel end, call playlist_parse_playlist_xml()
 * |               +--+ playlist_parse_playlist_xml()
 * |                  +--+ Set playlist->state = PLAYLIST_STATE_LISTED
 * +--+ packet_read_and_process() 
 * |   +--+ handle_channel()
 * |      +--+ channel_process()
 * |         +--+ playlist_browsetrack_callback()
 * |            +--- Buffer XML-data
 * .
 * .
 * +--- DONE
 * |
 *     
 */

#include <string.h>

#include "channel.h"
#include "commands.h"
#include "debug.h"
#include "ezxml.h"
#include "sp_opaque.h"
#include "util.h"
#include "adler32.h"

static int playlist_request_container(sp_session *session);
static int playlist_container_callback(CHANNEL *ch, unsigned char *payload, unsigned short len);
static int playlist_parse_container_xml(sp_session *session);

static int playlist_load_playlists(sp_session *session);
static int playlist_callback(CHANNEL *ch, unsigned char *payload, unsigned short len);
static int playlist_parse_playlist_xml(sp_playlist *playlist);

/* Calculate a playlist checksum. */
unsigned long playlist_checksum(sp_playlist *playlist) {
	unsigned long checksum = 1L;
	int i;

	if(playlist == NULL)
		return 1L;

	/* Loop over all tracks (make sure the last byte is 0x01). */
	for(i = 0; i < playlist->num_tracks; i++){
		playlist->tracks[i]->id[16] = 0x01;

		checksum = adler32_update(checksum, playlist->tracks[i]->id, 17);
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

		checksum = adler32_update(checksum, playlist->id, 17);
	}

	return checksum;
}

/* For initializing the playlist context, called by sp_session_init() */
struct playlist_ctx *playlist_create(void) {
	struct playlist_ctx *playlist_ctx;

	playlist_ctx = malloc(sizeof(playlist_ctx));
	if(playlist_ctx == NULL)
		return NULL;

	playlist_ctx->state = 0;
	playlist_ctx->buf = NULL;

	playlist_ctx->container = malloc(sizeof(sp_playlistcontainer));
	playlist_ctx->container->playlists = NULL;

	/* FIXME: Should be an array of callbacks and userdatas */
	playlist_ctx->container->userdata = NULL;
	playlist_ctx->container->callbacks = malloc(sizeof(sp_playlistcontainer_callbacks));
	memset(playlist_ctx->container->callbacks, 0, sizeof(sp_playlistcontainer_callbacks));

	return playlist_ctx;
}


/* For free'ing the playlist context, called by sp_session_release() */
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

			for(i = 0; i < playlist->num_tracks; i++) {
				/* FIXME: Unload track */
				free(playlist->tracks[i]);
			}

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
int playlist_process(sp_session *session) {
	int ret;

	switch(session->playlist_ctx->state) {
	case 0:
		/* Request playlist container */
		ret = playlist_request_container(session);
		if(ret == 0)
			session->playlist_ctx->state++;

		break;

	case 1:
		/* Do nothing, the channel callback will increase state */
		break;
		

	case 2:
		/* Parse playlist container */
		ret = playlist_parse_container_xml(session);
		if(ret == 0)
			session->playlist_ctx->state++;

		break;

	case 3:
		/* Update playlists (track IDs and track data) and retry fails ones */
		ret = playlist_load_playlists(session);
		if(ret < 0 || ret == 0)
			break;

		session->playlist_ctx->state++;
		ret = 0;
		break;

	case 4:
		/* FIXME: Done loading playlists */
		break;
	
	default:
		break;
	}

	return ret;
}


/* Request playlist container */
static int playlist_request_container(sp_session *session) {
	unsigned char container_id[17];

	DSFYDEBUG("Requesting available playlist\n");

	session->playlist_ctx->buf = buf_new();

	memset(container_id, 0, 17);
	return cmd_getplaylist(session, container_id, ~0, 
				playlist_container_callback, session->playlist_ctx);
}


/* Callback for playlist container buffering */
static int playlist_container_callback(CHANNEL *ch, unsigned char *payload, unsigned short len) {
	struct playlist_ctx *playlist_ctx = (struct playlist_ctx *)ch->private;

	switch(ch->state) {
	case CHANNEL_DATA:
		buf_append_data(playlist_ctx->buf, payload, len);
		break;

	case CHANNEL_ERROR:
		/* FIXME */
		buf_free(playlist_ctx->buf);
		playlist_ctx->buf = NULL;
		DSFYDEBUG("CHANNEL ERROR\n");
		break;

	case CHANNEL_END:
		playlist_ctx->state = 2;
		break;

	default:
		break;
	}

	return 0;
}


static int playlist_parse_container_xml(sp_session *session) {
	char *id_list, *id;
	char idstr[35];
	int position;
	ezxml_t root, node;
	sp_playlist *playlist;
	sp_playlistcontainer *container;

	/* NULL-terminate XML */
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

		memcpy(playlist->id, id, sizeof(playlist->id));
		playlist->num_tracks = 0;
		playlist->tracks = NULL;

		/* FIXME: Pull this info from XML ? */
		playlist->name = NULL;
		playlist->owner = NULL;

		playlist->lastrequest = 0;
		playlist->state = PLAYLIST_STATE_ADDED;

		playlist->buf = NULL;
		playlist->next = NULL;

		/* FIXME: Probably shouldn't carry around playlist positions in the playlist itself! */
		playlist->position = position;

		
		/* FIXME: Post a request instead? */
		if(container->callbacks->playlist_added)
			container->callbacks->playlist_added(container, playlist, position, container->userdata);
	}
	

	ezxml_free(root);

	buf_free(session->playlist_ctx->buf);
	session->playlist_ctx->buf = NULL;

	return 0;
}


/* Request playlist that haven't been loaded yet */
static int playlist_load_playlists(sp_session *session) {
	int ret = 0;
	char idstr[35];
	sp_playlist *playlist;

	int need_load_track_ids;

	for(playlist = session->playlist_ctx->container->playlists;
		playlist; playlist = playlist->next) {

		hex_bytes_to_ascii((unsigned char *)playlist->id, idstr, 17);


		if(playlist->state == PLAYLIST_STATE_NEW) {
			/* Skip playlists with no ID */
			continue;
		}


		if(playlist->state == PLAYLIST_STATE_LOADED) {
			/* FIXME: Browse tracks */


			continue;
		}
		


		need_load_track_ids = 0;
		if(playlist->state == PLAYLIST_STATE_ADDED) {
#ifdef _WIN32
			if(playlist->lastrequest + PLAYLIST_RETRY_TIMEOUT*1000 < GetTickCount()) {
#else
			if(playlist->lastrequest + PLAYLIST_RETRY_TIMEOUT < time(NULL)) {
#endif
				need_load_track_ids = 1;
			}
		}


		if(!need_load_track_ids)
			continue;


		ret =  cmd_getplaylist(session, playlist->id, ~0, 
				playlist_callback, playlist);

		if(ret < 0) {
			DSFYDEBUG("cmd_getplaylist() failed for playlist with ID '%s'\n", idstr);
			return ret;
		}
#ifdef _WIN32
		DSFYDEBUG("Requested playlist with ID '%s' since last request at %ds (since boot) is less than %d\n",
				idstr, playlist->lastrequest / 1000, GetTickCount() / 1000 - PLAYLIST_RETRY_TIMEOUT);
		playlist->lastrequest = GetTickCount();
#else
		DSFYDEBUG("Requested playlist with ID '%s' since last request at %ds (since 1970) is less than %d\n",
				idstr, playlist->lastrequest, time(NULL) - PLAYLIST_RETRY_TIMEOUT);
		playlist->lastrequest = time(NULL);
#endif
	}

	return 0;
}


/* Callback for playlist container buffering */
static int playlist_callback(CHANNEL *ch, unsigned char *payload, unsigned short len) {
	sp_playlist *playlist = (sp_playlist *)ch->private;

	switch(ch->state) {
	case CHANNEL_DATA:
		buf_append_data(playlist->buf, payload, len);
		break;

	case CHANNEL_ERROR:
		/* FIXME */
		buf_free(playlist->buf);
		playlist->buf = NULL;
		DSFYDEBUG("CHANNEL ERROR\n");
		break;

	case CHANNEL_END:
		/* This will set playlist->state == PLAYLIST_STATE_LISTED */
		playlist_parse_playlist_xml(playlist);
		break;

	default:
		break;
	}

	return 0;
}


static int playlist_parse_playlist_xml(sp_playlist *playlist) {
	char *id_list, *id;
	char idstr[35];
	int position;
	ezxml_t root, node;
	sp_track *track;

	/* NULL-terminate XML */
	buf_append_u8(playlist->buf, 0);

	root = ezxml_parse_str((char *)playlist->buf->ptr, playlist->buf->len);

	/* FIXME: Find out what XML looks like and then make appropriate changes */
	node = ezxml_get(root, "next-change", 0, "change", 0, "ops", 0, "add", 0, "items", -1);
	id_list = node->txt;
	
	for(id = strtok(id_list, ",\n"); id; id = strtok(NULL, ",\n")) {
		hex_bytes_to_ascii((unsigned char *)id, idstr, 17);
		DSFYDEBUG("Track ID '%s'\n", idstr);
	
		position = 0;
		playlist->tracks = (sp_track **)realloc(playlist->tracks, (playlist->num_tracks + 1) * sizeof(sp_track *));
		playlist->tracks[playlist->num_tracks] = malloc(sizeof(sp_track));
		track = playlist->tracks[playlist->num_tracks];
		playlist->num_tracks++;
		
		memcpy(track->id, id, sizeof(track->id));
		track->error = SP_ERROR_OK; /* Until we know better.. */
		track->loaded = 0;
		track->duration = 0; /* FIXME: From XML */
		track->index = position;

		playlist->state = PLAYLIST_STATE_LISTED;
		
		/* FIXME: Post a request instead? */
		/* FIXME: Need to support callbacks for multiple "listeners" */
		if(playlist->callbacks && playlist->callbacks->tracks_added)
			playlist->callbacks->tracks_added(playlist, playlist->tracks,
								1,
								playlist->num_tracks - 1,
								playlist->userdata);
	}
	

	ezxml_free(root);

	buf_free(playlist->buf);
	playlist->buf = NULL;

	return 0;
}

