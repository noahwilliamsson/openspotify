#include <stdio.h>
#include <string.h>
#include <assert.h>
#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif

#include <spotify/api.h>

#include "album.h"
#include "artist.h"
#include "browse.h"
#include "debug.h"
#include "ezxml.h"
#include "hashtable.h"
#include "sp_opaque.h"
#include "track.h"
#include "util.h"


SP_LIBEXPORT(bool) sp_track_is_loaded(sp_track *track) {

	return track->is_loaded;
}


SP_LIBEXPORT(bool) sp_track_is_available(sp_track *track) {

	return track->is_available;
}


SP_LIBEXPORT(sp_error) sp_track_error(sp_track *track) {

	return track->error;
}


SP_LIBEXPORT(int) sp_track_num_artists(sp_track *track) {

	return track->num_artists;
}


SP_LIBEXPORT(sp_artist *) sp_track_artist(sp_track *track, int index) {

	if(index > track->num_artists)
		return NULL;

	return track->artists[index];
}


SP_LIBEXPORT(sp_album *) sp_track_album(sp_track *track) {

	return track->album;
}


SP_LIBEXPORT(const char *) sp_track_name(sp_track *track) {

	return track->name;
}


SP_LIBEXPORT(int) sp_track_duration(sp_track *track) {

	return track->duration;
}


SP_LIBEXPORT(int) sp_track_popularity(sp_track *track) {

	/* XXX - This should be kept as a double and converted to int in here instead */
	return track->popularity;
}


SP_LIBEXPORT(int) sp_track_disc(sp_track *track) {

	return track->disc;
}


SP_LIBEXPORT(int) sp_track_index(sp_track *track) {

	return track->index;
}


SP_LIBEXPORT(void) sp_track_add_ref(sp_track *track) {

	track->ref_count++;
}


SP_LIBEXPORT(void) sp_track_release(sp_track *track) {
	assert(track->ref_count > 0);

	if(--track->ref_count)
		return;

	DSFYDEBUG("Freeing track %p because of zero ref_count\n", track);
	osfy_track_free(track);
}


/*
 * Functions for internal use
 *
 */


sp_track *osfy_track_add(sp_session *session, unsigned char id[16]) {
	sp_track *track;

	assert(session != NULL);

	if((track = (sp_track *)hashtable_find(session->hashtable_tracks, id)) != NULL)
		return track;


	track = (sp_track *)malloc(sizeof(sp_track));
	if(track == NULL)
		return NULL;

	DSFYDEBUG("Allocated track at %p\n", track);

	track->hashtable = session->hashtable_tracks;
	hashtable_insert(track->hashtable, id, track);

	memcpy(track->id, id, sizeof(track->id));
	memset(track->file_id, 0, sizeof(track->file_id));

	track->name = NULL;

	track->album = NULL;
	track->num_artists = 0;
	track->artists = NULL;

	track->is_available = 0;
	track->restricted_countries = NULL;
	track->allowed_countries = NULL;

	track->index = 0;
	track->disc = 0;
	track->duration = 0;
	track->popularity = 0;

	track->is_loaded = 0;
	track->error = SP_ERROR_RESOURCE_NOT_LOADED;

	track->ref_count = 0;

	return track;
}


void osfy_track_free(sp_track *track) {
	int i;

	assert(track->ref_count == 0);

	hashtable_remove(track->hashtable, track->id);

	if(track->name)
		free(track->name);


	for(i = 0; i < track->num_artists; i++)
		sp_artist_release(track->artists[i]);

	if(track->num_artists)
		free(track->artists);

	if(track->album)
		sp_album_release(track->album);

	if(track->restricted_countries)
		free(track->restricted_countries);
	
	if(track->allowed_countries)
		free(track->allowed_countries);

	DSFYDEBUG("Deallocated track at %p\n", track);

	free(track);
}


