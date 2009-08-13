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


static int osfy_artistbrowse_browse_callback(struct browse_callback_ctx *brctx);
static int osfy_artistbrowse_load_from_xml(sp_session *session, sp_artistbrowse *arb, ezxml_t root);


SP_LIBEXPORT(sp_artistbrowse *) sp_artistbrowse_create(sp_session *session, sp_artist *artist, artistbrowse_complete_cb *callback, void *userdata) {
	sp_artistbrowse *arb;
	void **container;
	struct browse_callback_ctx *brctx;

	arb = (sp_artistbrowse *)hashtable_find(session->hashtable_artistbrowses, artist->id);
	if(arb) {
		sp_artistbrowse_add_ref(arb);
		/* FIXME: Make sure it's actually loaded so it isn't called right after a previous call for the same artist */
		request_post_result(session, REQ_TYPE_ARTISTBROWSE, SP_ERROR_OK, arb);
		return arb;
	}

	arb = malloc(sizeof(sp_artistbrowse));
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
	arb->ref_count = 1;

	arb->hashtable = session->hashtable_artistbrowses;
	hashtable_insert(arb->hashtable, arb->artist->id, arb);

	
	/*
	 * Temporarily increase ref count for the albumbrowse so it's not free'd
	 * accidentily. It will be decreaed by the chanel callback.
	 *
	 */
	sp_artistbrowse_add_ref(arb);

	
	/* The album callback context */
	brctx = (struct browse_callback_ctx *)malloc(sizeof(struct browse_callback_ctx));
	
	
	brctx->session = session;
	brctx->req = NULL; /* Filled in by the request processor */
	brctx->buf = NULL; /* Filled in by the request processor */
	
	brctx->type = REQ_TYPE_ARTISTBROWSE;
	brctx->data.artistbrowses = (sp_artistbrowse **)malloc(sizeof(sp_artistbrowse *));
	brctx->data.artistbrowses[0] = arb;
	brctx->num_total = 1;
	brctx->num_browsed = 0;
	brctx->num_in_request = 0;
	
	/* Our gzip'd XML parser */
	brctx->browse_parser = osfy_artistbrowse_browse_callback;
	
	/* Request input container. Will be free'd when the request is finished. */
	container = (void **)malloc(sizeof(void *));
	*container = brctx;
	

	request_post(session, REQ_TYPE_ARTISTBROWSE, container);	
	
	return arb;
}


static int osfy_artistbrowse_browse_callback(struct browse_callback_ctx *brctx) {
	sp_artistbrowse *arb;
	int i;
	struct buf *xml;
	ezxml_t root;
	
	xml = despotify_inflate(brctx->buf->ptr, brctx->buf->len);
	{
		FILE *fd;
		DSFYDEBUG("Decompresed %d bytes data, xml=%p\n",
			  brctx->buf->len, xml);
		fd = fopen("browse-artistbrowses.xml", "w");
		if(fd) {
			fwrite(xml->ptr, xml->len, 1, fd);
			fclose(fd);
		}
	}
	
	root = ezxml_parse_str((char *) xml->ptr, xml->len);
	if(root == NULL) {
		DSFYDEBUG("Failed to parse XML\n");
		buf_free(xml);
		return -1;
	}
	
	for(i = 0; i < brctx->num_in_request; i++) {
		arb = brctx->data.artistbrowses[brctx->num_browsed + i];
		osfy_artistbrowse_load_from_xml(brctx->session, arb, root);
	}
	
	
	ezxml_free(root);
	buf_free(xml);
	
	
	/* Release references made in sp_artistbrowse_create() */
	for(i = 0; i < brctx->num_in_request; i++)
		sp_artistbrowse_release(brctx->data.artistbrowses[brctx->num_browsed + i]);
	
	
	return 0;
}


