#include <stdio.h>
#include <string.h>

#include <spotify/api.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif


int main(int argc, char **argv) {
	sp_session *session;
	sp_session_callbacks callbacks = {
			NULL,		/* logged_in */
			NULL,		/* logged_out */
			NULL,		/* metadata_updated */
			NULL,		/* connection_error */
			NULL,		/* message_to_user */
			NULL,		/* notify_main_thread */
			NULL,		/* music_delivery */
			NULL,		/* play_token_lost */
			NULL		/* log_message */
	};
	sp_session_config config = {
		1,				/* API version */
		"",				/* Cache location */
		"",				/* Settings location */
		NULL,			/* App key */
		0,				/* App key size */
		"openspotify-simple", /* User-Agent */
		&callbacks,		/* Callbacks */
		NULL			/* Callback data */
	};

	sp_error err;

	err = sp_session_init(&config, &session);
	printf("Session initalized, error is %d\n", err);

	err = sp_session_login(session, "user", "password");
	printf("Triggered login, error is %d\n", err);
#ifdef _WIN32
	Sleep(10*1000);
#else
	sleep(10);
#endif

	err = sp_session_logout(session);
	printf("Triggered logout, error is %d\n", err);

#ifdef _WIN32
	Sleep(10*1000);
#else
	sleep(10);
#endif

	return 0;
}
