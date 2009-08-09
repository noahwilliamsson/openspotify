#include <stdio.h>
#include <string.h>

#include <spotify/api.h>

#include "debug.h"
#include "sp_opaque.h"
#include "track.h"


SP_LIBEXPORT(bool) sp_track_is_loaded(sp_track *track) {

	return track_get_loaded(track);
}


SP_LIBEXPORT(sp_error) sp_track_error(sp_track *track) {

	return track->error;
}


SP_LIBEXPORT(int) sp_track_num_artists(sp_track *track) {

	return track->num_artists;
}


SP_LIBEXPORT(sp_artist *) sp_track_artist(sp_track *track, int index) {

	if(index > track->num_artists)
		return NULL;

	return track->artists[index];
}


SP_LIBEXPORT(sp_album *) sp_track_album(sp_track *track) {
	DSFYDEBUG("Not yet implemented\n");

	/* FIXME: Resolve track->album_id */

	return NULL;
}


SP_LIBEXPORT(const char *) sp_track_name(sp_track *track) {

	return track_get_name(track);
}


SP_LIBEXPORT(int) sp_track_duration(sp_track *track) {

	return track_get_duration(track);
}


SP_LIBEXPORT(int) sp_track_popularity(sp_track *track) {

	return track_get_popularity(track);
}


SP_LIBEXPORT(int) sp_track_disc(sp_track *track) {

	return track_get_disc(track);
}


SP_LIBEXPORT(int) sp_track_index(sp_track *track) {

	return track->index;
}


SP_LIBEXPORT(void) sp_track_add_ref(sp_track *track) {
	track_add_ref(track);
}


SP_LIBEXPORT(void) sp_track_release(sp_track *track) {
	track_del_ref(track);
}
