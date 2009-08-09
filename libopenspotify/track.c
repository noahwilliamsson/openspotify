/*
 * Tracks memory management
 *
 */


#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif

#include "debug.h"
#include "hashtable.h"
#include "sp_opaque.h"


sp_track *track_add(sp_session *session, unsigned char *id) {
	sp_track *track;

	if(session == NULL) {
		DSFYDEBUG("Called with NULL session, failing\n");
		return NULL;
	}

	if((track = (sp_track *)hashtable_find(session->hashtable_tracks, id)) != NULL)
		return track;


	track = (sp_track *)malloc(sizeof(sp_track));
	if(track == NULL)
		return NULL;

	hashtable_insert(session->hashtable_tracks, id, track);

	memcpy(track->id, id, sizeof(track->id));
	memset(track->file_id, 0, sizeof(track->file_id));
	memset(track->album_id, 0, sizeof(track->album_id));
	memset(track->cover_id, 0, sizeof(track->cover_id));

	track->name = NULL;
	track->title = NULL;
	track->album = NULL;

	track->num_artists = 0;
	track->artists = NULL;

	track->index = 0;
	track->disc = 0;

	track->duration = 0;
	track->popularity = 0;

	track->is_loaded = 0;
	track->playable = 0;
	track->error = SP_ERROR_OK;

	track->ref_count = 0;

	return track;
}


void track_free(sp_session *session, sp_track *track) {
	int i;

	hashtable_remove(session->hashtable_tracks, track->id);

	if(track->name)
		free(track->name);

	if(track->title)
		free(track->title);

	if(track->album)
		free(track->album);

	for(i = 0; i < track->num_artists; i++) {
		free(track->artists[i]);
	}
	if(track->num_artists)
		free(track->artists);

	free(track);
}


void track_set_title(sp_track *track, char *title) {
	track->title = realloc(track->title, strlen(title) + 1);
	strcpy(track->title, title);

	track->name = realloc(track->name, strlen(track->title) + 3 + (track->album? strlen(track->album): 0) + 1);
	strcpy(track->name, track->title);
	if(track->album == NULL)
		return;
	
	strcat(track->name, " - ");
	strcat(track->name, track->album);
}


void track_set_album(sp_track *track, char *album) {
	track->album = realloc(track->album, strlen(album) + 1);
	strcpy(track->album, album);

	track->name = realloc(track->name, (track->title? strlen(track->title): 0) + 3 + (track->album? strlen(track->album): 0) + 1);

	if(track->title) {
		strcpy(track->name, track->title);
		strcat(track->name, " - ");
		strcat(track->name, track->album);
	}
}


char *track_get_name(sp_track *track) {
	return track->name;
}


void track_set_album_id(sp_track *track, unsigned char id[16]) {
	memcpy(track->album_id, id, sizeof(track->album_id));
}


void track_set_cover_id(sp_track *track, unsigned char id[20]) {
	memcpy(track->cover_id, id, sizeof(track->cover_id));
}


void track_set_file_id(sp_track *track, unsigned char id[20]) {
	memcpy(track->file_id, id, sizeof(track->file_id));
}


void track_set_index(sp_track *track, int index) {
	track->index = index;
}


void track_set_duration(sp_track *track, int duration) {
	track->duration = duration;
}


int track_get_duration(sp_track *track) {
	return track->duration;
}


void track_set_popularity(sp_track *track, int popularity) {
	track->popularity = popularity;
}


int track_get_popularity(sp_track *track) {
	return track->popularity;
}


void track_set_disc(sp_track *track, int disc) {
	track->disc = disc;
}


int track_get_disc(sp_track *track) {
	return track->disc;
}


void track_set_loaded(sp_track *track, int loaded) {
	track->is_loaded = loaded;
}


int track_get_loaded(sp_track *track) {
	return track->is_loaded;
}


void track_set_playable(sp_track *track, int playable) {
	track->playable = playable;
}


void track_add_ref(sp_track *track) {
	track->ref_count++;
}


