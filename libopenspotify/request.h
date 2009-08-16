/* 
 * (c) 2009 Noah Williamsson <noah.williamsson@gmail.com>
 *
 */

#ifndef LIBOPENSPOTIFY_REQUEST_H
#define LIBOPENSPOTIFY_REQUEST_H

#include <spotify/api.h>

typedef enum {
	/* All requests created with request_post() have this state */
	REQ_STATE_NEW = 0,

	/* As soon as they start being processed they're upgraded to this state */
	REQ_STATE_RUNNING,

	/*
	 * When the iothread is done processing a request (or one is posted with
	 * request_post_result()) the request ends up with this state.
	 *
	 */
	REQ_STATE_RETURNED,

	/*
	 * request_mark_processed() sets this state to schedule the request
	 * for deletion by request_cleanup()
	 *
	 */
	REQ_STATE_PROCESSED
} request_state;


/*
 * When adding new types, have a look at request.c:request_notify_main_thread()
 * and session.c:sp_session_process_events() and add them to the
 * switch() statements if necessary
 *
 */
typedef enum {
	/*
	 * Request the iothread to login to Spotify's servers.
	 * Posted by sp_session_login() and processed by login_process().
	 * Also used by sp_session_process_events() to run the login callback.
	 *
	 */
	REQ_TYPE_LOGIN = 0,

	/*
	 * Request the iothread to logout the session.
	 * Posted by sp_session_logout() and processed by login_process()
	 * Also used by sp_session_process_events() to run the login callback.
	 *
	 */
	REQ_TYPE_LOGOUT,

	/*
	 * Whenever a CMD_PAUSE (play token lost) packet is received this
	 * notified the main thread about the fact so
	 * sp_session_process_events() can run the appropriate callback
	 *
	 */
	REQ_TYPE_PLAY_TOKEN_LOST,

	/* To notify the main thread with a message when CMD_NOTIFY is received */
	REQ_TYPE_NOTIFY,

	/*
	 * When the CMD_WELCOME packet is received a request_post() is made with REQ_TYPE_PC_LOAD.
	 * playlist_process() will then handle requesting the playlist container and loading 
	 * the playlist IDs contained.
	 *
	 */
	REQ_TYPE_PC_LOAD,

	/* Sent to the main thread to notify it about a new playlist being added */
	REQ_TYPE_PC_PLAYLIST_ADD,

	/* FIXME: Not yet supported */
	REQ_TYPE_PC_PLAYLIST_REMOVE,
	REQ_TYPE_PC_PLAYLIST_MOVE,

	/*
	 * When the playlist container is loaded, a REQ_TYPE_PLAYLIST_LOAD is posted to
	 * cause playlist_process() to initiate downloading of the playlist in question
	 * The main thread will be notified with this request type when the playlist is added.
	 *
	 */
	REQ_TYPE_PLAYLIST_LOAD,

	/* Sent to the main thread to notify the name of the playlist was set/updated */
	REQ_TYPE_PLAYLIST_RENAME,

	/*
	 * Posted by playlist loading functions to cause browse_process() to send
	 * track browsing requests for tracks contained in a playlist.
	 * Also 
	 * Also used to notify the main thread that metadata has been updated.
	 *
	 */
	REQ_TYPE_BROWSE_PLAYLIST_TRACKS,

	/*
	 * These three are used by sp_link_create_from_*() to initiate browsing of
	 * a single, not yet loaded album/artist/track.
	 * Also used to notify the main thread that metadata has been updated.
	 *
	 */
	REQ_TYPE_BROWSE_TRACK,
	REQ_TYPE_BROWSE_ALBUM,
	REQ_TYPE_BROWSE_ARTIST,

	/*
	 * Used to get information about a username
	 *
	 */
	REQ_TYPE_USER,

	/*
	 * Used to cause osfy_image_process_request() to download an image.
	 * Also used to notify the main thread that metadata has been updated.
	 *
	 */
	REQ_TYPE_IMAGE,

	/*
	 * Used by sp_albumbrowse.c to cause an album to be browsed.
	 * Also used to cause sp_session_process_events() to call the
	 * album browsing callback.
	 *
	 */
	REQ_TYPE_ALBUMBROWSE,

	/*
	 * Used by sp_albumbrowse.c to cause an album to be browsed.
	 * Also used to cause sp_session_process_events() to call the
	 * album browsing callback.
	 *
	 */
	REQ_TYPE_ARTISTBROWSE,

	/*
	 * An always running request that is periodically run by the
	 * iothread to save stuff to the cache and to do GC.
	 * Never returns.
	 *
	 */
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
				type == REQ_TYPE_NOTIFY? "REQ_TYPE_NOTIFY": \
				type == REQ_TYPE_PLAY_TOKEN_LOST? "PLAY_TOKEN_LOST": \
				type == REQ_TYPE_PC_LOAD? "PC_LOAD": \
				type == REQ_TYPE_PC_PLAYLIST_ADD? "PC_PLAYLIST_ADD": \
				type == REQ_TYPE_PC_PLAYLIST_REMOVE? "PC_PLAYLIST_REMOVE": \
				type == REQ_TYPE_PC_PLAYLIST_MOVE? "PC_PLAYLIST_MOVE": \
				type == REQ_TYPE_PLAYLIST_LOAD? "PLAYLIST_LOAD": \
				type == REQ_TYPE_PLAYLIST_RENAME? "PLAYLIST_RENAME": \
				type == REQ_TYPE_BROWSE_ALBUM? "BROWSE_ALBUM": \
				type == REQ_TYPE_BROWSE_ARTIST? "BROWSE_ARTIST": \
				type == REQ_TYPE_BROWSE_PLAYLIST_TRACKS? "BROWSE_PLAYLIST_TRACKS": \
				type == REQ_TYPE_BROWSE_TRACK? "BROWSE_TRACK": \
				type == REQ_TYPE_USER? "USER": \
				type == REQ_TYPE_IMAGE? "IMAGE": \
				type == REQ_TYPE_ALBUMBROWSE? "ALBUMBROWSE": \
				type == REQ_TYPE_ARTISTBROWSE? "ARTISTBROWSE": \
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
