#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <assert.h>
#include <signal.h>



#ifndef _WIN32
#include <syslog.h>
#include <sys/time.h>
#include <unistd.h>
#else
#include "gettimeofday.h"
#include <process.h>
#endif

#include "websocketserver.h"

static volatile int force_exit = 0;
static int versa, state;


static Client* _client=0;//new Client();

//TODO:
//1.server fd
//3.client request ws://xxx..xxx/xxx
static int
callback_echo(struct lws *wsi, enum lws_callback_reasons reason, void *user,
	      void *in, size_t len)
{
	struct per_session_data__echo *pss =
			(struct per_session_data__echo *)user;
	int n;
    _client->setWsi(wsi);
    _client->setSessionData(pss);
    if(reason != 31){
    lwsl_notice("callback_echo reason=%2d wsi=%p pss=%p\n",reason,wsi,pss);
    }
    const char* msg="this is server msg";
	switch (reason) {

    //client establish
    case LWS_CALLBACK_FILTER_NETWORK_CONNECTION://get server fd
        lwsl_notice("client requ, server fd=%d\n",lws_get_socket_fd(wsi));
        _client->setServerFd(lws_get_socket_fd(wsi));
        break;
    case LWS_CALLBACK_CLIENT_FILTER_PRE_ESTABLISH:
        lwsl_notice("===CLIENT_FILTER_PRE_ESTABLISH here url?==\n");
        break;
    case LWS_CALLBACK_WSI_CREATE:
        lwsl_notice("create LWS_CALLBACK_WSI_CREATE=%d\n",lws_get_socket_fd(wsi));
        break;
    case LWS_CALLBACK_SERVER_NEW_CLIENT_INSTANTIATED:
        lwsl_notice("net connect2 fd=%d\n",lws_get_socket_fd(wsi));
        //_client->onConnect();
        //lwsl_notice("LWS_CALLBACK_SERVER_NEW_CLIENT_INSTANTIATED fd=%d\n",wsi->sock);
        break;
    case LWS_CALLBACK_FILTER_HTTP_CONNECTION:
        char buf[50];
        memcpy(buf,in,len);
        lwsl_notice("client request url=%s\n",buf);
        break;
    case LWS_CALLBACK_PROTOCOL_INIT:
        lwsl_notice("net server fd=%d\n",lws_get_socket_fd(wsi));
        break;
    case LWS_CALLBACK_FILTER_PROTOCOL_CONNECTION:
        lwsl_notice("net client connet fd=%d\n",lws_get_socket_fd(wsi));
        _client->onConnect();
        break;
    case LWS_CALLBACK_ADD_POLL_FD:
        lwsl_notice("net add fd=%d,client->serverfd=%d\n",lws_get_socket_fd(wsi),_client->getServerFd());
        if(_client->getServerFd()==0){
            _client->setServerFd(lws_get_socket_fd(wsi));
        }else{//this is the client fd
            //_client->onConnect();
        }
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
		//memcpy(&pss->buf[LWS_PRE],msg, strlen(msg));
        //pss->len=strlen(msg);

        if(pss->len==0){
            break;
        }
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
		pss->len = 0;//(unsigned int)len;
		pss->rx += len;
        _client->onMessage(&pss->buf[LWS_PRE],len);

		lws_rx_flow_control(wsi, 0);
		lws_callback_on_writable(wsi);
		break;

	case LWS_CALLBACK_CLOSED:
        _client->onClose();
		state = 0;
		break;
	case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
		lwsl_notice("closed 2\n");
		state = 0;
		break;

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


int startWebSocketServer(int port,Client* client )
{
    _client=client;
	int n = 0;
	int use_ssl = 0;
	struct lws_context *context;
	int opts = 0;
	const char *_interface = NULL;
	int syslog_options = LOG_PID | LOG_PERROR;

	int listen_port = 80;
	struct lws_context_creation_info info;

	int debug_level = 7;
	memset(&info, 0, sizeof info);



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
	info.gid = -1;
	info.uid = -1;
	info.options = opts | LWS_SERVER_OPTION_VALIDATE_UTF8;

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

