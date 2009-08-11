#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif
#include <spotify/api.h>

#include "album.h"
#include "debug.h"
#include "hashtable.h"
#include "sp_opaque.h"
#include "track.h"


SP_LIBEXPORT(bool) sp_track_is_loaded(sp_track *track) {

	return track->is_loaded;
}


SP_LIBEXPORT(sp_error) sp_track_error(sp_track *track) {

	return track->error;
}


SP_LIBEXPORT(int) sp_track_num_artists(sp_track *track) {

	return track->num_artists;
}


SP_LIBEXPORT(sp_artist *) sp_track_artist(sp_track *track, int index) {

	if(index > track->num_artists)
		return NULL;

	return track->artists[index];
}


SP_LIBEXPORT(sp_album *) sp_track_album(sp_track *track) {

	return track->album;
}


SP_LIBEXPORT(const char *) sp_track_name(sp_track *track) {

	return track->name;
}


SP_LIBEXPORT(int) sp_track_duration(sp_track *track) {

	return track->duration;
}


SP_LIBEXPORT(int) sp_track_popularity(sp_track *track) {

	return track->popularity;
}


SP_LIBEXPORT(int) sp_track_disc(sp_track *track) {

	return track->disc;
}


SP_LIBEXPORT(int) sp_track_index(sp_track *track) {

	return track->index;
}


SP_LIBEXPORT(void) sp_track_add_ref(sp_track *track) {

	track->ref_count++;
}


SP_LIBEXPORT(void) sp_track_release(sp_track *track) {
	if(track->ref_count)
		track->ref_count--;

	if(track->ref_count)
		return;

	osfy_track_free(track);
}


/*
 * Functions for internal use
 *
 */


sp_track *osfy_track_add(sp_session *session, unsigned char id[16]) {
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

	track->hashtable = session->hashtable_tracks;
	hashtable_insert(track->hashtable, id, track);

	memcpy(track->id, id, sizeof(track->id));
	memset(track->file_id, 0, sizeof(track->file_id));

	track->name = NULL;
	track->title = NULL;
	track->album_name = NULL;

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


void osfy_track_free(sp_track *track) {
	int i;

	hashtable_remove(track->hashtable, track->id);

	if(track->name)
		free(track->name);

	if(track->title)
		free(track->title);

	if(track->album_name)
		free(track->album_name);


	for(i = 0; i < track->num_artists; i++) {
		free(track->artists[i]);
	}
	if(track->num_artists)
		free(track->artists);


	if(track->album)
		sp_album_release(track->album);


	free(track);
}


void osfy_track_title_set(sp_track *track, char *title) {
	track->title = realloc(track->title, strlen(title) + 1);
	strcpy(track->title, title);

	track->name = realloc(track->name, strlen(track->title) + 3 + (track->album_name? strlen(track->album_name): 0) + 1);
	strcpy(track->name, track->title);
	if(track->album_name == NULL)
		return;
	
	strcat(track->name, " - ");
	strcat(track->name, track->album_name);
}


void osfy_track_album_name_set(sp_track *track, char *album_name) {
	track->album_name = realloc(track->album_name, strlen(album_name) + 1);
	strcpy(track->album_name, album_name);

	track->name = realloc(track->name, (track->title? strlen(track->title): 0) + 3 + (track->album_name? strlen(track->album_name): 0) + 1);

	if(track->title) {
		strcpy(track->name, track->title);
		strcat(track->name, " - ");
		strcat(track->name, track->album_name);
	}
}


void osfy_track_album_set(sp_track *track, sp_album *album) {

	track->album = album;
}


void osfy_track_file_id_set(sp_track *track, unsigned char id[20]) {
	memcpy(track->file_id, id, sizeof(track->file_id));
}


void osfy_track_playable_set(sp_track *track, int playable) {
	track->playable = playable;
}


void osfy_track_duration_set(sp_track *track, int duration) {
	track->duration = duration;
}


void track_set_index(sp_track *track, int index) {
	track->index = index;
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


void osfy_track_disc_set(sp_track *track, int disc) {
	track->disc = disc;
}


int track_get_disc(sp_track *track) {
	return track->disc;
}


void osfy_track_loaded_set(sp_track *track, int loaded) {
	track->is_loaded = loaded;
}


int track_get_loaded(sp_track *track) {
	return track->is_loaded;
}


void osfy_track_garbage_collect(sp_session *session) {
	struct hashiterator *iter;
	struct hashentry *entry;
	sp_track *track;

	iter = hashtable_iterator_init(session->hashtable_tracks);
	while((entry = hashtable_iterator_next(iter))) {
		track = (sp_track *)entry->value;

		if(track->ref_count)
			continue;

		DSFYDEBUG("Freeing track %p because of zero ref_count\n", track);
		osfy_track_free(track);
	}

	hashtable_iterator_free(iter);
}


int osfy_track_metadata_save_to_disk(sp_session *session, char *filename) {
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

		if(track->is_loaded == 0)
			continue;

		fwrite(track->id, sizeof(track->id), 1, fd);
		fwrite(track->file_id, sizeof(track->file_id), 1, fd);
		#if 0
		fwrite(track->album->id, sizeof(track->album->id), 1, fd);
		#endif
		
		len = (track->title? strlen(track->title): 0);
		fwrite(&len, 1, 1, fd);
		fwrite(track->title, len, 1, fd);

		len = (track->album_name? strlen(track->album_name): 0);
		fwrite(&len, 1, 1, fd);
		fwrite(track->album_name, len, 1, fd);

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


int osfy_track_metadata_load_from_disk(sp_session *session, char *filename) {
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
			track = osfy_track_add(session, id16);
		else
			break;

		if(fread(id20, sizeof(id20), 1, fd) == 1)
			memcpy(track->file_id, id20, sizeof(track->file_id));
		else
			break;

		#if 0
		if(fread(id16, sizeof(id16), 1, fd) == 1) {
			album = sp_album_add(session, id16);
			track->album = album;
			sp_album_add_ref(album);
		}
		else
			break;
		#endif

		
		if(fread(&len, 1, 1, fd) == 1) {
			if(len && fread(buf, len, 1, fd) == 1) {
				buf[len] = 0;
				osfy_track_title_set(track, buf);
			}
		}
		else
			break;

		if(fread(&len, 1, 1, fd) == 1) {
			if(len && fread(buf, len, 1, fd) == 1) {
				buf[len] = 0;
				osfy_track_album_name_set(track, buf);
			}
		}
		else
			break;

		if(fread(&num, 1, 1, fd) == 1)
			track->index = ntohs(num);
		else
			break;

		if(fread(&num, sizeof(int), 1, fd) == 1)
			track->disc = ntohs(num);
		else
			break;

		if(fread(&num, sizeof(int), 1, fd) == 1)
			track->duration = ntohs(num);
		else
			break;


		if(fread(&num, sizeof(int), 1, fd) == 1)
			track->playable = ntohs(num);
		else
			break;

		track->is_loaded = 1;
	}

	fclose(fd);

	return 0;
}
