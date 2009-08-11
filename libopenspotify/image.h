#ifndef LIBOPENSPOTIFY_IMAGE_H
#define LIBOPENSPOTIFY_IMAGE_H

#include <spotify/api.h>

#include "request.h"

#define IMAGE_RETRY_TIMEOUT 120


struct image_ctx {
	sp_session *session;
	struct request *req;
	sp_image *image;
};


int osfy_image_process_request(sp_session *session, struct request *req);

#endif
