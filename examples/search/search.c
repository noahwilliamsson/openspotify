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
 * Example application showing how to use the search subsystem.
 */
#include <stdio.h>
#include <spotify/api.h>


/* --- Data --- */
extern int g_exit_code;

/// A global reference to the session object
static sp_session *g_session;
/// A global reference to the search we are currently investigating
static sp_search *g_search;


/**
 * Print the given track title together with some trivial metadata
 *
 * @param  track   The track object
 */
static void print_track(sp_track *track)
{
	int duration = sp_track_duration(track);

	printf("  Track \"%s\" [%d:%02d] has %d artist(s), %d%% popularity\n",
		sp_track_name(track),
		duration / 60000,
		(duration / 1000) / 60,
		sp_track_num_artists(track),
		sp_track_popularity(track));
}

/**
 * Print the given album metadata
 *
 * @param  album  The album object
 */
static void print_album(sp_album *album)
{
	printf("  Album \"%s\" (%d)\n",
	       sp_album_name(album),
	       sp_album_year(album));
}

/**
 * Print the given artist metadata
 *
 * @param  artist  The artist object
 */
static void print_artist(sp_artist *artist)
{
	printf("  Artist \"%s\"\n", sp_artist_name(artist));
}

/**
 * Print the given search result with as much information as possible
 *
 * @param  search   The search result
 */
static void print_search(sp_search *search)
{
	int i;

	printf("Query          : %s\n", sp_search_query(search));
	printf("Did you mean   : %s\n", sp_search_did_you_mean(search));
	printf("Tracks in total: %d\n", sp_search_total_tracks(search));
	puts("");

	for (i = 0; i < sp_search_num_tracks(search) && i < 10; ++i)
		print_track(sp_search_track(search, i));

	puts("");

	for (i = 0; i < sp_search_num_albums(search) && i < 10; ++i)
		print_album(sp_search_album(search, i));

	puts("");

	for (i = 0; i < sp_search_num_artists(search) && i < 10; ++i)
		print_artist(sp_search_artist(search, i));

	puts("");
}

/**
 * Called after the each browse request is done
 *
 * Will only actually log out when all browse requests are done
 */
static void terminate(void)
{
	sp_error error;

	error = sp_session_logout(g_session);

	if (SP_ERROR_OK != error) {
		fprintf(stderr, "failed to log out from Spotify: %s\n",
		                sp_error_message(error));
		g_exit_code = 5;
		return;
	}
}

/**
 * Callback for libspotify
 *
 * @param browse    The browse result object that is now done
 * @param userdata  The opaque pointer given to sp_artistbrowse_create()
 */
static void search_complete(sp_search *search, void *userdata)
{
	if (search && SP_ERROR_OK == sp_search_error(search))
		print_search(search);
	else
		fprintf(stderr, "Failed to search: %s\n",
		        sp_error_message(sp_search_error(search)));

	sp_search_release(g_search);
	g_search = NULL;
	terminate();
}

/**
 * Callback called when libspotify has new metadata available
 *
 * Not used in this example (but available to be able to reuse the session.c file
 * for other examples.)
 */
void metadata_updated(sp_session *session)
{
}

/**
 * Callback called when the session has successfully logged in
 *
 * This is where we start two browse requests; one artist and one album. They
 * will eventually call the album_complete() and artist_complete() callbacks.
 *
 * This
 */
void session_ready(sp_session *session)
{
	g_search = sp_search_create(session, "year:2003 never", 0, 10,
	                            &search_complete, NULL);

	if (!g_search) {
		fprintf(stderr, "failed to start search\n");
		g_exit_code = 6;
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
