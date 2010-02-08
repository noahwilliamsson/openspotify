#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <spotify/api.h>

#include "album.h"
#include "artist.h"
#include "browse.h"
#include "debug.h"
#include "ezxml.h"
#include "request.h"
#include "sp_opaque.h"
#include "track.h"
#include "util.h"


static int osfy_albumbrowse_browse_callback(struct browse_callback_ctx *brctx);
static int osfy_albumbrowse_load_from_xml(sp_session *session, sp_albumbrowse *alb, ezxml_t root);


SP_LIBEXPORT(sp_albumbrowse *) sp_albumbrowse_create(sp_session *session, sp_album *album, albumbrowse_complete_cb *callback, void *userdata) {
	sp_albumbrowse *alb;
	void **container;
	struct browse_callback_ctx *brctx;

	alb = (sp_albumbrowse *)hashtable_find(session->hashtable_albumbrowses, album->id);
	if(alb) {
		sp_albumbrowse_add_ref(alb);

		/* Only send result notification if the album browsing has completed */
		if(alb->error != SP_ERROR_IS_LOADING)
			request_post_result(session, REQ_TYPE_ALBUMBROWSE, alb->error, alb);

		return alb;
	}

	alb = malloc(sizeof(sp_albumbrowse));
	if(alb == NULL)
		return NULL;

	DSFYDEBUG("Allocated albumbrowse at %p\n", alb);
	
	alb->album = album;
	sp_album_add_ref(alb->album);
	DSFYDEBUG("Referenced album at %p\n", alb->album);

	
	alb->artist = NULL;

	alb->num_tracks = 0;
	alb->tracks = NULL;

	alb->num_copyrights = 0;
	alb->copyrights = NULL;

	alb->review = NULL;

	alb->callback = callback;
	alb->userdata = userdata;

	alb->error = SP_ERROR_IS_LOADING;

	alb->is_loaded = 0;
	alb->ref_count = 1;

	alb->hashtable = session->hashtable_albumbrowses;
	hashtable_insert(alb->hashtable, album->id, alb);


	
	/*
	 * Temporarily increase ref count for the albumbrowse so it's not free'd
	 * accidentily. It will be decreaed by the chanel callback.
	 *
	 */
	sp_albumbrowse_add_ref(alb);
	
	
	/* The album callback context */
	brctx = (struct browse_callback_ctx *)malloc(sizeof(struct browse_callback_ctx));
	
	brctx->session = session;
	brctx->req = NULL; /* Filled in by the request processor */
	brctx->buf = NULL; /* Filled in by the request processor */
	
	brctx->type = REQ_TYPE_ALBUMBROWSE;
	brctx->data.albumbrowses = (sp_albumbrowse **)malloc(sizeof(sp_albumbrowse *));
	brctx->data.albumbrowses[0] = alb;
	brctx->num_total = 1;
	brctx->num_browsed = 0;
	brctx->num_in_request = 0;
	
	
	/* Our gzip'd XML parser */
	brctx->browse_parser = osfy_albumbrowse_browse_callback;
	
	/* Request input container. Will be free'd when the request is finished. */
	container = (void **)malloc(sizeof(void *));
	*container = brctx;
	
	DSFYDEBUG("requesting REQ_TYPE_ALBUMBROWSE with container %p\n", container);
	request_post(session, REQ_TYPE_ALBUMBROWSE, container);	

	return alb;
}


static int osfy_albumbrowse_browse_callback(struct browse_callback_ctx *brctx) {
	sp_albumbrowse *alb;
	int i;
	struct buf *xml;
	ezxml_t root;
	
	xml = despotify_inflate(brctx->buf->ptr, brctx->buf->len);
#ifdef DEBUG
	{
		FILE *fd;
		DSFYDEBUG("Decompresed %d bytes data, xml=%p\n",
			  brctx->buf->len, xml);
		fd = fopen("browse-albumbrowse.xml", "w");
		if(fd) {
			fwrite(xml->ptr, xml->len, 1, fd);
			fclose(fd);
		}
	}
#endif
	
	root = ezxml_parse_str((char *) xml->ptr, xml->len);
	if(root == NULL) {
		DSFYDEBUG("Failed to parse XML\n");
		buf_free(xml);
		return -1;
	}
	
	for(i = 0; i < brctx->num_in_request; i++) {
		alb = brctx->data.albumbrowses[brctx->num_browsed + i];
		osfy_albumbrowse_load_from_xml(brctx->session, alb, root);
	}
	
	
	ezxml_free(root);
	buf_free(xml);
	
	
	/* Release references made in sp_albumbrowse_create() */
	for(i = 0; i < brctx->num_in_request; i++)
		sp_albumbrowse_release(brctx->data.albumbrowses[brctx->num_browsed + i]);
	
	
	return 0;
}


