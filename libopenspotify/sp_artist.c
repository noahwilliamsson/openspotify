#include <stdio.h>
#include <string.h>

#include <spotify/api.h>

#include "debug.h"
#include "sp_opaque.h"

SP_LIBEXPORT(const char *) sp_artist_name(sp_artist *artist) {

	return artist->name;
}


SP_LIBEXPORT(bool) sp_artist_is_loaded(sp_artist *artist) {

	return artist->is_loaded;
}


SP_LIBEXPORT(void) sp_artist_add_ref(sp_artist *artist) {

	artist->ref_count++;
}


SP_LIBEXPORT(void) sp_artist_release(sp_artist *artist) {
	if(artist->ref_count)
		artist->ref_count--;
	
	if(artist->ref_count)
		return;

	hashtable_remove(artist->hashtable, artist->id);

	if(artist->name)
		free(artist->name);
	
	free(artist);
}


/*
 * Functions for internal use
 *
 */
sp_artist *osfy_artist_add(sp_session *session, unsigned char id[16]) {
	sp_artist *artist;

	artist = (sp_artist *)hashtable_find(session->hashtable_artists, id);
	if(artist)
		return artist;

	artist = malloc(sizeof(sp_artist));
	if(artist == NULL)
		return NULL;
	
	memcpy(artist->id, id, sizeof(artist->id));

	artist->name = NULL;

	artist->is_loaded = 0;
	artist->ref_count = 0;

	artist->hashtable = session->hashtable_artists;
	hashtable_insert(artist->hashtable, artist->id, artist);

	return artist;
}
