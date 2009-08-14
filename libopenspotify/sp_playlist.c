#include <string.h>
#include <stdlib.h>

#include "debug.h"
#include "sp_opaque.h"


SP_LIBEXPORT(bool) sp_playlist_is_loaded (sp_playlist *playlist) {
	DSFYDEBUG("Not yet implemented\n");

	return (playlist->state == PLAYLIST_STATE_LOADED? 1: 0);
}


SP_LIBEXPORT(void) sp_playlist_add_callbacks (sp_playlist *playlist, sp_playlist_callbacks *callbacks, void *userdata) {
	
	playlist->callbacks = realloc(playlist->callbacks, sizeof(sp_playlist_callbacks *) * (1 + playlist->num_callbacks));
	playlist->userdata = realloc(playlist->userdata, sizeof(void *) * (1 + playlist->num_callbacks));

	playlist->callbacks[playlist->num_callbacks] = callbacks;
	playlist->userdata[playlist->num_callbacks] = userdata;
	
	playlist->num_callbacks++;
}


SP_LIBEXPORT(void) sp_playlist_remove_callbacks (sp_playlist *playlist, sp_playlist_callbacks *callbacks, void *userdata) {
	
	int i;
	
	do {
		for(i = 0; i < playlist->num_callbacks; i++) {
			if(playlist->callbacks[i] != callbacks || playlist->userdata[i] != userdata)
				continue;
			
			playlist->callbacks[i] = playlist->callbacks[playlist->num_callbacks - 1];
			playlist->userdata[i] = playlist->userdata[playlist->num_callbacks - 1];
			playlist->num_callbacks--;
			
			/* We don't bother with reallocating memory at this point */
			break;
		}
	} while(i != playlist->num_callbacks);
}


SP_LIBEXPORT(int) sp_playlist_num_tracks (sp_playlist *playlist) {

	return playlist->num_tracks;
}


SP_LIBEXPORT(sp_track *) sp_playlist_track (sp_playlist *playlist, int index) {

	if(index < 0 || index >= playlist->num_tracks)
		return NULL;

	return playlist->tracks[index];
}


SP_LIBEXPORT(const char *) sp_playlist_name (sp_playlist *playlist) {

	return playlist->name;
}


SP_LIBEXPORT(sp_error) sp_playlist_rename (sp_playlist *playlist, const char *new_name) {
	DSFYDEBUG("Not yet implemented\n");

	return SP_ERROR_OK;
}


SP_LIBEXPORT(sp_user *) sp_playlist_owner (sp_playlist *playlist) {

	return playlist->owner;
}


SP_LIBEXPORT(bool) sp_playlist_is_collaborative (sp_playlist *playlist) {

	return playlist->collaborative;
}


SP_LIBEXPORT(void) sp_playlist_set_collaborative (sp_playlist *playlist, bool collaborative) {
	DSFYDEBUG("Not yet implemented\n");

}


SP_LIBEXPORT(bool) sp_playlist_has_pending_changes (sp_playlist *playlist) {
	DSFYDEBUG("Not yet implemented\n");

	return 0;
}


SP_LIBEXPORT(sp_error) sp_playlist_add_tracks (sp_playlist *playlist, const sp_track **tracks, int num_tracks, int position) {
	DSFYDEBUG("Not yet implemented\n");

	return SP_ERROR_OK;
}


SP_LIBEXPORT(sp_error) sp_playlist_remove_tracks (sp_playlist *playlist, const int *tracks, int num_tracks) {
	DSFYDEBUG("Not yet implemented\n");

	return SP_ERROR_OK;
}


SP_LIBEXPORT(sp_error) sp_playlist_reorder_tracks (sp_playlist *playlist, const int *tracks, int num_tracks, int new_position) {
	DSFYDEBUG("Not yet implemented\n");

	return SP_ERROR_OK;
}


SP_LIBEXPORT(void) sp_playlistcontainer_add_callbacks (sp_playlistcontainer *pc, sp_playlistcontainer_callbacks *callbacks, void *userdata) {

	
	pc->callbacks = realloc(pc->callbacks, sizeof(sp_playlistcontainer_callbacks *) * (1 + pc->num_callbacks));
	pc->userdata = realloc(pc->userdata, sizeof(void *) * (1 + pc->num_callbacks));
	
	pc->callbacks[pc->num_callbacks] = callbacks;
	pc->userdata[pc->num_callbacks] = userdata;
	
	pc->num_callbacks++;
	
}


SP_LIBEXPORT(void) sp_playlistcontainer_remove_callbacks (sp_playlistcontainer *pc, sp_playlistcontainer_callbacks *callbacks, void *userdata) {
	int i;
	
	do {
		for(i = 0; i < pc->num_callbacks; i++) {
			if(pc->callbacks[i] != callbacks || pc->userdata[i] != userdata)
				continue;
			
			pc->callbacks[i] = pc->callbacks[pc->num_callbacks - 1];
			pc->userdata[i] = pc->userdata[pc->num_callbacks - 1];
			pc->num_callbacks--;
			
			/* We don't bother with reallocating memory at this point */
			break;
		}
	} while(i != pc->num_callbacks);
	
}


SP_LIBEXPORT(int) sp_playlistcontainer_num_playlists (sp_playlistcontainer *pc) {
	
	return pc->num_playlists;
}


SP_LIBEXPORT(sp_playlist *) sp_playlistcontainer_playlist (sp_playlistcontainer *pc, int index) {

	if(index < 0 || index >= pc->num_playlists)
		return NULL;
	
	return pc->playlists[index];
}


SP_LIBEXPORT(sp_playlist *) sp_playlistcontainer_add_new_playlist (sp_playlistcontainer *pc, const char *name) {
	DSFYDEBUG("Not yet implemented\n");

	return NULL;
}


SP_LIBEXPORT(sp_playlist *) sp_playlistcontainer_add_playlist (sp_playlistcontainer *pc, sp_link *link) {
	DSFYDEBUG("Not yet implemented\n");

	return NULL;
}


SP_LIBEXPORT(sp_error) sp_playlistcontainer_remove_playlist (sp_playlistcontainer *pc, int index) {
	DSFYDEBUG("Not yet implemented\n");

	return SP_ERROR_OK;
}


SP_LIBEXPORT(sp_error) sp_playlistcontainer_move_playlist (sp_playlistcontainer *pc, int index, int new_position) {
	DSFYDEBUG("Not yet implemented\n");

	return SP_ERROR_OK;
}