int osfy_track_load_from_xml(sp_session *session, sp_track *track, ezxml_t track_node) {
	unsigned char id[20];
	const char *str;
	double popularity;
	int i;
	ezxml_t node;
	

	/* Track UUID */
	if((node = ezxml_get(track_node, "id", -1)) == NULL) {
		DSFYDEBUG("Failed to find element 'id'\n");
		/* This might happen when a track that doesn't exist is browsed */

		/* FIXME: Shouldn't we set track->error to SP_ERROR_OTHER_PERMANENT ? */
		return -1;
	}
	
	DSFYDEBUG("Found track with ID '%s' in XML\n", node->txt);
	hex_ascii_to_bytes(node->txt, id, sizeof(track->id));

	/*
	 * FIXME:
	 * A request for track with id X might return a different track
	 * (i.e, the 'id' element differs from the id of the track requested)
	 * with one of the 'redirect' elements set to the requested track's id.
	 *
	 * Below is an example where track with id '3c1919e237ca4f2c9b5fc686b7a6f6c3'
	 * was browsed but a different track returned (a5a43c74af924171a50f0668aee36b43)
	 * '3c1919e237ca4f2c9b5fc686b7a6f6c3' appears in the redirect element.
	 *
	 * <id>a5a43c74af924171a50f0668aee36b43</id>
	 * <redirect>3c1919e237ca4f2c9b5fc686b7a6f6c3</redirect>
	 * <redirect>93934b1df8984c6586a63d18cd6ecfa6</redirect>
	 * <redirect>2e0d3f5a98014c40932a014b2a9eca69</redirect>
	 * <title>Insane in the Brain</title>
	 * <artist-id>9e74e7856a07496190ef2180d26003db</artist-id>
	 * <artist>Cypress Hill</artist>
	 * <album>Black Sunday</album>
	 * <album-id>c3711d81999b48529903bf708b8192da</album-id>
	 * <album-artist>Cypress Hill</album-artist>
	 * <album-artist-id>9e74e7856a07496190ef2180d26003db</album-artist-id>
	 * <year>1993</year>
	 * <track-number>3</track-number>
	 *
	 * Commented out:
	 *	assert(memcmp(track->id, id, sizeof(track->id)) == 0);
	 *
	 * Not quite sure what the proper way to handle this is.
	 *
	 */
	
	
	/* Track name */
	if((node = ezxml_get(track_node, "title", -1)) == NULL) {
		DSFYDEBUG("Failed to find element 'title'\n");
		return -1;
	}

	track->name = realloc(track->name, strlen(node->txt) + 1);
	strcpy(track->name, node->txt);
	

	/* Track popularity */
	if((node = ezxml_get(track_node, "popularity", -1)) == NULL) {
		DSFYDEBUG("Failed to find element 'popularity'\n");
		return -1;
	}
	
	sscanf(node->txt, "%lf", &popularity);
	track->popularity = (int)(100 * popularity);

	
	/*
	 * Grab ID of file
	 * Multiple files might be listed here, all with different bit rates
	 * Zero 'file' elements indicates the file is not available.
	 *
	 * Example:
	 * <files>
	 *   <file id="cfe68177e9eb9526b7b441f6147d1c5a9a07ca62" format="Ogg Vorbis,160000,1,32,4"/>
	 *   <file id="bf1314d9814795f64a995c6dc8a9b6cc12b952d6" format="Ogg Vorbis,96000,1,32,4"/>
	 * </files>
	 *
	 */
	memset(track->file_id, 0, sizeof(track->file_id));
	for(node = ezxml_get(track_node, "files", 0, "file", -1);
	    node;
	    node = node->next) {
		/* XXX - Only care about 160kbit/s files for now */
		str = ezxml_attr(node, "format");
		if(!strstr(str, "160000")) {
			continue;
		}

		str = ezxml_attr(node, "id");
		assert(str != NULL);
		hex_ascii_to_bytes(str, id, sizeof(track->file_id));
		memcpy(track->file_id, id, sizeof(track->file_id));
	}

	
	/* Track duration */
	if((node = ezxml_get(track_node, "length", -1)) != NULL) {
		track->duration = atoi(node->txt);
	}
	else {
		/* Track duration defaults to zero so no update is needed */
		DSFYDEBUG("Track has no length, files are not available\n");
	}


	/* Country restrictions */
	assert(track->allowed_countries == NULL);
	assert(track->restricted_countries == NULL);
	for(node = ezxml_get(track_node, "restrictions", 0, "restriction", -1);
	    node;
	    node = node->next) {
	    	str = ezxml_attr(node, "catalogues");


		/* There might be restrictions that do not apply for premium users */
		if(!str || !strstr(str, "premium"))
			continue;

		if((str = ezxml_attr(node, "allowed")) != NULL) {
			track->allowed_countries = realloc(track->allowed_countries, strlen(str) + 1);
			strcpy(track->allowed_countries, str);

			if(strstr(track->allowed_countries, session->country))
				track->is_available = 1;
		}

		if((str = ezxml_attr(node, "forbidden")) != NULL) {
			track->restricted_countries = realloc(track->restricted_countries, strlen(str) + 1);
			strcpy(track->restricted_countries, str);

			if(strstr(track->restricted_countries, session->country))
				track->is_available = 0;
			else
				track->is_available = 1;
		}
	}


	/* Tracks with no files can't be played */
	if(track->duration == 0)
		track->is_available = 0;


	/* Add artists */
	for(node = ezxml_get(track_node, "artist-id", -1);
	    node;
	    node = node->next) {
		hex_ascii_to_bytes(node->txt, id, 16);
		for(i = 0; i < track->num_artists; i++)
			if(memcmp(track->artists[i]->id, id, sizeof(track->artists[i]->id)) == 0)
				break;
	
		/* Do not add already added artists */
		if(i != track->num_artists)
			continue;
		
		DSFYDEBUG("Adding artist '%s' to track's list\n", node->txt);

		track->artists = realloc(track->artists, sizeof(sp_artist *) * (1 + track->num_artists));
		track->artists[track->num_artists] = osfy_artist_add(session, id);
		sp_artist_add_ref(track->artists[track->num_artists]);
		
		if(sp_artist_is_loaded(track->artists[track->num_artists]) == 0)
			osfy_artist_load_track_artist_from_xml(session, 
							       track->artists[track->num_artists],
							       track_node);
			
		
		track->num_artists++;
	}


	/* Track album */
	{
		char buf[33];
		hex_bytes_to_ascii(track->id, buf, 16);
		DSFYDEBUG("Loading album for track '%s'\n", buf);
	}
	if((node = ezxml_get(track_node, "album-id", -1)) != NULL) {
		/* Add album to track */
		if(track->album != NULL)
			sp_album_release(track->album);

		hex_ascii_to_bytes(node->txt, id, 16);
		track->album = sp_album_add(session, id);
		sp_album_add_ref(track->album);

		/* Load album from XML if necessary */
		if(sp_album_is_loaded(track->album) == 0) {
			{
				char buf[33];
				hex_bytes_to_ascii(track->album->id, buf, 16);
				DSFYDEBUG("Album '%s' not yet loaded, trying to load from XML\n", buf);
			}
			osfy_album_load_from_track_xml(session, track->album, track_node);
		}
		
		assert(sp_album_is_loaded(track->album));
		
		
	}
	
	
	track->is_loaded = 1;
	track->error = SP_ERROR_OK;

	return 0;
}


