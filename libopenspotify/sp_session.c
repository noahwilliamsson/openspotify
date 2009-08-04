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
	

	s->connectionstate = SP_CONNECTION_STATE_UNDEFINED;
	s->userdata = config->userdata;

	/* Low-level networking stuff */
	s->sock = -1;

	/* To allow main thread to communicate with network thread */
	s->requests = NULL;

	/* Spawn networking thread */
#ifdef _WIN32
	s->thr_main = GetCurrentThread();
	s->thr_network = CreateThread(NULL, 0, network_thread, s, 0, NULL);
#else
	s->thr_main = pthread_self();
	if(pthread_create(&s->thr_network, NULL, network_thread, s))
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

	DSFYDEBUG("Posting REQ_LOGIN\n");
	request_post(session, REQ_LOGIN);

	return SP_ERROR_OK;
}

SP_LIBEXPORT(sp_connectionstate) sp_session_connectionstate (sp_session *session) {
	DSFYDEBUG("Returning connection state %d\n", session->connectionstate);

	return session->connectionstate;
}


SP_LIBEXPORT(sp_error) sp_session_logout (sp_session *session) {

	DSFYDEBUG("Posting REQ_LOGOUT\n");
	request_post(session, REQ_LOGOUT);

	return SP_ERROR_OK;
}

/*
sp_user * 	sp_session_user (sp_session *session)
void * 	sp_session_userdata (sp_session *session)
void 	sp_session_process_events (sp_session *session, int *next_timeout)
sp_error 	sp_session_player_load (sp_session *session, sp_track *track)
sp_error 	sp_session_player_seek (sp_session *session, int offset)
sp_error 	sp_session_player_play (sp_session *session, bool play)
void 	sp_session_player_unload (sp_session *session)
sp_playlistcontainer * 	sp_session_playlistcontainer (sp_session *session)
*/


/*
 * Not present in the official library
 *
 */
SP_LIBEXPORT(sp_error) sp_session_release (sp_session *session) {


	/* Terminate networking thread */
	DSFYDEBUG("Terminating network thread\n");
#ifdef _WIN32
	TerminateThread(session->thr_network, 0);
	session->thr_network = (HANDLE)0;
#else
	pthread_cancel(session->thr_network);
	pthread_join(session->thr_network, NULL);
	session->thr_network = (pthread_t)0;
#endif

	if(session->login)
		login_release(session->login);

	free(session);

	DSFYDEBUG("Session released\n");

	return SP_ERROR_OK;
}
