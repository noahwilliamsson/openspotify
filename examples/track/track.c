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
 * Example application showing how to use the track subsystem.
 */
#include <stdio.h>
#include <spotify/api.h>


/* --- Data --- */
extern int g_exit_code;

/// A global reference to the track we are currently investigating
static sp_track *g_track;
/// A global reference to the image (cover) we are currently investigating
static sp_image *g_image;


/**
 * Logout session
 */
static void logout(sp_session *session)
{
	sp_error error = sp_session_logout(session);

	if (SP_ERROR_OK != error) {
		fprintf(stderr, "failed to log out from Spotify: %s\n", sp_error_message(error));
		g_exit_code = 5;
		return;
	}
}

/**
 * Print the given track title together with some trivial metadata
 *
 * @param  track   The track object
 */
static void print_track(sp_track *track)
{
	int duration = sp_track_duration(track);

	printf("Track \"%s\" [%d:%02d] has %d artist(s), %d%% popularity\n",
	       sp_track_name(track),
	       duration / 60000,
	       (duration / 1000) / 60,
	       sp_track_num_artists(track),
	       sp_track_popularity(track));
}

/**
 * Print some trivial metadata for a loaded image
 *
 * @param  image   The image object
 */
static void print_cover(sp_image *image, void *userdata)
{
	const char *format = "unknown";
	int alpha = 0, reverse = 0;
	void *data;
	int pitch;
	int width = sp_image_width(image);
	int height = sp_image_height(image);
	int x, y;
	int rsum = 0;
	int gsum = 0;
	int bsum = 0;
	sp_imageformat fmt = sp_image_format(image);

	switch (fmt) {
	case SP_IMAGE_FORMAT_RGB:
		format = "RGB";
		break;
	case SP_IMAGE_FORMAT_RGBA:
		format = "RGBA";
		alpha = 1;
		break;
	case SP_IMAGE_FORMAT_RGBA_PRE:
		format = "RGBA_PRE";
		alpha = 1;
		break;
	case SP_IMAGE_FORMAT_BGR:
		format = "BGR";
		reverse = 1;
		break;
	case SP_IMAGE_FORMAT_BGRA:
		format = "BGRA";
		alpha = 1;
		reverse = 1;
		break;
	case SP_IMAGE_FORMAT_BGRA_PRE:
		format = "BGRA_PRE";
		alpha = 1;
		reverse = 1;
		break;
	}

	data = sp_image_lock_pixels(image, &pitch);

	for (y = 0; y < height; ++y) {
		byte *p = (byte *) (data + y * pitch);
		for (x = 0; x < width; ++x) {
			if (reverse) {
				bsum += *p++;
				gsum += *p++;
				rsum += *p++;
			}
			else {
				rsum += *p++;
				gsum += *p++;
				bsum += *p++;
			}
			if (alpha)
				p++;
		}
	}

	printf("Cover %d x %d %s image. Average RGB color %d, %d, %d.\n",
	       width, height, format,
	       rsum / width / height,
	       gsum / width / height,
	       bsum / width / height);

	sp_image_unlock_pixels(image);

	sp_image_release(image);
	g_image = NULL;

	logout((sp_session *) userdata);
}

/**
 * Request the coverart image
 *
 * @param  track   The track object
 */
static void request_cover(sp_session *session, sp_track *track)
{
	sp_album *album;
	const byte *image_id;

	album = sp_track_album(g_track);
	if (!album) {
		fprintf(stderr, "No track album!\n");
		logout(session);
		return;
	}

	if (!sp_album_is_loaded(album)) {
		fprintf(stderr, "Track album not loaded!\n");
		logout(session);
		return;
	}

	image_id = sp_album_cover(album);

	if (!image_id) {
		fprintf(stderr, "No album coverart!\n");
		logout(session);
		return;
	}

	g_image = sp_image_create(session, image_id);

	if (!g_image) {
		fprintf(stderr, "The cover image could not be loaded!\n");
		logout(session);
		return;
	}

	sp_image_add_load_callback(g_image, &print_cover, session);
}

/**
 * Run some example code showing the track functions
 *
 * This function will be called repeatedly (on each metadata_update() callback)
 * until the track is loaded, and we can finally print its information.
 *
 * @param  session  The current session
 */
static void try_tracks(sp_session *session)
{
	// If we have already shown the track (track is NULL), or the track
	// metadata is not yet ready, do nothing.
	if (!g_track || !sp_track_is_loaded(g_track))
		return;

	print_track(g_track);

	request_cover(session, g_track);

	// We increased the reference count in session_ready(), better decrease it here.
	sp_track_release(g_track);
	g_track = NULL;
}

/**
 * Callback called when libspotify has new metadata available
 */
void metadata_updated(sp_session *session)
{
	try_tracks(session);
}

/**
 * Callback called when the session has successfully logged in
 */
void session_ready(sp_session *session)
{
	sp_link *link = sp_link_create_from_string("spotify:track:6JEK0CvvjDjjMUBFoXShNZ");

	if (!link) {
		fprintf(stderr, "failed to get link from a Spotify URI\n");
		g_exit_code = 6;
		return;
	}

	g_track = sp_link_as_track(link);

	if (!g_track) {
		fprintf(stderr, "not a track link\n");
		sp_link_release(link);
		g_exit_code = 7;
		return;
	}

	// We do not need the link reference, so we get our own reference to the track
	// and release the list.
	sp_track_add_ref(g_track);
	sp_link_release(link);

	try_tracks(session);
}

/**
 * Callback called when the session has been terminated.
 */
void session_terminated(void)
{
}