static int osfy_track_browse_callback(struct browse_callback_ctx *brctx);

/*
 * Initiate a browse of a single track
 * Used by sp_link.c if the obtained track is not loaded
 *
 */
int osfy_track_browse(sp_session *session, sp_track *track) {
	sp_track **tracks;
	void **container;
	struct browse_callback_ctx *brctx;
	
	/*
	 * Temporarily increase ref count for the track so it's not free'd
	 * accidentily. It will be decreaed by the chanel callback.
	 *
	 */
	sp_track_add_ref(track);
	
	
	/* The browse processor requires a list of tracks */
	tracks = (sp_track **)malloc(sizeof(sp_track *));
	*tracks = track;
	
	
	/* The track callback context */
	brctx = (struct browse_callback_ctx *)malloc(sizeof(struct browse_callback_ctx));
	
	brctx->session = session;
	brctx->req = NULL; /* Filled in by the request processor */
	brctx->buf = NULL; /* Filled in by the request processor */
	
	brctx->type = REQ_TYPE_BROWSE_TRACK;
	brctx->data.tracks = tracks;
	brctx->num_total = 1;
	brctx->num_browsed = 0;
	brctx->num_in_request = 0;
	
	
	/* Our gzip'd XML parser */
	brctx->browse_parser = osfy_track_browse_callback;
	
	/* Request input container. Will be free'd when the request is finished. */
	container = (void **)malloc(sizeof(void *));
	*container = brctx;
	
	return request_post(session, REQ_TYPE_BROWSE_ALBUM, container);
}


