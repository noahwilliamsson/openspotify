/*
 * $Id: handlers.c 398 2009-07-28 23:16:37Z noah-w $
 *
 * Default handlers for different types of commands
 *
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <assert.h>

#include "channel.h"
#include "commands.h"
#include "debug.h"
#include "handlers.h"
#include "packet.h"
#include "player.h"
#include "playlist.h"
#include "request.h"
#include "sp_opaque.h"
#include "user.h"
#include "util.h"


int handle_secret_block (sp_session * session, unsigned char *payload, int len)
{
	unsigned int *t;

	if (len != 336) {
		DSFYDEBUG ("Got cmd=0x02 with len %d, expected 336!\n", len);
		return -1;
	}

	t = (unsigned int *) payload;
	DSFYDEBUG ("Initial time %u (%ld seconds from now)\n",
		 ntohl (*t), time (NULL) - ntohl (*t));
	t++;
	DSFYDEBUG ("Future time %u (%ld seconds in the future)\n",
		 ntohl (*t), ntohl (*t) - time (NULL));

	t++;
	DSFYDEBUG ("Next value is %u\n", ntohl (*t));
	t++;
	DSFYDEBUG ("Next value is %u\n", ntohl (*t));

	#if 0 /* FIXME */
	assert (memcmp (session->rsa_pub_exp, payload + 16, 128) == 0);
	#endif

	/* At payload+16+128 is a  144 bytes (1536-bit) RSA signature */

	/*
	 * Actually the cache hash is sent before the server has sent any
	 * packets. It's just put here out of convenience, because this is
	 * one of the first packets ever by the server, and also not
	 * repeated during a session.
	 *
	 */

	return cmd_send_cache_hash (session);
}

int handle_ping (sp_session * session, unsigned char *payload, int len)
{

	/* Store timestamp and respond to the request */
	time_t t;
	assert (len == 4);
	memcpy (&t, payload, 4);
#if 0 /* FIXME */
	session->user_info.last_ping = ntohl (t);
#endif

	return cmd_ping_reply (session);
}

int handle_channel (sp_session *session, int cmd, unsigned char *payload, int len)
{
	if (cmd == CMD_CHANNELERR) {
		DSFYDEBUG ("Channel %d got error %d (0x%02x)\n",
			 ntohs (*(unsigned short *) payload),
			 ntohs (*(unsigned short *) (payload + 2)),
			 ntohs (*(unsigned short *) (payload + 2)))
	}
	
	return channel_process (session, payload, len, cmd == CMD_CHANNELERR);
}

int handle_aeskey (sp_session *session, unsigned char *payload, int len)
{
	CHANNEL *ch;
	int ret;

	DSFYDEBUG ("Server said 0x0d (AES key) for channel %d\n",
		   ntohs (*(unsigned short *) (payload + 2)))
	if ((ch = channel_by_id (session, ntohs
				    (*(unsigned short *) (payload + 2)))) !=
			   NULL) {
		ret = ch->callback (ch, payload + 4, len - 4);
		channel_unregister (session, ch);
	}
	else {
		DSFYDEBUG
			("Command 0x0d: Failed to find channel with ID %d\n",
			 ntohs (*(unsigned short *) (payload + 2)));
		ret = -1;
	}

	return ret;
}

static int handle_countrycode (sp_session * session, unsigned char *payload, int len) {
	int i;

	for(i = 0; i < len && i < (int)sizeof(session->country) - 1; i++)
		session->country[i] = (char)payload[i];

	return 0;
}

static int handle_prodinfo (sp_session * session, unsigned char *payload, int len)
{
#if 0 /* FIXME */
	xml_parse_prodinfo(&session->user_info, payload, len);
#endif
	return 0;
}


static int handle_playlist_state_changed (sp_session * session, unsigned char *payload, int len) {
	void *container;

	assert(len == 17);

	container = malloc(len);
	if(container == NULL)
		return -1;

	memcpy(container, payload, len);
	return request_post_result(session, REQ_TYPE_PLAYLIST_STATE_CHANGED, SP_ERROR_OK, container);
}


static int handle_notify (sp_session * session, unsigned char *payload, int len) {
	unsigned char *ptr;
	unsigned short notify_len;
	int i;

	ptr = payload;
	for(i = 0; len && i < 9; i++, len--)
		payload++;

	if(len < 2) {
		DSFYDEBUG("Failed to handle NOTIFY message\n");
		return -1;
	}

	notify_len = ntohs(*(unsigned short *)payload);
	payload += 2;
	len -= 2;
	if(notify_len > len) {
		DSFYDEBUG("Failed to handle NOTIFY message. Parsed length=%d, avail=%d\n",
				notify_len, len);
		return -1;
	}

	/* Needs to be free'd by the consumer */
	ptr = malloc(notify_len + 1);
	memcpy(ptr, payload, notify_len);
	ptr[notify_len] = 0;

	return request_post_result(session, REQ_TYPE_NOTIFY, SP_ERROR_OK, ptr);
}


int handle_packet (sp_session * session,
		   int cmd, unsigned char *payload, unsigned short len)
{
	int error = 0;

	switch (cmd) {
	case CMD_SECRETBLK:
		error = handle_secret_block (session, payload, len);
		break;

	case CMD_PING:
		error = handle_ping (session, payload, len);
		break;

	case CMD_CHANNELDATA:
	case CMD_CHANNELERR:
		error = handle_channel (session, cmd, payload, len);
		break;

	case CMD_AESKEY:
		error = handle_aeskey (session, payload, len);
		break;

	case CMD_SHAHASH:
		break;

	case CMD_COUNTRYCODE:
		error = handle_countrycode (session, payload, len);
		break;

	case CMD_P2P_INITBLK:
		DSFYDEBUG ("Server said 0x21 (P2P initalization block)\n")
			break;

	case CMD_PLAYLISTCHANGED:
		error = handle_playlist_state_changed(session, payload, len);
		break;

	case CMD_NOTIFY:
		/* HTML-notification, shown in a yellow bar in the official client */
		error = handle_notify (session, payload, len);
		break;

	case CMD_PRODINFO:
		/* Payload is uncompressed XML */
		error = handle_prodinfo (session, payload, len);
		break;

	case CMD_WELCOME:
		/* Lookup the session's username */
		if(!sp_user_is_loaded(session->user))
			user_lookup(session, session->user);
		
		/* Trigger loading of playlist container and contained playlists */
		request_post(session, REQ_TYPE_PC_LOAD, NULL);
		break;

	case CMD_TOKENLOST:
		request_post(session, REQ_TYPE_PLAY_TOKEN_LOST, NULL);
		break;
	}

	return error;
}
