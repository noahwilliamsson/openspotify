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

#include "debug.h"
#include "request.h"
#include "login.h"
#include "network.h"
#include "request.h"
#include "sp_opaque.h"


SP_LIBEXPORT(sp_error) sp_session_init (const sp_session_config *config, sp_session **sess) {
	sp_session *s;

	if(!config) // XXX - verify
		return SP_ERROR_INVALID_INDATA;

	if(config->api_version != 1)
		return SP_ERROR_BAD_API_VERSION;


	if((s = (sp_session *)malloc(sizeof(sp_session))) == NULL)
		return SP_ERROR_API_INITIALIZATION_FAILED;

	memset(s, 0, sizeof(sp_session));
	

	s->callbacks = (sp_session_callbacks *)malloc(sizeof(sp_session_callbacks));
	memcpy(s->callbacks, config->callbacks, sizeof(sp_session_callbacks));


	s->connectionstate = SP_CONNECTION_STATE_UNDEFINED;
	s->userdata = config->userdata;

	/* Low-level networking stuff */
	s->sock = -1;

	/* Incoming packet buffer */
	s->packet = NULL;

	/* To allow main thread to communicate with network thread */
	s->requests = NULL;

	/* Spawn networking thread */
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

	DSFYDEBUG("Posting REQ_TYPE_LOGOUT\n");
	request_post(session, REQ_TYPE_LOGOUT, NULL);

	return SP_ERROR_OK;
}


SP_LIBEXPORT(sp_user *) sp_session_user(sp_session *session) {
	static sp_user *user;

	/* FIXME: Ugly hack.. */
	if(user == NULL)
		user = malloc(sizeof(sp_user));

	if(user) {
		user->canonical_name = session->username;
		user->display_name = NULL;
		user->next = NULL;
	}

	return user;
}


SP_LIBEXPORT(void *) sp_session_userdata(sp_session *session) {

	return session->userdata;
}


SP_LIBEXPORT(void) sp_session_process_events(sp_session *session, int *next_timeout) {
	struct sp_request *request;

	while((request = request_fetch_next_result(session)) != NULL) {
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

		default:
			break;
		}


		/* Now that we've delievered the result, mark it for deletion */
		request_mark_processed(session, request);
	}


	*next_timeout = 1000;
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
	DSFYDEBUG("FIXME: Not yet implemented\n");

	return NULL;
}


/*
 * Not present in the official library
 * XXX - Might not be thread safe?
 *
 */
SP_LIBEXPORT(sp_error) sp_session_release (sp_session *session) {

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

	free(session->callbacks);

	free(session);

	DSFYDEBUG("Session released\n");

	return SP_ERROR_OK;
}
