/*
 * This file handles libopenspotify's communication with Spotify's service
 * and is run in a seperate thread.
 *
 * Communication with the main thread is done via calls in request.c
 *
 */


#include <string.h>
#ifdef _WIN32
#include <ws2tcpip.h>
#include <windows.h>
#else
#include <unistd.h>
#include <pthread.h>
#endif
#include <spotify/api.h>

#include "browse.h"
#include "cache.h"
#include "channel.h"
#include "debug.h"
#include "image.h"
#include "iothread.h"
#include "login.h"
#include "packet.h"
#include "player.h"
#include "playlist.h"
#include "request.h"
#include "search.h"
#include "shn.h"
#include "sp_opaque.h"
#include "toplistbrowse.h"
#include "user.h"
#include "util.h"


static int process_request(sp_session *s, struct request *req);
static int process_login_request(sp_session *s, struct request *req);
static int process_logout_request(sp_session *s, struct request *req);


/*
 * The thread routine started by sp_session_init()
 * It does not return but can be terminated by sp_session_release()
 *
 */

#ifdef _WIN32
DWORD WINAPI iothread(LPVOID data) {
#else
void *iothread(void *data) {
#endif
	sp_session *s = (sp_session *)data;
	struct request *req;
	int ret;
	int now;

#ifdef _WIN32
	/* Initialize Winsock */
	WSADATA wsadata;
	WSAStartup(MAKEWORD(2,2), &wsadata);
#endif

	for(;;) {
		request_cleanup(s);

		/* No locking needed since we're in control of request_cleanup() */
		now = get_millisecs();
		for(req = s->requests; req; req = req->next) {
			if(req->state != REQ_STATE_NEW && req->state != REQ_STATE_RUNNING)
				continue;

			if(req->next_timeout > now)
				continue;

			DSFYDEBUG("Processing request <type %s, state %s, input %p, timeout %d>\n",
					REQUEST_TYPE_STR(req->type), REQUEST_STATE_STR(req->state),
					req->input, req->next_timeout);
			ret = process_request(s, req);
			DSFYDEBUG("Request processing returned %d\n", ret);

			/* FIXME: Drop connection on errors! */
			if(ret != 0) {
				DSFYDEBUG("Request failed, good bye\n");
#ifdef _WIN32
#else
				exit(1);
#endif
			}
		}


		/* Packets can only be processed once we're logged in */
		if(s->connectionstate != SP_CONNECTION_STATE_LOGGED_IN) {
#ifdef _WIN32
			WaitForSingleObject(s->request_mutex, INFINITE);
#else
			pthread_mutex_lock(&s->request_mutex);
#endif
			if(s->requests == NULL) {
				DSFYDEBUG("Sleeping because there's nothing to do\n");
#ifdef _WIN32
				ReleaseMutex(s->request_mutex);
				WaitForSingleObject(s->idle_wakeup, INFINITE);
				WaitForSingleObject(s->request_mutex, INFINITE);
#else
				pthread_cond_wait(&s->idle_wakeup, &s->request_mutex);
#endif
				DSFYDEBUG("Woke up, a new request was posted\n");
			}
#ifdef _WIN32
			ReleaseMutex(s->request_mutex);
#else
			pthread_mutex_unlock(&s->request_mutex);
#endif

			continue;
		}


		/*
		 * Read and process zero or more packets
		 * Will sleep somewhere around 64ms if no
		 * data is available
		 */
		ret = packet_read_and_process(s);
		if(ret < 0) {
			DSFYDEBUG("process_packets() returned %d, disconnecting!\n", ret);
#ifdef _WIN32
			closesocket(s->sock);
#else
			close(s->sock);
#endif
			s->sock = -1;

			s->connectionstate = SP_CONNECTION_STATE_DISCONNECTED;

			request_post_result(s, REQ_TYPE_LOGOUT, SP_ERROR_OTHER_TRANSIENT, NULL);
		}
	}


}


/*
 * Route request handling to the appropriate handlers
 *
 */
static int process_request(sp_session *session, struct request *req) {
	int now = get_millisecs();

	if(session->connectionstate != SP_CONNECTION_STATE_LOGGED_IN
		&& (req->type != REQ_TYPE_LOGIN && req->type != REQ_TYPE_LOGOUT)) {
		if(req->state == REQ_STATE_NEW) {
			DSFYDEBUG("Postponing request <type %s, state %s, input %p> 10 seconds due to not logged in\n",
					REQUEST_TYPE_STR(req->type), REQUEST_STATE_STR(req->state), req->input);
			req->next_timeout = now + 10*1000;

			return 0;
		}
		else if(req->state == REQ_STATE_RUNNING) {
			DSFYDEBUG("Failing request <type %s, state %s, input %p> due to not logged in\n",
					REQUEST_TYPE_STR(req->type), REQUEST_STATE_STR(req->state), req->input);
			return request_set_result(session, req, SP_ERROR_OTHER_TRANSIENT, NULL);
		}
	}
	else if(req->state == REQ_STATE_NEW && session->num_channels >= 16) {
		DSFYDEBUG("%d channels active, postponing request <type %s, state %s, input %p>\n",
			  session->num_channels, REQUEST_TYPE_STR(req->type),
			  REQUEST_STATE_STR(req->state), req->input);

		if(req->next_timeout < now + 100)
			req->next_timeout = now + 100;
		
		return 0;
	}

	
	switch(req->type) {
	case REQ_TYPE_LOGIN:
		return process_login_request(session, req);
		break;

	case REQ_TYPE_LOGOUT:
		return process_logout_request(session, req);
		break;
	
	case REQ_TYPE_PC_LOAD:
	case REQ_TYPE_PLAYLIST_LOAD:
	case REQ_TYPE_PLAYLIST_CHANGE:
		return playlist_process(session, req);
		break;
	
	case REQ_TYPE_TOPLISTBROWSE:
		return toplistbrowse_process_request(session, req);
		break;

	case REQ_TYPE_SEARCH:
		return search_process_request(session, req);
		break;

	case REQ_TYPE_USER:
		return user_process_request(session, req);
		break;

	case REQ_TYPE_IMAGE:
		return osfy_image_process_request(session, req);
		break;

	case REQ_TYPE_ALBUMBROWSE:
	case REQ_TYPE_ARTISTBROWSE:
	case REQ_TYPE_BROWSE_ALBUM:
	case REQ_TYPE_BROWSE_ARTIST:
	case REQ_TYPE_BROWSE_PLAYLIST_TRACKS:
	case REQ_TYPE_BROWSE_TRACK:
		return browse_process(session, req);
		break;

	case REQ_TYPE_PLAYER_KEY:
	case REQ_TYPE_PLAYER_SUBSTREAM:
	case REQ_TYPE_PLAY_TOKEN_ACQUIRE:
	case REQ_TYPE_PLAY_TOKEN_LOST:
		return player_process_request(session, req);
		break;

	case REQ_TYPE_CACHE_PERIODIC:
		return cache_process(session, req);
		break;

	default:
		DSFYDEBUG("BUG: Unhandled request type '%s'\n", REQUEST_TYPE_STR(req->type));
		break;
	}

	return 0;
}


/*
 * High-level handling of login requests
 * Uses login specific routines from login.c
 *
 */
static int process_login_request(sp_session *s, struct request *req) {
	int ret;
	sp_error error;
	unsigned char key_recv[32], key_send[32];

	if(req->state == REQ_STATE_NEW) {
		req->state = REQ_STATE_RUNNING;

		s->login = login_create(s->username, s->password);
		if(s->login == NULL)
			return request_set_result(s, req, SP_ERROR_OTHER_TRANSIENT, NULL);
	}

	/*
	 * A call to sp_session_logout() will post a REQ_TYPE_LOGOUT
	 * It will trigger a call to process_logout_request() which in turn
	 * will call login_release() on s->login and set it to NULL.
	 * 
	 * We check for this condition here and return SP_ERROR_OTHER_TRANSIENT
	 *
	 */
	if(s->login == NULL) {
		/* Fail login with SP_ERROR_OTHER_TRANSIENT */
		return request_set_result(s, req, SP_ERROR_OTHER_TRANSIENT, NULL);
	}

	ret = login_process(s->login);
	if(ret == 0)
		return 0;
	else if(ret == 1) {
		login_export_session(s->login, &s->sock, key_recv, key_send);
		login_release(s->login);
		s->login = NULL;

		shn_key(&s->shn_recv, key_recv, sizeof(key_recv));
		s->key_recv_IV = 0;

		shn_key(&s->shn_send, key_send, sizeof(key_send));
		s->key_send_IV = 0;

		s->connectionstate = SP_CONNECTION_STATE_LOGGED_IN;

		DSFYDEBUG("Logged in\n");
		return request_set_result(s, req, SP_ERROR_OK, NULL);
	}

	switch(s->login->error) {
	case SP_LOGIN_ERROR_DNS_FAILURE:
	case SP_LOGIN_ERROR_NO_MORE_SERVERS:
		error = SP_ERROR_UNABLE_TO_CONTACT_SERVER;
		break;

	case SP_LOGIN_ERROR_UPGRADE_REQUIRED:
		error = SP_ERROR_CLIENT_TOO_OLD;
		break;

	case SP_LOGIN_ERROR_USER_BANNED:
		error = SP_ERROR_USER_BANNED;
		break;

	case SP_LOGIN_ERROR_USER_NOT_FOUND:
	case SP_LOGIN_ERROR_BAD_PASSWORD:
		error = SP_ERROR_BAD_USERNAME_OR_PASSWORD;
		break;

	case SP_LOGIN_ERROR_USER_NEED_TO_COMPLETE_DETAILS:
	case SP_LOGIN_ERROR_USER_COUNTRY_MISMATCH:
	case SP_LOGIN_ERROR_OTHER_PERMANENT:
		error = SP_ERROR_OTHER_PERMANENT;
		break;

	case SP_LOGIN_ERROR_SOCKET_ERROR:
	default:
		error = SP_ERROR_OTHER_TRANSIENT;
		break;
	}

	login_release(s->login);
	s->login = NULL;

	DSFYDEBUG("Login failed with error: %s\n", sp_error_message(error));
	return request_set_result(s, req, error, NULL);
}


/*
 * High-level handling of logout requests
 * FIXME: Do we need to send some kind of "goodbye" packet first?
 *
 * FIXME: The official library attempts to finish up things for a few
 *        seconds before forcing a logout.
 *        An example is when playlists are modified. They're modified 
 *        on the local side immediately but syncing the changes to the
 *        server might take additional time.
 */
static int process_logout_request(sp_session *session, struct request *req) {

	/* FIXME: We should probably cancel all requests in flight aswell */

	/* If a login is still in process, cancel it here */
        if(session->login) {
		/*
		 * We ought to cancel a pending login request here.
		 * Instead we rely on process_login_request() to detect
		 * that session->login became NULL
		 *
		 */
                login_release(session->login);
                session->login = NULL;
        }

	/* Get rid of our identity */
        if(session->user) {
                user_release(session->user);
                session->user = NULL;
        }

	memset(session->country, 0, sizeof(session->country));


	if(session->sock != -1) {
#ifdef _WIN32
		closesocket(session->sock);
#else
		close(session->sock);
#endif
		session->sock = -1;
	}

	session->connectionstate = SP_CONNECTION_STATE_LOGGED_OUT;

	/* Unregister all channels */
	channel_fail_and_unregister_all(session);

	return request_set_result(session, req, SP_ERROR_OK, NULL);
}
