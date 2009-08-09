/*
 * Cache a session object in Thread-Local Storage
 * Required for sp_link_create_from_string() to function in libopenspotify
 *
 */

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

#include "link.h"
#include "sp_opaque.h"


#ifdef _WIN32
static DWORD tls_session_ptr;
#else
static pthread_key_t tls_session_ptr;
#endif
static int initialized;


void libopenspotify_link_init(sp_session *session) {
#ifdef _WIN32
	if(!initialized)
	        tls_session_ptr = TlsAlloc();

        TlsSetValue(tls_session_ptr, session);
#else
	pthread_key_create(&tls_session_ptr, NULL);
        pthread_setspecific(tls_session_ptr, session);
#endif

	initialized = 2;
}


sp_session *libopenspotify_link_get_session(void) {
	sp_session *session;

	if(initialized != 2)
		return NULL;

#ifdef _WIN32
	session = (sp_session *)TlsGetValue(tls_session_ptr);
#else
	session = (sp_session *)pthread_getspecific(tls_session_ptr);
#endif

	return session;
}


void libopenspotify_link_release(void) {
	if(!initialized)
		return;

#ifdef _WIN32
	/*
	 * http://msdn.microsoft.com/en-us/library/ms686804(VS.85).aspx
	 * TlsFree(tls_session_ptr);
	 */
	TlsSetValue(tls_session_ptr, NULL);
	initialized = 1;
#else
	pthread_key_delete(tls_session_ptr);
#endif
	initialized = 0;
}
