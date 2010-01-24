#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <spotify/api.h>
#include <vorbis/vorbisfile.h>

#include "aes.h"
#include "buf.h"
#include "channel.h"
#include "commands.h"
#include "debug.h"
#include "player.h"
#include "rbuf.h"
#include "request.h"
#include "sp_opaque.h"
#include "util.h"


struct player_substream_ctx {
	sp_track *track;
	int offset;
	int length;
};


static void *player_main(void *arg);
static int player_schedule(sp_session *session);
static int player_deliver_pcm(sp_session *session, int ms);

/* Ogg/Vorbis callbacks */
static size_t player_ov_read(void *ptr, size_t size, size_t nmemb, void *private);
static int player_ov_seek(void *private, ogg_int64_t offset, int whence);
static long player_ov_tell(void *private);

static void player_seek_counter(struct player *player);
static int player_aes_callback(CHANNEL *ch, unsigned char *buf, unsigned short len);
static int player_substream_callback(CHANNEL *ch, unsigned char *buf, unsigned short len);


/*
 * Initialize the player and start the main thread
 * This function is called from the main thread
 *
 */
int player_init(sp_session *session) {

	session->player = malloc(sizeof(struct player));
	if(session->player == NULL)
		return -1;

	pthread_mutex_init(&session->player->mutex, NULL);
	pthread_cond_init(&session->player->cond, NULL);
	session->player->item_posted = 0;

	session->player->items = NULL;

	session->player->key = NULL;
	session->player->track = NULL;

	session->player->ogg = rbuf_new();
	session->player->stream_length = 0;
	session->player->pcm = buf_new();
	session->player->pcm_next_timeout_ms = 0;

	session->player->is_loaded = 0;
	session->player->is_playing = 0;
	session->player->is_paused = 0;
	session->player->is_eof = 0;
	session->player->is_downloading = 0;

	session->player->vf = NULL;
	session->player->callbacks.read_func = player_ov_read;
	session->player->callbacks.seek_func = player_ov_seek;
	session->player->callbacks.close_func = NULL;
	session->player->callbacks.tell_func = player_ov_tell;


	pthread_create(&session->player->thread, NULL, player_main, session);

	return 0;
}


/*
 * Release all resources held by the player
 *
 */
void player_free(sp_session *session) {
	pthread_cancel(session->player->thread);
	pthread_join(session->player->thread, NULL);

	pthread_cond_destroy(&session->player->cond);
	pthread_mutex_destroy(&session->player->mutex);

	DSFYDEBUG("Releasing player resources\n");
	if(session->player->vf) {
		ov_clear(session->player->vf);
		free(session->player->vf);
	}

	if(session->player->track)
		sp_track_release(session->player->track);

	if(session->player->key)
		free(session->player->key);

	buf_free(session->player->pcm);
	rbuf_free(session->player->ogg);


	free(session->player);
	session->player = NULL;
}


/*
 * This is the player thread and it's started by player_init()
 *
 *
 */
