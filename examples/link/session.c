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


/* --- Functions --- */
/// External function called when some metadata has been updated.
extern void SP_CALLCONV metadata_updated(sp_session *session);
/// External function called when successfully logged in. We do this to allow
/// the session driver itself to be reused for other examples.
extern void session_ready(sp_session *session);
/// External function called when the program is terminating.
extern void session_terminated(void);


/* --- Data --- */
/// The application key is specific to each project, and allows Spotify
/// to produce statistics on how our service is used.
extern const uint8_t g_appkey[];
/// The size of the application key.
extern const size_t g_appkey_size;

/// The exit code to return from the program. The main loop will continue while
/// it is negative.
int g_exit_code = -1;
/// A handle to the main thread, needed for synchronization between callbacks
/// and the main loop.
#ifdef _WIN32
static HANDLE g_main_thread = (HANDLE)0;  /* -1 is a valid HANDLE, 0 is not */
static HANDLE g_notify_event;
#else
static pthread_t g_main_thread = (pthread_t)0;
#endif


/* ------------------------  BEGIN SESSION CALLBACKS  ---------------------- */
/**
 * This callback is called when the user was logged in, but the connection to
 * Spotify was dropped for some reason.
 *
 * @sa sp_session_callbacks#connection_error
 */
static void SP_CALLCONV connection_error(sp_session *session, sp_error error)
{
	fprintf(stderr, "connection to Spotify failed: %s\n",
		sp_error_message(error));
	g_exit_code = 5;
}

/**
 * This callback is called when an attempt to login has succeeded or failed.
 *
 * @sa sp_session_callbacks#logged_in
 */
static void  SP_CALLCONV logged_in(sp_session *session, sp_error error)
{
	sp_user *me;
	const char *my_name;
	
	if (SP_ERROR_OK != error) {
		fprintf(stderr, "failed to log in to Spotify: %s\n",
			sp_error_message(error));
		g_exit_code = 4;
		return;
	}
	
	// Let us print the nice message...
	me = sp_session_user(session);
	my_name = (sp_user_is_loaded(me) ?
		   sp_user_display_name(me) :
		   sp_user_canonical_name(me));
	
	printf("Logged in to Spotify as user %s\n", my_name);
	
	session_ready(session);
}

/**
 * This callback is called when the session has logged out of Spotify.
 *
 * @sa sp_session_callbacks#logged_out
 */
static void SP_CALLCONV logged_out(sp_session *session)
{
	if (g_exit_code < 0)
		g_exit_code = 0;
}

/**
 * This callback is called from an internal libspotify thread to ask us to
 * reiterate the main loop.
 *
 * The most straight forward way to do this is using Unix signals. We use
 * SIGIO. signal(7) in Linux says "I/O now possible" which sounds reasonable.
 *
 * @sa sp_session_callbacks#notify_main_thread
 */
static void SP_CALLCONV notify_main_thread(sp_session *session)
{
#ifdef _WIN32
	PulseEvent(g_notify_event);
#else
	pthread_kill(g_main_thread, SIGIO);
#endif
}

/**
 * This callback is called for log messages.
 *
 * @sa sp_session_callbacks#log_message
 */
static void SP_CALLCONV log_message(sp_session *session, const char *data)
{
	fprintf(stderr, "log_message: %s\n", data);
}


/**
 * The structure containing pointers to the above callbacks.
 *
 * Any member may be NULL to ignore that event.
 *
 * @sa sp_session_callbacks
 */
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
/* -------------------------  END SESSION CALLBACKS  ----------------------- */

/**
 * The main loop. When this function returns, the program should terminate.
 *
 * To avoid having the \p SIGIO in notify_main_thread() interrupt libspotify,
 * we block it while processing events.
 *
 * @sa sp_session_process_events
 */
static void loop(sp_session *session)
{
#ifndef _WIN32
	sigset_t sigset;
	
	sigemptyset(&sigset);
	sigaddset(&sigset, SIGIO);
#endif
	
	while (g_exit_code < 0) {
		int timeout = -1;
		
#ifndef _WIN32
		pthread_sigmask(SIG_BLOCK, &sigset, NULL);
#endif
		
		sp_session_process_events(session, &timeout);
		
#ifdef _WIN32
		WaitForSingleObject(g_notify_event, timeout);
#else
		pthread_sigmask(SIG_UNBLOCK, &sigset, NULL);
		usleep(timeout * 1000);
#endif
	}
}


/**
 * A dummy function to ignore SIGIO.
 */
#ifndef _WIN32
static void sigIgn(int signo)
{
}
#endif


int main(int argc, char **argv)
{
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
		printf("Usage: link <username> <password>\n");
		return -1;
	}
	
	// Setup for waking up the main thread in notify_main_thread()
#ifdef _WIN32
	g_notify_event = CreateEvent(NULL, FALSE, FALSE, NULL);
#else
	g_main_thread = pthread_self();
	signal(SIGIO, &sigIgn);
#endif
	
	// Always do this. It allows libspotify to check for
	// header/library inconsistencies.
	config.api_version = SPOTIFY_API_VERSION;
	
	// The path of the directory to store the cache. This must be specified.
	// Please read the documentation on preferred values.
	config.cache_location = "tmp";
	
	// The path of the directory to store the settings. This must be specified.
	// Please read the documentation on preferred values.
	config.settings_location = "tmp";
	
	// The key of the application. They are generated by Spotify,
	// and are specific to each application using libspotify.
	config.application_key = g_appkey;
	config.application_key_size = g_appkey_size;
	
	// This identifies the application using some
	// free-text string [1, 255] characters.
	config.user_agent = "spotify-session-example";
	
	// Register the callbacks.
	config.callbacks = &g_callbacks;
	
	error = sp_session_init(&config, &session);
	
	if (SP_ERROR_OK != error) {
		fprintf(stderr, "failed to create session: %s\n",
			sp_error_message(error));
		return 2;
	}
	
	// Login using the credentials given on the command line.
	error = sp_session_login(session, username, password);
	
	if (SP_ERROR_OK != error) {
		fprintf(stderr, "failed to login: %s\n",
			sp_error_message(error));
		return 3;
	}
	
	loop(session);
	session_terminated();
	
	return 0;
}
