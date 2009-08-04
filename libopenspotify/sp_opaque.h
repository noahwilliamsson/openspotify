/* 
 * Opauqe data structures and definitions
 * (c) 2009 Noah Williamsson <noah.williamsson@gmail.com>
 *
 */
#ifndef LIBOPENSPOTIFY_OPAQUE_H
#define LIBOPENSPOTIFY_OPAQUE_H

#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <pthread.h>
#include <sys/socket.h>
#include <netdb.h>
#endif

#include <spotify/api.h>

#include "login.h"
#include "shn.h"



typedef enum { 
	REQ_NEW = 0,
	REQ_RUNNING,
	REQ_RETURNED,
	REQ_PROCESSED
} sp_request_state;

typedef enum {
	REQ_LOGIN = 0,
	REQ_LOGOUT
} sp_request_type;

struct sp_request {
	sp_request_type type;
	void *private;
	sp_request_state state;
	sp_error error;
	struct sp_request *next;
};
typedef struct sp_request sp_request;



struct sp_session {
	void *userdata;

	sp_session_callbacks *callbacks;

	/* Low-level network stuff */
	int sock;

	char username[256];
	char password[256];

	/* Used when logging in */
	struct login_ctx *login;


        /*
         * Stream cipher contexts
         *
         */
        unsigned int key_recv_IV;
        unsigned int key_send_IV;
        shn_ctx shn_recv;
        shn_ctx shn_send;


	/* Requests scoreboard */
	sp_request *requests;

	/* Keeps track of the login state */
	int login_state;

	/* High level connection state */
	sp_connectionstate connectionstate;


#ifdef _WIN32
	HANDLE request_mutex;
	HANDLE thread_main;
	HANDLE thread_networkwork;
#else
	pthread_mutex_t request_mutex;
	pthread_t thread_main;
	pthread_t thread_networkwork;
#endif
};

#endif
