#ifndef LIBOPENSPOTIFY_PLAYER_H
#define LIBOPENSPOTIFY_PLAYER_H

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

#include <spotify/api.h>
#include <vorbis/vorbisfile.h>

#include "aes.h"
#include "buf.h"
#include "channel.h"
#include "rbuf.h"
#include "request.h"


enum player_item_type {
	PLAYER_LOAD,		/* Load track */
	PLAYER_UNLOAD,		/* Unload track and reset */
	PLAYER_KEY,		/* Setup libvorbis for decoding a new track */

	PLAYER_PLAY,		/* Handle PLAY, PAUSE, STOP */
	PLAYER_PAUSE,
	PLAYER_STOP,

	PLAYER_SEEK,		/* Seek to a specific position */

	PLAYER_DATA,		/* A chunk of an encrypted file */
	PLAYER_DATALAST,	/* To notify that the last chunk has been received */
	PLAYER_EOF		/* No more chunks can be requested for this track */
};


/* For communication with the player */
struct player_item {
	enum player_item_type type;

	void *data;
	size_t len;

	struct player_item *next;
};


struct player {
#ifdef _WIN32
	HANDLE thread;

	/* Mutex protecting the action list */
	HANDLE mutex;

	/* Condition variables to signal the player there's work to do */
	HANDLE cond;
#else
	pthread_t thread;

	/* Mutex protecting the action list */
	pthread_mutex_t mutex;

	/* Condition variables to signal the player there's work to do */
	pthread_cond_t cond;
#endif

	int item_posted;
	int is_recursive;	/* Only set when scheduling from player_ov_read() */

	/* List of things to do */
	struct player_item *items;

	int is_loaded;		/* The player is initialized and ready for playback */
	int is_eof;		/* No more .ogg data can be fetched */
	int is_playing;		/* Set when playing/paused, unset when stopped */
	int is_paused;		/* Set when playback is paused */
	int is_downloading;	/* Pseudo semaphore, also used by the GetSubStream callback */

	/* libvorbis stuff */
	OggVorbis_File *vf;
	ov_callbacks callbacks;
	vorbis_info *vi;


	/* AES state */
	struct {
		unsigned int  state[4 * (10 + 1)];
		unsigned char counter[16];
		unsigned char keystream[16];
	} aes;

	/* AES key for this track */
	unsigned char *key;
	sp_track *track;


	/* Ogg/Vorbis data to decode */
	struct rbuf *ogg;
	size_t stream_length;	/* Size of stream, needed for seeks */

	/* PCM data that's been decoded */
	struct buf *pcm;
	int pcm_next_timeout_ms;
	sp_audioformat audioformat;
};


int player_init(sp_session *session);
void player_free(sp_session *session);
int player_push(sp_session *session, enum player_item_type type, void *data, size_t len);
int player_process_request(sp_session *session, struct request *req);
#endif
