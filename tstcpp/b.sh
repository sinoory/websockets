#g++ websocketserver.cpp -lwebsockets -L../build/lib/

g++ tst/tstserver.cpp websocketserver.cpp -lwebsockets -L../build/lib/ -g
