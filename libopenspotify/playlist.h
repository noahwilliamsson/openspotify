#ifndef LIBOPENSPOTIFY_PLAYLIST_H
#define LIBOPENSPOTIFY_PLAYLIST_H

#include <spotify/api.h>

#include "request.h"


#define PLAYLIST_RETRY_TIMEOUT	30


int playlist_process(sp_session *session, struct request *req);
void playlistcontainer_create(sp_session *session);
void playlistcontainer_release(sp_session *session);
void playlistcontainer_add_playlist(sp_session *session, sp_playlist *playlist);
sp_playlist *playlist_create(sp_session *session, unsigned char id[17]);
void playlist_set_name(sp_session *session, sp_playlist *playlist, const char *name);
void playlist_release(sp_session *session, sp_playlist *playlist);

unsigned int playlist_checksum(sp_playlist *playlist);
unsigned int playlistcontainer_checksum(sp_playlistcontainer *container);

#endif
