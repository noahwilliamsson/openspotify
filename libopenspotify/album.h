#ifndef LIBOPENSPOTIFY_ALBUM_H
#define LIBOPENSPOTIFY_ALBUM_H

#include <spotify/api.h>

#include "ezxml.h"

sp_album *sp_album_add(sp_session *session, unsigned char id[16]);
int osfy_album_load_from_xml(sp_session *session, sp_album *album, ezxml_t album_node);

#endif