static int osfy_albumbrowse_load_from_xml(sp_session *session, sp_albumbrowse *alb, ezxml_t root) {
	unsigned char id[20];
	int disc_number, i;
	sp_track *track;
	ezxml_t node, loop_node, track_node;
	

	DSFYDEBUG("Loading from XML\n");
	
	/* Load album from XML if not yet loaded */
	if(sp_album_is_loaded(alb->album) == 0)
		osfy_album_load_from_album_xml(session, alb->album, root);


	/* Load album type */
	if((node = ezxml_get(root, "album-type", -1)) != NULL) {
		if(!strcmp(node->txt, "album"))
			alb->album->type = SP_ALBUMTYPE_ALBUM;
		else if(!strcmp(node->txt, "single"))
			alb->album->type = SP_ALBUMTYPE_SINGLE;
		else if(!strcmp(node->txt, "compilation"))
			alb->album->type = SP_ALBUMTYPE_COMPILATION;
		else
			alb->album->type = SP_ALBUMTYPE_UNKNOWN;
	}


	/* Load artist */
	if((node = ezxml_get(root, "artist-id", -1)) == NULL) {
		DSFYDEBUG("Failed to find element 'artist-id'\n");
		alb->error = SP_ERROR_OTHER_PERMANENT;
		return -1;
	}
	
	if(alb->artist != NULL)
		sp_artist_release(alb->artist);
	
	hex_ascii_to_bytes(node->txt, id, 16);
	alb->artist = osfy_artist_add(session, id);
	sp_artist_add_ref(alb->artist);
	
	if(sp_artist_is_loaded(alb->artist) == 0) {
		DSFYDEBUG("Loading artist '%s' from XML returned by album browsing\n", node->txt);
		osfy_artist_load_track_artist_from_xml(session, alb->artist, root);
	}
	
	assert(sp_artist_is_loaded(alb->artist));
	
	/* Loop over each disc and add tracks */
	assert(alb->num_tracks == 0);
	for(loop_node = ezxml_get(root, "discs", 0, "disc", -1);
	    loop_node;
	    loop_node = loop_node->next) {

		/* Cache disc number */
		if((node = ezxml_get(loop_node, "disc-number", -1)) == NULL) {
			DSFYDEBUG("BUG: Found no 'disc-numner' under discs -> disc\n");
			continue;
		}
		
		disc_number = atoi(node->txt);

		
		/* Loop over each track and add it to the albumbrowse tracks list */
		for(track_node = ezxml_get(loop_node, "track", -1), i = 1;
		    track_node;
		    track_node = track_node->next, i++) {
		
			/* Extract track ID and add it */
			if((node = ezxml_get(track_node, "id", -1)) == NULL)
				continue;
				
			hex_ascii_to_bytes(node->txt, id, 16);
			track = osfy_track_add(session, id);

			
			/* Load track details from XML if not already loaded */
			if(sp_track_is_loaded(track) == 0)
				osfy_track_load_from_xml(session, track, track_node);
			
			assert(sp_track_is_loaded(track));

			
			/* Set disc number */
			track->disc = disc_number;
			
			
			/* Set album (as it's not available under the track node) */
			if(track->album == NULL) {
				track->album = alb->album;
				sp_album_add_ref(track->album);
			}


			/* Mark track as available if the album is available and the album has a non-zero duration (i.e, associated files) */
			if(!track->is_available && track->duration) {
				DSFYDEBUG("Track '%s' marked as not available but has files, force-marking track as %savailable\n",
						node->txt, !alb->album->is_available? "not ": "");
				track->is_available = alb->album->is_available;
			}
			
			
			/* Set track index on disc */
			if(track->index == 0)
				track->index = i;
			
			
			/* Add track to albumbrowse and increase the track's ref count */
			alb->tracks = realloc(alb->tracks, sizeof(sp_track *) * (1 + alb->num_tracks));
			alb->tracks[alb->num_tracks] = track;
			sp_track_add_ref(alb->tracks[alb->num_tracks]);
			alb->num_tracks++;
		}
		
		assert(alb->num_tracks > 0);
	}
	

	/* Loop over each copyright and add copyright text */
	assert(alb->num_copyrights == 0);
	for(node = ezxml_get(root, "copyright", 0, "c", -1);
	    node;
	    node = node->next) {
		alb->copyrights = realloc(alb->copyrights, sizeof(char *) * (1 + alb->num_copyrights));
		alb->copyrights[alb->num_copyrights] = strdup(node->txt);
		alb->num_copyrights++;
	}
	
	/* Add review */
	assert(alb->review == NULL);
	if((node = ezxml_get(root, "review", -1)) != NULL)
		alb->review = strdup(node->txt);
	else
		alb->review = strdup("");

	

	alb->is_loaded = 1;
	alb->error = SP_ERROR_OK;
	
	return 0;
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

	assert(alb->ref_count > 0);
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


	if(alb->album) {
		sp_album_release(alb->album);
		DSFYDEBUG("Unreferenced album at %p\n", alb->album);
	}


	if(alb->artist)
		sp_artist_release(alb->artist);

	DSFYDEBUG("Deallocated albumbrowse at %p\n", alb);

	free(alb);
}
