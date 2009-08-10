#include <stdio.h>
#include <string.h>

#include <spotify/api.h>

#include "artist.h"
#include "album.h"
#include "debug.h"
#include "link.h"
#include "util.h"
#include "sp_opaque.h"
#include "track.h"

/* The Visual C++ compiler doesn't know 'snprintf'... */
#ifdef _MSC_VER
#define snprintf _snprintf
#endif

static void baseconvert(const char *src, char *dest, int frombase, int tobase, int padlen);
static void id_bytes_to_uri(const unsigned char* id, char* uri);
static void id_uri_to_bytes(const char* uri, unsigned char* id);

SP_LIBEXPORT(sp_link *) sp_link_create_from_string (const char *link) {
	const char *ptr;
	sp_link *lnk;
	unsigned char id[16];
	sp_session *session;
	
	if(link == NULL)
		return NULL;
	
	ptr = link;
	
	/* Check if link starts with "spotify:" */
	if(strncmp("spotify:", ptr, 8) != 0)
		return NULL;

	ptr += 8;
	
	/* Get session. */
	session = libopenspotify_link_get_session();
	
	if(session == NULL)
		return NULL;
	
	/* Allocate memory for link. */
	if((lnk = (sp_link *)malloc(sizeof(sp_link))) == NULL)
		return NULL;
	
	lnk->type      = SP_LINKTYPE_INVALID;
	lnk->data.data = NULL;
	lnk->refs      = 1;
	
	/* Link refers to a track. */
	if(strncmp("track:", ptr, 6) == 0){
		ptr += 6;

		id_uri_to_bytes(ptr, id);

		lnk->type       = SP_LINKTYPE_TRACK;
		lnk->data.track = osfy_track_add(session, id);
	}
	/* Link refers to an album. */
	else if(strncmp("album:", ptr, 6) == 0){
		ptr += 6;

		id_uri_to_bytes(ptr, id);

		lnk->type       = SP_LINKTYPE_ALBUM;
		lnk->data.album = sp_album_add(session, id);
	}
	/* Link refers to an artist. */
	else if(strncmp("artist:", ptr, 7) == 0){
		ptr += 7;

		id_uri_to_bytes(ptr, id);

		lnk->type        = SP_LINKTYPE_ARTIST;
		lnk->data.artist = osfy_artist_add(session, id);
	}
	/* Link is a search query. */
	else if(strncmp("search:", ptr, 7) == 0){
		ptr += 7;

		lnk->type        = SP_LINKTYPE_SEARCH;
		lnk->data.search = NULL; //FIXME: search_add / query is @ ptr
	}
	/* Link probably refers to a playlist. */
	else if(strncmp("user:", ptr, 5) == 0){
		char username[256];
		unsigned char len;
		
		ptr += 5;
		len  = strchr(ptr, ':') - ptr;
		
		/* Copy username from link. */
		strncpy(username, ptr, len);
		username[len] = 0;
		
		ptr += len + 1;
		
		/* Link actually refers to a playlist. */
		if(strncmp("playlist:", ptr, 9) == 0){
			ptr += 9;

			id_uri_to_bytes(ptr, id);

			lnk->type          = SP_LINKTYPE_PLAYLIST;
			lnk->data.playlist = NULL; //FIXME: playlist_add
		}
		else{
			sp_link_release(lnk);
			
			return NULL;
		}
	}
	else {
		sp_link_release(lnk);
		
		return NULL;
	}
	
	DSFYDEBUG("Created link with type %d from '%s'.\n", lnk->type, link);

	return lnk;
}

SP_LIBEXPORT(sp_link *) sp_link_create_from_track (sp_track *track, int offset) {
	sp_link *link;
	
	if(track == NULL)
		return NULL;
	
	if(offset != 0)
		DSFYDEBUG("TODO: offset not implemented\n");
	
	if((link = (sp_link *)malloc(sizeof(sp_link))) == NULL)
		return NULL;
	
	link->type       = SP_LINKTYPE_TRACK;
	link->data.track = track;
	sp_track_add_ref(track);
	link->refs       = 1;
	
	return link;
}

SP_LIBEXPORT(sp_link *) sp_link_create_from_album (sp_album *album) {
	sp_link *link;
	
	if(album == NULL)
		return NULL;
	
	if((link = (sp_link *)malloc(sizeof(sp_link))) == NULL)
		return NULL;
	
	link->type       = SP_LINKTYPE_ALBUM;
	link->data.album = album;
	sp_album_add_ref(album);
	link->refs       = 1;
	
	return link;
}

SP_LIBEXPORT(sp_link *) sp_link_create_from_artist (sp_artist *artist) {
	sp_link *link;
	
	if(artist == NULL)
		return NULL;
	
	if((link = (sp_link *)malloc(sizeof(sp_link))) == NULL)
		return NULL;
	
	link->type        = SP_LINKTYPE_ARTIST;
	link->data.artist = artist;
	sp_artist_add_ref(artist);
	link->refs        = 1;
	
	return link;
}

SP_LIBEXPORT(sp_link *) sp_link_create_from_search (sp_search *search) {
	sp_link *link;
	
	if(search == NULL)
		return NULL;
	
	if((link = (sp_link *)malloc(sizeof(sp_link))) == NULL)
		return NULL;
	
	link->type        = SP_LINKTYPE_SEARCH;
	link->data.search = search;
	link->refs        = 1;
	
	return link;
}

