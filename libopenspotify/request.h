/* 
 * (c) 2009 Noah Williamsson <noah.williamsson@gmail.com>
 *
 */

#ifndef LIBOPENSPOTIFY_EVENT_H
#define LIBOPENSPOTIFY_EVENT_H

#include <spotify/api.h>

typedef enum {
	REQ_STATE_NEW = 0,
	REQ_STATE_RUNNING,
	REQ_STATE_RETURNED,
	REQ_STATE_PROCESSED
} request_state;


/*
 * When adding new types, have a look at request.c:request_notify_main_thread()
 * and session.c:sp_session_process_events() and add them to the
 * switch() statements if necessary
 *
 */
typedef enum {
	REQ_TYPE_LOGIN = 0,
	REQ_TYPE_LOGOUT,
	REQ_TYPE_PLAY_TOKEN_LOST,
	REQ_TYPE_NOTIFY,
	REQ_TYPE_PLAYLIST_LOAD_CONTAINER,
	REQ_TYPE_PLAYLIST_LOAD_PLAYLIST,
	REQ_TYPE_BROWSE_TRACK,
	REQ_TYPE_CACHE_PERIODIC
} request_type;


struct request {
	request_type type;
	request_state state;
	void *input;
	void *output;
	sp_error error;
	int next_timeout;
	struct request *next;
};

#define REQUEST_TYPE_STR(type) (type == REQ_TYPE_LOGIN? "LOGIN": \
				type == REQ_TYPE_LOGOUT? "LOGOUT": \
				type == REQ_TYPE_PLAY_TOKEN_LOST? "PLAY_TOKEN_LOST": \
				type == REQ_TYPE_PLAYLIST_LOAD_CONTAINER? "PLAYLIST_LOAD_CONTAINER": \
				type == REQ_TYPE_PLAYLIST_LOAD_PLAYLIST? "PLAYLIST_LOAD_PLAYLIST": \
				type == REQ_TYPE_PLAYLIST_LOAD_PLAYLIST? "PLAYLIST_LOAD_PLAYLIST": \
				type == REQ_TYPE_BROWSE_TRACK? "BROWSE_TRACK": \
				type == REQ_TYPE_CACHE_PERIODIC? "CACHE_PERIODIC": \
				"UNKNOWN")

#define REQUEST_STATE_STR(state) (state == REQ_STATE_NEW? "NEW": \
				state == REQ_STATE_RUNNING? "RUNNING": \
				state == REQ_STATE_RETURNED? "RETURNED": \
				state == REQ_STATE_PROCESSED? "PROCESSED": \
				"INVALID")

int request_post(sp_session *session, request_type type, void *input);
int request_post_result(sp_session *session, request_type type, sp_error error, void *output);
int request_set_result(sp_session *session, struct request *req, sp_error error, void *output);
struct request *request_fetch_next_result(sp_session *session, int *next_timeout);
void request_mark_processed(sp_session *session, struct request *req);
void request_cleanup(sp_session *session);
#endif
