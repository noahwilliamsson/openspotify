/*
 * $Id: dns.h 182 2009-03-12 08:21:53Z zagor $
 *
 */

#ifndef DESPOTIFY_DNS_H
#define DESPOTIFY_DNS_H

struct dns_srv_records {
	char *host;
	char *port;
	int prio;
	int tried;

	struct dns_srv_records *next;
};

struct dns_srv_records *dns_get_service_list(char *);
void dns_free_list(struct dns_srv_records *);

#endif
