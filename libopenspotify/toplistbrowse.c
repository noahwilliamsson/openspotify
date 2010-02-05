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
	
	toplistbrowse_ctx->req = req;
	req->state = REQ_STATE_RUNNING;
	req->next_timeout = get_millisecs() + TOPLISTBROWSE_RETRY_TIMEOUT;
	
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
			toplistbrowse_ctx = buf_new();
			
			/* The request processor will retry this round */
			break;
			
		case CHANNEL_END:
			if(toplistbrowse_parse_xml(toplistbrowse_ctx) == 0) {
				toplistbrowse_ctx->toplistbrowse->error = SP_ERROR_OK;
				toplistbrowse_ctx->toplistbrowse->is_loaded = 1;
			}
			else
				toplistbrowse_ctx->toplistbrowse->error = SP_ERROR_OTHER_PERMANENT;

			request_set_result(toplistbrowse_ctx->session, toplistbrowse_ctx->req, toplistbrowse_ctx->toplistbrowse->error, toplistbrowse_ctx->toplistbrowse);

			buf_free(toplistbrowse_ctx->buf);
			free(toplistbrowse_ctx);
			break;
			
		default:
			break;
	}
	
	return 0;
	
}


static int toplistbrowse_parse_xml(struct toplistbrowse_ctx *toplistbrowse_ctx) {
	struct buf *xml;
	sp_toplistbrowse *toplistbrowse = toplistbrowse_ctx->toplistbrowse;
	ezxml_t root;
	
	xml = despotify_inflate(toplistbrowse_ctx->buf->ptr, toplistbrowse_ctx->buf->len);
	if(xml == NULL)
		return -1;
	
#ifdef DEBUG
	{
		FILE *fd;
		fd = fopen("toplistbrowse.xml", "w");
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



	/* XXX - Figure this out! */

	toplistbrowse->error = SP_ERROR_OTHER_PERMANENT;


	ezxml_free(root);
	buf_free(xml);
	
	return 0;
}
