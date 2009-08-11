#ifndef LIBOPENSPOTIFY_BROWSE_H
#define LIBOPENSPOTIFY_BROWSE_H

#include <spotify/api.h>

#include "buf.h"
#include "hashtable.h"
#include "request.h"


#define BROWSE_RETRY_TIMEOUT	30


struct browse_callback_ctx;
typedef int (*browse_parser) (struct browse_callback_ctx *brctx);

struct browse_callback_ctx {
	/* Provides access to the session's hashtables among other things */
	sp_session *session;

	/* The request, so we can store the result */
	struct request *req;
	
	/* For keeping the gzip'd XML */
	struct buf *buf;

	/* Type of objects, same as request->type */
	int type;

	union {
		sp_playlist *playlist;
		sp_artist **artists;
		sp_album **albums;
		sp_track **tracks;
		sp_albumbrowse **albumbrowses;
		sp_artistbrowse **artistbrowses;
	} data;

	/* Total number of objects in the above struct */
	int num_total;

	/* Total number of objects we've browsed so far */
	int num_browsed;
	
	/* Number of items in the current request */
	int num_in_request;
	
	/* Gzip'd XML parser, provided by the caller */
	browse_parser browse_parser;
};


int browse_process(sp_session *session, struct request *req);

#endif