void track_del_ref(sp_track *track) {
	if(track->ref_count)
		track->ref_count--;
}


void track_garbage_collect(sp_session *session) {
	struct hashiterator *iter;
	struct hashentry *entry;
	sp_track *track;

	iter = hashtable_iterator_init(session->hashtable_tracks);
	while((entry = hashtable_iterator_next(iter))) {
		track = (sp_track *)entry->value;

		if(track->ref_count)
			continue;

		DSFYDEBUG("Freeing track %p because of zero ref_count\n", track);
		track_free(session, track);
	}

	hashtable_iterator_free(iter);
}


int track_save_metadata_to_disk(sp_session *session, char *filename) {
	FILE *fd;
	struct hashiterator *iter;
	struct hashentry *entry;
	sp_track *track;
	unsigned char len;
	unsigned int num;

	if((fd = fopen(filename, "w")) == NULL)
		return -1;
	
	iter = hashtable_iterator_init(session->hashtable_tracks);
	while((entry = hashtable_iterator_next(iter))) {
		track = (sp_track *)entry->value;

		if(track_get_loaded(track) == 0)
			continue;

		fwrite(track->id, sizeof(track->id), 1, fd);
		fwrite(track->file_id, sizeof(track->file_id), 1, fd);
		fwrite(track->album_id, sizeof(track->album_id), 1, fd);
		fwrite(track->cover_id, sizeof(track->cover_id), 1, fd);
		
		len = (track->title? strlen(track->title): 0);
		fwrite(&len, 1, 1, fd);
		fwrite(track->title, len, 1, fd);

		len = (track->album? strlen(track->album): 0);
		fwrite(&len, 1, 1, fd);
		fwrite(track->album, len, 1, fd);

		num = htons(track->index);
		fwrite(&num, sizeof(int), 1, fd);
		num = htons(track->disc);
		fwrite(&num, sizeof(int), 1, fd);
		num = htons(track->playable);
		fwrite(&num, sizeof(int), 1, fd);
	}

	hashtable_iterator_free(iter);
	fclose(fd);

	return 0;
}


int track_load_metadata_from_disk(sp_session *session, char *filename) {
	FILE *fd;
	unsigned char len, id16[16], id20[20];
	unsigned int num;
	char buf[256 + 1];
	sp_track *track;

	if((fd = fopen(filename, "r")) == NULL)
		return -1;
	
	buf[256] = 0;
	while(!feof(fd)) {
		if(fread(id16, sizeof(id16), 1, fd) == 1)
			track = track_add(session, id16);
		else
			break;

		if(fread(id20, sizeof(id20), 1, fd) == 1)
			track_set_file_id(track, id20);
		else
			break;


		if(fread(id16, sizeof(id16), 1, fd) == 1)
			track_set_album_id(track, id16);
		else
			break;

		if(fread(id20, sizeof(id20), 1, fd) == 1)
			track_set_cover_id(track, id20);
		else
			break;
		
		if(fread(&len, 1, 1, fd) == 1) {
			if(len && fread(buf, len, 1, fd) == 1) {
				buf[len] = 0;
				track_set_title(track, buf);
			}
		}
		else
			break;

		if(fread(&len, 1, 1, fd) == 1) {
			if(len && fread(buf, len, 1, fd) == 1) {
				buf[len] = 0;
				track_set_album(track, buf);
			}
		}
		else
			break;

		if(fread(&num, 1, 1, fd) == 1)
			track_set_index(track, ntohs(num));
		else
			break;

		if(fread(&num, sizeof(int), 1, fd) == 1)
			track_set_disc(track, ntohs(num));
		else
			break;

		if(fread(&num, sizeof(int), 1, fd) == 1)
			track_set_duration(track, ntohs(num));
		else
			break;


		if(fread(&num, sizeof(int), 1, fd) == 1)
			track_set_playable(track, ntohs(num));
		else
			break;

		track_set_loaded(track, ntohs(num));
	}

	fclose(fd);

	return 0;
}
