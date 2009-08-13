/*
 * For caching of metadata
 * Assumes key is at least sizeof(unsigned int) bytes
 *
 */

#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#ifdef __linux__
#define _GNU_SOURCE
#endif
#include <pthread.h>
#endif


#include "hashtable.h"


struct hashtable *hashtable_create(int keysize) {
	struct hashtable *hashtable;

	if(keysize < sizeof(unsigned int))
		return NULL;

	hashtable = malloc(sizeof(struct hashtable));

	hashtable->size = 2048;
	hashtable->keysize = keysize;
	hashtable->count = 0;

	hashtable->entries = (struct hashentry **)malloc(sizeof(struct hashentry *) * hashtable->size);
	memset(hashtable->entries, 0, sizeof(struct hashentry *) * hashtable->size);

	hashtable->num_to_free = 0;
	hashtable->freelist = NULL;

#ifdef _WIN32
	hashtable->mutex = CreateMutex(NULL, FALSE, NULL);
#else
	pthread_mutexattr_init(&hashtable->mutex_attr);
	pthread_mutexattr_settype(&hashtable->mutex_attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&hashtable->mutex, &hashtable->mutex_attr);
#endif

	return hashtable;
}


void hashtable_insert(struct hashtable *hashtable, void *key, void *value) {
	struct hashentry *entry;
	int index;

	index = *(unsigned int *)key & (hashtable->size - 1u);

#ifdef _WIN32
	WaitForSingleObject(hashtable->mutex, INFINITE);
#else
	pthread_mutex_lock(&hashtable->mutex);
#endif

	if((entry = hashtable->entries[index]) == NULL)
		entry = hashtable->entries[index] = malloc(sizeof(struct hashentry));
	else {
		while(entry->next)
			entry = entry->next;

		entry->next = malloc(sizeof(struct hashentry));
		entry = entry->next;
	}

	entry->key = malloc(hashtable->keysize);
	memcpy(entry->key, key, hashtable->keysize);
	entry->value = value;

	entry->next = NULL;

	hashtable->count++;

#ifdef _WIN32
	ReleaseMutex(hashtable->mutex);
#else
	pthread_mutex_unlock(&hashtable->mutex);
#endif

}


void hashtable_remove(struct hashtable *hashtable, void *key) {
	struct hashentry *entry, *prev;
	int index;

	index = *(unsigned int *)key & (hashtable->size - 1u);
	if((entry = hashtable->entries[index]) == NULL)
		return;

#ifdef _WIN32
	WaitForSingleObject(hashtable->mutex, INFINITE);
#else
	pthread_mutex_lock(&hashtable->mutex);
#endif

	for(prev = NULL; entry; entry = entry->next) {
		if(!memcmp(entry->key, key, hashtable->keysize))
			break;

		prev = entry;
	}

	if(entry == NULL)
		return;
	
	if(prev == NULL)
		hashtable->entries[index] = entry->next;
	else
		prev->next = entry->next;
		
	free(entry->key);
	if(entry->dont_free) {
		hashtable->freelist = realloc(hashtable->freelist, sizeof(struct hashentry *) * (1 + hashtable->num_to_free));
		hashtable->freelist[hashtable->num_to_free] = entry;
		hashtable->num_to_free++;
	}
	else {
		free(entry);
	}

	hashtable->count--;

#ifdef _WIN32
	ReleaseMutex(hashtable->mutex);
#else
	pthread_mutex_unlock(&hashtable->mutex);
#endif
}


void *hashtable_find(struct hashtable *hashtable, const void *key) {
	struct hashentry *entry;
	int index;
	void *value  = NULL;

	index = *(unsigned int *)key & (hashtable->size - 1u);

#ifdef _WIN32
	WaitForSingleObject(hashtable->mutex, INFINITE);
#else
	pthread_mutex_lock(&hashtable->mutex);
#endif

	if(hashtable->entries[index] != NULL) {
		for(entry = hashtable->entries[index]; entry; entry = entry->next) {
			if(memcmp(entry->key, key, hashtable->keysize) == 0) {
				value = entry->value;
				break;
			}
		}
	}

#ifdef _WIN32
	ReleaseMutex(hashtable->mutex);
#else
	pthread_mutex_unlock(&hashtable->mutex);
#endif
	
	return value;
}


struct hashiterator *hashtable_iterator_init(struct hashtable *hashtable) {
	struct hashiterator *iter;

#ifdef _WIN32
	WaitForSingleObject(hashtable->mutex, INFINITE);
#else
	pthread_mutex_lock(&hashtable->mutex);
#endif

	iter = malloc(sizeof(struct hashiterator));
	iter->hashtable = hashtable;
	iter->offset = -1;
	iter->entry = NULL;

	return iter;
}


struct hashentry *hashtable_iterator_next(struct hashiterator *iter) {
	if(iter->entry != NULL) {
		iter->entry->dont_free = 0;

		if((iter->entry = iter->entry->next) != NULL) {
			iter->entry->dont_free = 1;
			return iter->entry;
		}
	}

	for(++iter->offset; iter->offset < iter->hashtable->size; iter->offset++) {
		if((iter->entry = iter->hashtable->entries[iter->offset]) != NULL) {
			iter->entry->dont_free = 1;
			return iter->entry;
		}
	}

	return NULL;
}


void hashtable_iterator_free(struct hashiterator *iter) {
	int i;

	for(i = 0; i < iter->hashtable->num_to_free; i++)
		free(iter->hashtable->freelist[i]);

	iter->hashtable->num_to_free = 0;

#ifdef _WIN32
	ReleaseMutex(iter->hashtable->mutex);
#else
	pthread_mutex_unlock(&iter->hashtable->mutex);
#endif

	free(iter);
}


void hashtable_free(struct hashtable *hashtable) {
	int i;
	struct hashentry *entry, *next;

	for(i = 0; i < hashtable->num_to_free; i++)
		free(hashtable->freelist[i]);

	for(i = 0; i < hashtable->size; i++) {
		if((entry = hashtable->entries[i]) == NULL)
			continue;

		do {
			free(entry->key);
			free(entry->value);
			next = entry->next;
			free(entry);
		} while((entry = next) != NULL);
	}

#ifdef _WIN32
	CloseHandle(hashtable->mutex);
#else
	pthread_mutexattr_destroy(&hashtable->mutex_attr);
	pthread_mutex_destroy(&hashtable->mutex);
#endif

	free(hashtable);
}
