/* 
 * Opauqe data structures and definitions
 * (c) 2009 Noah Williamsson <noah.williamsson@gmail.com>
 *
 */
#ifndef LIBOPENSPOTIFY_OPAQUE_H
#define LIBOPENSPOTIFY_OPAQUE_H

#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <pthread.h>
#include <sys/socket.h>
#include <netdb.h>
#include <time.h>
#endif

#include <spotify/api.h>

#include "hashtable.h"
#include "login.h"
#include "shn.h"



/* For request.c */
typedef enum { 
	REQ_STATE_NEW = 0,
	REQ_STATE_RUNNING,
	REQ_STATE_RETURNED,
	REQ_STATE_PROCESSED
} sp_request_state;

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
} sp_request_type;

typedef struct sp_request {
	sp_request_type type;
	sp_request_state state;
	void *input;
	void *output;
	sp_error error;
	int next_timeout;
	struct sp_request *next;
} sp_request;


/* sp_album.c */
struct sp_album {
	unsigned char id[16];
	unsigned char image_id[20];

	char *name;
	int year;

	sp_artist *artist;

	int is_loaded;
	int ref_count;
};


/* sp_artist.c */
struct sp_artist {
	unsigned char id[16];

	char *name;

	int is_loaded;
	int ref_count;
};


/* sp_link.c */
struct sp_link {
	sp_linktype type;
	
	union {
		void        *data;
		sp_track    *track;
		sp_album    *album;
		sp_artist   *artist;
		sp_search   *search;
		sp_playlist *playlist;
	} data;
	
	int refs;
};


/* sp_playlist.c */
enum playlist_state {
	PLAYLIST_STATE_NEW = 0,	/* Initial state */
	PLAYLIST_STATE_ADDED,	/* Just added */
	PLAYLIST_STATE_LISTED,	/* Have track IDs */
	PLAYLIST_STATE_LOADED	/* Have loaded tracks */
};

struct sp_playlist {
	unsigned char id[17];
	char *name;
	sp_user *owner;
	int position;

	sp_track **tracks;
	int num_tracks;

	int lastrequest;

	enum playlist_state state;

	/* FIXME: Convert to  an array of userdata and callbacks */
	void *userdata;
	sp_playlist_callbacks *callbacks;

	struct buf *buf;
	struct sp_playlist *next;
};

struct sp_playlistcontainer {
	/* FIXME: Might be an array of userdata and callbacks */
	void *userdata;
	sp_playlistcontainer_callbacks *callbacks;

	/* List of individual playlists */
	sp_playlist *playlists;
};

/* sp_track.c */
struct sp_track {
	unsigned char id[16];
	unsigned char file_id[20];
	unsigned char album_id[16];
	unsigned char cover_id[20];

	char *title;
	char *album;
	char *name;

	int num_artists;
	sp_artist **artists;

	int index;
	int disc;
	int duration;
	int popularity;

	/* FIXME: Need more members */
	int is_loaded;
	int playable;
	sp_error error;

	int ref_count;
};


/* sp_user.c */
struct sp_user {
	char *canonical_name;
	char *display_name;

	struct sp_user *next;
};


struct playlist_ctx {
        /* For retrieving the container playlist */
        struct buf *buf;

        /* Callbacks and list of playlists */
        sp_playlistcontainer *container;
};


/* sp_session.c and most other API functions */
struct sp_session {
	void *userdata;

	struct sp_user *user;

	sp_session_callbacks *callbacks;

	/* Low-level network stuff */
	int sock;

	char username[256];
	char password[256];

	/* Used when logging in */
	struct login_ctx *login;


        /* Stream cipher context */
        unsigned int key_recv_IV;
        unsigned int key_send_IV;
        shn_ctx shn_recv;
        shn_ctx shn_send;

	struct buf *packet;


	/* Requests scoreboard */
	sp_request *requests;


	/* High level connection state */
	sp_connectionstate connectionstate;


	/* For keeping track of playlists and related states */
	struct playlist_ctx *playlist_ctx;

	/* Tracks memory management */
	struct hashtable *hashtable_tracks;


#ifdef _WIN32
	HANDLE request_mutex;
	HANDLE idle_wakeup;
	HANDLE thread_main;
	HANDLE thread_network;
#else
	pthread_mutex_t request_mutex;
	pthread_cond_t idle_wakeup;
	pthread_t thread_main;
	pthread_t thread_network;
#endif
};

#endif
