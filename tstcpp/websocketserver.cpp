/*
 * libwebsockets-test-echo
 *
 * Copyright (C) 2010-2016 Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * The person who associated a work with this deed has dedicated
 * the work to the public domain by waiving all of his or her rights
 * to the work worldwide under copyright law, including all related
 * and neighboring rights, to the extent allowed by law. You can copy,
 * modify, distribute and perform the work, even for commercial purposes,
 * all without asking permission.
 *
 * The test apps are intended to be adapted for use in your code, which
 * may be proprietary.  So unlike the library itself, they are licensed
 * Public Domain.
 */

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <assert.h>
#include <signal.h>

#include "../lib/libwebsockets.h"


#ifndef _WIN32
#include <syslog.h>
#include <sys/time.h>
#include <unistd.h>
#else
#include "gettimeofday.h"
#include <process.h>
#endif

static volatile int force_exit = 0;
static int versa, state;

#define INSTALL_DATADIR ""
#define LOCAL_RESOURCE_PATH INSTALL_DATADIR"/libwebsockets-test-server"

#define MAX_ECHO_PAYLOAD 1024

struct per_session_data__echo {
	size_t rx, tx;
	unsigned char buf[LWS_PRE + MAX_ECHO_PAYLOAD];
	unsigned int len;
	unsigned int index;
	int final;
	int continuation;
	int binary;
};

//TODO:
//1.server fd
//2.clients fd,ip
//3.client request ws://xxx..xxx/xxx
static int
callback_echo(struct lws *wsi, enum lws_callback_reasons reason, void *user,
	      void *in, size_t len)
{
	struct per_session_data__echo *pss =
			(struct per_session_data__echo *)user;
	int n;

    if(reason != 31){
    lwsl_notice("callback_echo reason=%2d\n",reason);
    }
    char* msg="this is server msg";
	switch (reason) {

#ifndef LWS_NO_SERVER
    //client establish
    case LWS_CALLBACK_FILTER_NETWORK_CONNECTION:
        //lwsl_notice("net connect fd=%d\n",wsi->sock);
        break;
    case LWS_CALLBACK_WSI_CREATE:
        //lwsl_notice("create fd=%d\n",wsi->sock);
        break;
    case LWS_CALLBACK_SERVER_NEW_CLIENT_INSTANTIATED:
        //lwsl_notice("LWS_CALLBACK_SERVER_NEW_CLIENT_INSTANTIATED fd=%d\n",wsi->sock);
        break;
    case LWS_CALLBACK_ADD_POLL_FD:
        break;
    case LWS_CALLBACK_ESTABLISHED:
        break;
    //client close
    case LWS_CALLBACK_DEL_POLL_FD:
        break;
    case LWS_CALLBACK_WSI_DESTROY:
        break;
    //sever exit
    //LWS_CALLBACK_DEL_POLL_FD
    case LWS_CALLBACK_PROTOCOL_DESTROY:
        break;

	case LWS_CALLBACK_SERVER_WRITEABLE:
do_tx:
		memcpy(&pss->buf[LWS_PRE],msg, strlen(msg));
        pss->len=strlen(msg);

		n = LWS_WRITE_CONTINUATION;
		if (!pss->continuation) {
			if (pss->binary)
				n = LWS_WRITE_BINARY;
			else
				n = LWS_WRITE_TEXT;
			pss->continuation = 1;
		}
		if (!pss->final)
			n |= LWS_WRITE_NO_FIN;
		lwsl_info("+++ test-echo: writing %d, with final %d\n",
			  pss->len, pss->final);


		pss->tx += pss->len;
		n = lws_write(wsi, &pss->buf[LWS_PRE], pss->len, (lws_write_protocol)n);
		if (n < 0) {
			lwsl_err("ERROR %d writing to socket, hanging up\n", n);
			return 1;
		}
		if (n < (int)pss->len) {
			lwsl_err("Partial write\n");
			return -1;
		}
		pss->len = -1;
		if (pss->final)
			pss->continuation = 0;
		lws_rx_flow_control(wsi, 1);
		break;

	case LWS_CALLBACK_RECEIVE:
do_rx:
		pss->final = lws_is_final_fragment(wsi);
		pss->binary = lws_frame_is_binary(wsi);
		lwsl_info("+++ test-echo: RX len %d final %d, pss->len=%d\n",
			  len, pss->final, (int)pss->len);

		memcpy(&pss->buf[LWS_PRE], in, len);
		//assert((int)pss->len == -1);
		pss->len = (unsigned int)len;
		pss->rx += len;

		lws_rx_flow_control(wsi, 0);
		lws_callback_on_writable(wsi);
		break;
#endif

#ifndef LWS_NO_CLIENT
	/* when the callback is used for client operations --> */

	case LWS_CALLBACK_CLOSED:
	case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
		lwsl_notice("closed\n");
		state = 0;
		break;

	case LWS_CALLBACK_CLIENT_ESTABLISHED:
		lwsl_notice("Client has connected\n");
		pss->index = 0;
		pss->len = -1;
		state = 2;
		break;

	case LWS_CALLBACK_CLIENT_RECEIVE:
#ifndef LWS_NO_SERVER
		if (versa)
			goto do_rx;
#endif
		lwsl_notice("Client RX: %s", (char *)in);
		break;

	case LWS_CALLBACK_CLIENT_WRITEABLE:
#ifndef LWS_NO_SERVER
		if (versa) {
			if (pss->len != (unsigned int)-1)
				goto do_tx;
			break;
		}
#endif
		/* we will send our packet... */
		pss->len = sprintf((char *)&pss->buf[LWS_PRE],
				   "hello from libwebsockets-test-echo client pid %d index %d\n",
				   getpid(), pss->index++);
		lwsl_notice("Client TX: %s", &pss->buf[LWS_PRE]);
		n = lws_write(wsi, &pss->buf[LWS_PRE], pss->len, LWS_WRITE_TEXT);
		if (n < 0) {
			lwsl_err("ERROR %d writing to socket, hanging up\n", n);
			return -1;
		}
		if (n < (int)pss->len) {
			lwsl_err("Partial write\n");
			return -1;
		}
		break;
#endif
	case LWS_CALLBACK_CLIENT_CONFIRM_EXTENSION_SUPPORTED:
		/* reject everything else except permessage-deflate */
		if (strcmp((char*)in, "permessage-deflate"))
			return 1;
		break;

	default:
		break;
	}

	return 0;
}



