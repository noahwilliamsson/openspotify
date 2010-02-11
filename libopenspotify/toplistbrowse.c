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
#include "toplistbrowse.h"
#include "sp_opaque.h"
#include "track.h"
#include "util.h"


static int toplistbrowse_callback(CHANNEL *ch, unsigned char *payload, unsigned short len);
static int toplistbrowse_parse_xml(struct toplistbrowse_ctx *toplistbrowse_ctx);


int toplistbrowse_process_request(sp_session *session, struct request *req) {
	struct toplistbrowse_ctx *toplistbrowse_ctx = *(struct toplistbrowse_ctx **)req->input;
	sp_toplistbrowse *toplistbrowse = toplistbrowse_ctx->toplistbrowse;

	/*
	 * Prevent request from happening again.
	 * If there's an error the channel callback will reset the timeout
	 *
	 */
	req->next_timeout = INT_MAX;
	req->state = REQ_STATE_RUNNING;

	toplistbrowse_ctx->req = req;

	DSFYDEBUG("Initiating toplistbrowse with type %d, region %d\n", toplistbrowse->type, toplistbrowse->region);

	return cmd_toplistbrowse(session, toplistbrowse->type, toplistbrowse->region, toplistbrowse_callback, toplistbrowse_ctx);
}


static int toplistbrowse_callback(CHANNEL *ch, unsigned char *payload, unsigned short len) {
	int skip_len;
	struct toplistbrowse_ctx *toplistbrowse_ctx = (struct toplistbrowse_ctx *)ch->private;

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

			buf_append_data(toplistbrowse_ctx->buf, payload, len);
			break;

		case CHANNEL_ERROR:
			DSFYDEBUG("Got a channel ERROR, retrying within %d seconds\n", TOPLISTBROWSE_RETRY_TIMEOUT);
			buf_free(toplistbrowse_ctx->buf);
			toplistbrowse_ctx->buf = buf_new();

			/* Reset timeout so the request can be retried */
			toplistbrowse_ctx->req->next_timeout = get_millisecs() + TOPLISTBROWSE_RETRY_TIMEOUT*1000;
			break;

		case CHANNEL_END:
			if(toplistbrowse_parse_xml(toplistbrowse_ctx) == 0) {
				toplistbrowse_ctx->toplistbrowse->error = SP_ERROR_OK;
				toplistbrowse_ctx->toplistbrowse->is_loaded = 1;
			}
			else
				toplistbrowse_ctx->toplistbrowse->error = SP_ERROR_OTHER_PERMANENT;

			request_set_result(toplistbrowse_ctx->session, toplistbrowse_ctx->req, toplistbrowse_ctx->toplistbrowse->error, toplistbrowse_ctx->toplistbrowse);

			/* Release reference made in sp_toplistbrowse_create() */
			sp_toplistbrowse_release(toplistbrowse_ctx->toplistbrowse);

			buf_free(toplistbrowse_ctx->buf);
			free(toplistbrowse_ctx);
			break;

		default:
			break;
	}

	return 0;

}


static int toplistbrowse_parse_xml(struct toplistbrowse_ctx *toplistbrowse_ctx) {
	unsigned char id[16];
	struct buf *xml;
	sp_toplistbrowse *toplistbrowse = toplistbrowse_ctx->toplistbrowse;
	ezxml_t root, listnode, node;
	sp_artist *artist;
	sp_album *album;
	sp_track *track;

	xml = despotify_inflate(toplistbrowse_ctx->buf->ptr, toplistbrowse_ctx->buf->len);
	if(xml == NULL)
		return -1;

#ifdef DEBUG
	{
		FILE *fd;
		char buf[64];
		sprintf(buf, "toplistbrowse-%04x-%04x.xml", toplistbrowse->type, toplistbrowse->region);
		fd = fopen(buf, "w");
		if(fd) {
			fwrite(xml->ptr, xml->len, 1, fd);
			fclose(fd);
		}
	}
#endif

	root = ezxml_parse_str((char *)xml->ptr, xml->len);
	if(root == NULL) {
		DSFYDEBUG("Failed to parse XML\n");
		buf_free(xml);
		return -1;
	}


	for(listnode = ezxml_get(root, "artists", 0, "artist", -1);
	    listnode;
	    listnode = listnode->next) {

		if((node = ezxml_get(listnode, "id", -1)) == NULL)
			continue;

		hex_ascii_to_bytes(node->txt, id, 16);
		artist = osfy_artist_add(toplistbrowse_ctx->session, id);

		if(!sp_artist_is_loaded(artist))
			osfy_artist_load_artist_from_xml(toplistbrowse_ctx->session, artist, listnode);

		sp_artist_add_ref(artist);
		toplistbrowse->artists = (sp_artist **)realloc(toplistbrowse->artists, (toplistbrowse->num_artists + 1) * sizeof(sp_artist *));
		toplistbrowse->artists[toplistbrowse->num_artists++] = artist;
	}


	for(listnode = ezxml_get(root, "albums", 0, "album", -1);
	    listnode;
	    listnode = listnode->next) {

		if((node = ezxml_get(listnode, "id", -1)) == NULL)
			continue;

		hex_ascii_to_bytes(node->txt, id, 16);
		album = sp_album_add(toplistbrowse_ctx->session, id);

		if(!sp_album_is_loaded(album))
			osfy_album_load_from_search_xml(toplistbrowse_ctx->session, album, listnode);

		sp_album_add_ref(album);
		toplistbrowse->albums = (sp_album **)realloc(toplistbrowse->albums, (toplistbrowse->num_albums + 1) * sizeof(sp_album *));
		toplistbrowse->albums[toplistbrowse->num_albums++] = album;
	}


	for(listnode = ezxml_get(root, "tracks", 0, "track", -1);
	    listnode;
	    listnode = listnode->next) {

		if((node = ezxml_get(listnode, "id", -1)) == NULL)
			continue;

		hex_ascii_to_bytes(node->txt, id, 16);
		track = osfy_track_add(toplistbrowse_ctx->session, id);

		if(!sp_track_is_loaded(track))
			osfy_track_load_from_xml(toplistbrowse_ctx->session, track, listnode);

		sp_track_add_ref(track);
		toplistbrowse->tracks = (sp_track **)realloc(toplistbrowse->tracks, (toplistbrowse->num_tracks + 1) * sizeof(sp_track *));
		toplistbrowse->tracks[toplistbrowse->num_tracks++] = track;
	}


	ezxml_free(root);
	buf_free(xml);

	return 0;
}
