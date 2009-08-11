#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <spotify/api.h>

#include "debug.h"
#include "ezxml.h"
#include "request.h"
#include "sp_opaque.h"


SP_LIBEXPORT(bool) sp_album_is_loaded(sp_album *album) {

	return album->is_loaded;
}


SP_LIBEXPORT(sp_artist *) sp_album_artist(sp_album *album) {

	return album->artist;
}


SP_LIBEXPORT(const byte *) sp_album_cover(sp_album *album) {

	return album->image->id;
}


SP_LIBEXPORT(const char *) sp_album_name(sp_album *album) {

	return album->name;
}


SP_LIBEXPORT(int) sp_album_year(sp_album *album) {

	return album->year;
}


SP_LIBEXPORT(void) sp_album_add_ref(sp_album *album) {

	album->ref_count++;
}


SP_LIBEXPORT(void) sp_album_release(sp_album *album) {

	assert(album->ref_count > 0);

	if(--album->ref_count)
		return;

	hashtable_remove(album->hashtable, album->id);

	if(album->name)
		free(album->name);

	if(album->artist)
		sp_artist_release(album->artist);

	if(album->image)
		sp_image_release(album->image);

	free(album);
}


/*
 * Functions for internal use
 *
 */
sp_album *sp_album_add(sp_session *session, unsigned char id[16]) {
	sp_album *album;


	album = (sp_album *)hashtable_find(session->hashtable_albums, id);
	if(album)
		return album;

	album = malloc(sizeof(sp_album));

	memcpy(album->id, id, sizeof(album->id));

	album->artist = NULL;
	album->image = NULL;

	album->name = NULL;
	album->year = -1;

	album->is_loaded = 0;
	album->ref_count = 0;

	album->hashtable = session->hashtable_albums;
	hashtable_insert(album->hashtable, album->id, album);


	return album;
}


int osfy_album_load_from_xml(sp_session *session, sp_album *album, ezxml_t album_node) {
	unsigned char id[20];
	
	/* Assume XML from tracking browsing */
	if((node = ezxml_get(album_node, "album-id", -1)) != NULL) {
		/* Album UUID */
		hex_ascii_to_bytes(node->txt, id, sizeof(album->id));
		assert(memcmp(album->id, id, sizeof(album->id)) == 0);

		/* Album name */
		if((node = ezxml_get(track_node, "album", -1)) == NULL) {
			DSFYDEBUG("Failed to find element 'album'\n");
			return -1;
		}
		
		assert(strlen(node->txt) < 256);
		album->name = realloc(album->name, strlen(node->txt) + 1);
		strcpy(album->name, node->txt);

		
		/* Album year */
		if((node = ezxml_get(track_node, "year", -1)) == NULL) {
			DSFYDEBUG("Failed to find element 'year'\n");
			return -1;
		}
		
		album->year = atoi(node->txt);

		
		/* Album artist */
		if((node = ezxml_get(track_node, "album-artist-id", -1)) == NULL) {
			DSFYDEBUG("Failed to find element 'album-artist-id'\n");
			return -1;
		}

		if(album->artist != NULL)
			sp_artist_release(album->artist);
		
		hex_ascii_to_bytes(node->txt, id, 16);
		album->artist = osfy_artist_add(session, id);
		sp_artist_add_ref(album->artist)


		/* Album cover */
		if((node = ezxml_get(track_node, "cover", -1)) == NULL) {
			DSFYDEBUG("Failed to find element 'cover'\n");
			return -1;
		}
		
		if(album->image != NULL)
			sp_image_release(album->image);
		
		hex_ascii_to_bytes(node->txt, id, 20);
		album->image = osfy_image_create(session, id);
		sp_image_add_ref(album->image)
		

		/* Done loading */
		album->is_loaded = 1;
	}
	else {
		DSFYDEBUG("Failed to find element 'album-id'\n");
		return -1;
	}

	return 0;
}
