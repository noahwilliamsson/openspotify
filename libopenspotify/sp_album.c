#include <stdio.h>
#include <string.h>

#include <spotify/api.h>

#include "debug.h"
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
	if(album->ref_count)
		album->ref_count--;

	if(album->ref_count)
		return;

	/* FIXME: Deallocate here when ref count reaches 0 */
	hashtable_remove(album->hashtable, album->id);

	if(album->name)
		free(album->name);

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

	album->image = NULL;

	album->name = NULL;
	album->year = -1;

	album->is_loaded = 0;
	album->ref_count = 0;

	album->hashtable = session->hashtable_albums;
	hashtable_insert(album->hashtable, album->id, album);

	return album;
}
