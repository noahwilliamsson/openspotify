#ifndef LIBOPENSPOTIFY_TRACK_H
#define LIBOPENSPOTIFY_TRACK_H

#include "sp_opaque.h"

sp_track *track_add(sp_session *session, unsigned char *id);
void track_free(sp_session *session, sp_track *track);
void track_set_title(sp_track *track, char *title);
void track_set_album(sp_track *track, char *album);
char *track_get_name(sp_track *track);
void track_set_album_id(sp_track *track, unsigned char id[16]);
void track_set_cover_id(sp_track *track, unsigned char id[20]);
void track_set_file_id(sp_track *track, unsigned char id[20]);
void track_set_loaded(sp_track *track, int loaded);
void track_set_index(sp_track *track, int index);
void track_set_duration(sp_track *track, int duration);
int track_get_duration(sp_track *track);
void track_set_popularity(sp_track *track, int popularity);
int track_get_popularity(sp_track *track);
void track_set_disc(sp_track *track, int disc);
int track_get_disc(sp_track *track);
void track_set_loaded(sp_track *track, int playable);
int track_get_loaded(sp_track *track);
void track_set_playable(sp_track *track, int playable);
void track_add_ref(sp_track *track);
void track_del_ref(sp_track *track);
void track_garbage_collect(sp_session *session);
void track_save_metadata_to_disk(sp_session *session, char *filename);
void track_load_metadata_from_disk(sp_session *session, char *filename);
#endif
