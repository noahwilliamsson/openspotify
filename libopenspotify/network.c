/*
 *
 *
 *
 */


#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <unistd.h>
#endif
#include <spotify/api.h>

#include "debug.h"
#include "login.h"
#include "network.h"
#include "request.h"
#include "sp_opaque.h"
#include "shn.h"


static int process_request(sp_session *s, sp_request *req);
static int process_packets(sp_session *s);
static int process_login_request(sp_session *s, sp_request *req);
static int process_logout_request(sp_session *s, sp_request *req);


/*
 * Network thread
 * Main thread can communicate using the primitives in request.c
 * Results from operations are also returned using stuff in request.c
 *
 */

#ifdef _WIN32
DWORD WINAPI network_thread(LPVOID data) {
#else
void *network_thread(void *data) {
#endif
	sp_session *s = (sp_session *)data;
	sp_request *req;
	int ret;

#ifdef _WIN32
	WSADATA wsadata;
	WSAStartup(MAKEWORD(2,2), &wsadata);
#endif

	for(;;) {
/* FIXME: XXX - Sleep on something while we're not connected and there are no events! */
#ifdef _WIN32
		Sleep(300);
#else
		usleep(300*1000);
#endif
		request_cleanup(s);

		/* No locking needed since we're in control of request_cleanup() */
		for(req = s->requests; req; req = req->next) {
			if(req->state != REQ_STATE_NEW && req->state != REQ_STATE_RUNNING)
				continue;

			DSFYDEBUG("Processing request of type %d in state %d\n", req->type, req->state);
			ret = process_request(s, req);
			DSFYDEBUG("Got return value %d from processing of event of type %d in state %d\n", ret, req->type, req->state);
		}

		if(s->connectionstate != SP_CONNECTION_STATE_LOGGED_IN)
			continue;

		DSFYDEBUG("Processing packets\n");
		ret = process_packets(s);
	}


}


/*
 * Process requests posted by request.c routines
 *
 * Also notify main thread when we're calling
 * request_set_result()
 *
 */
static int process_request(sp_session *s, sp_request *req) {
	switch(req->type) {
	case REQ_TYPE_LOGIN:
		return process_login_request(s, req);
		break;

	case REQ_TYPE_LOGOUT:
		return process_logout_request(s, req);
		break;

	default:
		break;
	}

	return 0;
}


/*
 * Process packets once the connection has been negotiated 
 *
 */
static int process_packets(sp_session *s) {
	DSFYDEBUG("Processing packets on socket %d\n", s->sock);

	return 0;
}


static int process_login_request(sp_session *s, sp_request *req) {
	int ret;
	sp_error error;
	unsigned char key_recv[32], key_send[32];

	if(req->state == REQ_STATE_NEW) {
		req->state = REQ_STATE_RUNNING;
		s->login = login_create(s->username, s->password);
		if(s->login == NULL)
			return request_set_result(s, req, SP_ERROR_OTHER_TRANSIENT);
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

		return request_set_result(s, req, SP_ERROR_OK);
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
		error = SP_ERROR_OTHER_PERMAMENT;
		break;

	case SP_LOGIN_ERROR_SOCKET_ERROR:
	default:
		error = SP_ERROR_OTHER_TRANSIENT;
		break;
	}

	login_release(s->login);
	s->login = NULL;

	return request_set_result(s, req, error);
}


static int process_logout_request(sp_session *s, sp_request *req) {

	if(s->sock != -1) {
#ifdef _WIN32
		closesocket(s->sock);
#else
		close(s->sock);
#endif
	}


	s->connectionstate = SP_CONNECTION_STATE_LOGGED_OUT;

	return request_set_result(s, req, SP_ERROR_OK);
}
