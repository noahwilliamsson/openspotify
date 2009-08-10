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


/* --- Data --- */
extern int g_exit_code;

/// A global reference to the session object
static sp_session *g_session;
/// A global reference to the artist we are currently investigating
static sp_albumbrowse *g_albumbrowse;
/// A global reference to the album we are currently investigating
static sp_artistbrowse *g_artistbrowse;


/**
 * Print the given track title together with some trivial metadata
 *
 * @param  track   The track object
 */
static void print_track(sp_track *track)
{
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

/**
 * Print the given album browse result and as much information as possible
 *
 * @param  browse  The browse result
 */
static void print_albumbrowse(sp_albumbrowse *browse)
{
	int i;

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

/**
 * Print the given artist browse result and as much information as possible
 *
 * @param  browse  The browse result
 */
static void print_artistbrowse(sp_artistbrowse *browse)
{
	int i;

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

/**
 * Called after the each browse request is done
 *
 * Will only actually log out when all browse requests are done
 */
static void try_terminate(void)
{
	sp_error error;

	if (g_albumbrowse || g_artistbrowse)
		return;

	error = sp_session_logout(g_session);

	if (SP_ERROR_OK != error) {
		fprintf(stderr, "failed to log out from Spotify: %s\n", sp_error_message(error));
		g_exit_code = 5;
		return;
	}
}

/**
 * Callback for libspotify
 *
 * @param browse    The browse result object that is now done
 * @param userdata  The opaque pointer given to sp_albumbrowse_create()
 */
static void SP_CALLCONV album_complete(sp_albumbrowse *browse, void *userdata)
{
	if (browse && SP_ERROR_OK == sp_albumbrowse_error(browse))
		print_albumbrowse(browse);
	else
		fprintf(stderr, "Failed to browse album: %s\n",
		        sp_error_message(sp_albumbrowse_error(browse)));

	sp_albumbrowse_release(g_albumbrowse);
	g_albumbrowse = NULL;
	try_terminate();
}

/**
 * Callback for libspotify
 *
 * @param browse    The browse result object that is now done
 * @param userdata  The opaque pointer given to sp_artistbrowse_create()
 */
static void SP_CALLCONV artist_complete(sp_artistbrowse *browse, void *userdata)
{
	if (browse && SP_ERROR_OK == sp_artistbrowse_error(browse))
		print_artistbrowse(browse);
	else
		fprintf(stderr, "Failed to browse artist: %s\n",
		        sp_error_message(sp_artistbrowse_error(browse)));

	sp_artistbrowse_release(g_artistbrowse);
	g_artistbrowse = NULL;
	try_terminate();
}

/**
 * Callback called when libspotify has new metadata available
 *
 * Not used in this example (but available to be able to reuse the session.c file
 * for other examples.)
 */
void SP_CALLCONV metadata_updated(sp_session *session)
{
}

/**
 * Callback called when the session has successfully logged in
 *
 * This is where we start two browse requests; one artist and one album. They
 * will eventually call the album_complete() and artist_complete() callbacks.
 */
void session_ready(sp_session *session)
{
	sp_link *link = sp_link_create_from_string("spotify:artist:0gxyHStUsqpMadRV0Di1Qt");
	sp_artist *artist;
	sp_album *album;

	if (!link) {
		fprintf(stderr, "failed to get link from a Spotify URI\n");
		g_exit_code = 6;
		return;
	}

	artist = sp_link_as_artist(link);

	if (!artist) {
		fprintf(stderr, "not an artist link\n");
		sp_link_release(link);
		g_exit_code = 7;
		return;
	}

	g_artistbrowse = sp_artistbrowse_create(session, artist, &artist_complete, NULL);
	sp_link_release(link);

	if (!g_artistbrowse) {
		fprintf(stderr, "failed to start artist browse\n");
		g_exit_code = 10;
		return;
	}

	link = sp_link_create_from_string("spotify:album:3L0CYQZR9jjFLbyX8ZZ6UP");

	if (!link) {
		fprintf(stderr, "failed to get link from a Spotify URI\n");
		g_exit_code = 8;
		return;
	}

	album = sp_link_as_album(link);

	if (!album) {
		fprintf(stderr, "not an album link\n");
		sp_link_release(link);
		g_exit_code = 9;
		return;
	}

	g_albumbrowse = sp_albumbrowse_create(session, album, &album_complete, NULL);
	sp_link_release(link);

	if (!g_albumbrowse) {
		fprintf(stderr, "failed to start album browse\n");
		g_exit_code = 10;
		return;
	}

	g_session = session;
}

/**
 * Callback called when the session has been terminated.
 */
void session_terminated(void)
{
}
