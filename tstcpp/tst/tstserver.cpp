#include "../websocketserver.h"
#include <map>
using namespace std;

class MultyClient: public Client{
    public:
    virtual void onConnect(){
        printf("----client onConnect fd=%d\n",getCurrentFd());
	    char buf[50];
        sprintf(buf,"onConne welcome ,client %d\n",getCurrentFd());
        sendMessage(buf,strlen(buf));
        wsmap.insert(std::pair<int,struct lws*>(getCurrentFd(),mCurrentWsi));
    }
    virtual void onMessage(unsigned char*msg,int len){
        unsigned char m[30]={0};
        memcpy(m,msg,len);
        printf("---onMessage len=%d m=%s\n",len,m);

	    char bufm[50];
        sprintf((char*)bufm,"welcome ,client %d\n",getCurrentFd());
        std::map<int,struct lws*>::iterator it;
        for(it=wsmap.begin();it!=wsmap.end();it++){
            printf("sending to %d\n",it->first);
            sendMessage(bufm,strlen(bufm),it->second);
        }

	    char buf[256];
        char ip[50];
        getClientIp(ip,sizeof(ip));
        lwsl_notice("----onMessage from %s\n", ip);

    };
    virtual void onClose(){
        printf("---client %d onClose\n",getCurrentFd());
        wsmap.erase(getCurrentFd());
    };

    std::map<int,struct lws*>wsmap;
    
};

int main(){
    startWebSocketServer(7681,new MultyClient());
    return 0;
}
