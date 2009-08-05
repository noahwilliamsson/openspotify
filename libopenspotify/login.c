/*
 * Handle login to Spotify's service
 *
 * login_create() creates a new login context
 * login_process() iterates over the steps involved in logging in and returns -1
 * for errors (and sets ->error), returns 0 to indicate it needs to be called
 * again and finally returns 1 on success.
 * login_export_session() exports the socket and shannon keys
 *
 *
 */

#ifdef _WIN32
#include <ws2tcpip.h>
#include <winsock2.h>
#else
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <fcntl.h>
#endif
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <openssl/dh.h>
#include <openssl/rsa.h>
#include <openssl/rand.h>
#include <spotify/api.h>

#include "buf.h"
#include "debug.h"
#include "dns.h"
#include "hmac.h"
#include "login.h"
#include "sha1.h"
#include "util.h"

#define SPOTIFY_SRV_HOSTNAME	"_spotify-client._tcp.spotify.com"


static int send_client_parameters(struct login_ctx *l);
static int receive_server_parameters(struct login_ctx *l);
static void key_init(struct login_ctx *l);
static void auth_generate_auth_hash(struct login_ctx *l);
static void auth_generate_auth_hmac(struct login_ctx *l);
static void puzzle_solve (struct login_ctx *l);
static int send_client_auth_packet(struct login_ctx *l);
static int receive_server_auth_response(struct login_ctx *l);


static unsigned char DH_generator[1] = { 2 };
static unsigned char DH_prime[] = {
        /* Well-known Group 1, 768-bit prime */
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc9,
        0x0f, 0xda, 0xa2, 0x21, 0x68, 0xc2, 0x34, 0xc4, 0xc6,
        0x62, 0x8b, 0x80, 0xdc, 0x1c, 0xd1, 0x29, 0x02, 0x4e,
        0x08, 0x8a, 0x67, 0xcc, 0x74, 0x02, 0x0b, 0xbe, 0xa6,
        0x3b, 0x13, 0x9b, 0x22, 0x51, 0x4a, 0x08, 0x79, 0x8e,
        0x34, 0x04, 0xdd, 0xef, 0x95, 0x19, 0xb3, 0xcd, 0x3a,
        0x43, 0x1b, 0x30, 0x2b, 0x0a, 0x6d, 0xf2, 0x5f, 0x14,
        0x37, 0x4f, 0xe1, 0x35, 0x6d, 0x6d, 0x51, 0xc2, 0x45,
        0xe4, 0x85, 0xb5, 0x76, 0x62, 0x5e, 0x7e, 0xc6, 0xf4,
        0x4c, 0x42, 0xe9, 0xa6, 0x3a, 0x36, 0x20, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};


struct login_ctx *login_create(char *username, char *password) {
	struct login_ctx *l;

	l = malloc(sizeof(struct login_ctx));
	if(l == NULL)
		return NULL;

	l->state = 0;

	l->error = SP_LOGIN_ERROR_OK;

	l->sock = -1;
	l->service_records = NULL;
	l->server_ai = NULL;

	l->client_parameters = NULL;
	l->server_parameters = NULL;

	/* Client key pair */
	l->rsa = RSA_generate_key(1024, 65537, NULL, NULL);

	/* Diffie-Hellman parameters */
	l->dh = DH_new ();
	l->dh->p = BN_bin2bn(DH_prime, 96, NULL);
	l->dh->g = BN_bin2bn(DH_generator, 1, NULL);
	DH_generate_key(l->dh);

        strncpy(l->username, username, sizeof(l->username) - 1);
        l->username[sizeof(l->username) - 1] = 0;

        strncpy(l->password, password, sizeof(l->password) - 1);
        l->password[sizeof(l->password) - 1] = 0;


	DSFYDEBUG("Login context created at %p\n", l);

	return l;
}


