/* 
 * For communication between the main thread and the network thread
 * (c) 2009 Noah Williamsson <noah.williamsson@gmail.com>
 *
 */

#include <stdlib.h>
#include "sp_opaque.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <time.h>
#endif

#include <spotify/api.h>

#include "debug.h"
#include "request.h"
#include "util.h"


static void request_notify_main_thread(sp_session *session, struct request *request);


/*
 * Post a new request to be processed by the networking thread
 * If input is non-NULL, it will be free'd at the end of the request
 *
 */
int request_post(sp_session *session, request_type type, void *input) {
	struct request *req;

#ifdef _WIN32
	/* Wake up the network thread if it's sleeping */
	PulseEvent(session->idle_wakeup);
	WaitForSingleObject(session->request_mutex, INFINITE);
#else
	pthread_mutex_lock(&session->request_mutex);
#endif

	if(session->requests == NULL) {
		req = session->requests = malloc(sizeof(struct request));
	}
	else {
		for(req = session->requests; req->next; req = req->next);
		req->next = malloc(sizeof(struct request));
		req = req->next;
	}

	req->type = type;
	req->state = REQ_STATE_NEW;
	req->error = 0;
	req->input = input;
	req->output = NULL;
	req->next = NULL;
	req->next_timeout = 0;

#ifdef _WIN32
	ReleaseMutex(session->request_mutex);
#else
	/* Notify the network thread if it's waiting for something to do */
	pthread_cond_signal(&session->idle_wakeup);
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
int request_post_result(sp_session *session, request_type type, sp_error error, void *output) {
	struct request *req;

#ifdef _WIN32
	WaitForSingleObject(session->request_mutex, INFINITE);
#else
	pthread_mutex_lock(&session->request_mutex);
#endif

	if(session->requests == NULL) {
		req = session->requests = malloc(sizeof(struct request));
	}
	else {
		for(req = session->requests; req->next; req = req->next);
		req->next = malloc(sizeof(struct request));
		req = req->next;
	}

	req->type = type;
	req->state = REQ_STATE_RETURNED;
	req->error = error;
	req->input = NULL;
	req->output = output;
	req->next = NULL;
	req->next_timeout = 0;

	DSFYDEBUG("Posted results for <type %s, state %s, input %p, timeout %d> with output <error %d, output %p>\n",
		REQUEST_TYPE_STR(req->type), REQUEST_STATE_STR(req->state), req->input, req->next_timeout,
		req->error, req->output);

	request_notify_main_thread(session, req);

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
int request_set_result(sp_session *session, struct request *req, sp_error error, void *output) {
#ifdef _WIN32
	WaitForSingleObject(session->request_mutex, INFINITE);
#else
	pthread_mutex_lock(&session->request_mutex);
#endif

	req->error = error;
	req->output = output;
	req->state = REQ_STATE_RETURNED;
	
	DSFYDEBUG("Returned results for <type %s, state %s, input %p, timeout %d> with output <error %d, output %p>\n",
		REQUEST_TYPE_STR(req->type), REQUEST_STATE_STR(req->state), req->input, req->next_timeout,
		req->error, req->output);
	request_notify_main_thread(session, req);

#ifdef _WIN32
	ReleaseMutex(session->request_mutex);
#else
	pthread_mutex_unlock(&session->request_mutex);
#endif

	return 0;
}


/* For selecting which requests we should notify the main thread about */
static void request_notify_main_thread(sp_session *session, struct request *request) {

	switch(request->type) {
	case REQ_TYPE_LOGIN:
	case REQ_TYPE_LOGOUT:
	case REQ_TYPE_PLAY_TOKEN_LOST:
	case REQ_TYPE_NOTIFY:
	case REQ_TYPE_IMAGE:
	case REQ_TYPE_USER:
	case REQ_TYPE_PC_PLAYLIST_ADD:
	case REQ_TYPE_PC_PLAYLIST_REMOVE:
	case REQ_TYPE_PC_PLAYLIST_MOVE:
	case REQ_TYPE_PLAYLIST_RENAME:
	case REQ_TYPE_PLAYLIST_STATE_CHANGED:
	case REQ_TYPE_PC_LOAD:
	case REQ_TYPE_ALBUMBROWSE:
	case REQ_TYPE_ARTISTBROWSE:
	case REQ_TYPE_BROWSE_ALBUM:
	case REQ_TYPE_BROWSE_ARTIST:
	case REQ_TYPE_BROWSE_TRACK:
		session->callbacks->notify_main_thread(session);
		DSFYDEBUG("Notified main thread for <type %s, state %s, input %p>\n",
			REQUEST_TYPE_STR(request->type), REQUEST_STATE_STR(request->state),
			request->input);
		break;

	case REQ_TYPE_PLAYLIST_LOAD:
		{
			char idstr[35];
			hex_bytes_to_ascii(((sp_playlist *)request->output)->id, idstr, 17);
			DSFYDEBUG("Request <type %s, state %s, input %p>, playlist '%s' is LISTED\n",
				REQUEST_TYPE_STR(request->type), REQUEST_STATE_STR(request->state),
				request->input, idstr);
		}
		break;

	case REQ_TYPE_BROWSE_PLAYLIST_TRACKS:
		{
			char idstr[35];
			hex_bytes_to_ascii(((sp_playlist *)request->output)->id, idstr, 17);
			DSFYDEBUG("Request <type %s, state %s, input %p>, playlist '%s' is LOADED\n",
				REQUEST_TYPE_STR(request->type), REQUEST_STATE_STR(request->state),
				request->input, idstr);
		}
		break;

	default:
		break;
	}
}


/* For the main thread: Fetch next entry with state REQ_STATE_RETURNED */
struct request *request_fetch_next_result(sp_session *session, int *next_timeout) {
	struct request *walker, *request;
	int timeout;

#ifdef _WIN32
	WaitForSingleObject(session->request_mutex, INFINITE);
#else
	pthread_mutex_lock(&session->request_mutex);
#endif

	/* Default timeout (milliseconds) */
	*next_timeout = 5000;

	request = NULL;
	for(walker = session->requests; walker; walker = walker->next) {
		if(!request && walker->state == REQ_STATE_RETURNED)
			request = walker;

		if(walker->next_timeout == 0)
			continue;

		timeout = (int) (walker->next_timeout - get_millisecs());

		/* FIXME: Sensible to always sleep one second? */
		if(timeout < 1000)
			timeout = 1000;

		if(timeout < *next_timeout)
			*next_timeout = timeout;
	}


#ifdef _WIN32
	ReleaseMutex(session->request_mutex);
#else
	pthread_mutex_unlock(&session->request_mutex);
#endif

	return request;
}


/*
 * Mark an request as processed by request_process_requests()
 * Data will be free'd by request_cleanup()
 *
 */
void request_mark_processed(sp_session *session, struct request *req) {
#ifdef _WIN32
	WaitForSingleObject(session->request_mutex, INFINITE);
#else
	pthread_mutex_lock(&session->request_mutex);
#endif

	req->state = REQ_STATE_PROCESSED;
	DSFYDEBUG("Finished processing for <type %s, state %s, input %p, timeout %d> with output <error %d, output %p>\n",
		REQUEST_TYPE_STR(req->type), REQUEST_STATE_STR(req->state), req->input, req->next_timeout,
		req->error, req->output);

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
	struct request *prev, *walker;
#ifdef _WIN32
	WaitForSingleObject(session->request_mutex, INFINITE);
#else
	pthread_mutex_lock(&session->request_mutex);
#endif


	do {
		prev = NULL;
		for(walker = session->requests; walker; walker = walker->next) {
			if(walker->state == REQ_STATE_PROCESSED)
				break;

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
