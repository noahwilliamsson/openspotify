#include <stdio.h>
#include <string.h>

#include <spotify/api.h>

#include "debug.h"
#include "sp_opaque.h"

SP_LIBEXPORT(bool) sp_track_is_loaded(sp_track *track) {

	return track->is_loaded;
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

}


SP_LIBEXPORT(const char *) sp_track_name(sp_track *track) {

	return track->name;
}


SP_LIBEXPORT(int) sp_track_duration(sp_track *track) {

	return track->duration;
}


SP_LIBEXPORT(int) sp_track_popularity(sp_track *track) {

	return track->popularity;
}


SP_LIBEXPORT(int) sp_track_disc(sp_track *track) {

	return track->disc;
}


SP_LIBEXPORT(int) sp_track_index(sp_track *track) {

	return track->index;
}


SP_LIBEXPORT(void) sp_track_add_ref(sp_track *track) {
	track->ref_count++;
}


SP_LIBEXPORT(void) sp_track_release(sp_track *track) {
	if(track->ref_count)
		track->ref_count--;

	/* FIXME: free track when ref count reaches zero? */
}
