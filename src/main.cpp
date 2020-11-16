#include <chrono>
#include <iostream>
#include <thread>

#include <jack/jack.h>
#include <jack/types.h>

#include "multiplexer.hpp"

void jack_shutdown()
{
    std::cout << "Server shut down, exiting...\n";
    exit(1);
}

int main()
{
    Multiplexer multiplexer{2};
    multiplexer.initJack();

    while (true)
    {
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(1s);
        // break;
    }

    return 0;
}