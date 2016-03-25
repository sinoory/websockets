#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../lib/libwebsockets.h"

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

class Client{
    public:
    virtual void onMessage(unsigned char*msg,int len){};
    virtual void onClose(){};

    void sendMessage(char* msg,int len){
		memcpy(&mCurrentSessionData->buf[LWS_PRE],msg, len);
        mCurrentSessionData->len=len;
    }
    int getCurrentFd(){
        return lws_get_socket_fd(mCurrentWsi);
    }
    int getClientIp(char* ipbuf,int buflen){
        char name[100], rip[50];
        lws_get_peer_addresses(mCurrentWsi, getCurrentFd(), name,
                               sizeof(name), ipbuf, buflen);
        return 0;
    }

    void setWsi(struct lws* wsi){mCurrentWsi=wsi;}
    void setSessionData(per_session_data__echo* pss){mCurrentSessionData=pss;}
    private:
    struct lws* mCurrentWsi;
    struct per_session_data__echo* mCurrentSessionData; 
};


int startWebSocketServer(int port,Client* client );
