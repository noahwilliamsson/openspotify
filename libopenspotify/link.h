#ifndef LIBOPENSPOTIFY_LINK_H
#define LIBOPENSPOTIFY_LINK_H

#include "sp_opaque.h"

void libopenspotify_link_init(sp_session *session);
sp_session *libopenspotify_link_get_session(void);
void libopenspotify_link_release(void);

#endif
