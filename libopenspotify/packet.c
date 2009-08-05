/*
 * Read and process packets and provide a routine for writing packets
 *
 */

#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <unistd.h>
#include <sys/select.h>
#include <time.h>
#endif
#include <assert.h>

#include "debug.h"
#include "handlers.h"
#include "packet.h"
#include "sp_opaque.h"
#include "util.h"


int packet_read_and_process(sp_session *session) {
	fd_set rfds;
	struct timeval tv;
	int ret;
	struct buf *packet;
	PHEADER header;
	unsigned char nonce[4];


	if(session->packet == NULL)
		session->packet = buf_new();
	else if(session->packet->len == session->packet->size)
		buf_extend(session->packet, session->packet->size);


	FD_ZERO(&rfds);
	FD_SET(session->sock, &rfds);
	
	tv.tv_sec = 0;
	tv.tv_usec = 64 * 1000;

	ret = select(session->sock + 1, &rfds, NULL, NULL, &tv);
	if(ret <= 0)
		return ret;


	ret = recv(session->sock,
			session->packet->ptr + session->packet->len, 
			session->packet->size - session->packet->len, 0);
	if(ret <= 0)
		return -1;


	/* We need a complete packet header of three bytes */
	session->packet->len += ret;
	while(session->packet->len >= 3) {

		/* Set nonce for Shannon */
		*(unsigned int *)nonce = htonl(session->key_recv_IV);
		shn_nonce(&session->shn_recv, nonce, 4);


		/* Decrypt the packet header */
		memcpy(&header, session->packet->ptr, 3);
		shn_decrypt(&session->shn_recv, (unsigned char *)&header, 3);


		/* Make sure we have the entire payload aswell as the MAC */
		header.len = ntohs(header.len);
		DSFYDEBUG("%d bytes buffered, header.cmd=0x%02x, header.len=%d\n",
			session->packet->len, header.cmd, header.len);
		if(session->packet->len < 3 + header.len + 4)
			break;


		/* Extract the full packet, leaving eventual additional data as is */
		packet = buf_consume(session->packet, 3 + header.len + 4);


		/* Copy back the decrypted header and decrypt the payload */
		memcpy(packet->ptr, &header, 3);
		shn_decrypt(&session->shn_recv, packet->ptr + 3, header.len);


		/* Increment receiving IV */
		session->key_recv_IV++;


		if(handle_packet(session, header.cmd, packet->ptr + 3, header.len)) {
			DSFYDEBUG("handle_packet() failed with an error\n");
			return -1;
		}
	}


	return 0;
}


int packet_write (sp_session * session, unsigned char cmd,
		  unsigned char *payload, unsigned short len)
{
	unsigned char nonce[4];
	unsigned char *buf, *ptr;
	PHEADER *h;
	int ret;

	*(unsigned int *) nonce = htonl (session->key_send_IV);
	shn_nonce (&session->shn_send, nonce, 4);

	buf = (unsigned char *) malloc (3 + len + 4);

	h = (PHEADER *) buf;
	h->cmd = cmd;
	h->len = htons (len);

	ptr = buf + 3;
	if (payload != NULL)
	{
 		memcpy (ptr, payload, len);
	}
	
	DSFYDEBUG("Sending packet with command 0x%02x, length %d, IV=%d\n",
		 h->cmd, ntohs (h->len), session->key_send_IV);

	shn_encrypt (&session->shn_send, buf, 3 + len);
	ptr += len;

	shn_finish (&session->shn_send, ptr, 4);

	ret = block_write (session->sock, buf, 3 + len + 4);

	free(buf);

	session->key_send_IV++;


	if(ret != 3 + len + 4) {
		DSFYDEBUG ("block_write() only wrote %d of %d bytes\n", ret,
			   3 + len + 4);
		return -1;
	}

	return 0;
}