/* Free a login context */
void login_release(struct login_ctx *l) {

	/* Only close the socket if an error occurred */
	if(l->error != SP_LOGIN_ERROR_OK && l->sock != -1) {
		DSFYDEBUG("Closing open socket %d, login error was %d\n",
			l->sock, l->error);
#ifdef _WIN32
		closesocket(l->sock);
#else
		close(l->sock);
#endif
	}

	if(l->service_records)
		dns_free_list(l->service_records);

	if(l->server_ai != NULL)
		freeaddrinfo(l->server_ai);

	RSA_free(l->rsa);
	DH_free(l->dh);

	if(l->client_parameters)
		buf_free(l->client_parameters);

	if(l->server_parameters)
		buf_free(l->server_parameters);


	free(l);

	DSFYDEBUG("Done free'ing login context\n");
}


int login_process(struct login_ctx *l) {
	int connect_error, i;
	int ret = 0;
	socklen_t len;
	struct dns_srv_records *svc;
	struct addrinfo h, *ai;
	fd_set wfds;
	struct timeval tv;

	switch(l->state) {

	case 0:
		/* Lookup service records in DNS */
		if(l->service_records)
			dns_free_list(l->service_records);

		l->service_records = dns_get_service_list(SPOTIFY_SRV_HOSTNAME);
		if(l->service_records == NULL) {
			l->error = SP_LOGIN_ERROR_DNS_FAILURE;
			DSFYDEBUG("Failed to lookup Spotify service in DNS\n");
			return -1;
		}

		l->state++;
		break;

	case 1:
		/* Pick a host in the list we have not yet tried */
		for(svc = l->service_records; svc; svc = svc->next)
			if(svc->tried == 0)
				break;

		if(svc == NULL) {
			l->state = 0;
			l->error = SP_LOGIN_ERROR_NO_MORE_SERVERS;
			DSFYDEBUG("Run out of hostnames in SRV record list\n");
			return -1;
		}


		/* Lookup available address records (IPv4/IPv6) for the host */
		l->server_ai_skip = 0;
		l->server_ai_wait = 0;
		if(l->server_ai) {
			freeaddrinfo(l->server_ai);
			l->server_ai = NULL;
		}

		memset(&h, 0, sizeof(h));
		h.ai_family = PF_UNSPEC;
		h.ai_socktype = SOCK_STREAM;
		h.ai_protocol = IPPROTO_TCP;
		ret = getaddrinfo(svc->host, svc->port, &h, &l->server_ai);
		if(ret < 0) {
			l->state = 0;
			l->error = SP_LOGIN_ERROR_DNS_FAILURE;
			DSFYDEBUG("Failed to lookup addresses for %s:%s\n", svc->host, svc->port);
			return -1;
		}

		DSFYDEBUG("Will connect to host %s:%s\n", svc->host, svc->port);
		l->state++;
		break;

	case 2:
		/* Pick a suitable address we have not yet tried to connect to */
		i = 0;
		for(ai = l->server_ai; ai; ai = ai->ai_next) {
			if(i++ != l->server_ai_skip)
				continue;

			l->server_ai_skip++;
			if(ai->ai_family != AF_INET && ai->ai_family != AF_INET6)
				continue;

			break;
		}

		if(ai == NULL) {
			/* Out of addresses for this server, try next server */
			DSFYDEBUG("Run out of addresses for this host\n");
			l->state--;
			break;
		}


		if(l->sock != -1) {
			DSFYDEBUG("Closing already open socket %d\n", l->sock);
#ifdef _WIN32
			closesocket(l->sock);
#else
			close(l->sock);
#endif
		}


		l->sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if(l->sock < 0) {
			l->state = 0;
			l->error = SP_LOGIN_ERROR_SOCKET_ERROR;
			return -1;
		}


		DSFYDEBUG("Initiating connect()\n");
#ifdef _WIN32
		i = 1;
		if(ioctlsocket(l->sock, FIONBIO, &i) < 0 
			|| (connect(l->sock, (struct sockaddr *)ai->ai_addr, ai->ai_addrlen) < 0 && WSAGetLastError() != WSAEWOULDBLOCK)) {
#else
		if((i = fcntl(l->sock, F_GETFL, 0)) < 0
			|| fcntl(l->sock, F_SETFL, i | O_NONBLOCK) < 0
			|| (connect(l->sock, (struct sockaddr *)ai->ai_addr, ai->ai_addrlen) < 0 && errno != EINPROGRESS)) {
#endif

			/* XXX - We'll just try with the next server */
			DSFYDEBUG("fcntl() or connect() failed with errno %d, will retry with next server\n", errno);
			l->state--;
			break;
		}

		l->state++;
		break;

	case 3:
		FD_ZERO(&wfds);
		FD_SET(l->sock, &wfds);

#define WAIT_MS	200
#define MAX_WAIT_MS 3000
		tv.tv_sec = 0;
		tv.tv_usec = WAIT_MS * 1000;
		l->server_ai_wait += WAIT_MS;

		ret = select(l->sock + 1, NULL, &wfds, NULL, &tv);
		if(ret < 0) {
			l->state = 0;
			l->error = SP_LOGIN_ERROR_SOCKET_ERROR;

			break;
		}
		else if(ret == 0) {
			if(l->server_ai_wait >= MAX_WAIT_MS) {
				DSFYDEBUG("Connection timeout out (waited %dms), retrying with next server in list\n", l->server_ai_wait);
				/* Connect timeout */
#ifdef _WIN32
				closesocket(l->sock);
#else
				close(l->sock);
#endif
				l->sock = -1;

				/* Retry with next server */
				l->state--;
			}

			break;
		}


		len = sizeof(connect_error);
		if((ret = getsockopt(l->sock, SOL_SOCKET, SO_ERROR, (char *)&connect_error, &len)) < 0) {
			l->state = 0;
			l->error = SP_LOGIN_ERROR_SOCKET_ERROR;
			break;
		}
		else if(connect_error != 0) {
			DSFYDEBUG("Connection failed with error %d, retrying with next server in list\n", connect_error);
#ifdef _WIN32
			closesocket(l->sock);
#else
			close(l->sock);
#endif
			l->sock = -1;

			/* Retry with next server */
			l->state--;
			break;
		}

		DSFYDEBUG("Connected to server\n");
		l->state++;
		break;

	case 4:
		ret = send_client_parameters(l);
		if(ret < 0)
			l->state = 0;
		else
			l->state++;

		DSFYDEBUG("Initial packet sent, return value was %d, login error is %d\n", ret, l->error);
		break;

	case 5:
		/* Receive server parameters and eventually compute session key */
		ret = receive_server_parameters(l);
		DSFYDEBUG("Recieved initial packet, return value was %d, login error is %d\n", ret, l->error);
		if(ret < 0)
			l->state = 0;
		else
			l->state++;

		break;

	case 6:
		/* Compute a session key and authenticate the client */
		auth_generate_auth_hash(l);
		key_init(l);

		/* Solve the puzzle, might take some time.. */
		puzzle_solve(l);

		/*
	         * Compute HMAC over the initial packets, a byte representing
	         * the length of the random data, an unknown byte, two bytes
	         * representing the length of the puzzle (value 0x0008), 
	         * four zero bytes, the random data (if any), and finally the
	         * puzzle solution
		 *
		 * Key is part of a digest computed in key_init()
		 *
		 */
		auth_generate_auth_hmac(l);

		l->state++;
		break;

	case 7:
		/* Authenticate the client */
		ret = send_client_auth_packet(l);
		DSFYDEBUG("Sent auth packet, return value was %d, login error is %d\n", ret, l->error);
		if(ret < 0)
			l->state = 0;
		else
			l->state++;

		break;

	case 8:
		/* Read the server's authentication response */
		ret = receive_server_auth_response(l);
		DSFYDEBUG("Got auth response, return value was %d, login error is %d\n", ret, l->error);
		if(ret == 0)
			return 1;

		l->state = 0;
		l->error = -12;
		break;
	}

	return ret;
}


void login_export_session(struct login_ctx *login, int *sock, unsigned char *key_recv, unsigned char *key_send) {
	*sock = login->sock;
	memcpy(key_recv, login->key_recv, sizeof(login->key_recv));
	memcpy(key_send, login->key_send, sizeof(login->key_send));
}


/*
 * $Id: keyexchange.c 405 2009-07-29 16:05:30Z noah-w $
 *
 */

static int send_client_parameters(struct login_ctx *l) {
	int ret;
	unsigned char client_pub_key[96];
	unsigned char rsa_pub_exp[128];
	unsigned int len_idx;
	unsigned char bytevalue;

	struct buf* b = buf_new();

	buf_append_u16 (b, 3); /* protocol version */

	len_idx = b->len;
	buf_append_u16(b, 0); /* packet length - updated later */
	buf_append_u32(b, 0); /* unknown */
	buf_append_u32(b, 0x00030c00); /* unknown */
	buf_append_u32(b, 99999); /* revision */
	buf_append_u32(b, 0); /* unknown */
	buf_append_u32(b, 0x01000000); /* unknown */
	buf_append_data(b, "\x01\x04\x01\x01", 4); /* client ID */
	buf_append_u32(b, 0); /* unknown */

	/* Random bytes(?) */
	RAND_bytes(l->client_random_16, 16);
	buf_append_data (b, l->client_random_16, 16);


	BN_bn2bin (l->dh->pub_key, client_pub_key);
	buf_append_data (b, client_pub_key, sizeof(client_pub_key));

	BN_bn2bin (l->rsa->n, rsa_pub_exp);
	buf_append_data (b, rsa_pub_exp, sizeof(rsa_pub_exp));

	buf_append_u8 (b, 0); /* length of random data */
	bytevalue = strlen(l->username);
	buf_append_u8 (b, bytevalue);
	buf_append_u16(b, 0x0100); /* unknown */
        /* <-- random data would go here */
	DSFYDEBUG("Sending username '%s'\n", l->username);
	buf_append_data (b, (unsigned char *) l->username,
			   strlen(l->username));
	buf_append_u8 (b, 0x40); /* unknown */

	/*
	 * Update length bytes
	 *
	 */
	b->ptr[len_idx] = (b->len >> 8) & 0xff;
	b->ptr[len_idx+1] = b->len & 0xff;


        ret = send(l->sock, b->ptr, b->len, 0);
	if (ret <= 0) {
		DSFYDEBUG("connection lost\n");
		buf_free(b);
		l->error = SP_LOGIN_ERROR_SOCKET_ERROR;
		return -1;
	}
	else if (ret != b->len) {
                DSFYDEBUG("only wrote %d of %d bytes\n", ret, b->len);
		buf_free(b);
		l->error = SP_LOGIN_ERROR_SOCKET_ERROR;
		return -1;
	}

        /* save initial server packet for auth hmac generation */
        l->client_parameters = b;

	return 0;
}


static int receive_server_parameters(struct login_ctx *l) {
	char buf[512];
	unsigned char padlen, username_len;
        unsigned short chalen[4];
	int ret;
        struct buf* save = buf_new();

        /* read 2 status bytes */
        ret = block_read(l->sock, l->server_random_16, 2);
	if(ret < 2) {
       		DSFYDEBUG("Failed to read status bytes, return value was %d, errno is %d\n", ret, errno);
		l->error = SP_LOGIN_ERROR_SOCKET_ERROR;
		return -1;
	}

        if (l->server_random_16[0] != 0) {
		DSFYDEBUG("Bad response: %#02x %#02x\n",
				l->server_random_16[0], l->server_random_16[1]);
		switch (l->server_random_16[1]) {
                case 1: /* client upgrade required */
			l->error = SP_LOGIN_ERROR_UPGRADE_REQUIRED;
			return -1;

                case 3: /* user not found */
			l->error = SP_LOGIN_ERROR_USER_NOT_FOUND;
                    	return -1;

                case 4: /* account has been disabled */
			l->error = SP_LOGIN_ERROR_USER_BANNED;
                    	return -1;

                case 6: /* you need to complete your account details */
			l->error = SP_LOGIN_ERROR_USER_NEED_TO_COMPLETE_DETAILS;
                    	return -1;

                case 9: /* country mismatch */
			l->error = SP_LOGIN_ERROR_USER_COUNTRY_MISMATCH;
                    	return -1;

                default: /* unknown error */
			l->error = SP_LOGIN_ERROR_OTHER_PERMANENT;
                    	return -1;
            }
        }


        /* read remaining 14 random bytes */
        ret = block_read(l->sock, l->server_random_16 + 2, 14);
	if(ret < 14) {
		DSFYDEBUG("Failed to read server random\n");
		l->error = SP_LOGIN_ERROR_SOCKET_ERROR;
		return -1;
	}
        buf_append_data(save, l->server_random_16, 16);


        /* read public key */
        ret = block_read(l->sock, l->remote_pub_key, 96);
	if (ret != 96) {
		DSFYDEBUG("Failed to read 'remote_pub_key'\n");
		l->error = SP_LOGIN_ERROR_SOCKET_ERROR;
		return -1;
	}
        buf_append_data(save, l->remote_pub_key, 96);


        /* read server blob */
        ret = block_read(l->sock, buf, 256);
	if (ret != 256) {
		DSFYDEBUG("Failed to read 'random_256', got %d of 256 bytes\n", ret);
		l->error = SP_LOGIN_ERROR_SOCKET_ERROR;
		return -1;
	}
        buf_append_data(save, buf, 256);


        /* read salt */
        ret = block_read(l->sock, l->salt, 10);
	if (ret != 10) {
		DSFYDEBUG("Failed to read 'salt'\n");
		l->error = SP_LOGIN_ERROR_SOCKET_ERROR;
		return -1;
	}
        buf_append_data(save, l->salt, 10);


        /* read padding length */
        ret = block_read(l->sock, &padlen, 1);
	if (ret != 1) {
		DSFYDEBUG("Failed to read 'padding length'\n");
		l->error = SP_LOGIN_ERROR_SOCKET_ERROR;
		return -1;
	}
	assert (padlen > 0);
        buf_append_u8(save, padlen);

        /* read username length */
        ret = block_read(l->sock, &username_len, 1);
	if (ret != 1) {
		DSFYDEBUG("Failed to read 'username_len'\n");
		l->error = SP_LOGIN_ERROR_SOCKET_ERROR;
		return -1;
	}
        buf_append_u8(save, username_len);


        /* read challenge lengths */
        ret = block_read(l->sock, chalen, 8);
	if (ret != 8) {
		DSFYDEBUG("Failed to read challenge lengths\n");
		l->error = SP_LOGIN_ERROR_SOCKET_ERROR;
		return -1;
	}
        buf_append_data(save, chalen, 8);


        /* read packet padding */
        ret = block_read(l->sock, buf, padlen);
	if (ret != padlen) {
		DSFYDEBUG("Failed to read 'padding'\n");
		l->error = SP_LOGIN_ERROR_SOCKET_ERROR;
		return -1;
	}
        buf_append_data(save, buf, padlen);


        /* read username */
        ret = block_read(l->sock,
                         l->username, username_len);
	if (ret != username_len) {
		DSFYDEBUG("Failed to read 'username'\n");
		l->error = SP_LOGIN_ERROR_SOCKET_ERROR;
		return -1;
	}

        buf_append_data(save, l->username, username_len);
	l->username[username_len] = 0;


        /* read puzzle challenge */
        {
            int puzzle_len = ntohs(chalen[0]);
            int len1 = ntohs(chalen[1]);
            int len2 = ntohs(chalen[2]);
            int len3 = ntohs(chalen[3]);
            int totlen = puzzle_len + len1 + len2 + len3;

            struct buf* b = buf_new();
            buf_extend(b, totlen);

		DSFYDEBUG("Reading a total of %d bytes puzzle challenge\n", totlen);
            ret = block_read(l->sock, b->ptr, totlen);
            if (ret != totlen) {
                DSFYDEBUG("Failed to read puzzle\n");
                buf_free(b);
		l->error = SP_LOGIN_ERROR_SOCKET_ERROR;
                return -1;
            }
            buf_append_data(save, b->ptr, totlen);


            if (b->ptr[0] == 1) {
		l->puzzle_denominator = b->ptr[1];
		l->puzzle_magic = ntohl( *((int*)(b->ptr + 2)));
            }
            else {
		DSFYDEBUG("Unexpected puzzle challenge with first byte 0x%02x\n", b->ptr[0]);
		hexdump8x32("receive_server_parameters, puzzle", b->ptr, totlen);
		l->error = SP_LOGIN_ERROR_OTHER_PERMANENT;
		buf_free(b);
		return -1;
            }

            buf_free(b);
        }

        l->server_parameters = save;

	return 0;
}

/*
 * Initialize common crypto keys used for communication
 *
 * This step takes place after the initial two packets
 * have been exchanged.
 *
 */
static void key_init(struct login_ctx *l) {
	BIGNUM *pub_key;
	unsigned char message[53];
	unsigned char hmac_output[20 * 5];
	unsigned char *ptr, *hmac_ptr;
	unsigned int mac_len;
	int i;


	/*
	 * Compute DH shared key
	 * It's used in the call to HMAC() below
	 *
	 */
	pub_key = BN_bin2bn(l->remote_pub_key, 96, NULL);
	if((i = DH_compute_key(l->shared_key, pub_key, l->dh)) < 0) {
		/* XXX */
		return;
	}

#ifdef DEBUG_LOGIN
	hexdump8x32 ("key_init, my private key", l->my_priv_key, 96);
	hexdump8x32 ("key_init, my public key", l->client_pub_key, 96);
	hexdump8x32 ("key_init, remote public key", l->remote_pub_key,
		     96);
	hexdump8x32 ("key_init, shared key", l->shared_key, 96);
#endif
        BN_free(pub_key);



	/*
	 * Prepare a message to authenticate.
	 *
	 * Prior to the 19th of December 2008 Spotify happily told clients 
	 * (including ours!) almost everything it knew about a particular
	 * user, if they asked for it.
	 *
	 * Legitimate requests for this is for example when you add
	 * someone else's shared playlist.
	 *
	 * This allowed clients to see not only the last four digits of the 
	 * credit card used to subscribe to the premium service, whether
	 * the user was a paying customer or preferred commercials, but 
	 * also very interesting stuff such as the hash computed from
	 * SHA(salt || " " || password).
	 *
	 * In theory (HE HE!) this allowed any registered user to request
	 * somebody else's user data, get ahold of the hash, and then use
	 * it to authenticate as that user.
	 *
	 * Fortunately, at lest for Spotify and it's users, this is not
	 * the case anymore. (R.I.P poor misfeature)
	 *
	 * However, we urge people to change their passwords for reasons
	 * left as an exercise for the reader to figure out.
	 *
	 */
	ptr = message;
	memcpy (ptr, l->auth_hash, sizeof (l->auth_hash));
	ptr += sizeof (l->auth_hash);

	memcpy (ptr, l->client_random_16, 16);
	ptr += 16;

	memcpy (ptr, l->server_random_16, 16);
	ptr += 16;

	/*
	 * Run HMAC over the message, using the DH shared key as key
	 *
	 */
	hmac_ptr = hmac_output;
	mac_len = 20;
	for (i = 1; i <= 5; i++) {
		/*
		 * Change last byte of message to authenticate
		 *
		 */
		*ptr = i;

#ifdef DEBUG_LOGIN
		hexdump8x32 ("key_init, HMAC message", message,
			     sizeof (message));
#endif

	        sha1_hmac(l->shared_key, 96, message,
			  sizeof (message), hmac_ptr);

		/*
		 * Overwrite the 20 first bytes of the message with output from this round
		 *
		 */
		memcpy (message, hmac_ptr, 20);
		hmac_ptr += 20;
	}

	/*
	 * Use computed HMAC to setup keys for the
	 * stream cipher
	 *
	 */
	memcpy (l->key_send, hmac_output + 20, 32);
	memcpy (l->key_recv, hmac_output + 52, 32);


	/*
	 * The first 20 bytes of the HMAC output is used
	 * to key another HMAC computed for the second
	 * authentication packet sent by the client.
	 *
	 */
	memcpy (l->key_hmac, hmac_output, 20);

#ifdef DEBUG_LOGIN
	hexdump8x32 ("key_init, key_hmac", l->key_hmac, 20);
	hexdump8x32 ("key_init, key_send", l->key_send, 32);
	hexdump8x32 ("key_init, key_recv", l->key_recv, 32);
#endif
}



/*
 * $Id: auth.c 399 2009-07-29 11:50:46Z noah-w $
 *
 * Code for dealing with authentication against
 * the server.
 *
 * Used after exchanging the two first packets to
 * exchange the next two packets.
 *
 */

static void auth_generate_auth_hash(struct login_ctx *l) {
	unsigned char space = ' ';
	SHA1_CTX ctx;

	SHA1Init(&ctx);

	SHA1Update(&ctx, l->salt, 10);
	SHA1Update(&ctx, &space, 1);
	SHA1Update(&ctx, (unsigned char *)l->password, strlen(l->password));

	SHA1Final(l->auth_hash, &ctx);

#ifdef DEBUG_LOGIN
	hexdump8x32("auth_generate_auth_hash, auth_hash", l->auth_hash, 20);
#endif
}


static void auth_generate_auth_hmac(struct login_ctx *l) {
        struct buf* buf = buf_new();
	
	buf_append_data(buf, l->client_parameters->ptr,
                        l->client_parameters->len);
	buf_append_data(buf,  l->server_parameters->ptr,
                        l->server_parameters->len);
        buf_append_u8(buf, 0); /* random data length */
        buf_append_u8(buf, 0); /* unknown */
        buf_append_u16(buf, 8); /* puzzle solution length */
        buf_append_u32(buf, 0); /* unknown */
        /* <-- random data would go here */
        buf_append_data(buf, l->puzzle_solution, 8);

#ifdef DEBUG_LOGIN
	hexdump8x32 ("auth_generate_auth_hmac, HMAC message", buf->ptr,
		     buf->len);
	hexdump8x32 ("auth_generate_auth_hmac, HMAC key", l->key_hmac,
		     sizeof (l->key_hmac));
#endif

	sha1_hmac(l->key_hmac, sizeof(l->key_hmac),
		    buf->ptr, buf->len, l->auth_hmac);

#ifdef DEBUG_LOGIN
	hexdump8x32 ("auth_generate_auth_hmac, HMAC digest", l->auth_hmac,
		     sizeof(l->auth_hmac));
#endif

	buf_free(buf);
}


static int send_client_auth_packet(struct login_ctx *l) {
	int ret;
        struct buf* buf = buf_new();

	buf_append_data(buf, l->auth_hmac, 20);
        buf_append_u8(buf, 0); /* random data length */
        buf_append_u8(buf, 0); /* unknown */
        buf_append_u16(buf, 8); /* puzzle solution length */
        buf_append_u32(buf, 0);
        /* <-- random data would go here */
	buf_append_data (buf, l->puzzle_solution, 8);

#ifdef DEBUG_LOGIN
	hexdump8x32("send_client_auth_packet, second client packet", buf->ptr,
		     buf->len);
#endif

        ret = send(l->sock, buf->ptr, buf->len, 0);
	if (ret <= 0) {
		DSFYDEBUG("Connection was reset\n");
		buf_free(buf);
		l->error = SP_LOGIN_ERROR_SOCKET_ERROR;
		return -1;
	}
	else if (ret != buf->len) {
		DSFYDEBUG("Only wrote %d of %d bytes\n",
			ret, buf->len);
		buf_free(buf);
		l->error = SP_LOGIN_ERROR_SOCKET_ERROR;
		return -1;
	}

	buf_free(buf);
	
	return 0;
}


static int receive_server_auth_response(struct login_ctx *l) {
	unsigned char buf[256];
	unsigned char payload_len;
	int ret;

        ret = block_read(l->sock, buf, 2);
	if (ret != 2) {
		DSFYDEBUG("Failed to read 'status' + length byte, got %d bytes\n", ret);
		l->error = SP_LOGIN_ERROR_SOCKET_ERROR;
		return -1;
	}

	if (buf[0] != 0x00) {
		DSFYDEBUG("Authentication failed with error 0x%02x, bad password?\n", buf[1]);
		l->error = SP_LOGIN_ERROR_BAD_PASSWORD;
		return -1;
	}

	/* Payload length + this byte must not be zero(?) */
	assert (buf[1] > 0);

	payload_len = buf[1];

        ret = block_read (l->sock, buf, payload_len);
	if (ret != payload_len) {
		DSFYDEBUG("Failed to read 'payload', got %d of %u bytes\n",
			ret, payload_len);
		l->error = SP_LOGIN_ERROR_SOCKET_ERROR;
       		return -1;
	}
#ifdef DEBUG_LOGIN
	hexdump8x32("receive_server_auth_response, payload", buf, payload_len);
#endif

	return 0;
}



/*
 * $Id: puzzle.c 291 2009-04-08 14:22:33Z zagor $
 *
 * Zero-modulus bruteforce puzzle to prevent 
 * Denial of Service and password bruteforce attacks
 *
 */


#if !defined srandom || !defined random
#define srandom srand
#define random rand
#endif

static void puzzle_solve (struct login_ctx *l) {
	SHA1_CTX ctx;
	unsigned char digest[20];
	unsigned int *nominator_from_hash;
	unsigned int denominator;
	unsigned int seed;
	int i;

	/*
	 * Modulus operation by a power of two.
	 * "Most programmers learn this trick early"
	 * Well, fuck me. I'm just here for the party.
	 *
	 */
	denominator = 1 << l->puzzle_denominator;
	denominator--;

	/*
	 * Compute a hash over random data until
	 * (last dword byteswapped XOR magic number) mod
	 * denominator by server produces zero.
	 *
	 */

#ifdef _WIN32
	seed = GetTickCount() ^ (GetTickCount() << 9);
#else
	seed = time(NULL) ^ (time(NULL) << 9);
#endif
	srandom(seed);
	nominator_from_hash = (unsigned int *) (digest + 16);
	do {
		SHA1Init (&ctx);
		SHA1Update (&ctx, l->server_random_16, 16);

		/* Let's waste some precious pseudorandomness */
		for (i = 0; i < 8; i++)
			l->puzzle_solution[i] = random ();
		SHA1Update (&ctx, l->puzzle_solution, 8);

		SHA1Final (digest, &ctx);

		/* byteswap (XXX - htonl() won't work on bigendian machines!) */
		*nominator_from_hash = htonl (*nominator_from_hash);

		/* XOR with a fancy magic */
		*nominator_from_hash ^= l->puzzle_magic;
	} while (*nominator_from_hash & denominator);

#ifdef DEBUG_LOGIN
	hexdump8x32 ("auth_solve_puzzle, puzzle_solution", l->puzzle_solution, 8);
#endif
}
