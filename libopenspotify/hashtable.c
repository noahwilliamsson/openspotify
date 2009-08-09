/*
 * For caching of metadata
 *
 */

#include <stdlib.h>
#include <string.h>

#include "hashtable.h"


struct hashtable *hashtable_create(int keysize) {
	struct hashtable *hashtable;

	hashtable = malloc(sizeof(struct hashtable));

	hashtable->size = 2048;
	hashtable->keysize = keysize;
	hashtable->count = 0;

	hashtable->entries = (struct hashentry **)malloc(sizeof(struct hashentry *) * hashtable->size);
	memset(hashtable->entries, 0, sizeof(struct hashentry *) * hashtable->size);

	return hashtable;
}


void hashtable_insert(struct hashtable *hashtable, void *key, void *value) {
	struct hashentry *entry;
	int index;

	index = *(unsigned int *)key & (hashtable->size - 1u);
	if((entry = hashtable->entries[index]) == NULL)
		entry = hashtable->entries[index] = malloc(sizeof(struct hashentry *));
	else {
		while(entry->next)
			entry = entry->next;

		entry->next = malloc(sizeof(struct hashentry *));
		entry = entry->next;
	}

	entry->key = malloc(hashtable->keysize);
	memcpy(entry->key, key, hashtable->keysize);
	entry->value = value;

	entry->next = NULL;
}


void *hashtable_find(struct hashtable *hashtable, void *key) {
	struct hashentry *entry;
	int index;

	index = *(unsigned int *)key & (hashtable->size - 1u);
	if(hashtable->entries[index] == NULL)
		return NULL;
	
	for(entry = hashtable->entries[index]; entry; entry = entry->next)
		if(memcmp(entry->key, key, hashtable->keysize))
			return entry->value;
	
	return NULL;
}


struct hashiterator *hashtable_iterate_init(struct hashtable *hashtable) {
	struct hashiterator *iter;

	iter = malloc(sizeof(struct hashiterator));
	iter->hashtable = hashtable;
	iter->offset = -1;
	iter->entry = NULL;

	return iter;
}


struct hashentry *hashtable_iterate_next(struct hashiterator *iter) {
	if(iter->entry != NULL && (iter->entry = iter->entry->next) != NULL)
		return iter->entry;

	for(++iter->offset; iter->offset < iter->hashtable->size; iter->offset++)
		if((iter->entry = iter->hashtable->entries[iter->offset]) != NULL)
			return iter->entry;

	return NULL;
}


void hashtable_iterate_free(struct hashiterator *iter) {
	free(iter);
}


void hashtable_free(struct hashtable *hashtable) {
	int i;
	struct hashentry *entry, *next;

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

	free(hashtable);
}

