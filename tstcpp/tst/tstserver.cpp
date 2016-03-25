#include "../websocketserver.h"


class MultyClient: public Client{
    public:
    virtual void onMessage(unsigned char*msg,int len){
        unsigned char m[30]={0};
        memcpy(m,msg,len);
        printf("---onMessage len=%d m=%s\n",len,m);
        char* repmsg=(char*)"srv_msg_from_onMessage";
        sendMessage(repmsg,strlen(repmsg));

	    char buf[256];
        char ip[50];
        getClientIp(ip,sizeof(ip));
        lwsl_notice("----onMessage from %s\n", ip);

    };
    virtual void onClose(){printf("---client %d onClose\n",getCurrentFd());};


};

int main(){
    startWebSocketServer(8999,new MultyClient());
    return 0;
}