static void *player_main(void *arg) {
	int ret;
	ssize_t num_bytes;
	char pcm[4096 * 4];
	sp_session *session = (sp_session *)arg;
	struct player *player = session->player;


	for(;;) {

		/* Process items and decode PCM-data */
		ret = player_schedule(session);


		if(!player->is_loaded) {
			/*
			 * No need to call the Ogg/Vorbis unless we're key'd
			 *
			 */
			continue;
		}


		/*
		 * Buffer up some PCM-data by decoding the Ogg/Vorbis data
		 * This is only meaningful while we have not yet received EOF (I think?)
		 *
		 * 1 second of PCM sound is this many bytes:
		 * <sample rate in samples/second> * <number of channels> * <bytes per sample>
		 *
		 */
		while(!player->is_eof && player->pcm->len < player->vi->rate * player->vi->channels * 2 * 2) {

			num_bytes = ov_read(player->vf, pcm, sizeof(pcm), 0 /* little-endian */, 2 /* 16-bit */, 1, NULL);
			if(num_bytes == OV_HOLE) {
				DSFYDEBUG("ov_read() failed with OV_HOLE, setting EOF\n");
				player_push(session, PLAYER_EOF, NULL, 0);
				break;
			}
			else if(num_bytes == OV_EBADLINK) {
				DSFYDEBUG("ov_read() failed with OV_EBADLINK, setting EOF\n");
				player_push(session, PLAYER_EOF, NULL, 0);
				break;
			}
			else if(num_bytes == OV_EINVAL) {
				DSFYDEBUG("ov_read() failed with OV_EINVAL, setting EOF\n");
				player_push(session, PLAYER_EOF, NULL, 0);
				break;
			}
			else if(num_bytes == 0) {
				DSFYDEBUG("ov_read() returned EOF, have %zu bytes ogg, %d bytes PCM, is_eof:%d\n",
						rbuf_length(player->ogg), player->pcm->len, player->is_eof);
				player_push(session, PLAYER_EOF, NULL, 0);
				break;
			}

			buf_append_data(player->pcm, pcm, num_bytes);
		}
	}

	return NULL;
}


/*
 * Signalling for the player thread
 * This appends a work item to the player's FIFO and notifies
 * the player to wake up in player_schedule()
 *
 */
int player_push(sp_session *session, enum player_item_type type, void *data, size_t len) {
	struct player *player = session->player;
	struct player_item *item;

	pthread_mutex_lock(&player->mutex);


	if((item = player->items) == NULL) {
		player->items = malloc(sizeof(struct player_item));
		item = player->items;
	}
	else {
		while(item->next)
			item = item->next;

		item->next = malloc(sizeof(struct player_item));
		item = item->next;
	}

	item->type = type;
	if(data != NULL) {
		item->data = malloc(len);
		memcpy(item->data, data, len);
	}
	else {
		item->data = NULL;
	}

	item->len = len;
	item->next = NULL;


	/* Signal the condition */
	player->item_posted = 1;
	pthread_cond_signal(&player->cond);


	pthread_mutex_unlock(&player->mutex);

	return 0;
}


/*
 * This function dequeues items off the signalling FIFO
 *
 * It's called both from the thread's main loop and from
 * Ogg/Vorbis ov_read() via the callback player_ov_read()
 *
 */
