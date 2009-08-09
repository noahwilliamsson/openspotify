#ifndef LIBOPENSPOTIFY_BROWSE_H
#define LIBOPENSPOTIFY_BROWSE_H

#include "buf.h"
#include "sp_opaque.h"

#define BROWSE_RETRY_TIMEOUT	30

int browse_process(sp_session *session, struct request *req);

#endif
