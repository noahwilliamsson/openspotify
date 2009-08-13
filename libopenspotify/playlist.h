#ifndef LIBOPENSPOTIFY_PLAYLIST_H
#define LIBOPENSPOTIFY_PLAYLIST_H

#include <spotify/api.h>

#include "request.h"


#define PLAYLIST_RETRY_TIMEOUT	30


void playlist_create(sp_session *session);
void playlist_release(sp_session *session);
int playlist_process(sp_session *session, struct request *req);

unsigned long playlist_checksum(sp_playlist *playlist);
unsigned long playlistcontainer_checksum(sp_playlistcontainer *container);

#endif
