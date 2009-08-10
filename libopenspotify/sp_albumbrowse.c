#include <stdlib.h>

#include <spotify/api.h>

#include "debug.h"
#include "request.h"
#include "sp_opaque.h"
#include "track.h"

SP_LIBEXPORT(sp_albumbrowse *) sp_albumbrowse_create(sp_session *session, sp_album *album, albumbrowse_complete_cb *callback, void *userdata) {

	void **container;
	sp_albumbrowse *alb;

	alb = (sp_albumbrowse *)hashtable_find(session->hashtable_albumbrowses, album->id);
	if(alb)
		return alb;

	alb = malloc(sizeof(sp_albumbrowse));
	if(alb == NULL)
		return NULL;

	alb->album = album;
	sp_album_add_ref(alb->album);

	alb->artist = NULL;

	alb->num_tracks = 0;
	alb->tracks = NULL;

	alb->num_copyrights = 0;
	alb->copyrights = NULL;

	alb->review = NULL;

	alb->callback = callback;
	alb->userdata = userdata;

	alb->error = SP_ERROR_OK;

	alb->is_loaded = 0;
	alb->ref_count = 1;

	alb->hashtable = session->hashtable_albumbrowses;
	hashtable_insert(alb->hashtable, album->id, alb);

	container = (void **)malloc(sizeof(void *));
	*container = alb;
	request_post(session, REQ_TYPE_ALBUMBROWSE, container);

	return alb;
}


SP_LIBEXPORT(bool) sp_albumbrowse_is_loaded(sp_albumbrowse *alb) {

	return alb->is_loaded;
}


SP_LIBEXPORT(sp_error) sp_albumbrowse_error(sp_albumbrowse *alb) {

	return alb->error;
}


SP_LIBEXPORT(sp_album *) sp_albumbrowse_album(sp_albumbrowse *alb) {

	return alb->album;
}


SP_LIBEXPORT(sp_artist *) sp_albumbrowse_artist(sp_albumbrowse *alb) {

	return alb->artist;
}


SP_LIBEXPORT(int) sp_albumbrowse_num_copyrights(sp_albumbrowse *alb) {

	return alb->num_copyrights;
}


SP_LIBEXPORT(const char *) sp_albumbrowse_copyright(sp_albumbrowse *alb, int index) {

	if(index < 0 || index >= alb->num_copyrights)
		return NULL;

	return alb->copyrights[index];
}


SP_LIBEXPORT(int) sp_albumbrowse_num_tracks(sp_albumbrowse *alb) {

	return alb->num_tracks;
}


SP_LIBEXPORT(sp_track *) sp_albumbrowse_track(sp_albumbrowse *alb, int index) {

	if(index < 0 || index >= alb->num_tracks)
		return NULL;

	return alb->tracks[index];
}


SP_LIBEXPORT(const char *) sp_albumbrowse_review(sp_albumbrowse *alb) {

	return alb->review;
}


SP_LIBEXPORT(void) sp_albumbrowse_add_ref(sp_albumbrowse *alb) {

	alb->ref_count++;
}


SP_LIBEXPORT(void) sp_albumbrowse_release(sp_albumbrowse *alb) {
	int i;

	if(alb->ref_count)
		alb->ref_count--;

	if(alb->ref_count)
		return;

	hashtable_remove(alb->hashtable, alb->album->id);


	for(i = 0; i < alb->num_tracks; i++)
		sp_track_release(alb->tracks[i]);

	if(alb->num_tracks)
		free(alb->tracks);


	for(i = 0; i < alb->num_copyrights; i++)
		free(alb->copyrights[i]);

	if(alb->num_copyrights)
		free(alb->copyrights);


	if(alb->review)
		free(alb->review);


	if(alb->album)
		sp_album_release(alb->album);


	/* FIXME: free artist */
}
