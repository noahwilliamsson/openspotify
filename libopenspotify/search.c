#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <spotify/api.h>

#include "album.h"
#include "artist.h"
#include "buf.h"
#include "commands.h"
#include "debug.h"
#include "ezxml.h"
#include "search.h"
#include "sp_opaque.h"
#include "track.h"
#include "util.h"


static int search_callback(CHANNEL *ch, unsigned char *payload, unsigned short len);
static int search_parse_xml(struct search_ctx *search_ctx);


int search_process_request(sp_session *session, struct request *req) {
	struct search_ctx *search_ctx = *(struct search_ctx **)req->input;
	sp_search *search = search_ctx->search;

	/*
	 * Prevent request from happening again.
	 * If there's an error the channel callback will reset the timeout
	 *
	 */
	req->next_timeout = INT_MAX;
	req->state = REQ_STATE_RUNNING;

	search_ctx->req = req;

	DSFYDEBUG("Initiating search with query '%s', track %d/%d, album %d/%d, artist %d/%d\n",
		  search->query, search->track_offset, search->track_count,
		  search->album_offset, search->album_count,
		  search->artist_offset, search->artist_count);


	/* FIXME: Should investigate how album/artist offset/count is supplied */
	return cmd_search(session, search->query, search->track_offset, search->track_count, search_callback, search_ctx);
}


static int search_callback(CHANNEL *ch, unsigned char *payload, unsigned short len) {
	int skip_len;
	struct search_ctx *search_ctx = (struct search_ctx *)ch->private;

	switch(ch->state) {
		case CHANNEL_DATA:
			/* Skip a minimal gzip header */
			if (ch->total_data_len < 10) {
				skip_len = 10 - ch->total_data_len;
				while(skip_len && len) {
					skip_len--;
					len--;
					payload++;
				}

				if (len == 0)
					break;
			}

			buf_append_data(search_ctx->buf, payload, len);
			break;

		case CHANNEL_ERROR:
			DSFYDEBUG("Got a channel ERROR, retrying within %d seconds\n", SEARCH_RETRY_TIMEOUT);
			buf_free(search_ctx->buf);
			search_ctx->buf = buf_new();

			/* Reset timeout so the request can be retried */
			search_ctx->req->next_timeout = get_millisecs() + SEARCH_RETRY_TIMEOUT*1000;

			break;

		case CHANNEL_END:
			if(search_parse_xml(search_ctx) == 0) {
				search_ctx->search->error = SP_ERROR_OK;
				search_ctx->search->is_loaded = 1;
			}
			else
				search_ctx->search->error = SP_ERROR_OTHER_PERMANENT;

			request_set_result(search_ctx->session, search_ctx->req, search_ctx->search->error, search_ctx->search);

			buf_free(search_ctx->buf);
			free(search_ctx);
			break;

		default:
			break;
	}

	return 0;

}


static int search_parse_xml(struct search_ctx *search_ctx) {
	int i, count;
	struct buf *xml;
	unsigned char id[16];
	sp_search *search = search_ctx->search;
	ezxml_t root, node, artist_node, album_node, track_node;
	sp_artist *artist;
	sp_album *album;
	sp_track *track;

	xml = despotify_inflate(search_ctx->buf->ptr, search_ctx->buf->len);
	if(xml == NULL)
		return -1;

#ifdef DEBUG
	{
		FILE *fd;
		fd = fopen("search.xml", "w");
		if(fd) {
			fwrite(xml->ptr, xml->len, 1, fd);
			fclose(fd);
		}
	}
#endif

	root = ezxml_parse_str((char *) xml->ptr, xml->len);
	if(root == NULL) {
		DSFYDEBUG("Failed to parse XML\n");
		buf_free(xml);
		return -1;
	}


	/* Version check */
	if((node = ezxml_get(root, "version", -1)) == NULL
		|| atoi(node->txt) != 1) {
		DSFYDEBUG("Unsupported search XML version!\n");
		buf_free(xml);
		return -1;
	}


	/* Search hint */
	if(search->did_you_mean)
		free(search->did_you_mean);

	if((node = ezxml_get(root, "did-you-mean", -1)) != NULL)
		search->did_you_mean = strdup(node->txt);
	else
		search->did_you_mean = strdup("");


	/*
	 * Get number of total artists
	 * The value of the 'total-artists' element might be larger than the
	 * number of artists actually returned.
	 *
	 */
	if((node = ezxml_get(root, "total-artists", -1)) == NULL)
		return -1;

	count = atoi(node->txt);
	search->artists = malloc(count * sizeof(sp_artist *));


	/* Load artists */
	search->num_artists = 0;
	for(i = 0, artist_node = ezxml_get(root, "artists", 0, "artist", -1);
	    i < count && artist_node;
	    i++, artist_node = artist_node->next) {

		if((node = ezxml_get(artist_node, "id", -1)) == NULL)
			return -1;


		hex_ascii_to_bytes(node->txt, id, 16);
		artist = osfy_artist_add(search_ctx->session, id);

		if(!sp_artist_is_loaded(artist))
			osfy_artist_load_artist_from_xml(search_ctx->session, artist, artist_node);

		sp_artist_add_ref(artist);
		search->artists[search->num_artists] = artist;
		search->num_artists++;
	}


	/* Get number of total albums */
	if((node = ezxml_get(root, "total-albums", -1)) == NULL)
		return -1;

	count = atoi(node->txt);
	search->albums = malloc(count * sizeof(sp_album *));


	/* Load albums */
	search->num_albums = 0;
	for(i = 0, album_node = ezxml_get(root, "albums", 0, "album", -1);
	    i < count && album_node;
	    i++, album_node = album_node->next) {

		if((node = ezxml_get(album_node, "id", -1)) == NULL)
			return -1;


		hex_ascii_to_bytes(node->txt, id, 16);
		album = sp_album_add(search_ctx->session, id);

		if(!sp_album_is_loaded(album))
			osfy_album_load_from_search_xml(search_ctx->session, album, album_node);

		sp_album_add_ref(album);
		search->albums[search->num_albums] = album;
		search->num_albums++;
	}


	/* Get number of total tracks */
	if((node = ezxml_get(root, "total-tracks", -1)) == NULL)
		return -1;

	search->total_tracks = atoi(node->txt);


	/* Load tracks */
	search->num_tracks = 0;
	for(i = 0, track_node = ezxml_get(root, "tracks", 0, "track", -1);
	    track_node;
	    i++, track_node = track_node->next) {

		if((node = ezxml_get(track_node, "id", -1)) == NULL)
			return -1;


		hex_ascii_to_bytes(node->txt, id, 16);
		track = osfy_track_add(search_ctx->session, id);

		if(!sp_track_is_loaded(track))
			osfy_track_load_from_xml(search_ctx->session, track, track_node);

		search->tracks = realloc(search->tracks, sizeof(sp_track *) * (1 + search->num_tracks));
		sp_track_add_ref(track);
		search->tracks[search->num_tracks] = track;
		search->num_tracks++;
	}


	ezxml_free(root);

	return 0;
}
