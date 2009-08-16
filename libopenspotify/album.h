#ifndef LIBOPENSPOTIFY_ALBUM_H
#define LIBOPENSPOTIFY_ALBUM_H

#include <spotify/api.h>

#include "ezxml.h"

sp_album *sp_album_add(sp_session *session, unsigned char id[16]);
void osfy_album_free(sp_album *album);
int osfy_album_load_from_album_xml(sp_session *session, sp_album *album, ezxml_t album_node);
int osfy_album_load_from_search_xml(sp_session *session, sp_album *album, ezxml_t album_node);
int osfy_album_load_from_track_xml(sp_session *session, sp_album *album, ezxml_t album_node);
int osfy_album_browse(sp_session *session, sp_album *album);

#endif
