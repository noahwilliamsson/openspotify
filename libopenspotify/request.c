/* 
 * For communication between the main thread and the network thread
 * (c) 2009 Noah Williamsson <noah.williamsson@gmail.com>
 *
 */

#include <stdlib.h>
#include <spotify/api.h>
#include "sp_opaque.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

#include "debug.h"


/*
 * Post a new request to be processed by the networking thread
 *
 */
int request_post(sp_session *session, sp_request_type type) {
	sp_request *req;

#ifdef _WIN32
	WaitForSingleObject(session->request_mutex, INFINITE);
#else
	pthread_mutex_lock(&session->request_mutex);
#endif

	if(session->requests == NULL) {
		req = session->requests = malloc(sizeof(sp_request));
	}
	else {
		for(req = session->requests; req->next; req = req->next);
		req->next = malloc(sizeof(sp_request));
		req = req->next;
	}

	req->type = type;
	req->state = REQ_STATE_NEW;
	req->error = 0;
	req->private = NULL;
	req->next = NULL;

#ifdef _WIN32
	ReleaseMutex(session->request_mutex);
#else
	pthread_mutex_unlock(&session->request_mutex);
#endif

	return 0;
}


/*
 * Set the error to be returned by sp_session_process_requests()
 *
 */
int request_set_result(sp_session *session, sp_request *req, sp_error error) {
#ifdef _WIN32
	WaitForSingleObject(session->request_mutex, INFINITE);
#else
	pthread_mutex_lock(&session->request_mutex);
#endif

	req->error = error;
	req->state = REQ_STATE_RETURNED;
	
	DSFYDEBUG("Setting REQ_STATE_RETURNED and error %d on request with type %d\n", error, req->type);

#ifdef _WIN32
	ReleaseMutex(session->request_mutex);
#else
	pthread_mutex_unlock(&session->request_mutex);
#endif

	return 0;
}


/*
 * Mark an request as processed by sp_request_process_requests()
 * Data will be free'd by request_cleanup()
 *
 */
void request_mark_processed(sp_session *session, sp_request *req) {
#ifdef _WIN32
	WaitForSingleObject(session->request_mutex, INFINITE);
#else
	pthread_mutex_lock(&session->request_mutex);
#endif

	DSFYDEBUG("Setting REQ_STATE_PROCESSED on request with type %d and error %d\n", req->type, req->error);
	req->state = REQ_STATE_PROCESSED;

#ifdef _WIN32
	ReleaseMutex(session->request_mutex);
#else
	pthread_mutex_unlock(&session->request_mutex);
#endif
}


/*
 * Remove requests in REQ_STATE_PROCESSED state
 *
 */
void request_cleanup(sp_session *session) {
	sp_request *prev, *walker;
#ifdef _WIN32
	WaitForSingleObject(session->request_mutex, INFINITE);
#else
	pthread_mutex_lock(&session->request_mutex);
#endif


	do {
		prev = NULL;
		for(walker = session->requests; walker; walker = walker->next) {
			if(walker->state == REQ_STATE_PROCESSED) {
				DSFYDEBUG("Event with type %d has state REQ_STATE_PROCESSED, will free() it\n", walker->type);
				break;
			}

			prev = walker;
		}

		if(!walker)
			break;

		if(prev)
			prev->next = walker->next;
		else
			session->requests = walker->next;

		free(walker);
	} while(1);

#ifdef _WIN32
	ReleaseMutex(session->request_mutex);
#else
	pthread_mutex_unlock(&session->request_mutex);
#endif
}
