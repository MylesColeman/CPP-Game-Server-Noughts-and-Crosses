#include "GameServer.h"

int main()
{
    GameServer server(4300);
    server.tcp_start();
    
    return 0;
}
