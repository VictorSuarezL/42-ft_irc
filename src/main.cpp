#include <Server.hpp>
#include <Logger.hpp>
#include <iostream>
#include <Channel.hpp>

int main(int argc, char *argv[]) {
    if (argc != 3) {
        Logger::error("Usage: " + std::string(argv[0]) + " <port> <password>");
        return 1;
    }
    std::string input;
    Server server(argv[1], argv[2]);
    while (true) {
        std::cout << "Enter a raw IRC message: ";
        std::getline(std::cin, input);
        Message msg;
        msg = msg.parse(input);
        msg.printMessage();
        server.dispatchMessage(msg);
    }
    return 0;
}