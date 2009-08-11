#ifndef LIBOPENSPOTIFY_ARTIST_H
#define LIBOPENSPOTIFY_ARTIST_H

#include <spotify/api.h>

#include "ezxml.h"


sp_artist *osfy_artist_add(sp_session *session, unsigned char id[16]);
int osfy_artist_load_from_artist_xml(sp_session *session, sp_artist *artist, ezxml_t artist_node);
int osfy_artist_load_from_album_or_track_xml(sp_session *session, sp_artist *artist, ezxml_t artist_node);
int osfy_artist_browse(sp_session *session, sp_artist *artist);

#endif
