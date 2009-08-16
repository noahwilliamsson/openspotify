#ifndef LIBOPENSPOTIFY_SEARCH_H
#define LIBOPENSPOTIFY_SEARCH_H

#include <spotify/api.h>

#include "buf.h"
#include "request.h"


#define SEARCH_RETRY_TIMEOUT	30*1000


struct search_ctx {
        sp_session *session;
        struct request *req;
	struct buf *buf;
        sp_search *search;
};


int search_process_request(sp_session *session, struct request *req);

#endif
