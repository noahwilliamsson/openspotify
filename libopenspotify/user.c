#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <spotify/api.h>

#include "buf.h"
#include "channel.h"
#include "commands.h"
#include "debug.h"
#include "ezxml.h"
#include "request.h"
#include "sp_opaque.h"
#include "user.h"
#include "util.h"


struct user_ctx {
        sp_session *session;
        struct request *req;
	struct buf *buf;
        sp_user *user;
};


static int user_callback(CHANNEL *ch, unsigned char *payload, unsigned short len);
static int user_parse_xml(struct user_ctx *user_ctx);


sp_user *user_add(sp_session *session, const char *name) {
	char name_key[256];
	sp_user *user;
	
	strncpy(name_key, name, sizeof(name_key) - 1);
	name_key[sizeof(name_key) - 1] = 0;
	
	user = (sp_user *)hashtable_find(session->hashtable_users, name_key);
	if(user)
		return user;
	
	user = malloc(sizeof(sp_user));
	if(user == NULL)
		return NULL;
	
	strncpy(user->canonical_name, name, sizeof(user->canonical_name) - 1);
	user->canonical_name[sizeof(user->canonical_name) - 1] = 0;
	
	DSFYDEBUG("New sp_user object with canonical name '%s'\n", user->canonical_name);
	
	user->display_name = NULL;

	user->hashtable = session->hashtable_users;
	hashtable_insert(user->hashtable, user->canonical_name, user);

	user->error = SP_ERROR_RESOURCE_NOT_LOADED;
	user->is_loaded = 0;

	user->ref_count = 0;
	
	return user;
}


int user_lookup(sp_session *session, sp_user *user) {
	struct user_ctx *user_ctx;
	void **container;

	if(user->is_loaded || user->error != SP_ERROR_RESOURCE_NOT_LOADED) {
		DSFYDEBUG("Attempt to lookup user '%s', which is either loaded (is_loaded=%d) "
			"or in a different state than SP_ERROR_RESOURCE_NOT_LOADED (currently %d)\n",
			user->canonical_name, user->is_loaded, user->error);

		return 0;
	}

	/* Note that we're loading this resource */
	user->error = SP_ERROR_IS_LOADING;

	user_ctx = (struct user_ctx *)malloc(sizeof(struct user_ctx));
	if(user_ctx == NULL)
		return -1;

	/* Temporarily increase ref count so it doesn't disappear */
	user_add_ref(user);

	user_ctx->session = session;
	user_ctx->req = NULL;
	user_ctx->buf = buf_new();
	user_ctx->user = user;
	
        container = (void **)malloc(sizeof(void *));
        *container = user_ctx;
	
	return request_post(session, REQ_TYPE_USER, container);
}


void user_release(sp_user *user) {
	assert(user->ref_count > 0);
	
	if(--user->ref_count)
		return;

	user_free(user);
}


void user_free(sp_user *user) {
	if(user->display_name)
		free(user->display_name);

	hashtable_remove(user->hashtable, user->canonical_name);

	free(user);
}


void user_add_ref(sp_user *user) {
	user->ref_count++;
}


int user_process_request(sp_session *session, struct request *req) {
	struct user_ctx *user_ctx = *(struct user_ctx **)req->input;
	
	user_ctx->req = req;
	req->state = REQ_STATE_RUNNING;
	req->next_timeout = get_millisecs() + USER_RETRY_TIMEOUT;
	
	DSFYDEBUG("Request details on sp_user '%s'\n", user_ctx->user->canonical_name);
	return cmd_userinfo(session, user_ctx->user->canonical_name, user_callback, user_ctx);
}


static int user_callback(CHANNEL *ch, unsigned char *payload, unsigned short len) {
	int skip_len;
	struct user_ctx *user_ctx = (struct user_ctx *)ch->private;
	
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

			buf_append_data(user_ctx->buf, payload, len);
			break;
			
		case CHANNEL_ERROR:
			DSFYDEBUG("Got a channel ERROR, retrying within %d seconds\n", USER_RETRY_TIMEOUT);
			buf_free(user_ctx->buf);
			user_ctx->buf = buf_new();
			
			/* The request processor will retry this round */
			break;
			
		case CHANNEL_END:
			if(user_parse_xml(user_ctx) == 0) {
				request_set_result(user_ctx->session, user_ctx->req, SP_ERROR_OK, user_ctx->user);
			
				buf_free(user_ctx->buf);
				free(user_ctx);
			}
			else {
				buf_free(user_ctx->buf);
				user_ctx->buf = buf_new();
			}
			break;
			
		default:
			break;
	}
	
	return 0;
	
}


static int user_parse_xml(struct user_ctx *user_ctx) {
	struct buf *xml;
	ezxml_t root, node;
	
	xml = despotify_inflate(user_ctx->buf->ptr, user_ctx->buf->len);
	if(xml == NULL)
		return -1;
	
	{
		FILE *fd;
		char buf[512];
		sprintf(buf, "user-%s.xml", user_ctx->user->canonical_name);
		fd = fopen(buf, "w");
		if(fd) {
			fwrite(xml->ptr, xml->len, 1, fd);
			fclose(fd);
		}
	}


	root = ezxml_parse_str((char *) xml->ptr, xml->len);
	if(root == NULL) {
		DSFYDEBUG("Failed to parse XML\n");
		buf_free(xml);
		return -1;
	}

	node = ezxml_get(root, "user", 0, "username", -1);
	if(node && strcmp(user_ctx->user->canonical_name, node->txt) == 0) {
		node = ezxml_get(root, "user", 0, "verbatim_username", -1);
		if(user_ctx->user->display_name != NULL)
			free(user_ctx->user->display_name);

		user_ctx->user->display_name = strdup(node->txt);

		DSFYDEBUG("User '%s' is LOADED\n", user_ctx->user->canonical_name);
		user_ctx->user->error = SP_ERROR_OK;
		user_ctx->user->is_loaded = 1;
	}

	ezxml_free(root);
	
	return 0;
}
