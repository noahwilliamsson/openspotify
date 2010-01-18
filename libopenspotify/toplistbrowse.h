#ifndef LIBOPENSPOTIFY_TOPLISTBROWSE_H
#define LIBOPENSPOTIFY_TOPLISTBROWSE_H

#include <spotify/api.h>

#include "buf.h"
#include "request.h"


#define TOPLISTBROWSE_RETRY_TIMEOUT	30*1000


struct toplistbrowse_ctx {
        sp_session *session;
        struct request *req;
	struct buf *buf;
        sp_toplistbrowse *toplistbrowse;
};


int toplistbrowse_process_request(sp_session *session, struct request *req);

#endif
