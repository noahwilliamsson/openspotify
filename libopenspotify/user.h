#ifndef LIBOPENSPOTIFY_USER_H
#define LIBOPENSPOTIFY_USER_H

#include <spotify/api.h>

#include "request.h"


#define USER_RETRY_TIMEOUT	30*1000


sp_user *user_add(sp_session *session, const char *name);
int user_lookup(sp_session *session, sp_user *user);
void user_release(sp_user *user);
void user_free(sp_user *user);
void user_add_ref(sp_user *user);
int user_process_request(sp_session *session, struct request *req);

#endif