static int player_schedule(sp_session *session) {
	struct player *player = session->player;
	struct player_item *item;
	int num_processed_items;
	int ret;
	struct timeval tv;
	struct timespec ts;
	int cur_ms;
	void **container;

	pthread_mutex_lock(&player->mutex);
	while(!player->item_posted) {

		if(!player->is_loaded || !player->is_playing || player->is_paused || player->pcm->len == 0) {
			/*
			 * Nothing interesting is going on right now so we'll just sleep
			 *
			 */
			pthread_cond_wait(&player->cond, &player->mutex);

			/* Check condition variable and likely abort the loop */
			continue;
		}


		/*
		 * We're actively playing and have PCM data left to push
		 * down the pipe so we set a timeout to be able to deliver
		 * it on time.
		 *
		 */
		cur_ms = get_millisecs();
		gettimeofday(&tv, NULL);
		ts.tv_sec = tv.tv_sec + 0;
		ts.tv_nsec = 1000 * tv.tv_usec;
		if(player->pcm_next_timeout_ms > cur_ms) {
			ts.tv_nsec += 1000000 * (player->pcm_next_timeout_ms - cur_ms);
			if(ts.tv_nsec > 1000000000) {
				ts.tv_sec++;
				ts.tv_nsec -= 1000000000;
			}
		}


		DSFYDEBUG("WAIT timed: Waiting until %dms (max), time now is %dms\n", player->pcm_next_timeout_ms, cur_ms);;
		if(pthread_cond_timedwait(&player->cond, &player->mutex, &ts) != 0) {
			if(player_deliver_pcm(session, 100)) {
				/* FIXME: Is this really needed? */
				DSFYDEBUG("WAIT timeout: Out of PCM-data, giving up\n");
				break;
			}

			DSFYDEBUG("WAIT timeout: Delivered PCM-data, have %d bytes left\n", player->pcm->len); 


			#define PCM_MS_TO_BYTES(player, milliseconds) \
				(2 * player->vi->channels * player->vi->rate * milliseconds/1000)
			if(player->pcm->len < PCM_MS_TO_BYTES(player, 300)) {
				DSFYDEBUG("WAIT timeout: Not enough PCM data (%d bytes, worth %ldms) at time %dms, aborting\n",
					player->pcm->len,
					player->pcm->len / (2*player->vi->channels*player->vi->rate/1000),
					get_millisecs());
				break;
			}
		}
	}


	/*
	 * Handle any notifications sent to the player thread using player_push()
	 *
	 */
	num_processed_items = 0;
	player->item_posted = 0;
	while((item = player->items) != NULL) {

		player->items = item->next;
		pthread_mutex_unlock(&player->mutex);

		/* Process request */
		switch(item->type) {
		case PLAYER_LOAD:
			/* The sp_track* is referenced by sp_session_player_load() */
			player->track = *(sp_track **)item->data;

			player->is_loaded = 0;
			player->is_eof = 0;
			player->is_playing = 0;
			player->is_paused = 0;
			player->stream_length = 0;
			rbuf_seek_reader(player->ogg, 0, SEEK_SET);
			rbuf_seek_writer(player->ogg, 0, SEEK_SET);

			container = malloc(sizeof(sp_track *));
			*container = player->track;
			sp_track_add_ref(player->track); /* player_process_request() calls sp_track_release() */
			request_post(session, REQ_TYPE_PLAYER_KEY, container);
			break;

		case PLAYER_KEY:
			player->key = malloc(item->len);
			memcpy(player->key, item->data, item->len);

			/* Expand file key */
			rijndaelKeySetupEnc (player->aes.state, player->key, 128);


			/*
			 * To support seeks we need to provide an educated estimate on how
			 * many bytes this file contain. If the guess turns out to be too 
			 * short, seeks beyond this position will fail. If it turns out to 
			 * be to long, requests for data will fail with EOF. Hmm. 
			 *
			 * ov_open_callbacks() will do fseek(.., 0, SEEK_END); ftell(); in
			 * order to determine the range for which seeks can be made.
			 *
			 */


			DSFYDEBUG("SCHEDULER: INIT new track, calling ov_open_callbacks()\n");
			player->vf = calloc(1, sizeof(OggVorbis_File));
			ret = ov_open_callbacks(session, player->vf, NULL, 0, player->callbacks);
			if(ret) {
				DSFYDEBUG("ov_open_callbacks() failed with error %d (%s)\n",
						ret,
						ret == OV_ENOTVORBIS? "not Vorbis":
						ret == OV_EBADHEADER? "bad header":
						ret == OV_EREAD? "read failure":
						"unknown, check <vorbis/codec.h>");
				free(player->vf);
				player->vf = NULL;
				break;
			}

			DSFYDEBUG("SCHEDULER: INIT new track, calling ov_info()\n");
			player->vi = ov_info(player->vf, -1);
			DSFYDEBUG("SCHEDULER: INIT new track, sample rate at %ldHz with %d channels and bitrate %ld\n",
					player->vi->rate, player->vi->channels, player->vi->bitrate_nominal);


			player->audioformat.sample_type = SP_SAMPLETYPE_INT16_NATIVE_ENDIAN;
			player->audioformat.sample_rate = player->vi->rate;
			player->audioformat.channels = player->vi->channels;

			player->is_loaded = 1;
			break;

		case PLAYER_PLAY:
			player->is_paused = 0;
			player->is_playing = 1;
			player->pcm_next_timeout_ms = get_millisecs();
			break;

		case PLAYER_PAUSE:
			player->is_paused = 1;
			break;

		case PLAYER_STOP:
			player->is_playing = 0;
			break;

		case PLAYER_EOF:
			DSFYDEBUG("SCHEDULER: Got PLAYER_EOF, setting EOF-flag\n");
			player->is_eof = 1;
			break;

		case PLAYER_SEEK:
			DSFYDEBUG("SCHEDULER: SEEK request to offset %zums\n\n", item->len);
			if(!player->is_loaded) {
				/*
				 * FIXME: In case we're getting a SEEK request before 
				 * the track is loaded (i.e, before the initial chunks
				 * have been read) we can't seek.
				 * Hence we should cache the seek request and retry later.
				 *
				 */
				DSFYDEBUG("got SEEK request but data is not yet loaded..\n\n");
				break;
			}


			ret = ov_raw_seek(player->vf, (player->vi->bitrate_nominal / 8) * (item->len / 1000.0));
			if(ret == 0) {
				/* Seek succeeded, flush PCM output buffer */
				buf_free(buf_consume(player->pcm, player->pcm->len));
				if(player->is_playing && !player->is_paused)
					session->callbacks->music_delivery(session, &player->audioformat, player->pcm->ptr, 0);
			}

			break;

		case PLAYER_DATA:
			rbuf_write(player->ogg, item->data, item->len);
			break;

		case PLAYER_DATALAST:
			player->is_downloading = 0;
			DSFYDEBUG("SCHEDULER: Got PLAYER_DATALAST, done downloading this chunk!\n");
			break;

		case PLAYER_UNLOAD:
			if(player->track) {
				sp_track_release(player->track);
				player->track = NULL;

				if(player->key) {
					free(player->key);
					player->key = NULL;
				}
			}

			player->is_loaded = 0;
			player->is_eof = 0;
			player->is_playing = 0;
			player->is_paused = 0;
			if(player->vf) {
				ov_clear(player->vf);
				free(player->vf);
				player->vf = NULL;
			}
			/* Fall through */

			rbuf_free(player->ogg);
			player->ogg = rbuf_new();
			player->stream_length = 0;

			buf_free(player->pcm);
			player->pcm = buf_new();
			break;

		default:
			break;
		}


		if(item->data != NULL && item->len) /* Only free if len > 0 */
			free(item->data);
		free(item);

		pthread_mutex_lock(&player->mutex);
		num_processed_items++;
	}

	pthread_mutex_unlock(&player->mutex);

	return num_processed_items;
}


