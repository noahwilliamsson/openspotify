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
 * Example application showing how to use the album, artist, and browsing subsystems.
 */
#include <stdio.h>
#include <spotify/api.h>

#include "debug.h"
#include "test.h"


extern int g_exit_code;
static sp_session *g_session;
static sp_albumbrowse *g_albumbrowse;
static sp_artistbrowse *g_artistbrowse;
static sp_search *g_search;


static void print_artistbrowse(sp_artistbrowse *browse);
static void SP_CALLCONV test_artistbrowse_callback(sp_artistbrowse *browse, void *userdata);
static void SP_CALLCONV test_albumbrowse_callback(sp_albumbrowse *browse, void *userdata);
static void test_search_callback(sp_search *result, void *userdata);
static void print_albumbrowse(sp_albumbrowse *browse);


void test_username(sp_session *session, void *arg) {
        sp_user *me;

	me = sp_session_user(session);

        if(!sp_user_is_loaded(me)) {
                DSFYDEBUG("User not loaded\n");
                return;
        }

        DSFYDEBUG("display name='%s'\n", sp_user_display_name(me));
        DSFYDEBUG("canonical name='%s'\n", sp_user_canonical_name(me));

        test_finish();
}


void test_link_artist(sp_session *session, void *arg) {
	char *uri = "spotify:artist:1cB013ULmW96lglRcrWTut";
	sp_link *link;
	static sp_artist *artist;

	if(artist == NULL) {
		link = sp_link_create_from_string(uri);
		artist = sp_link_as_artist(link);
		sp_artist_add_ref(artist);
		sp_link_release(link);
	}


	if(sp_artist_is_loaded(artist) == 0) {
		DSFYDEBUG("Artist at %p is NOT loaded\n", artist);
		return;
	}
	else {
		DSFYDEBUG("Artist at %p IS loaded (%s)\n", artist, sp_artist_name(artist));
		sp_artist_release(artist);
		test_finish();
	}
}



void test_artistbrowse(sp_session *session, void *arg) {
	/* Carefully chosen with umlauts in track, artist and album name */
	char *uri = "spotify:artist:3H7Ez7cwaYw4L3ELy4v3Lc";
	sp_link *link;
	static sp_artist *artist;
	static int waiting;

	if(waiting)
		return;

	if(artist == NULL) {
		link = sp_link_create_from_string(uri);
		artist = sp_link_as_artist(link);
		sp_artist_add_ref(artist);
		sp_link_release(link);
	}


	if(sp_artist_is_loaded(artist) == 0) {
		DSFYDEBUG("Artist is NOT loaded\n");
		return;
	}

	DSFYDEBUG("Calling sp_artistbrowse_create()\n");
	g_artistbrowse = sp_artistbrowse_create(session, artist, test_artistbrowse_callback, NULL);
	sp_artist_release(artist);
	DSFYDEBUG("Released artist\n");
	waiting++;
}


void test_logout(sp_session *session, void *arg) {
	sp_error error;
	static int waiting;

	if(waiting)
		return;

	DSFYDEBUG("Calling sp_session_logout()\n");
	error = sp_session_logout(g_session);

	if (SP_ERROR_OK != error) {
		fprintf(stderr, "failed to log out from Spotify: %s\n", sp_error_message(error));
		g_exit_code = 5;
		return;
	}
}


static void SP_CALLCONV test_artistbrowse_callback(sp_artistbrowse *browse, void *userdata) {

	DSFYDEBUG("Called with args browse=%p, userdata=%p\n", browse, userdata);
	if (browse && SP_ERROR_OK == sp_artistbrowse_error(browse))
		print_artistbrowse(browse);
	else
		fprintf(stderr, "Failed to browse artist: %s\n",
		        sp_error_message(sp_artistbrowse_error(browse)));

	DSFYDEBUG("sp_artistbrowse_release()\n");
	sp_artistbrowse_release(g_artistbrowse);
	g_artistbrowse = NULL;

	test_finish();
}


void test_albumbrowse(sp_session *session, void *arg) {
	/* Carefully chosen with umlauts in track, artist and album name */
	//char *uri = "spotify:album:2iouxw5ZVeqUt59FWoxlz7";
	char *uri = "spotify:album:7aq1X1n4Ps6e2w7WWtiUK6";
	//char *uri = "spotify:album:0n9UNlm1GEdeCkZ412p2SZ";
	sp_link *link;
	static sp_album *album;
	static int waiting;

	if(waiting)
		return;

	if(album == NULL) {
		link = sp_link_create_from_string(uri);
		album = sp_link_as_album(link);
		sp_album_add_ref(album);
		sp_link_release(link);
	}


	if(sp_album_is_loaded(album) == 0) {
		DSFYDEBUG("Album is NOT loaded\n");
		return;
	}

	DSFYDEBUG("Calling sp_albumbrowse_create()\n");
	g_albumbrowse = sp_albumbrowse_create(session, album, test_albumbrowse_callback, NULL);
	sp_album_release(album);
	DSFYDEBUG("Released album\n");
	waiting++;
}


