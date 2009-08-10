#include <stdlib.h>

#include <spotify/api.h>

#include "sp_opaque.h"

#include "hashtable.h"
#include "request.h"


SP_LIBEXPORT(sp_artistbrowse *) sp_artistbrowse_create(sp_session *session, sp_artist *artist, artistbrowse_complete_cb *callback, void *userdata) {
	sp_artistbrowse *arb;
	void **container;

	arb = (sp_artistbrowse *)hashtable_find(session->hashtable_artistbrowse, artist->id);
	if(arb)
		return arb;

	arb = malloc(sizeof(arb));
	if(arb == NULL)
		return NULL;

	arb->artist = artist;
	sp_artist_add_ref(artist);

	arb->num_tracks = 0;
	arb->tracks = NULL;

	arb->num_portraits = 0;
	arb->portraits = 0;

	arb->num_similar_artists = 0;
	arb->similar_artists = NULL;

	arb->biography = NULL;

	arb->callback = callback;
	arb->userdata = userdata;

	arb->error = SP_ERROR_OK;

	arb->is_loaded = 0;
	arb->ref_count = 0;

	arb->hashtable = session->hashtable_artistbrowses;
	hashtable_insert(arb->hashtable, arb->artist->id, arb);

        container = (void **)malloc(sizeof(void *));
        *container = arb;
        request_post(session, REQ_TYPE_ARTISTBROWSE, container);

	return arb;
}


SP_LIBEXPORT(bool) sp_artistbrowse_is_loaded(sp_artistbrowse *arb) {

	return arb->is_loaded;
}


SP_LIBEXPORT(sp_error) sp_artistbrowse_error(sp_artistbrowse *arb) {

	return arb->error;
}


SP_LIBEXPORT(sp_artist *) sp_artistbrowse_artist(sp_artistbrowse *arb) {

	return arb->artist;
}


SP_LIBEXPORT(int) sp_artistbrowse_num_portraits(sp_artistbrowse *arb) {

	return arb->num_portraits;
}


SP_LIBEXPORT(const byte *) sp_artistbrowse_portrait(sp_artistbrowse *arb, int index) {
	if(index < 0 || index >= arb->num_portraits)
		return NULL;

	return arb->portraits[index];
}


SP_LIBEXPORT(int) sp_artistbrowse_num_tracks(sp_artistbrowse *arb) {

	return arb->num_tracks;
}


SP_LIBEXPORT(sp_track *) sp_artistbrowse_track(sp_artistbrowse *arb, int index) {
	if(index < 0 || index >= arb->num_tracks)
		return NULL;

	return arb->tracks[index];
}

SP_LIBEXPORT(int) sp_artistbrowse_num_similar_artists(sp_artistbrowse *arb) {

	return arb->num_similar_artists;
}


SP_LIBEXPORT(sp_artist *) sp_artistbrowse_similar_artist(sp_artistbrowse *arb, int index) {
	if(index < 0 || index >= arb->num_similar_artists)
		return NULL;

	return arb->similar_artists[index];
}


SP_LIBEXPORT(const char *) sp_artistbrowse_biography(sp_artistbrowse *arb) {

	return arb->biography;
}


SP_LIBEXPORT(void) sp_artistbrowse_add_ref(sp_artistbrowse *arb) {

	arb->ref_count++;
}


SP_LIBEXPORT(void) sp_artistbrowse_release(sp_artistbrowse *arb) {
	int i;

	if(arb->ref_count)
		arb->ref_count--;

	if(arb->ref_count)
		return;


	if(arb->artist)
		sp_artist_release(arb->artist);


	for(i = 0; i < arb->num_tracks; i++)
		sp_track_release(arb->tracks[i]);
	
	if(arb->num_tracks)
		free(arb->tracks);


	for(i = 0; i < arb->num_portraits; i++)
		free(arb->portraits[i]);
	
	if(arb->num_portraits)
		free(arb->portraits);


	for(i = 0; i < arb->num_similar_artists; i++)
		free(arb->similar_artists[i]);
	
	if(arb->num_similar_artists)
		free(arb->similar_artists);


	free(arb);
}