static int player_ov_seek(void *private, ogg_int64_t offset, int whence) {
	sp_session *session = (sp_session *)private;
	struct player *player = session->player;


	/* Don't seek while we're downloading data as it would screw things up right now */
	while(player->is_downloading) {
		player_schedule(session);
	}


	if(whence == SEEK_END) {
		whence = SEEK_SET;
		offset = player->stream_length - offset;
	}

	rbuf_seek_reader(player->ogg, offset, whence);
	rbuf_seek_writer(player->ogg, offset, whence);

	/* Reset EOF */
	player->is_eof = 0;

	return 0;
}


static long player_ov_tell(void *private) {
	sp_session *session = (sp_session *)private;
	struct player *player = session->player;

	DSFYDEBUG("TELL Returning position %zu\n", rbuf_tell(player->ogg));
	return rbuf_tell(player->ogg);
}


/*
 * Called by OggVorbis to read data
 * http://www.xiph.org/vorbis/doc/vorbisfile/callbacks.html
 *
 * ov_read() is using this function to pull more data.
 * This function will in turn try to extract data from the player's Ogg-buffer.
 * If there's not enough data it'll request more data and then do event processing
 * until there a new event.
 *
 * This may or may not be successful, i.e, in cases where the buffers are drained
 * or another track is loaded.
 *
 * FIXME: One problem here is that we still need to deliver PCM-data while
 * waiting for the source to deliver more data..
 *
 */
