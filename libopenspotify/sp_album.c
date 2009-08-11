#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <spotify/api.h>

#include "album.h"
#include "artist.h"
#include "browse.h"
#include "debug.h"
#include "ezxml.h"
#include "image.h"
#include "request.h"
#include "sp_opaque.h"
#include "util.h"


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
	ezxml_t node;

	
	if((node = ezxml_get(album_node, "name", -1)) != NULL) {
		/* XML from album browsing */
		
		if((node = ezxml_get(album_node, "id", -1)) == NULL) {
			DSFYDEBUG("Failed to find element 'id'\n");
			return -1;
		}
		
		/* Album name */
		if((node = ezxml_get(album_node, "name", -1)) == NULL) {
			DSFYDEBUG("Failed to find element 'name'\n");
			return -1;
		}
		
		assert(strlen(node->txt) < 256);
		album->name = realloc(album->name, strlen(node->txt) + 1);
		strcpy(album->name, node->txt);
		
		
		/* Album year */
		if((node = ezxml_get(album_node, "year", -1)) == NULL) {
			DSFYDEBUG("Failed to find element 'year'\n");
			return -1;
		}
		
		album->year = atoi(node->txt);
		
		
		/* Album artist */
		if((node = ezxml_get(album_node, "artist-id", -1)) == NULL) {
			DSFYDEBUG("Failed to find element 'artist-id'\n");
			return -1;
		}
		
		if(album->artist != NULL)
			sp_artist_release(album->artist);
		
		hex_ascii_to_bytes(node->txt, id, 16);
		album->artist = osfy_artist_add(session, id);
		sp_artist_add_ref(album->artist);
		
		if(sp_artist_is_loaded(album->artist) == 0)
			osfy_artist_load_from_xml(session, album->artist, album_node);
		
		
		/* Album cover */
		if((node = ezxml_get(album_node, "cover", -1)) == NULL) {
			DSFYDEBUG("Failed to find element 'cover'\n");
			return -1;
		}
		
		if(album->image != NULL)
			sp_image_release(album->image);
		
		hex_ascii_to_bytes(node->txt, id, 20);
		album->image = osfy_image_create(session, id);
		sp_image_add_ref(album->image);
		
		
		/* Done loading */
		album->is_loaded = 1;
	}
	else if((node = ezxml_get(album_node, "album-id", -1)) != NULL) {
		/* XML from track browsing */

		/* Album UUID */
		hex_ascii_to_bytes(node->txt, id, sizeof(album->id));
		assert(memcmp(album->id, id, sizeof(album->id)) == 0);

		/* Album name */
		if((node = ezxml_get(album_node, "album", -1)) == NULL) {
			DSFYDEBUG("Failed to find element 'album'\n");
			return -1;
		}
		
		assert(strlen(node->txt) < 256);
		album->name = realloc(album->name, strlen(node->txt) + 1);
		strcpy(album->name, node->txt);

		
		/* Album year */
		if((node = ezxml_get(album_node, "year", -1)) == NULL) {
			DSFYDEBUG("Failed to find element 'year'\n");
			return -1;
		}
		
		album->year = atoi(node->txt);

		
		/* Album artist */
		if((node = ezxml_get(album_node, "album-artist-id", -1)) == NULL) {
			DSFYDEBUG("Failed to find element 'album-artist-id'\n");
			return -1;
		}

		if(album->artist != NULL)
			sp_artist_release(album->artist);
		
		hex_ascii_to_bytes(node->txt, id, 16);
		album->artist = osfy_artist_add(session, id);
		sp_artist_add_ref(album->artist);
		
		if(sp_artist_is_loaded(album->artist) == 0)
			osfy_artist_load_from_xml(session, album->artist, album_node);



		/* Album cover */
		if((node = ezxml_get(album_node, "cover", -1)) == NULL) {
			DSFYDEBUG("Failed to find element 'cover'\n");
			return -1;
		}
		
		if(album->image != NULL)
			sp_image_release(album->image);
		
		hex_ascii_to_bytes(node->txt, id, 20);
		album->image = osfy_image_create(session, id);
		sp_image_add_ref(album->image);
		

		/* Done loading */
		album->is_loaded = 1;
	}
	else {
		DSFYDEBUG("Failed to find element 'album-id'\n");
		return -1;
	}

	return 0;
}


static int osfy_album_browse_callback(struct browse_callback_ctx *brctx);

/*
 * Initiate a browse of a single album
 * Used by sp_link.c if the obtained album is not loaded
 *
 */
int osfy_album_browse(sp_session *session, sp_album *album) {
	sp_album **albums;
	void **container;
	struct browse_callback_ctx *brctx;
	
	/*
	 * Temporarily increase ref count for the album so it's not free'd
	 * accidentily. It will be decreaed by the chanel callback.
	 *
	 */
	sp_album_add_ref(album);
	
	
	/* The browse processor requires a list of albums */
	albums = (sp_album **)malloc(sizeof(sp_album *));
	*albums = album;
	
	
	/* The album callback context */
	brctx = (struct browse_callback_ctx *)malloc(sizeof(struct browse_callback_ctx));
	
	brctx->session = session;
	brctx->req = NULL; /* Filled in by the request processor */
	brctx->buf = NULL; /* Filled in by the request processor */
	
	brctx->type = REQ_TYPE_BROWSE_ARTIST;
	brctx->data.albums = albums;
	brctx->num_total = 1;
	brctx->num_browsed = 0;
	brctx->num_in_request = 0;
	
	
	/* Our gzip'd XML parser */
	brctx->browse_parser = osfy_album_browse_callback;
	
	/* Request input container. Will be free'd when the request is finished. */
	container = (void **)malloc(sizeof(void *));
	*container = brctx;
	
	return request_post(session, REQ_TYPE_BROWSE_ALBUM, container);
}


static int osfy_album_browse_callback(struct browse_callback_ctx *brctx) {
	sp_album **albums;
	int i;
	struct buf *xml;
	ezxml_t root;
	
	xml = despotify_inflate(brctx->buf->ptr, brctx->buf->len);
	{
		FILE *fd;
		DSFYDEBUG("Decompresed %d bytes data, xml=%p\n",
			  brctx->buf->len, xml);
		fd = fopen("browse-albums.xml", "w");
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
	
	albums = brctx->data.albums;
	for(i = 0; i < brctx->num_in_request; i++) {
		osfy_album_load_from_xml(brctx->session, albums[brctx->num_browsed + i], root);
	}
	
	
	ezxml_free(root);
	buf_free(xml);
	

	/* Release references made in osfy_album_browse() */
	for(i = 0; i < brctx->num_in_request; i++)
		sp_album_release(albums[brctx->num_browsed + i]);
	
	
	return 0;
}

