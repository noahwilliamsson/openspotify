#include <spotify/api.h>

#include "hashtable.h"
#include "request.h"
#include "track.h"
#include "util.h"


void cache_init(sp_session *session) {
	osfy_track_metadata_load_from_disk(session, "tracks.cache");
}


int cache_process(sp_session *session, struct request *req) {
	/* Do some garbage collection */
	osfy_track_garbage_collect(session);

	/* Save metadata to disk */
	osfy_track_metadata_save_to_disk(session, "tracks.cache");

	req->next_timeout = get_millisecs() + 5*60*1000;

	return 0;
}