static size_t player_ov_read(void *dest, size_t size, size_t nmemb, void *private) {
	sp_session *session = (sp_session *)private;
	struct player *player = session->player;
	size_t request_offset;
	void *data;
	size_t bytes_to_consume, previous_bytes;
	int did_download;
	struct player_substream_ctx *psc;

	int i, j;
	int block;
	unsigned char *plaintext;
	unsigned char *ciphertext, *w, *x, *y, *z;
	int do_spotify_header = 0;


	DSFYDEBUG("OV_READ: Want %zu (%zux%zu) bytes from offset %zu, have %zu, is_downloading:%d\n",
			size * nmemb, size, nmemb,
			rbuf_tell(player->ogg), rbuf_length(player->ogg),
			player->is_downloading);


	previous_bytes = 0;
	if(rbuf_tell(player->ogg) % 4096 != 0) {
		/*
		 * We're off a 4096 byte boundary.
		 * Position the reader at the start of this block
		 *
		 */
		previous_bytes = rbuf_tell(player->ogg);
		previous_bytes &= 4095;
		DSFYDEBUG("OV_READ: Off-boundary at position %zu, seeking back %zu bytes\n", rbuf_tell(player->ogg), previous_bytes);
		rbuf_seek_reader(player->ogg, rbuf_tell(player->ogg) - previous_bytes, SEEK_SET);
	}


	bytes_to_consume = size * nmemb;
	if(player->stream_length && rbuf_tell(player->ogg) + bytes_to_consume > player->stream_length) {
		bytes_to_consume = player->stream_length - rbuf_tell(player->ogg);
	}


	did_download = 0;
	while(!player->is_eof && rbuf_length(player->ogg) < bytes_to_consume / 2) {
		if(!did_download && !player->is_downloading) {
			/* FIXME: ... */
			if(((bytes_to_consume - rbuf_length(player->ogg) + 4095) & ~4095) == 0) break;

			/* Prevent downloading more than one chunk per ov_read() */
			did_download = 1;


			/* Calculate the request offset on a 4096 byte boundary */
			request_offset = rbuf_tell(player->ogg) + rbuf_length(player->ogg);
			request_offset &= ~4095;


			/* Request more data */
			psc = (struct player_substream_ctx *)malloc(sizeof(struct player_substream_ctx));
			psc->track = player->track;
			sp_track_add_ref(psc->track);
			psc->offset = request_offset;
			psc->length = (bytes_to_consume - rbuf_length(player->ogg) + 4095) & ~4095;


			DSFYDEBUG("OV_READ: INSIDE: Requesting %d bytes from pos %d (reader at %zu, have %zu bytes)\n",
					psc->length, psc->offset,
					rbuf_tell(player->ogg), rbuf_length(player->ogg));


			/* Needed to determine whether or not we got what we wanted in the channel callback */
			player->is_downloading = psc->length;

			/* Position writer at the aligned offset */
			rbuf_seek_writer(player->ogg, request_offset, SEEK_SET);

			request_post(session, REQ_TYPE_PLAYER_SUBSTREAM, psc);
		}


		/*
		 * Handle requests and deliver PCM-data
		 *
		 */
		player_schedule(session);
	}



	/*
	 * We cannot decode the last page so fail if we didn't get enough data
	 *
	 */
	if(rbuf_length(player->ogg) < 1024 /* We cannot decode the last chunk */) {
		return 0;
	}


	if(rbuf_tell(player->ogg) == 0) {
		DSFYDEBUG("OV_READ: WILL STRIP HEADER, pos:%zu, len:%zu\n\n",
			rbuf_tell(player->ogg), rbuf_length(player->ogg));
		do_spotify_header = 1;
	}


	bytes_to_consume = rbuf_length(player->ogg);
	if(bytes_to_consume > size * nmemb)
		bytes_to_consume = size * nmemb;

	bytes_to_consume &= ~1023;
	assert(bytes_to_consume >= 1024);


	/* Setup counter according to the buffer position */
	player_seek_counter(player);


	/* Load data from the rbuf */
	data = malloc(bytes_to_consume);
	if(data == NULL)
		return 0;

	rbuf_read(player->ogg, data, bytes_to_consume);


	/* Decrypt each 1024 byte block */
	plaintext = dest;
	for (block = 0; block < bytes_to_consume / 1024; block++) {

		/* Deinterleave the 4x256 byte blocks */
		ciphertext = plaintext + block * 1024;
		w = data + block * 1024 + 0 * 256;
		x = data + block * 1024 + 1 * 256;
		y = data + block * 1024 + 2 * 256;
		z = data + block * 1024 + 3 * 256;

		for (i = 0; i < 1024 && (block * 1024 + i) < bytes_to_consume; i += 4) {
			*ciphertext++ = *w++;
			*ciphertext++ = *x++;
			*ciphertext++ = *y++;
			*ciphertext++ = *z++;
		}


		/* Decrypt 1024 bytes block. This will fail for the last block. */
		for (i = 0; i < 1024 && (block * 1024 + i) < bytes_to_consume; i += 16) {

			/* Produce 16 bytes of keystream from the counter */
			rijndaelEncrypt(player->aes.state, 10,
							player->aes.counter,
							player->aes.keystream);

			/* Increment counter */
			for (j = 15; j >= 0; j--) {
				player->aes.counter[j] += 1;
				if (player->aes.counter[j] != 0)
					break;
			}

			/* Produce plaintext by XORing ciphertext with keystream */
			for (j = 0; j < 16; j++)
				plaintext[block * 1024 + i + j] ^= player->aes.keystream[j];
		}
	}

	free(data);


	/*
	 * If we had to request the beginning of the block
	 * instead of the actual position, fix this here
	 *
	 */
	if(previous_bytes) {
		bytes_to_consume -= previous_bytes;
		memmove(dest, dest + previous_bytes, bytes_to_consume);
	}


	if(do_spotify_header) {
		/*
		 * Thanks to Jonas Larsson <jonas@hallerud.se> for figuring out the
		 * header and letting despotify@gmail.com know how it worked.
		 *
		 * Also thanks to fxb for the help figuring out the details so
		 * seeking could be implemented.
		 *
		 * This piece is crucial for being able to seek with libvorbisfile
		 *
		 */
		unsigned char *ptrlen = (unsigned char *)dest + 0x24;

		player->stream_length = *(int *)ptrlen;
		player->stream_length &= ~4095;
		player->stream_length -= 167;

		bytes_to_consume -= 167;
		memmove(dest, dest + 167, bytes_to_consume);
	}


	return bytes_to_consume;
}


