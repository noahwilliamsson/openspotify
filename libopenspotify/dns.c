/*
 * $Id: dns.c 182 2009-03-12 08:21:53Z zagor $
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#ifdef _WIN32
#include <windows.h>
#include <windns.h>
#else
#include <arpa/nameser.h>
#include <arpa/nameser_compat.h>
#include <resolv.h>
#endif

#include "dns.h"

static int initialized;
static struct dns_srv_records *list_insert_by_prio(struct dns_srv_records **, int);

struct dns_srv_records *dns_get_service_list(char *hostname) {
	struct dns_srv_records *root = NULL, *entry;
	
#ifdef _WIN32
	PDNS_RECORD pRoot = NULL, p;

	if(DnsQuery_A(hostname, DNS_TYPE_SRV, DNS_QUERY_STANDARD, NULL, &pRoot, NULL) != 0)
		return NULL;

	for(p = pRoot; p != NULL; p = p->pNext) {
		if(p->wType != DNS_TYPE_SRV)
			continue;

		entry = list_insert_by_prio(&root, p->Data.SRV.wPriority);

		entry->host = strdup(p->Data.SRV.pNameTarget);
		entry->port = malloc(6);
		sprintf(entry->port, "%u", p->Data.SRV.wPort);
		entry->prio = p->Data.SRV.wPriority;
		entry->tried = 0;
	}

	DnsRecordListFree(pRoot, DnsFreeRecordListDeep);
#else
	int alen, hlen;
	char host[1024];
	unsigned char answer[1024], *p;
	unsigned short atype, prio, weight, port;
	HEADER *h = (HEADER *) answer;


	if (!initialized++)
		res_init ();
	if ((alen =
	     res_search (hostname, ns_c_in, ns_t_srv, answer,
			 sizeof (answer))) < 0)
		return NULL;

	p = answer + NS_HFIXEDSZ;
	h->qdcount = ntohs (h->qdcount);
	h->ancount = ntohs (h->ancount);
	while (h->qdcount--) {
		if ((hlen = dn_skipname (p, answer + alen)) < 0)
			return NULL;
		p += hlen + NS_QFIXEDSZ;
	}

	while (h->ancount--) {
		if ((hlen =
		     dn_expand (answer, answer + alen, p, host,
				sizeof (host))) < 0)
			break;

		p += hlen;
		GETSHORT (atype, p);

		p += 6;
		GETSHORT (hlen, p);
		if (atype != ns_t_srv) {
			p += hlen;
			continue;
		}

		GETSHORT (prio, p);
		GETSHORT (weight, p);
		GETSHORT (port, p);
		if ((hlen =
		     dn_expand (answer, answer + alen, p, host,
				sizeof (host))) < 0)
			break;

		p += hlen;


		entry = list_insert_by_prio(&root, prio);
		entry->host = strdup(host);
		entry->port = malloc(6);
		sprintf(entry->port, "%u", port);
		entry->prio = prio;
		entry->tried = 0;
	}
    #endif

	return root;
}


static struct dns_srv_records *list_insert_by_prio(struct dns_srv_records **root, int prio) {
	struct dns_srv_records *entry, *walker;


	if((walker = *root) == NULL) {
		entry = (struct dns_srv_records *)malloc(sizeof(struct dns_srv_records));
		entry->next = NULL;
		*root = entry;
	}
	else if(prio <= walker->prio) {
		entry = (struct dns_srv_records *)malloc(sizeof(struct dns_srv_records));
		entry->next = *root;
		*root = entry;
	}
	else {
		while(walker->next && prio > walker->next->prio)
			walker = walker->next;

		entry = (struct dns_srv_records *)malloc(sizeof(struct dns_srv_records));
		if(walker->next)
			entry->next = walker->next->next;
		else
			entry->next = NULL;

		walker->next = entry;
	}

	return entry;
}


void dns_free_list(struct dns_srv_records *entry) {
	struct dns_srv_records *saved;

	for(; entry; entry = saved) {
		free(entry->host);
		free(entry->port);
		saved = entry->next;
		free(entry);
	}
}
