#ifndef LIBOPENSPOTIFY_HASH_H
#define LIBOPENSPOTIFY_HASH_H

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

struct hashentry {
	void *key;
	void *value;
	unsigned int hash;
	int dont_free;
	struct hashentry *next;
};

struct hashtable {
	int size;
	int keysize;
	int count;
#ifdef _WIN32
	HANDLE *mutex;
#else
	pthread_mutex_t mutex;
	pthread_mutexattr_t mutex_attr;
#endif
	struct hashentry **entries;
	int num_to_free;
	struct hashentry **freelist;
};

struct hashiterator {
	int offset;
	struct hashentry *entry;
	struct hashtable *hashtable;
};

struct hashtable *hashtable_create(int keysize);
void hashtable_insert(struct hashtable *hashtable, void *key, void *value);
void *hashtable_find(struct hashtable *hashtable, const void *key);
void hashtable_insert(struct hashtable *hashtable, void *key, void *value);
void hashtable_remove(struct hashtable *hashtable, void *key);
struct hashiterator *hashtable_iterator_init(struct hashtable *hashtable);
struct hashentry *hashtable_iterator_next(struct hashiterator *iter);
void hashtable_iterator_free(struct hashiterator *iter);
void hashtable_free(struct hashtable *hashtable);

#endif