static struct lws_protocols protocols[] = {
	/* first protocol must always be HTTP handler */

	{
		"",		/* name - can be overriden with -e */
		callback_echo,
		sizeof(struct per_session_data__echo),	/* per_session_data_size */
		MAX_ECHO_PAYLOAD,
	},
	{
		NULL, NULL, 0		/* End of list */
	}
};


void sighandler(int sig)
{
	force_exit = 1;
}


int main(int argc, char **argv)
{
	int n = 0;
	int port = 7681;
	int use_ssl = 0;
	struct lws_context *context;
	int opts = 0;
	const char *_interface = NULL;
	char ssl_cert[256] = LOCAL_RESOURCE_PATH"/libwebsockets-test-server.pem";
	char ssl_key[256] = LOCAL_RESOURCE_PATH"/libwebsockets-test-server.key.pem";
#ifndef _WIN32
	int syslog_options = LOG_PID | LOG_PERROR;
#endif
	int client = 0;
	int listen_port = 80;
	struct lws_context_creation_info info;

	int debug_level = 7;
#ifndef LWS_NO_DAEMONIZE
	int daemonize = 0;
#endif

	memset(&info, 0, sizeof info);


#ifndef LWS_NO_DAEMONIZE
	/*
	 * normally lock path would be /var/lock/lwsts or similar, to
	 * simplify getting started without having to take care about
	 * permissions or running as root, set to /tmp/.lwsts-lock
	 */
	if (!client && daemonize && lws_daemonize("/tmp/.lwstecho-lock")) {
		fprintf(stderr, "Failed to daemonize\n");
		return 1;
	}
#endif

	/* we will only try to log things according to our debug_level */
	setlogmask(LOG_UPTO (LOG_DEBUG));
	openlog("lwsts", syslog_options, LOG_DAEMON);

	/* tell the library what debug level to emit and to send it to syslog */
	lws_set_log_level(debug_level, lwsl_emit_syslog);

	//lwsl_notice("libwebsockets test server echo - license LGPL2.1+SLE\n");

    lwsl_notice("Running in server mode\n");
    listen_port = port;

	info.port = listen_port;
	info.iface = _interface;
	info.protocols = protocols;
	if (use_ssl && !client) {
		info.ssl_cert_filepath = ssl_cert;
		info.ssl_private_key_filepath = ssl_key;
	} else
		if (use_ssl && client) {
			info.ssl_cert_filepath = NULL;
			info.ssl_private_key_filepath = NULL;
		}
	info.gid = -1;
	info.uid = -1;
	info.options = opts | LWS_SERVER_OPTION_VALIDATE_UTF8;
#ifndef LWS_NO_EXTENSIONS
	info.extensions = exts;
#endif

	context = lws_create_context(&info);
	if (context == NULL) {
		lwsl_err("libwebsocket init failed\n");
		return -1;
	}


	signal(SIGINT, sighandler);

	n = 0;
	while (n >= 0 && !force_exit) {
		n = lws_service(context, 10);
	}
	lws_context_destroy(context);

	lwsl_notice("libwebsockets-test-echo exited cleanly\n");
	closelog();

	return 0;
}
