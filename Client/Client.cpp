// Client.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <chrono>
#include <conio.h>
#include "logger.h"
#include "client.h"

constexpr uint16_t DEF_PORT = 64000;

int main(){
    log_instance.init("client.log");
    CLIENT.start("127.0.0.1", DEF_PORT);
    while (true) {
        if (_kbhit() && _getch() == VK_ESCAPE) {
            CLIENT.stop();
            break;
        }
    }
    return 0;
}