/*
 * Deliever a chunk of PCM-data
 *
 */
static int player_deliver_pcm(sp_session *session, int ms) {
	struct player *player = session->player;
	ssize_t num_bytes;
	int num_frames;
	struct buf *pcmout;

	if(!player->is_playing || player->is_paused) {
		/* Do not deliver if not playing.. */
		DSFYDEBUG("PCM: Not playing..\n");
		return 0;
	}


	DSFYDEBUG("PCM: play:%d, pause:%d, pcmlen:%d, ms:%d\n", player->is_playing, player->is_paused, player->pcm->len, get_millisecs());
	if(player->pcm->len) {
		/* Calculate the next timeout */
		player->pcm_next_timeout_ms = get_millisecs() + ms;


		num_bytes = player->vi->rate * player->vi->channels * 2 / 10;
		if(player->pcm->len < num_bytes)
			num_bytes = player->pcm->len;

		DSFYDEBUG("PCM: Current time:%d, next invocation at %d, sending %zu byets\n",
				get_millisecs(), player->pcm_next_timeout_ms, num_bytes);


		if(session->callbacks->music_delivery) {
			pcmout = buf_new();
			buf_append_data(pcmout, player->pcm->ptr, num_bytes);

			num_frames = num_bytes / (player->vi->channels << 1);
			num_frames = session->callbacks->music_delivery(session, &player->audioformat, pcmout->ptr, num_frames);
			num_bytes = num_frames * (player->vi->channels << 1);
		}

		if(num_bytes)
			buf_free(buf_consume(player->pcm, num_bytes));
	}

	if(player->is_eof && player->pcm->len == 0) {
		if(session->callbacks->end_of_track)
			session->callbacks->end_of_track(session);

		player->is_playing = 0;
		rbuf_seek_reader(player->ogg, 0, SEEK_SET);
		rbuf_seek_writer(player->ogg, 0, SEEK_SET);
		return 1;
	}

	return 0;
}