static int osfy_artistbrowse_load_from_xml(sp_session *session, sp_artistbrowse *arb, ezxml_t root) {
	unsigned char id[20];
	int disc_number, i;
	sp_track *track;
	sp_album *album;
	ezxml_t node, album_node, loop_node, track_node;
	
	
	/* Load artist from XML if not yet loaded */
	if(sp_artist_is_loaded(arb->artist) == 0)
		osfy_artist_load_artist_from_xml(session, arb->artist, root);
	
	assert(sp_artist_is_loaded(arb->artist));


	/* Load portraits */
	for(loop_node = ezxml_get(root, "bios", 0, "bio", 0, "portraits", 0, "portrait", -1);
	    loop_node;
	    loop_node = loop_node->next) {
		
		if((node = ezxml_get(loop_node, "id", -1)) == NULL)
			continue;
		
		arb->portraits = realloc(arb->portraits, sizeof(unsigned char *) * (1 + arb->num_portraits));
		arb->portraits[arb->num_portraits] = malloc(20);

		hex_ascii_to_bytes(node->txt, arb->portraits[arb->num_portraits], 20);

		arb->num_portraits++;
	}		
	
	
	/* Load biography */
	if((node = ezxml_get(root, "bios", 0, "bio", 0, "text", -1)) != NULL) {
		if(arb->biography != NULL)
			free(arb->biography);
		
		arb->biography = strdup(node->txt);
	}


	/* Load similar artists */
	for(loop_node = ezxml_get(root, "similar-artists", 0, "artist", -1);
	    loop_node;
	    loop_node = loop_node->next) {
		
		if((node = ezxml_get(loop_node, "id", -1)) == NULL)
			continue;
		
		arb->similar_artists = realloc(arb->similar_artists, sizeof(sp_artist *) * (1 + arb->num_similar_artists));
		hex_ascii_to_bytes(node->txt, id, 16);

		arb->similar_artists[arb->num_similar_artists]
					= osfy_artist_add(session, id);
		sp_artist_add_ref(arb->similar_artists[arb->num_similar_artists]);

		if(sp_artist_is_loaded(arb->similar_artists[arb->num_similar_artists]) == 0) {
			DSFYDEBUG("Loading similar artist from artistbrowse XML\n");
			osfy_artist_load_artist_from_xml(session, 
						arb->similar_artists[arb->num_similar_artists],
							       loop_node);
		}
		assert(sp_artist_is_loaded(arb->similar_artists[arb->num_similar_artists]));
		
		arb->num_similar_artists++;
	}
	
	
	
	
	/* Loop over each album listed */
	assert(arb->num_tracks == 0);
	for(album_node = ezxml_get(root, "albums", 0, "album", -1);
	    album_node;
	    album_node = album_node->next) {

		/* Extract album ID and add it */
		if((node = ezxml_get(album_node, "id", -1)) == NULL)
			continue;

		hex_ascii_to_bytes(node->txt, id, 16);
		album = sp_album_add(session, id);

		
		/* Load album if necessary */
		if(sp_album_is_loaded(album) == 0)
		   osfy_album_load_from_album_xml(session, album, album_node);
		   
		assert(sp_album_is_loaded(album));
		
		
		/* Loop over each disc in the album and add tracks */
		for(loop_node = ezxml_get(album_node, "discs", 0, "disc", -1);
		    loop_node;
		    loop_node = loop_node->next) {
			
			/* Cache disc number */
			if((node = ezxml_get(loop_node, "disc-number", -1)) == NULL) {
				DSFYDEBUG("BUG: Found no 'disc-numner' under discs -> disc\n");
				continue;
			}
			
			disc_number = atoi(node->txt);
			
			
			/* Loop over each track and add it to the artistbrowse tracks list */
			for(track_node = ezxml_get(loop_node, "track", -1), i = 1;
			    track_node;
			    track_node = track_node->next, i++) {
				
				/* Extract track ID and add it */
				if((node = ezxml_get(track_node, "id", -1)) == NULL)
					continue;

				hex_ascii_to_bytes(node->txt, id, 16);
				track = osfy_track_add(session, id);


				/* Add album to track */
				if(track->album)
					sp_album_release(track->album);
				
				track->album = album;
				sp_album_add_ref(track->album);

								
				/* Set disc number */
				track->disc = disc_number;


				/* Set track index on disc */
				if(track->index == 0)
					track->index = i;


				/* Load track details from XML if not already loaded */
				if(sp_track_is_loaded(track) == 0)
					osfy_track_load_from_xml(session, track, track_node);

				assert(sp_track_is_loaded(track));


				/* Add track to artistbrowse and increase the track's ref count */
				arb->tracks = realloc(arb->tracks, sizeof(sp_track *) * (1 + arb->num_tracks));
				arb->tracks[arb->num_tracks] = track;
				sp_track_add_ref(arb->tracks[arb->num_tracks]);
				   
				arb->num_tracks++;
			}
			
		} /* for each disc in album */


	} /* for each album */
	
	assert(arb->num_tracks != 0);
	
	arb->is_loaded = 1;
	arb->error = SP_ERROR_OK;
	
	return 0;
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

	assert(arb->ref_count > 0);
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
		sp_artist_release(arb->similar_artists[i]);
	
	if(arb->num_similar_artists)
		free(arb->similar_artists);


	free(arb);
}
