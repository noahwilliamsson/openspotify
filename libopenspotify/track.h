#ifndef LIBOPENSPOTIFY_TRACK_H
#define LIBOPENSPOTIFY_TRACK_H

#include <spotify/api.h>

#include "ezxml.h"


sp_track *osfy_track_add(sp_session *session, unsigned char id[16]);
void osfy_track_free(sp_track *track);
int osfy_track_load_from_xml(sp_session *session, sp_track *track, ezxml_t track_node);
int osfy_track_browse(sp_session *session, sp_track *track);
void osfy_track_garbage_collect(sp_session *session);
int osfy_track_metadata_save_to_disk(sp_session *session, char *filename);
int osfy_track_metadata_load_from_disk(sp_session *session, char *filename);

#endif
