/*
 * $Id: commands.h 289 2009-03-30 16:31:58Z dstien $
 *
 */

#ifndef DESPOTIFY_COMMANDS_H
#define DESPOTIFY_COMMANDS_H

#include "channel.h"
#include "sp_opaque.h"

/* Core functionality */
#define CMD_SECRETBLK	0x02
#define CMD_PING	0x04
#define CMD_CHANNELDATA	0x09
#define CMD_CHANNELERR	0x0a
#define CMD_CHANNELABRT	0x0b
#define CMD_REQKEY	0x0c
#define CMD_AESKEY	0x0d
#define CMD_SHAHASH     0x10
#define CMD_IMAGE	0x19
#define CMD_TOKENNOTIFY	0x4f

/* Rights management */
#define CMD_COUNTRYCODE	0x1b

/* P2P related */
#define CMD_P2P_SETUP	0x20
#define CMD_P2P_INITBLK	0x21

/* Search and meta data */
#define CMD_BROWSE		0x30
#define CMD_SEARCH		0x31
#define CMD_GETPLAYLIST		0x35
#define CMD_CHANGEPLAYLIST	0x36

/* Session management */
#define CMD_NOTIFY	0x42
#define CMD_LOG		0x48
#define CMD_PONG	0x49
#define CMD_PONGACK	0x4a
#define CMD_PAUSE	0x4b
#define CMD_REQUESTAD	0x4e
#define CMD_REQUESTPLAY	0x4f

/* Internal */
#define CMD_PRODINFO	0x50
#define CMD_WELCOME	0x69

/* browse types */
#define BROWSE_ARTIST  1
#define BROWSE_ALBUM   2
#define BROWSE_TRACK   3

/* special playlist revision */
#define PLAYLIST_CURRENT	~0

int cmd_send_cache_hash (sp_session *);
int cmd_token_notify (sp_session *);
int cmd_aeskey (sp_session *, unsigned char *, unsigned char *, channel_callback,
		void *);
int cmd_search (sp_session *, char *, unsigned int, unsigned int, channel_callback,
		void *);
int cmd_requestad (sp_session *, unsigned char);
int cmd_request_image (sp_session *, unsigned char *, channel_callback, void *);
int cmd_action (sp_session *, unsigned char *, unsigned char *);
int cmd_getsubstreams (sp_session *, unsigned char *, unsigned int, unsigned int,
		       unsigned int, channel_callback, void *);
int cmd_browse (sp_session *, unsigned char, unsigned char *, int,
		channel_callback, void *);
int cmd_getplaylist (sp_session *, unsigned char *, int, channel_callback,
		     void *);
int cmd_changeplaylist (sp_session *, unsigned char *, char*, int, int, int, int,
			channel_callback, void *);
int cmd_ping_reply (sp_session *);
#endif
