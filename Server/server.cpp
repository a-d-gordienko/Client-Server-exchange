// Server.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <string>
#include <conio.h>
#include "logger.h"
#include "srv.h"

int main()
{
    log_instance.init("server.log");
    log_write->info("start server");
    SERVER.start();
    while (true) {
        if (_kbhit() && _getch() == VK_ESCAPE) {
            SERVER.stop();
            break;
        }
    }
    log_write->info("stop server");
    return 0;
}
