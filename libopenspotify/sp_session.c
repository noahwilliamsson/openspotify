#include <stdlib.h>
#include <string.h>
#include <assert.h>
#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#else
#include <pthread.h>
#endif

#include <spotify/api.h>

#include "cache.h"
#include "debug.h"
#include "request.h"
#include "link.h"
#include "login.h"
#include "network.h"
#include "playlist.h"
#include "sp_opaque.h"


SP_LIBEXPORT(sp_error) sp_session_init (const sp_session_config *config, sp_session **sess) {
	sp_session *s;

	if(!config) // XXX - verify
		return SP_ERROR_INVALID_INDATA;

	/* Check if API version matches. */
	if(config->api_version != 1)
		return SP_ERROR_BAD_API_VERSION;

	/* Maximum user-agent length is 4096 bytes (including null-terminator). */
	if(config->user_agent == NULL || strlen(config->user_agent) > 4095)
		return SP_ERROR_BAD_USER_AGENT;

	/* Application key needs to have 321 bytes with the first byte being 0x01. */
	if(config->application_key == NULL || config->application_key_size != 321 ||
		((char *)config->application_key)[0] != 0x01)
		return SP_ERROR_BAD_APPLICATION_KEY;

	/* Allocate memory for our session. */
	if((s = (sp_session *)malloc(sizeof(sp_session))) == NULL)
		return SP_ERROR_API_INITIALIZATION_FAILED;

	memset(s, 0, sizeof(sp_session));
	
	/* Allocate memory for callbacks and copy them to our session. */
	s->callbacks = (sp_session_callbacks *)malloc(sizeof(sp_session_callbacks));
	
	memcpy(s->callbacks, config->callbacks, sizeof(sp_session_callbacks));

	/* Connection state is undefined (We were never logged in).*/
	s->connectionstate = SP_CONNECTION_STATE_UNDEFINED;
	s->userdata        = config->userdata;

	/* Login context, needed by network.c and login.c */
	s->login = NULL;

	/* Playlist container object */
	playlist_create(s);

	/* Albums/artists/tracks memory management */
	s->hashtable_albums = hashtable_create(16);
	s->hashtable_albumbrowses = hashtable_create(16);
	s->hashtable_artistbrowses = hashtable_create(16);
	s->hashtable_artists = hashtable_create(16);
	s->hashtable_images = hashtable_create(20);
	s->hashtable_tracks = hashtable_create(16);

	/* Allocate memory for user info. */
	if((s->user = (sp_user *)malloc(sizeof(sp_user))) == NULL)
		return SP_ERROR_API_INITIALIZATION_FAILED;

	/* Low-level networking stuff. */
	s->sock = -1;

	/* Incoming packet buffer */
	s->packet = NULL;

	/* To allow main thread to communicate with network thread */
	s->requests = NULL;

	/* Channels */
	s->channels = NULL;
	s->next_channel_id = 0;
	s->num_channels = 0;


	/* Spawn networking thread. */
#ifdef _WIN32
	s->request_mutex = CreateMutex(NULL, FALSE, NULL);
	s->idle_wakeup = CreateEvent(NULL, FALSE, FALSE, NULL);
	s->thread_main = GetCurrentThread();
	s->thread_network = CreateThread(NULL, 0, network_thread, s, 0, NULL);
#else
	pthread_mutex_init(&s->request_mutex, NULL);
	pthread_cond_init(&s->idle_wakeup, NULL);

	s->thread_main = pthread_self();
	if(pthread_create(&s->thread_network, NULL, network_thread, s))
		return SP_ERROR_OTHER_TRANSIENT;
#endif

	/* Helper function for sp_link_create_from_string() */
	libopenspotify_link_init(s);

	/* Load album, artist and track cache */
	cache_init(s);

	/* Run garbage collector and save metadata to disk periodically */
	request_post(s, REQ_TYPE_CACHE_PERIODIC, NULL);

	DSFYDEBUG("Session initialized at %p\n", s);
	
	*sess = s;

	return SP_ERROR_OK;
}


SP_LIBEXPORT(sp_error) sp_session_login (sp_session *session, const char *username, const char *password) {
	strncpy(session->username, username, sizeof(session->username) - 1);
	session->username[sizeof(session->username) - 1] = 0;

	strncpy(session->password, password, sizeof(session->password) - 1);
	session->password[sizeof(session->password) - 1] = 0;

	DSFYDEBUG("Posting REQ_TYPE_LOGIN\n");
	request_post(session, REQ_TYPE_LOGIN, NULL);

	return SP_ERROR_OK;
}

SP_LIBEXPORT(sp_connectionstate) sp_session_connectionstate (sp_session *session) {
	DSFYDEBUG("Returning connection state %d\n", session->connectionstate);

	return session->connectionstate;
}


SP_LIBEXPORT(sp_error) sp_session_logout (sp_session *session) {
	if(session->login) {
		login_release(session->login);
		session->login = NULL;
	}


	DSFYDEBUG("Posting REQ_TYPE_LOGOUT\n");
	request_post(session, REQ_TYPE_LOGOUT, NULL);

	return SP_ERROR_OK;
}


