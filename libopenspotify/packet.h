/*
 * $Id: packet.h 182 2009-03-12 08:21:53Z zagor $
 *
 */

#ifndef DESPOTIFY_PACKET_H
#define DESPOTIFY_PACKET_H

#include "sp_opaque.h"

#ifdef _MSC_VER
#	pragma pack( push, packing )
#	pragma pack( 1 )
#	define PACK_STRUCT
#elif defined( __GNUC__ )
#	define PACK_STRUCT	__attribute__((packed))
#else
#error "Unsupported compiler"
#endif

/*
 * Packet header
 *
 */

struct packet_header
{
	unsigned char cmd;
	unsigned short len;
} PACK_STRUCT;
typedef struct packet_header PHEADER;


int packet_read_and_process(sp_session *session);
int packet_write (sp_session *, unsigned char, unsigned char *, unsigned short);
#endif