static void SP_CALLCONV test_albumbrowse_callback(sp_albumbrowse *browse, void *userdata) {
	DSFYDEBUG("browse=%p, userdata=%p\n", browse, userdata);
	if (browse && SP_ERROR_OK == sp_albumbrowse_error(browse))
		print_albumbrowse(browse);
	else
		fprintf(stderr, "Failed to browse album: %s\n",
		        sp_error_message(sp_albumbrowse_error(browse)));

	sp_albumbrowse_release(g_albumbrowse);
	g_albumbrowse = NULL;

	test_finish();
}


static void SP_CALLCONV test_search(sp_session *session, void *arg) {
	static int waiting;

	if(waiting)
		return;

	g_search = sp_search_create(session, "knark", 0, 5, test_search_callback, NULL);
	waiting++;
	DSFYDEBUG("Initiated search\n");
}

static void test_search_callback(sp_search *result, void *userdata) {
	DSFYDEBUG("Search completed, tracks=%d, albums=%d, artists=%d\n", 
		sp_search_num_tracks(result), sp_search_num_albums(result),
		sp_search_num_artists(result));

	test_finish();
}


static void SP_CALLCONV test_search_as_link(sp_session *session, void *arg) {
	char buf[256];
	sp_link *link;

	link = sp_link_create_from_search(g_search);
	sp_search_release(g_search);

	sp_link_as_string(link, buf, sizeof(buf) - 1);
	buf[sizeof(buf) - 1] = 0;

	DSFYDEBUG("Previous search as an URI: '%s'\n", buf);

	sp_link_release(link);

	test_finish();
}



void SP_CALLCONV metadata_updated(sp_session *session) {
	DSFYDEBUG("SESSION CALLBACK\n");
	test_run();
}


static void print_track(sp_track *track) {
	int duration = sp_track_duration(track);

	printf("  Track %s [%d:%02d] has %d artist(s), %d%% popularity, %d on disc %d\n",
		sp_track_name(track),
		duration / 60000,
		(duration / 1000) / 60,
		sp_track_num_artists(track),
		sp_track_popularity(track),
		sp_track_index(track),
		sp_track_disc(track));
}


static void print_albumbrowse(sp_albumbrowse *browse) {
	int i;

	DSFYDEBUG("browse=%p\n", browse);
	printf("Album browse of \"%s\" (%d)\n", sp_album_name(sp_albumbrowse_album(browse)), sp_album_year(sp_albumbrowse_album(browse)));

	for (i = 0; i < sp_albumbrowse_num_copyrights(browse); ++i)
		printf("  Copyright: %s\n", sp_albumbrowse_copyright(browse, i));

	printf("  Tracks: %d\n", sp_albumbrowse_num_tracks(browse));
	printf("  Review: %.60s...\n", sp_albumbrowse_review(browse));
	puts("");

	for (i = 0; i < sp_albumbrowse_num_tracks(browse); ++i)
		print_track(sp_albumbrowse_track(browse, i));

	puts("");
}


static void print_artistbrowse(sp_artistbrowse *browse) {
	int i;

	DSFYDEBUG("browse=%p\n", browse);
	printf("Artist browse of \"%s\"\n", sp_artist_name(sp_artistbrowse_artist(browse)));

	for (i = 0; i < sp_artistbrowse_num_similar_artists(browse); ++i)
		printf("  Similar artist: %s\n", sp_artist_name(sp_artistbrowse_similar_artist(browse, i)));

	printf("  Portraits: %d\n", sp_artistbrowse_num_portraits(browse));
	printf("  Tracks   : %d\n", sp_artistbrowse_num_tracks(browse));
	printf("  Biography: %.60s...\n", sp_artistbrowse_biography(browse));
	puts("");

	for (i = 0; i < sp_artistbrowse_num_tracks(browse); ++i)
		print_track(sp_artistbrowse_track(browse, i));

	puts("");
}


void session_ready(sp_session *session) {

	/* Add some tests */
	test_add("get username", test_username, NULL);
	test_add("link artist", test_link_artist, NULL);
	test_add("search: knark", test_search, NULL);
	test_add("search as link", test_search_as_link, NULL);
	test_add("artistbrowse", test_artistbrowse, NULL);
	test_add("albumbrowse", test_albumbrowse, NULL);

	/* Logout as the final test */
	test_add("logout", test_logout, NULL);


	/* Start tests */
	test_start();


	g_session = session;
}


void session_terminated(void) {

	/* FIXME: We should call sp_session_release() too, but that isn't available in libspotify.. */
	DSFYDEBUG("Good bye\n");
}
