#include <Server.hpp>

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        Logger::error("Usage: " + std::string(argv[0]) + " <port> <password>");
        return 1;
    }

    Server server(argv[1], argv[2]);
    signal(SIGINT, server.signalHandler);
    while (!server.getServerStop()) {
        server.run();
    }
    server.serverShutdown();
    return 0;
}