#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <spotify/api.h>

#include "album.h"
#include "artist.h"
#include "browse.h"
#include "debug.h"
#include "ezxml.h"
#include "hashtable.h"
#include "request.h"
#include "sp_opaque.h"
#include "track.h"
#include "util.h"
#include "search.h"


SP_LIBEXPORT(sp_search *) sp_search_create (sp_session *session, const char *query, int track_offset, int track_count, int album_offset, int album_count, int artist_offset, int artist_count, search_complete_cb *callback, void *userdata) {
	char query_key[256];
	sp_search *search;
	void **container;
	struct search_ctx *search_ctx;

	strncpy(query_key, query, sizeof(query_key) - 1);
	query_key[sizeof(query_key) - 1] = 0;
	
	search = (sp_search *)hashtable_find(session->hashtable_searches, query_key);
	if(search) {
		sp_search_add_ref(search);

		/* FIXME: Make sure it's actually loaded so it isn't called right after a previous call for the same artist */
		request_post_result(session, REQ_TYPE_SEARCH, SP_ERROR_OK, search);
		return search;
	}

	search = malloc(sizeof(sp_search));
	if(search == NULL)
		return NULL;
	

	search->query = strdup(query);
	search->did_you_mean = NULL;

	search->track_offset = track_offset;
	search->track_count = track_count;

	/* Currently not used due to lack of support in cmd_search() */
	search->album_offset = album_offset;
	search->album_count = album_count;
	search->artist_offset = artist_offset;
	search->artist_count = artist_count;

	search->callback = callback;
	search->userdata = userdata;
	
	search->num_albums = 0;
	search->albums = 0;

	search->num_artists = 0;
	search->artists = NULL;

	search->num_tracks = 0;
	search->tracks = NULL;
	
	search->error = SP_ERROR_IS_LOADING;
	search->is_loaded = 0;
	search->ref_count = 1;

	search->hashtable = session->hashtable_searches;
	hashtable_insert(search->hashtable, query_key, search);

	
	/*
	 * Temporarily increase ref count for the albumbrowse so it's not free'd
	 * accidentily. It will be decreaed by the chanel callback.
	 *
	 */
	sp_search_add_ref(search);

	
	/* The album callback context */
	search_ctx = (struct search_ctx *)malloc(sizeof(struct search_ctx));
	
	
	search_ctx->session = session;
	search_ctx->req = NULL; /* Filled in by the request processor */
	search_ctx->buf = buf_new();
	search_ctx->search = search;
	
	/* Request input container. Will be free'd when the request is finished. */
	container = (void **)malloc(sizeof(void *));
	*container = search_ctx;
	

	request_post(session, REQ_TYPE_SEARCH, container);
	
	return search;
}


SP_LIBEXPORT(bool) sp_search_is_loaded(sp_search *search) {

	return search->is_loaded;
}


SP_LIBEXPORT(sp_error) sp_search_error(sp_search *search) {

	return search->error;
}


SP_LIBEXPORT(int) sp_search_num_tracks(sp_search *search) {
	
	return search->num_tracks;
}


SP_LIBEXPORT(sp_track *) sp_search_track(sp_search *search, int index) {
	if(index < 0 || index >= search->num_tracks)
		return NULL;
	
	return search->tracks[index];
}


SP_LIBEXPORT(int) sp_search_num_albums(sp_search *search) {
	
	return search->num_albums;
}


SP_LIBEXPORT(sp_album *) sp_search_album(sp_search *search, int index) {
	if(index < 0 || index >= search->num_albums)
		return NULL;
	
	return search->albums[index];
}


SP_LIBEXPORT(int) sp_search_num_artists(sp_search *search) {

	return search->num_artists;
}


SP_LIBEXPORT(sp_artist *) sp_search_artist(sp_search *search, int index) {
	if(index < 0 || index >= search->num_artists)
		return NULL;

	return search->artists[index];
}


SP_LIBEXPORT(const char *) sp_search_did_you_mean(sp_search *search) {
	
	return search->did_you_mean;
}


SP_LIBEXPORT(const char *) sp_search_query(sp_search *search) {
	
	return search->query;
}


SP_LIBEXPORT(int) sp_search_total_tracks(sp_search *search) {
	
	return search->total_tracks;
}


SP_LIBEXPORT(void) sp_search_add_ref(sp_search *search) {
	
	search->ref_count++;
}


SP_LIBEXPORT(void) sp_search_release(sp_search *search) {
	int i;
	char query_key[256];
	
	assert(search->ref_count > 0);
	search->ref_count--;

	if(search->ref_count)
		return;

	
	strncpy(query_key, search->query, sizeof(query_key) - 1);
	query_key[sizeof(query_key) - 1] = 0;

	hashtable_remove(search->hashtable, query_key);
	free(search->query);

	
	for(i = 0; i < search->num_albums; i++)
		free(search->albums[i]);
	
	if(search->num_albums)
		free(search->albums);
	
	
	for(i = 0; i < search->num_artists; i++)
		sp_artist_release(search->artists[i]);
	
	if(search->num_artists)
		free(search->artists);
	

	for(i = 0; i < search->num_tracks; i++)
		sp_track_release(search->tracks[i]);
	
	if(search->num_tracks)
		free(search->tracks);

	
	if(search->did_you_mean)
		free(search->did_you_mean);
	

	free(search);
}
