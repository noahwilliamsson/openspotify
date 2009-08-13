/**
 * Copyright (c) 2006-2009 Spotify Ltd
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 *
 * Example application showing the simple case of logging in.
 */

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
typedef unsigned char uint8_t;
#include <windows.h>
#else
#include <stdint.h>
#include <libgen.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#endif

// The one and only entrypoint to the libspotify API
#include <spotify/api.h>


#include "debug.h"
#include "test.h"


/* --- Functions --- */
/// External function called when some metadata has been updated.
extern void SP_CALLCONV metadata_updated(sp_session *session);
/// External function called when successfully logged in. We do this to allow
/// the session driver itself to be reused for other examples.
extern void session_ready(sp_session *session);
/// External function called when the program is terminating.
extern void session_terminated(void);


extern const uint8_t g_appkey[];
extern const size_t g_appkey_size;

int g_exit_code = -1;
#ifdef _WIN32
static HANDLE g_main_thread = (HANDLE)0;  /* -1 is a valid HANDLE, 0 is not */
static HANDLE g_notify_event;
#else
static pthread_t g_main_thread = (pthread_t)0;
#endif


static void SP_CALLCONV connection_error(sp_session *session, sp_error error) {
	DSFYDEBUG("SESSION CALLBACK\n");
	fprintf(stderr, "connection to Spotify failed: %s\n",
	                sp_error_message(error));
	g_exit_code = 5;
}


static void  SP_CALLCONV logged_in(sp_session *session, sp_error error) {
	sp_user *me;
	const char *my_name;

	DSFYDEBUG("SESSION CALLBACK\n");
	if (SP_ERROR_OK != error) {
		fprintf(stderr, "failed to log in to Spotify: %s\n",
		                sp_error_message(error));
		g_exit_code = 4;
		return;
	}

	DSFYDEBUG("Running session_ready()\n");
	session_ready(session);
}


static void SP_CALLCONV logged_out(sp_session *session) {
	DSFYDEBUG("SESSION CALLBACK\n");
	if (g_exit_code < 0)
		g_exit_code = 0;
}


static void SP_CALLCONV notify_main_thread(sp_session *session) {
	DSFYDEBUG("SESSION CALLBACK\n");
#ifdef _WIN32
	PulseEvent(g_notify_event);
#else
	pthread_kill(g_main_thread, SIGIO);
#endif
}

static void SP_CALLCONV log_message(sp_session *session, const char *data) {
	DSFYDEBUG("SESSION CALLBACK\n");
	fprintf(stderr, "log_message: %s\n", data);
}


static sp_session_callbacks g_callbacks = {
	&logged_in,
	&logged_out,
	&metadata_updated,
	&connection_error,
	NULL,
	&notify_main_thread,
	NULL,
	NULL,
	&log_message
};




static void event_loop(sp_session *session) {
	int timeout = -1;
#ifndef _WIN32
	sigset_t sigset;

	sigemptyset(&sigset);
	sigaddset(&sigset, SIGIO);
#endif

	while (g_exit_code < 0) {

#ifndef _WIN32
		pthread_sigmask(SIG_BLOCK, &sigset, NULL);
#endif

		DSFYDEBUG("Calling sp_session_process_events()\n");
		sp_session_process_events(session, &timeout);

		if(test_run() < 0) {
			DSFYDEBUG("Done running test, existing event loop\n");
			break;
		}

#ifdef _WIN32
		WaitForSingleObject(g_notify_event, timeout);
#else
		pthread_sigmask(SIG_UNBLOCK, &sigset, NULL);
		usleep(timeout * 1000);
#endif
	}

	DSFYDEBUG("Exiting from loop()\n");
}


#ifndef _WIN32
static void sigIgn(int signo) {
}
#endif


int main(int argc, char **argv) {
	sp_session_config config;
	sp_error error;
	sp_session *session;

	char username[256];
	char password[256];
	char *ptr;
	
	if(argc == 1) {
		printf("Username: ");
		ptr = fgets(username, sizeof(username) - 1, stdin);
		while(*ptr) { if(*ptr == '\r' || *ptr == '\n') *ptr = 0; ptr++; }
		
		printf("Password: ");
		ptr = fgets(password, sizeof(password) - 1, stdin);
		while(*ptr) { if(*ptr == '\r' || *ptr == '\n') *ptr = 0; ptr++; }
	}
	else if(argc == 3) {
		strcpy(username, argv[1]);
		strcpy(password, argv[2]);
	}
	else {
		printf("Usage: browse <username> <password>\n");
		return -1;
	}


#ifdef _WIN32
	g_notify_event = CreateEvent(NULL, FALSE, FALSE, NULL);
#else
	g_main_thread = pthread_self();
	signal(SIGIO, &sigIgn);
#endif




	config.api_version = SPOTIFY_API_VERSION;
	config.cache_location = "tmp";
	config.settings_location = "tmp";
	config.application_key = g_appkey;
	config.application_key_size = g_appkey_size;
	config.user_agent = "spotify-session-example";
	config.callbacks = &g_callbacks;

	DSFYDEBUG("Calling sp_session_init()\n");
	error = sp_session_init(&config, &session);
	if (SP_ERROR_OK != error) {
		fprintf(stderr, "failed to create session: %s\n",
		                sp_error_message(error));
		return 2;
	}

	test_init(session);


	DSFYDEBUG("Calling sp_session_login()\n");
	error = sp_session_login(session, username, password);
	if (SP_ERROR_OK != error) {
		fprintf(stderr, "failed to login: %s\n",
		                sp_error_message(error));
		return 3;
	}



	DSFYDEBUG("Calling event_loop()\n");
	event_loop(session);


	DSFYDEBUG("Calling session_terminated()\n");
	session_terminated();


	return 0;
}
