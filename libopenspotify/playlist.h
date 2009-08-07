#ifndef LIBOPENSPOTIFY_PLAYLIST_H
#define LIBOPENSPOTIFY_PLAYLIST_H

#include "buf.h"
#include "sp_opaque.h"

#define PLAYLIST_RETRY_TIMEOUT	30

unsigned long playlist_checksum(sp_playlist *playlist);
unsigned long playlistcontainer_checksum(sp_playlistcontainer *container);

struct playlist_ctx *playlist_create(void);
void playlist_release(struct playlist_ctx *);
int playlist_process(sp_session *session, sp_request *req);

#endif
