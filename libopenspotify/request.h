/* 
 * (c) 2009 Noah Williamsson <noah.williamsson@gmail.com>
 *
 */

#ifndef LIBOPENSPOTIFY_EVENT_H
#define LIBOPENSPOTIFY_EVENT_H

#include <spotify/api.h>
#include "sp_opaque.h"

int request_post(sp_session *session, sp_request_type type, void *input);
int request_post_result(sp_session *session, sp_request_type type, sp_error error, void *output);
int request_set_result(sp_session *session, sp_request *req, sp_error error, void *output);
sp_request *request_fetch_next_result(sp_session *session);
void request_mark_processed(sp_session *session, sp_request *req);
void request_cleanup(sp_session *session);
#endif
