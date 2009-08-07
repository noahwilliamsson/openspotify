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
 * If input is non-NULL, it will be free'd at the end of the request
 *
 */
int request_post(sp_session *session, sp_request_type type, void *input) {
	sp_request *req;

#ifdef _WIN32
	/* Wake up the network thread if it's sleeping */
	PulseEvent(session->idle_wakeup);
	WaitForSingleObject(session->request_mutex, INFINITE);
#else
	/* Notify the network thread if it's waiting for something to do */
	pthread_cond_signal(&session->idle_wakeup);
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
	req->input = input;
	req->output = NULL;
	req->next = NULL;

#ifdef _WIN32
	ReleaseMutex(session->request_mutex);
#else
	pthread_mutex_unlock(&session->request_mutex);
#endif

	return 0;
}


/*
 * Post a new request to be processed by the main thread
 * (this is basically an "already processed" request from the net thread's PoV)
 *
 * If output is non-NULL, it will have to be free'd by the consumer,
 * i.e, sp_session_process_events() or one of the callbacks
 *
 */
int request_post_result(sp_session *session, sp_request_type type, sp_error error, void *output) {
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
	req->state = REQ_STATE_RETURNED;
	req->error = error;
	req->input = NULL;
	req->output = output;
	req->next = NULL;

	DSFYDEBUG("Posted request results with type %d and error %d\n", req->type, error);
	session->callbacks->notify_main_thread(session);
	DSFYDEBUG("Notified main thread via session->callbacks->notify_main_thread()\n");

#ifdef _WIN32
	ReleaseMutex(session->request_mutex);
#else
	pthread_mutex_unlock(&session->request_mutex);
#endif

	return 0;
}


/*
 * Set error and an eventual output value for the request.
 * The output value will need to be free'd by the consumer,
 * i.e. sp_session_process_events() or the callback
 *
 */
int request_set_result(sp_session *session, sp_request *req, sp_error error, void *output) {
#ifdef _WIN32
	WaitForSingleObject(session->request_mutex, INFINITE);
#else
	pthread_mutex_lock(&session->request_mutex);
#endif

	req->error = error;
	req->output = output;
	req->state = REQ_STATE_RETURNED;
	
	DSFYDEBUG("Setting REQ_STATE_RETURNED and error %d on request with type %d\n", error, req->type);
	session->callbacks->notify_main_thread(session);
	DSFYDEBUG("Notified main thread via session->callbacks->notify_main_thread()\n");

#ifdef _WIN32
	ReleaseMutex(session->request_mutex);
#else
	pthread_mutex_unlock(&session->request_mutex);
#endif

	return 0;
}


/* For the main thread: Fetch next entry with state REQ_STATE_RETURNED */
sp_request *request_fetch_next_result(sp_session *session) {
	struct sp_request *request;

#ifdef _WIN32
	WaitForSingleObject(session->request_mutex, INFINITE);
#else
	pthread_mutex_lock(&session->request_mutex);
#endif

	for(request = session->requests; request; request = request->next)
		if(request->state == REQ_STATE_RETURNED)
			break;

#ifdef _WIN32
	ReleaseMutex(session->request_mutex);
#else
	pthread_mutex_unlock(&session->request_mutex);
#endif

	return request;
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

		/* Free input variable, if set */
		if(walker->input)
			free(walker->input);

		free(walker);
	} while(1);

#ifdef _WIN32
	ReleaseMutex(session->request_mutex);
#else
	pthread_mutex_unlock(&session->request_mutex);
#endif
}
