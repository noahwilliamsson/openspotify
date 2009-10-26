/*
 * $Id: channel.c 123 2009-03-02 17:32:12Z jorgenpt $
 *
 * Register and callbak interface for handling 
 * data channels inside a single connection to
 * the server.
 *
 */

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "debug.h"
#include "channel.h"
#include "sp_opaque.h"
#include "util.h"

CHANNEL *channel_register (sp_session *session, char *name, channel_callback callback,
			   void *private)
{
	CHANNEL *ch;
	int id;

	/*
	 * Pick a channel id and lazily set next to last available + 1
	 */
	id = session->next_channel_id++;
	if ((ch = session->channels) != NULL) {
		for (; ch; ch = ch->next) {
			if (ch->channel_id >= session->next_channel_id)
				session->next_channel_id = ch->channel_id + 1;
		}
	}

	ch = malloc (sizeof (CHANNEL));
	if (!ch)
		return NULL;

	ch->channel_id = id;
	ch->header_id = 0;
	ch->state = CHANNEL_HEADER;

	ch->total_header_len = 0;
	ch->total_data_len = 0;

	if (name)
		strncpy (ch->name, name, sizeof (ch->name) - 1);
	else
		ch->name[0] = 0;
	ch->name[sizeof (ch->name) - 1] = 0;

	ch->callback = callback;
	ch->private = private;

	ch->next = session->channels;
	session->channels = ch;

	session->num_channels++;

	DSFYDEBUG("Registered channel '%s' with id %d\n", ch->name, ch->channel_id);

	return ch;
}

void channel_unregister (sp_session *session, CHANNEL * ch)
{
	CHANNEL *tmp;

	DSFYDEBUG
		("channel %d: unregistering, %d headers, %u bytes header, %u bytes payload\n",
		 ch->channel_id, ch->header_id, ch->total_header_len,
		 ch->total_data_len);

	if (ch == session->channels)
		session->channels = ch->next;
	else {
		for (tmp = session->channels; tmp; tmp = tmp->next)
			if (tmp->next == ch)
				break;

		assert (tmp != NULL);

		tmp->next = ch->next;
	}

	if (ch->channel_id < session->next_channel_id)
		session->next_channel_id = ch->channel_id;

	free (ch);

	session->num_channels--;
}

CHANNEL *channel_by_id (sp_session *session, unsigned short channel_id)
{
	CHANNEL *ch;

	for (ch = session->channels; ch; ch = ch->next)
		if (ch->channel_id == channel_id)
			break;

	return ch;
}

int channel_process (sp_session *session, unsigned char *buf, unsigned short len, int error)
{
	CHANNEL *ch;
	unsigned short channel_id;
	int ret;
	unsigned char *ptr;
	unsigned short header_len, consumed_len;
	
	/* Extract channel ID */
	memcpy(&channel_id, buf, 2);
	channel_id = ntohs(channel_id);
	buf += 2;
	len -= 2;

	/* Find a matching channel */
	for (ch = session->channels; ch; ch = ch->next) {
		if (ch->channel_id == channel_id)
			break;
	}

	if (ch == NULL) {
		DSFYDEBUG
			("channel_process(): Failed to find channel for id=%u (error flag: %d), lost %u bytes data\n",
			 channel_id, error, len);

		return 0;
	}

	/*
	 * If we're in a error state, let the
	 * callback routine know about it
	 *
	 */
	if (error)
		ch->state = CHANNEL_ERROR;

	/*
	 * Handle header data
	 *
	 */
	if (ch->state == CHANNEL_HEADER) {
		assert (len >= 2);

		ptr = buf;
		consumed_len = 0;
		while (consumed_len < len) {
			/* Extract length of next data */
			memcpy(&header_len, ptr, 2);
			header_len = ntohs(header_len);

			ptr += 2;
			consumed_len += 2;

			if (header_len == 0)
				break;

			if (consumed_len + header_len > len) {
				DSFYDEBUG
					("not enough data! channel %d, header_len %d, len %d\n",
					 ch->channel_id, header_len, len);
				fhexdump8x32 (stderr, "payload:", ptr, len);
				return 0;
			}
			ch->header_id++;
			DSFYDEBUG
				("channel %d: Entering callback (header %d) for channel '%s', %d bytes data\n",
				 ch->channel_id, ch->header_id, ch->name,
				 header_len);
			ch->callback (ch, ptr, header_len);

			ptr += header_len;
			consumed_len += header_len;
			ch->total_header_len += header_len;
		}

		assert (consumed_len == len);

		/* Upgrade state if this was the last (zero size) header */
		if (header_len == 0)
			ch->state = CHANNEL_DATA;

		return 0;
	}

	/*
	 * Now we're either in the CHANNEL_DATA or CHANNEL_ERROR state
	 * If in CHANNEL_DATA, and length is zero, switch to CHANNEL_END,
	 * thus letting the callback routine know this is the last packet
	 *
	 */
	if (len == 0)
		ch->state = CHANNEL_END;

	DSFYDEBUG
		("channel %d: Entering callback (state: %s) for channel '%s', %d bytes data\n",
		 ch->channel_id,
		 ch->state == CHANNEL_DATA ? "data" : ch->state ==
		 CHANNEL_ERROR ? "error" : "end", ch->name, len);
	ret = ch->callback (ch, buf, len);
	ch->total_data_len += len;

	/* Deallocate channel if in END or ERROR state */
	if (ch->state & (CHANNEL_END | CHANNEL_ERROR))
		channel_unregister (session, ch);

	return ret;
}


void channel_fail_and_unregister_all(sp_session *session) {
	CHANNEL *ch;

	while((ch = session->channels) != NULL) {
		DSFYDEBUG
			("channel %d: Forcing failure via callback (current state: %s) for channel '%s'\n",
			 ch->channel_id,
			 ch->state == CHANNEL_HEADER ? "header":
			 ch->state == CHANNEL_DATA ? "data":
			 ch->state == CHANNEL_ERROR ? "error": "end", ch->name);

		ch->state = CHANNEL_ERROR;
		ch->callback(ch, NULL, 0);

		channel_unregister(session, ch);
	}
}