/*
 * Update the counter according to the rbuf's current position
 *
 */
static void player_seek_counter(struct player *player) {
	int i;
	size_t pos;

	/* Nonce */
	memcpy(player->aes.counter, "\x72\xe0\x67\xfb\xdd\xcb\xcf\x77"
			"\xeb\xe8\xbc\x64\x3f\x63\x0d\x93", 16);

	pos = rbuf_tell(player->ogg) >> 4;
        for(i = 15; pos; pos >>= 8) {
                pos += player->aes.counter[i];
                player->aes.counter[i--] = pos & 0xff;
        }
}


/*
 * Handle player-specific requests
 * Called from the I/O thread context by process_request()
 *
 */
int player_process_request(sp_session *session, struct request *req) {
	int ret;
	sp_track *track;
	struct player_substream_ctx *psc;

	DSFYDEBUG("REQUEST: Got request %s\n", REQUEST_TYPE_STR(req->type));
	switch(req->type) {
	case REQ_TYPE_PLAYER_KEY:
		track = *(sp_track **)req->input;
                ret = cmd_aeskey(session, track->file_id, track->id, player_aes_callback, session);
		sp_track_release(track);
		ret = request_set_result(session, req, ret? SP_ERROR_OTHER_PERMANENT: SP_ERROR_OK, NULL);
		break;

	case REQ_TYPE_PLAYER_SUBSTREAM:
		psc = (struct player_substream_ctx *)req->input;
		ret = cmd_getsubstreams(session, psc->track->file_id, psc->offset, psc->length, 200*1000, player_substream_callback, session);
		sp_track_release(psc->track);

		/* This will free our player_substream_ctx */
		ret = request_set_result(session, req, ret? SP_ERROR_OTHER_PERMANENT: SP_ERROR_OK, NULL);
		break;

	default:
		ret = -1;
		break;
	}

	return ret;
}


/*
 * AES key channel callback, called in the context of iothread.c 
 *
 */
static int player_aes_callback(CHANNEL* ch, unsigned char* buf, unsigned short len) {
	sp_session *session = (sp_session *)ch->private;
	void *container;

	if(ch->state != CHANNEL_DATA)
		return 0;

	container = malloc(len); /* Free'd by player_schedule() */
	memcpy(container, buf, len);

	return player_push(session, PLAYER_KEY, container, len);
}


/*
 * GetSubStream channel callback, called in the context of iothread.c 
 *
 */
static int player_substream_callback(CHANNEL * ch, unsigned char *buf, unsigned short len) {
	sp_session *session = ch->private;
	struct player *player = session->player;
	void *container;

	switch (ch->state) {
	case CHANNEL_HEADER:
		break;

	case CHANNEL_DATA:
		container = malloc(len); /* Free'd by player_schedule() */
		memcpy(container, buf, len);

		/* Push data onto the sound buffer queue */
		player_push(session, PLAYER_DATA, container, len);
		break;

	case CHANNEL_ERROR:
		DSFYDEBUG("got CHANNEL_ERROR, setting DATALAST and EOF-flag\n");
		player_push(session, PLAYER_DATALAST, NULL, 0);
		player_push(session, PLAYER_EOF, NULL, 0);
		break;

	case CHANNEL_END:
		if(player->is_downloading == ch->total_data_len) {
			player_push(session, PLAYER_DATALAST, NULL, 0);
		}
		else {
			/* This is the last chunk as we didn't get everything we wanted */
			DSFYDEBUG("SUBSTREAM: EOF, got %d of %d bytes\n", ch->total_data_len, player->is_downloading);
			player_push(session, PLAYER_DATALAST, NULL, 0);
			player_push(session, PLAYER_EOF, NULL, 0);
		}
		break;
	}

	return 0;
}

