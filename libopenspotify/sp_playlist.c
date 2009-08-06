#ifndef LIBOPENSPOTIFY_PLAYLIST_H
#define LIBOPENSPOTIFY_PLAYLIST_H

#include "debug.h"
#include "sp_opaque.h"


SP_LIBEXPORT(bool) sp_playlist_is_loaded (sp_playlist *playlist) {
	DSFYDEBUG("Not yet implemented\n");

}


SP_LIBEXPORT(void) sp_playlist_add_callbacks (sp_playlist *playlist, sp_playlist_callbacks *callbacks, void *userdata) {
	DSFYDEBUG("Not yet implemented\n");

}


SP_LIBEXPORT(void) sp_playlist_remove_callbacks (sp_playlist *playlist, sp_playlist_callbacks *callbacks, void *userdata) {
	DSFYDEBUG("Not yet implemented\n");

}


SP_LIBEXPORT(int) sp_playlist_num_tracks (sp_playlist *playlist) {
	DSFYDEBUG("Not yet implemented\n");

}


SP_LIBEXPORT(sp_track *) sp_playlist_track (sp_playlist *playlist, int index) {
	DSFYDEBUG("Not yet implemented\n");

}


SP_LIBEXPORT(const char *) sp_playlist_name (sp_playlist *playlist) {
	DSFYDEBUG("Not yet implemented\n");

}


SP_LIBEXPORT(sp_error) sp_playlist_rename (sp_playlist *playlist, const char *new_name) {
	DSFYDEBUG("Not yet implemented\n");

}


SP_LIBEXPORT(sp_user *) sp_playlist_owner (sp_playlist *playlist) {
	DSFYDEBUG("Not yet implemented\n");

}


SP_LIBEXPORT(bool) sp_playlist_is_collaborative (sp_playlist *playlist) {
	DSFYDEBUG("Not yet implemented\n");

}


SP_LIBEXPORT(void) sp_playlist_set_collaborative (sp_playlist *playlist, bool collaborative) {
	DSFYDEBUG("Not yet implemented\n");

}


SP_LIBEXPORT(bool) sp_playlist_has_pending_changes (sp_playlist *playlist) {
	DSFYDEBUG("Not yet implemented\n");

}


SP_LIBEXPORT(sp_error) sp_playlist_add_tracks (sp_playlist *playlist, const sp_track **tracks, int num_tracks, int position) {
	DSFYDEBUG("Not yet implemented\n");

}


SP_LIBEXPORT(sp_error) sp_playlist_remove_tracks (sp_playlist *playlist, const int *tracks, int num_tracks) {
	DSFYDEBUG("Not yet implemented\n");

}


SP_LIBEXPORT(sp_error) sp_playlist_reorder_tracks (sp_playlist *playlist, const int *tracks, int num_tracks, int new_position) {
	DSFYDEBUG("Not yet implemented\n");

}


SP_LIBEXPORT(void) sp_playlistcontainer_add_callbacks (sp_playlistcontainer *pc, sp_playlistcontainer_callbacks *callbacks, void *userdata) {
	DSFYDEBUG("Not yet implemented\n");

}


SP_LIBEXPORT(void) sp_playlistcontainer_remove_callbacks (sp_playlistcontainer *pc, sp_playlistcontainer_callbacks *callbacks, void *userdata) {
	DSFYDEBUG("Not yet implemented\n");

}


SP_LIBEXPORT(int) sp_playlistcontainer_num_playlists (sp_playlistcontainer *pc) {
	DSFYDEBUG("Not yet implemented\n");

}


SP_LIBEXPORT(sp_playlist *) sp_playlistcontainer_playlist (sp_playlistcontainer *pc, int index) {
	DSFYDEBUG("Not yet implemented\n");

}


SP_LIBEXPORT(sp_playlist *) sp_playlistcontainer_add_new_playlist (sp_playlistcontainer *pc, const char *name) {
	DSFYDEBUG("Not yet implemented\n");

}


SP_LIBEXPORT(sp_playlist *) sp_playlistcontainer_add_playlist (sp_playlistcontainer *pc, sp_link *link) {
	DSFYDEBUG("Not yet implemented\n");

}


SP_LIBEXPORT(sp_error) sp_playlistcontainer_remove_playlist (sp_playlistcontainer *pc, int index) {
	DSFYDEBUG("Not yet implemented\n");

}


SP_LIBEXPORT(sp_error) sp_playlistcontainer_move_playlist (sp_playlistcontainer *pc, int index, int new_position) {
	DSFYDEBUG("Not yet implemented\n");

}

#endif
