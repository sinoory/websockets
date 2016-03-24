#include "../websocketserver.h"


class MultyClient: public Client{
};

int main(){
    startWebSocketServer(8999,new MultyClient());
    return 0;
}