SP_LIBEXPORT(sp_link *) sp_link_create_from_playlist (sp_playlist *playlist) {
	sp_link *link;
	
	if(playlist == NULL)
		return NULL;
	
	if((link = (sp_link *)malloc(sizeof(sp_link))) == NULL)
		return NULL;
	
	link->type          = SP_LINKTYPE_PLAYLIST;
	link->data.playlist = playlist;
	link->refs          = 1;
	
	return link;
}

SP_LIBEXPORT(int) sp_link_as_string (sp_link *link, char *buffer, int buffer_size) {
	unsigned char uri[23];

	if(link == NULL || buffer == NULL || buffer_size < 0)
		return -1;

	switch(link->type){
		case SP_LINKTYPE_TRACK:
			id_bytes_to_uri(link->data.track->id, uri);

			return snprintf(buffer, buffer_size, "spotify:track:%s", uri);
		case SP_LINKTYPE_ALBUM:
			id_bytes_to_uri(link->data.album->id, uri);

			return snprintf(buffer, buffer_size, "spotify:album:%s", uri);
		case SP_LINKTYPE_ARTIST:
			id_bytes_to_uri(link->data.artist->id, uri);

			return snprintf(buffer, buffer_size, "spotify:artist:%s", uri);
		case SP_LINKTYPE_SEARCH:
			//return snprintf(buffer, buffer_size, "spotify:search:%s", link->data.search->query);
			return snprintf(buffer, buffer_size, "spotify:search:TODO"); // struct sp_search not defined yet.
		case SP_LINKTYPE_PLAYLIST:
			id_bytes_to_uri(link->data.playlist->id, uri);

			return snprintf(buffer, buffer_size, "spotify:playlist:%s", uri);
		case SP_LINKTYPE_INVALID:
		default:
			return -1;
	}
}

SP_LIBEXPORT(sp_linktype) sp_link_type (sp_link *link) {
	if(link == NULL)
		return SP_LINKTYPE_INVALID;

	return link->type;
}

SP_LIBEXPORT(sp_track *) sp_link_as_track (sp_link *link) {
	if(link == NULL || link->type != SP_LINKTYPE_TRACK)
		return NULL;

	return link->data.track;
}

SP_LIBEXPORT(sp_album *) sp_link_as_album (sp_link *link) {
	if(link == NULL || link->type != SP_LINKTYPE_ALBUM)
		return NULL;

	return link->data.album;
}

SP_LIBEXPORT(sp_artist *) sp_link_as_artist (sp_link *link) {
	if(link == NULL || link->type != SP_LINKTYPE_ARTIST)
		return NULL;

	return link->data.artist;
}

SP_LIBEXPORT(void) sp_link_add_ref (sp_link *link) {
	++(link->refs);

	switch(link->type) {
	case SP_LINKTYPE_ALBUM:
		sp_album_add_ref(link->data.album);
		break;
	case SP_LINKTYPE_ARTIST:
		sp_artist_add_ref(link->data.artist);
		break;
	case SP_LINKTYPE_TRACK:
		sp_track_add_ref(link->data.track);
		break;
	default:
		break;
	}
}

SP_LIBEXPORT(void) sp_link_release (sp_link *link) {
	if(link == NULL)
		return;

	switch(link->type) {
	case SP_LINKTYPE_ALBUM:
		sp_album_release(link->data.album);
		break;
	case SP_LINKTYPE_ARTIST:
		sp_artist_release(link->data.artist);
		break;
	case SP_LINKTYPE_TRACK:
		sp_track_release(link->data.track);
		break;
	default:
		break;
	}

	if(--(link->refs) <= 0) {
		free(link);
	}
}

static void baseconvert(const char *src, char *dest, int frombase, int tobase, int padlen) {
    static const char alphabet[] =
        "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/";
    int number[128];
    int i, len, newlen, divide;

    len = strlen(src);

    for (i = 0; i < len; i++)
        number[i] = strchr(alphabet, src[i]) - alphabet;

    memset(dest, '0', padlen);
    dest[padlen] = 0;

    padlen--;

    do {
        divide = 0;
        newlen = 0;

        for (i = 0; i < len; i++) {
            divide = divide * frombase + number[i];
            if (divide >= tobase) {
                number[newlen++] = divide / tobase;
                divide = divide % tobase;
            } else if (newlen > 0) {
                number[newlen++] = 0;
            }
        }
        len = newlen;

        dest[padlen--] = alphabet[divide];
    } while (newlen != 0);
}

/* 
 * Convert an id (16 bytes) to a base62 encoded string.
 * URI buffer needs to have at least 23 bytes and is
 * automatically null-terminated by this function.
 */
static void id_bytes_to_uri(const unsigned char* id, char* uri){
	char hex[33];

	if(id == NULL || uri == NULL)
		return;

	hex_bytes_to_ascii(id, hex, 16);

	hex[32] = 0;

    baseconvert(hex, uri, 16, 62, 22);

	uri[22] = 0;
}

/* 
 * Convert a URI (22 character string, null-terminated) to an id.
 * id buffer needs to have at least 16 bytes.
 */
static void id_uri_to_bytes(const char* uri, unsigned char* id){
	char hex[33];

	if(uri == NULL || id == NULL)
		return;

    baseconvert(uri, hex, 62, 16, 32);

	hex[32] = 0;

	hex_ascii_to_bytes(hex, id, 16);
}
