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
 * Example application showing how to use the link functions of libspotify.
 *
 * This example uses the session/session.c file from the session example.
 */
#include <stdio.h>
#include <spotify/api.h>


/* --- Data --- */
/// Defined in session.c
extern int g_exit_code;


/**
 * Return a text string for the link type of the given link
 *
 * @param  link  The link object
 * @return a text string
 */
static const char* get_link_type_label(sp_link *link)
{
	static const char *LINK_TYPES[] = {
		"invalid",
		"track",
		"album",
		"artist",
		"search",
		"playlist"
	};

	return LINK_TYPES[sp_link_type(link)];
}

/**
 * Print the given link in a rather ugly, but illustrative, format
 *
 * @param  link  The link object
 */
static void print_link(sp_link *link)
{
	char spotify_uri[256];

	if (0 > sp_link_as_string(link, spotify_uri, sizeof(spotify_uri))) {
		fprintf(stderr, "failed to render Spotify URI from link\n");
		g_exit_code = 8;
		return;
	}

	printf("%s link %s\n", get_link_type_label(link), spotify_uri);
}

/**
 * Do stuff with some of the functions in the link subsystem.
 */
static void try_links(sp_session *session)
{
	const char SPOTIFY_URI[] = "spotify:track:6JEK0CvvjDjjMUBFoXShNZ";
	sp_link *link = sp_link_create_from_string(SPOTIFY_URI);

	if (!link) {
		fprintf(stderr, "failed to get link from a Spotify URI\n");
		g_exit_code = 6;
		return;
	}

	print_link(link);

	// The create function will have increased the reference count for us.
	sp_link_release(link);
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

/// Called by session.c when logged in
void session_ready(sp_session *session)
{
	sp_error error;

	try_links(session);
	error = sp_session_logout(session);

	if (SP_ERROR_OK != error) {
		fprintf(stderr, "failed to log out from Spotify: %s\n",
		                sp_error_message(error));
		g_exit_code = 5;
		return;
	}
}

/// Called by session.c when logged out
void session_terminated(void)
{
}
