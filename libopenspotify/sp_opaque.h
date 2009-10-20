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

#if defined(__GNUC__) && __GNUC__ < 4
#error "Old compiler. :( You need to remove these lines and -fvisibility=hidden from the Makefile"
#elif defined(__GNUC__)
#undef SP_LIBEXPORT
#define SP_LIBEXPORT(x) __attribute__((visibility("default"))) x
#endif

#include "channel.h"
#include "hashtable.h"
#include "login.h"
#include "shn.h"


/* sp_album.c */
struct sp_album {
	unsigned char id[16];
	sp_image *image;

	char *name;
	int year;

	sp_artist *artist;

	int is_available;
	int is_loaded;
	int ref_count;

	struct hashtable *hashtable;
};


/* sp_albumbrowse.c */
struct sp_albumbrowse {
	sp_album *album;

	sp_artist *artist;

	int num_tracks;
	sp_track **tracks;

	int num_copyrights;
	char **copyrights;

	char *review;

	albumbrowse_complete_cb *callback;
	void *userdata;

	sp_error error;

	int is_loaded;
	int ref_count;

	struct hashtable *hashtable;
};


/* sp_artistbrowse.h */
struct sp_artistbrowse {
	sp_artist *artist;

	int num_tracks;
	sp_track **tracks;

	int num_portraits;
	unsigned char **portraits;

	int num_similar_artists;
	sp_artist **similar_artists;

	char *biography;

	artistbrowse_complete_cb *callback;
	void *userdata;

	sp_error error;

	int is_loaded;
	int ref_count;

	struct hashtable *hashtable;
};


/* sp_artist.c */
struct sp_artist {
	unsigned char id[16];

	char *name;

	int is_loaded;
	int ref_count;

	struct hashtable *hashtable;
};


/* sp_image.c */
struct sp_image {
	unsigned char id[20];

	int width;
	int height;

	struct buf *data;

	sp_imageformat format;

	image_loaded_cb *callback;
	void *userdata;

	sp_error error;

	int ref_count;
	int is_loaded;

	struct hashtable *hashtable;
};


/* sp_link.c */
struct sp_link {
	sp_linktype type;
	
	union {
		void	*data;
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

	char name[256];
	sp_user *owner;
	int position;
	int shared;

	sp_track **tracks;
	int num_tracks;

	enum playlist_state state;

	int num_callbacks;
	sp_playlist_callbacks **callbacks;
	void **userdata;

	struct buf *buf;
};


struct sp_playlistcontainer {
	/* List of individual playlists */
	int num_playlists;
	sp_playlist **playlists;

	int num_callbacks;
	sp_playlistcontainer_callbacks **callbacks;
	void **userdata;

	/* For retrieving the container playlist */
	struct buf *buf;
};


/* sp_search.c */
struct sp_search {
	char *query;
	int offset;
	int count;
	
	search_complete_cb *callback;
	void *userdata;
	
	char *did_you_mean;
	
	int num_albums;
	sp_album **albums;
	
	int num_artists;
	sp_artist **artists;
	
	int num_tracks;
	sp_track **tracks;
	
	int total_tracks;
	
	int is_loaded;
	sp_error error;
	
	int ref_count;
	
	struct hashtable *hashtable;
};


/* sp_track.c */
struct sp_track {
	unsigned char id[16];
	unsigned char file_id[20];

	char *name;

	sp_album *album;

	int num_artists;
	sp_artist **artists;

	char *restricted_countries;
	char *allowed_countries;
	int is_available;

	int index;
	int disc;
	int duration;
	int popularity;

	int is_loaded;
	sp_error error;

	int ref_count;

	struct hashtable *hashtable;
};


/* sp_user.c */
struct sp_user {
	char canonical_name[256];
	char *display_name;
	
	struct hashtable *hashtable;

	int is_loaded;
	int ref_count;
};


/* sp_session.c and most other API functions */
struct sp_session {
	void *userdata;
	sp_session_callbacks *callbacks;
	
	sp_user *user;

	/* Low-level network stuff */
	int sock;


	/* Used when logging in */
	char username[256];
	char password[256];
	struct login_ctx *login;


	/* Stream cipher context */
	unsigned int key_recv_IV;
	unsigned int key_send_IV;
	shn_ctx shn_recv;
	shn_ctx shn_send;

	struct buf *packet;

	/* Channels */
	CHANNEL *channels;
	int num_channels;
	int next_channel_id;

	/* Requests scoreboard */
	struct request *requests;


	/* High level connection state */
	sp_connectionstate connectionstate;


	/* For keeping track of playlists and related states */
	sp_playlistcontainer *playlistcontainer;

	/* Album/artist/track memory memory management */
	struct hashtable *hashtable_albumbrowses;
	struct hashtable *hashtable_artistbrowses;
	struct hashtable *hashtable_albums;
	struct hashtable *hashtable_artists;
	struct hashtable *hashtable_images;
	struct hashtable *hashtable_searches;
	struct hashtable *hashtable_tracks;
	struct hashtable *hashtable_users;


#ifdef _WIN32
	HANDLE request_mutex;
	HANDLE idle_wakeup;
	HANDLE thread_main;
	HANDLE thread_io;
#else
	pthread_mutex_t request_mutex;
	pthread_cond_t idle_wakeup;
	pthread_t thread_main;
	pthread_t thread_io;
#endif
};

#endif