static int osfy_track_browse_callback(struct browse_callback_ctx *brctx) {
	sp_track **tracks;
	int i;
	struct buf *xml;
	ezxml_t root, node;
	
	xml = despotify_inflate(brctx->buf->ptr, brctx->buf->len);
	if(xml == NULL) {
		DSFYDEBUG("Failed to decompress track XML\n");
		return -1;
	}

#ifdef DEBUG
	{
		FILE *fd = fopen("browse-tracks.xml", "w");
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
	
	tracks = brctx->data.tracks;
	for(i = 0; i < brctx->num_in_request; i++) {
		node = ezxml_get(root, "tracks", 0, "track", -1);
		if(osfy_track_load_from_xml(brctx->session, tracks[brctx->num_browsed + i], node)) {
			DSFYDEBUG("Failed to load track %d of %d from XML, error is %d\n", 
				brctx->num_browsed + i + 1, brctx->num_in_request,
				tracks[brctx->num_browsed + i]->error);
		}
	}
	
	
	ezxml_free(root);
	buf_free(xml);
	
	
	/* Release references made in osfy_track_browse() */
	for(i = 0; i < brctx->num_in_request; i++)
		sp_track_release(tracks[brctx->num_browsed + i]);
	
	
	return 0;
}


void osfy_track_garbage_collect(sp_session *session) {
	struct hashiterator *iter;
	struct hashentry *entry;
	sp_track *track;

	iter = hashtable_iterator_init(session->hashtable_tracks);
	while((entry = hashtable_iterator_next(iter))) {
		track = (sp_track *)entry->value;

		if(track->ref_count)
			continue;

		DSFYDEBUG("Freeing track %p because of zero ref_count\n", track);
		osfy_track_free(track);
	}

	hashtable_iterator_free(iter);
}


int osfy_track_metadata_save_to_disk(sp_session *session, char *filename) {
	FILE *fd;
	struct hashiterator *iter;
	struct hashentry *entry;
	sp_track *track;
	unsigned char len;
	unsigned int num;

	if((fd = fopen(filename, "w")) == NULL)
		return -1;
	
	iter = hashtable_iterator_init(session->hashtable_tracks);
	while((entry = hashtable_iterator_next(iter))) {
		track = (sp_track *)entry->value;

		if(track->is_loaded == 0)
			continue;

		fwrite(track->id, sizeof(track->id), 1, fd);
		fwrite(track->file_id, sizeof(track->file_id), 1, fd);
		#if 0
		fwrite(track->album->id, sizeof(track->album->id), 1, fd);
		#endif
		
		len = (track->name? strlen(track->name): 0);
		fwrite(&len, 1, 1, fd);
		fwrite(track->name, len, 1, fd);

		num = htons(track->index);
		fwrite(&num, sizeof(int), 1, fd);
		num = htons(track->disc);
		fwrite(&num, sizeof(int), 1, fd);
	}

	hashtable_iterator_free(iter);
	fclose(fd);

	return 0;
}


int osfy_track_metadata_load_from_disk(sp_session *session, char *filename) {
	FILE *fd;
	unsigned char len, id16[16], id20[20];
	unsigned int num;
	char buf[256 + 1];
	sp_track *track;

	if((fd = fopen(filename, "r")) == NULL)
		return -1;
	
	/* FIXME: Don't assume lengths on track/artist/album names */
	buf[256] = 0;
	while(!feof(fd)) {
		if(fread(id16, sizeof(id16), 1, fd) == 1)
			track = osfy_track_add(session, id16);
		else
			break;

		if(fread(id20, sizeof(id20), 1, fd) == 1)
			memcpy(track->file_id, id20, sizeof(track->file_id));
		else
			break;

		#if 0
		if(fread(id16, sizeof(id16), 1, fd) == 1) {
			album = sp_album_add(session, id16);
			track->album = album;
			sp_album_add_ref(album);
		}
		else
			break;
		#endif

		
		if(fread(&len, 1, 1, fd) == 1) {
			if(len && fread(buf, len, 1, fd) == 1) {
				buf[len] = 0;

				track->name = realloc(track->name, strlen(buf) + 1);
				strcpy(track->name, buf);
			}
		}
		else
			break;


		if(fread(&num, 1, 1, fd) == 1)
			track->index = ntohs(num);
		else
			break;

		if(fread(&num, sizeof(int), 1, fd) == 1)
			track->disc = ntohs(num);
		else
			break;

		if(fread(&num, sizeof(int), 1, fd) == 1)
			track->duration = ntohs(num);
		else
			break;


		track->is_loaded = 1;
	}

	fclose(fd);

	return 0;
}
