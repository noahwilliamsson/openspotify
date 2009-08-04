#ifndef LIBOPENSPOTIFY_LOGIN_H
#define LIBOPENSPOTIFY_LOGIN_H

#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netdb.h>
#endif

#include <openssl/dh.h>
#include <openssl/rsa.h>

#include "buf.h"
#include "dns.h"

enum sp_login_error {
	SP_LOGIN_ERROR_OK = 0,

	SP_LOGIN_ERROR_DNS_FAILURE,
	SP_LOGIN_ERROR_NO_MORE_SERVERS,
	SP_LOGIN_ERROR_SOCKET_ERROR,

	SP_LOGIN_ERROR_USER_NOT_FOUND = 20,
	SP_LOGIN_ERROR_USER_BANNED,
	SP_LOGIN_ERROR_BAD_PASSWORD,
	SP_LOGIN_ERROR_UPGRADE_REQUIRED,
	SP_LOGIN_ERROR_USER_NEED_TO_COMPLETE_DETAILS,
	SP_LOGIN_ERROR_USER_COUNTRY_MISMATCH,
	SP_LOGIN_ERROR_OTHER_PERMANENT,
};

struct login_ctx {
	int state;

	enum sp_login_error error;

	int sock;
        struct dns_srv_records *service_records;
        struct addrinfo *server_ai;
        int server_ai_skip;
        int server_ai_wait;
	
        char username[256];
        char password[256];

	/* Random bytes exchanges in initial packets */
        unsigned char client_random_16[16];
        unsigned char server_random_16[16];

	/* Server public key */
        unsigned char remote_pub_key[96];

	/* Client 1024-bit RSA key pair */
        RSA *rsa;

        /* For HMAC generation */
        struct buf *client_parameters;
        struct buf *server_parameters;

        /*
         * Computed as SHA(salt || " " || password)
         *
         * Knowing somebody else's hash allows for
         * impersonation.
         *
         * Spotify quit being overly informative
         * about people's hashes 2008-12-19.
         *
         */
        unsigned char salt[10];
        unsigned char auth_hash[20];

	/* Session key */
        DH *dh;
        unsigned char shared_key[96];

        /*
         * Output from HMAC/SHA1
         * 
         * Used for keying HMAC() in auth_generate_auth_hmac()
         * and for keying Shannon
         *
         */
	unsigned char auth_hmac[20];
        unsigned char key_hmac[20];
        unsigned char key_recv[32];
        unsigned char key_send[32];


        /*
         * Waste some CPU time while computing
         * a 32-bit value, that byteswapped and
         * XOR'ed with a magic, modulus
         * 2^puzzle_denominator becomes zero.
         *
         */
        unsigned char puzzle_denominator;
        unsigned char puzzle_solution[8];
        int puzzle_magic;
};

struct login_ctx *login_create(char *username, char *password);
void login_release(struct login_ctx *l);
int login_process(struct login_ctx *);
void login_export_session(struct login_ctx *l, int *sock, unsigned char *key_recv, unsigned char *key_send);

#endif