SP_LIBEXPORT(sp_user *) sp_session_user(sp_session *session) {
	/* TODO: fetch that info via command 0x57. */
	if(session->user){
		session->user->canonical_name = session->username;
		session->user->display_name   = session->username;
		session->user->next           = NULL;
	}

	return session->user;
}


SP_LIBEXPORT(void *) sp_session_userdata(sp_session *session) {
	return session->userdata;
}


SP_LIBEXPORT(void) sp_session_process_events(sp_session *session, int *next_timeout) {
	struct request *request;
	sp_albumbrowse *alb;
	sp_artistbrowse *arb;
	sp_image *image;

	while((request = request_fetch_next_result(session, next_timeout)) != NULL) {
		DSFYDEBUG("Processing finished request of type %d, error %d\n",
			request->type, request->error);

		/* FIXME: Verify that these callbacks are indeed called from the main thread! */
		switch(request->type) {
		case REQ_TYPE_LOGIN:
			if(session->callbacks->logged_in == NULL)
				break;

			session->callbacks->logged_in(session, request->error);
			break;
			
		case REQ_TYPE_LOGOUT:
			if(session->callbacks->logged_out == NULL)
				break;

			session->callbacks->logged_out(session);
			break;

		case REQ_TYPE_NOTIFY:
			if(session->callbacks->message_to_user == NULL)
				break;

			/* We'll leak memory here for each login made :( */
			session->callbacks->message_to_user(session, request->output);
			break;

		case REQ_TYPE_ALBUMBROWSE:
			alb = (sp_albumbrowse *)request->output;
			if(alb->callback)
				alb->callback(alb, alb->userdata);

			break;

		case REQ_TYPE_ARTISTBROWSE:
	                arb = (sp_artistbrowse *)request->output;
	                if(arb->callback)
	                        arb->callback(arb, arb->userdata);

			break;

		case REQ_TYPE_BROWSE_ALBUM:
		case REQ_TYPE_BROWSE_ARTIST:
		case REQ_TYPE_BROWSE_TRACK:
			DSFYDEBUG("Metadata updated for request <type %s, state %s, input %p> in main thread\n",
				  REQUEST_TYPE_STR(request->type), REQUEST_STATE_STR(request->type), request->input);
			session->callbacks->metadata_updated(session);
			break;

		case REQ_TYPE_IMAGE:
			image = (sp_image *)request->output;
			if(image->callback)
				image->callback(image, image->userdata);
			break;

		default:
			break;
		}


		/* Now that we've delievered the result, mark it for deletion */
		request_mark_processed(session, request);
	}

}


SP_LIBEXPORT(sp_error) sp_session_player_load(sp_session *session, sp_track *track) {
	DSFYDEBUG("FIXME: Not yet implemented\n");

	return SP_ERROR_OK;
}


SP_LIBEXPORT(sp_error) sp_session_player_seek(sp_session *session, int offset) {
	DSFYDEBUG("FIXME: Not yet implemented\n");

	return SP_ERROR_OK;
}


SP_LIBEXPORT(sp_error) sp_session_player_play(sp_session *session, bool play) {
	DSFYDEBUG("FIXME: Not yet implemented\n");

	return SP_ERROR_OK;
}


SP_LIBEXPORT(void) sp_session_player_unload(sp_session *session) {
	DSFYDEBUG("FIXME: Not yet implemented\n");

}


SP_LIBEXPORT(sp_playlistcontainer *) sp_session_playlistcontainer(sp_session *session) {

	/* FIXME: Docs says "return ... for the currently logged in user. What if not logged in? */
	return session->playlistcontainer;
}


/*
 * Not present in the official library
 * XXX - Might not be thread safe?
 *
 */
SP_LIBEXPORT(sp_error) sp_session_release (sp_session *session) {

	/* Unregister channels */
	DSFYDEBUG("Unregistering any active channels\n");
	channel_fail_and_unregister_all(session);

	/* Kill networking thread */
	DSFYDEBUG("Terminating network thread\n");
#ifdef _WIN32
	TerminateThread(session->thread_network, 0);
	session->thread_network = (HANDLE)0;
	CloseHandle(session->idle_wakeup);
	CloseHandle(session->request_mutex);
#else
	pthread_cancel(session->thread_network);
	pthread_join(session->thread_network, NULL);
	session->thread_network = (pthread_t)0;
	pthread_mutex_destroy(&session->request_mutex);
	pthread_cond_destroy(&session->idle_wakeup);
#endif

	if(session->packet)
		buf_free(session->packet);

	if(session->login)
		login_release(session->login);

	playlist_release(session);

	if(session->hashtable_albums)
		hashtable_free(session->hashtable_albums);

	if(session->hashtable_albumbrowses)
		hashtable_free(session->hashtable_albumbrowses);

	if(session->hashtable_artists)
		hashtable_free(session->hashtable_artists);

	if(session->hashtable_artistbrowses)
		hashtable_free(session->hashtable_artistbrowses);

	if(session->hashtable_images)
		hashtable_free(session->hashtable_images);

	if(session->hashtable_tracks)
		hashtable_free(session->hashtable_tracks);

	free(session->callbacks);

	/* Helper function for sp_link_create_from_string() */
	libopenspotify_link_release();

	free(session);

	DSFYDEBUG("Session released\n");

	return SP_ERROR_OK;
}
