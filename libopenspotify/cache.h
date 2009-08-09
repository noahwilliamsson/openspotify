#ifndef LIBOPENSPOTIFY_CACHE_H
#define LIBOPENSPOTIFY_CACHE_H

#include "sp_opaque.h"

void cache_init(sp_session *session);
int cache_process(sp_session *session, struct request *req);

#endif
