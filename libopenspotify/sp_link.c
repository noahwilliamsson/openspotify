#include <stdio.h>
#include <string.h>

#include <spotify/api.h>

#include "debug.h"
#include "sp_opaque.h"

/* The Visual C++ compiler doesn't know 'snprintf'... */
#ifdef _MSC_VER
#define snprintf _snprintf
#endif

SP_LIBEXPORT(sp_link *) sp_link_create_from_string (const char *link) {
	const char *ptr;
	sp_link *lnk;
	
	if(link == NULL)
		return NULL;
	
	ptr = link;
	
	/* Check if link starts with "spotify:" */
	if(strncmp("spotify:", ptr, 8) != 0)
		return NULL;

	ptr += 8;
	
	/* Allocate memory for link. */
	if((lnk = (sp_link *)malloc(sizeof(sp_link))) == NULL)
		return NULL;
	
	lnk->type      = SP_LINKTYPE_INVALID;
	lnk->data.data = NULL;
	lnk->refs      = 1;
	
	/* Link refers to a track. */
	if(strncmp("track:", ptr, 6) == 0){
		ptr += 6;

		lnk->type       = SP_LINKTYPE_TRACK;
		lnk->data.track = NULL; //FIXME: create track / base 64 encoded id is @ ptr
	}
	/* Link refers to an album. */
	else if(strncmp("album:", ptr, 6) == 0){
		ptr += 6;

		lnk->type       = SP_LINKTYPE_ALBUM;
		lnk->data.album = NULL; //FIXME: create album / base 64 encoded id is @ ptr
	}
	/* Link refers to an artist. */
	else if(strncmp("artist:", ptr, 7) == 0){
		ptr += 7;
		
		lnk->type        = SP_LINKTYPE_ARTIST;
		lnk->data.artist = NULL; //FIXME: create artist / base 64 encoded id is @ ptr
	}
	/* Link is a search query. */
	else if(strncmp("search:", ptr, 7) == 0){
		ptr += 7;

		lnk->type        = SP_LINKTYPE_SEARCH;
		lnk->data.search = NULL; //FIXME: create search / query is @ ptr
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
			
			lnk->type          = SP_LINKTYPE_PLAYLIST;
			lnk->data.playlist = NULL; //FIXME: create playlist / base 64 encoded id is @ ptr
		}
		else{
			sp_link_release(lnk);
			
			return NULL;
		}
	}
	else{
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
	if(link == NULL || buffer == NULL || buffer_size < 0)
		return -1;
	
	switch(link->type){
		case SP_LINKTYPE_TRACK:
			//return snprintf(buffer, buffer_size, "spotify:track:%s", link->data.track->id);
			return snprintf(buffer, buffer_size, "spotify:track:TODO");
		case SP_LINKTYPE_ALBUM:
			//return snprintf(buffer, buffer_size, "spotify:album:%s", link->data.album->id);
			return snprintf(buffer, buffer_size, "spotify:album:TODO");
		case SP_LINKTYPE_ARTIST:
			//return snprintf(buffer, buffer_size, "spotify:artist:%s", link->data.artist->id);
			return snprintf(buffer, buffer_size, "spotify:artist:TODO");
		case SP_LINKTYPE_SEARCH:
			//return snprintf(buffer, buffer_size, "spotify:search:%s", link->data.search->query);
			return snprintf(buffer, buffer_size, "spotify:search:TODO");
		case SP_LINKTYPE_PLAYLIST:
			//return snprintf(buffer, buffer_size, "spotify:playlist:%s", link->data.playlist->id);
			return snprintf(buffer, buffer_size, "spotify:playlist:TODO");
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
}

SP_LIBEXPORT(void) sp_link_release (sp_link *link) {
	if(link != NULL && --(link->refs) <= 0)
		free(link);
}
