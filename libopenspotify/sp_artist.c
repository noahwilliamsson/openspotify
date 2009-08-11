#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <spotify/api.h>

#include "artist.h"
#include "debug.h"
#include "hashtable.h"
#include "request.h"
#include "sp_opaque.h"
#include "util.h"


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

	assert(artist->ref_count > 0);

	if(--artist->ref_count)
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


int osfy_artist_load_from_xml(sp_session *session, sp_artist *artist, ezxml_t artist_node) {
	unsigned char id[20];
	ezxml_t node;
	
	/* Assume XML from tracking browsing */
	if((node = ezxml_get(artist_node, "artist", -1)) != NULL) {
		/* Album UUID */
		hex_ascii_to_bytes(node->txt, id, sizeof(artist->id));
		assert(memcmp(artist->id, id, sizeof(artist->id)) == 0);
		
		/* Album name */
		if((node = ezxml_get(artist_node, "artist", -1)) == NULL) {
			DSFYDEBUG("Failed to find element 'artist'\n");
			return -1;
		}
		
		assert(strlen(node->txt) < 256);
		artist->name = realloc(artist->name, strlen(node->txt) + 1);
		strcpy(artist->name, node->txt);
		
		artist->is_loaded = 1;
	}
	else {
		DSFYDEBUG("Failed to find element 'artist'\n");
	}
	
	return 0;
}
