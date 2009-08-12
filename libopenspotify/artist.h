#ifndef LIBOPENSPOTIFY_ARTIST_H
#define LIBOPENSPOTIFY_ARTIST_H

#include <spotify/api.h>

#include "ezxml.h"


sp_artist *osfy_artist_add(sp_session *session, unsigned char id[16]);
void osfy_artist_free(sp_artist *artist);
int osfy_artist_load_artist_from_xml(sp_session *session, sp_artist *artist, ezxml_t artist_node);
int osfy_artist_load_track_artist_from_xml(sp_session *session, sp_artist *artist, ezxml_t artist_node);
int osfy_artist_load_album_artist_from_xml(sp_session *session, sp_artist *artist, ezxml_t artist_node);
int osfy_artist_browse(sp_session *session, sp_artist *artist);

#endif
