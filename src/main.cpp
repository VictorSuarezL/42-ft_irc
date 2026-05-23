#include "Server.hpp"
#include "Logger.hpp"

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        Logger::error("Usage: " + std::string(argv[0]) + " <port> <password>");
        return 1;
    }
    else
    {
        std::string password = argv[2];

        Server server(argv[1], password);

        while (true)
        // for(int i = 0; i < 10; ++i)
        {

            // if(server.checkConnections() > 0) {
            //     Logger::info("New connection established.");
            server.run();
            // Logger::debug("Server is running... (iteration " + std::to_string(i) + ")");
        }
    }
}
