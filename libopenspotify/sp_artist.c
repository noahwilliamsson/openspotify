#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <spotify/api.h>

#include "artist.h"
#include "browse.h"
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
	
	if((node = ezxml_get(artist_node, "name", -1)) != NULL) {
		/* XML from artist browsing */
		
		/* Artist UUID */
		hex_ascii_to_bytes(node->txt, id, sizeof(artist->id));
		assert(memcmp(artist->id, id, sizeof(artist->id)) == 0);
		
		/* Artist name */
		if((node = ezxml_get(artist_node, "name", -1)) == NULL) {
			DSFYDEBUG("Failed to find element 'name'\n");
			return -1;
		}

		assert(strlen(node->txt) < 256);
		artist->name = realloc(artist->name, strlen(node->txt) + 1);
		strcpy(artist->name, node->txt);
		
		artist->is_loaded = 1;
	}
	else if((node = ezxml_get(artist_node, "artist", -1)) != NULL) {
		/* XML from track browsing */

		/* Artist UUID */
		hex_ascii_to_bytes(node->txt, id, sizeof(artist->id));
		assert(memcmp(artist->id, id, sizeof(artist->id)) == 0);
		
		/* Artist name */
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
		DSFYDEBUG("Failed to find element 'name' (browse artist) or 'artist' (browse track)\n");
		return -1;
	}
	
	return 0;
}


static int osfy_artist_browse_callback(struct browse_callback_ctx *brctx);

/*
 * Initiate a browse of a single artist
 * Used by sp_link.c if the obtained artist is not loaded
 *
 */
int osfy_artist_browse(sp_session *session, sp_artist *artist) {
	sp_artist **artists;
	void **container;
	struct browse_callback_ctx *brctx;
	
	/*
	 * Temporarily increase ref count for the artist so it's not free'd
	 * accidentily. It will be decreaed by the chanel callback.
	 *
	 */
	sp_artist_add_ref(artist);
	

	/* The browse processor requires a list of artists */
	artists = (sp_artist **)malloc(sizeof(sp_artist *));
	*artists = artist;

	
	/* The artist callback context */
	brctx = (struct browse_callback_ctx *)malloc(sizeof(struct browse_callback_ctx));
	
	brctx->session = NULL; /* Filled in by the request processor */
	brctx->req = NULL; /* Filled in by the request processor */
	brctx->buf = NULL; /* Filled in by the request processor */
	
	brctx->type = REQ_TYPE_BROWSE_ARTIST;
	brctx->data.artists = artists;
	brctx->num_total = 1;
	brctx->num_browsed = 0;
	brctx->num_in_request = 0;
	
	
	/* Our gzip'd XML parser */
	brctx->browse_parser = osfy_artist_browse_callback;
	
	/* Request input container. Will be free'd when the request is finished. */
	container = (void **)malloc(sizeof(void *));
	*container = brctx;
	
	return request_post(session, REQ_TYPE_BROWSE_ARTIST, container);
}


static int osfy_artist_browse_callback(struct browse_callback_ctx *brctx) {
	sp_artist **artists;
	int i;
	struct buf *xml;
	ezxml_t root;
	
	xml = despotify_inflate(brctx->buf->ptr, brctx->buf->len);
	{
		FILE *fd;
		DSFYDEBUG("Decompresed %d bytes data, xml=%p\n",
			  brctx->buf->len, xml);
		fd = fopen("browse-artists.xml", "w");
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
	
	
	artists = brctx->data.artists;
	for(i = 0; i < brctx->num_in_request; i++) {
		osfy_artist_load_from_xml(brctx->session, artists[brctx->num_browsed + i], root);
	}

	
	ezxml_free(root);
	buf_free(xml);


	/* Release references made in osfy_artist_browse() */
	for(i = 0; i < brctx->num_in_request; i++)
		sp_artist_release(artists[brctx->num_browsed + i]);
	
	
	return 0;
}

