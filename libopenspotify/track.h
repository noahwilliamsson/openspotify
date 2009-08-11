#ifndef LIBOPENSPOTIFY_TRACK_H
#define LIBOPENSPOTIFY_TRACK_H

#include <spotify/api.h>

sp_track *osfy_track_add(sp_session *session, unsigned char id[16]);
void osfy_track_free(sp_track *track);
void osfy_track_title_set(sp_track *track, char *title);
void osfy_track_album_name_set(sp_track *track, char *album_name);
void osfy_track_album_set(sp_track *track, sp_album *album);
void osfy_track_file_id_set(sp_track *track, unsigned char id[20]);
void osfy_track_playable_set(sp_track *track, int playable);
void osfy_track_duration_set(sp_track *track, int duration);
void osfy_track_disc_set(sp_track *track, int disc);
void osfy_track_loaded_set(sp_track *track, int loaded);
void track_set_popularity(sp_track *track, int popularity);
void osfy_track_garbage_collect(sp_session *session);
int osfy_track_metadata_save_to_disk(sp_session *session, char *filename);
int osfy_track_metadata_load_from_disk(sp_session *session, char *filename);
#endif
