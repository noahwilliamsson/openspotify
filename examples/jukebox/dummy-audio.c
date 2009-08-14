#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

#include "audio.h"


static void* alsa_audio_start(void *aux) {
	return NULL;
}



void audio_init(audio_fifo_t *af) {
#ifdef _WIN32
#else
        pthread_t tid;
#endif

        TAILQ_INIT(&af->q);
        af->qlen = 0;

#ifdef _WIN32
#else
        pthread_mutex_init(&af->mutex, NULL);
        pthread_cond_init(&af->cond, NULL);

        pthread_create(&tid, NULL, alsa_audio_start, af);
#endif
}

void audio_fifo_flush(audio_fifo_t *af) {
        audio_fifo_data_t *afd;

        pthread_mutex_lock(&af->mutex);

        while((afd = TAILQ_FIRST(&af->q))) {
                TAILQ_REMOVE(&af->q, afd, link);
                free(afd);
        }

        af->qlen = 0;
        pthread_mutex_unlock(&af->mutex);
}


