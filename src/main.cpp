#include <Server.hpp>
#include <Logger.hpp>
#include <Message.hpp>
#include <iostream>

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
            server.run();
            // Logger::debug("Server is running... (iteration " + std::to_string(i) + ")");
        }
    }

    Message msg;
    msg.setCommand("PRIVMSG");
    msg.addArg("#channel");
    msg.addArg("user");
    msg.setTrailing("Hello, World!");

    // Output the message components
    std::cout << "Command: " << msg.getCommand() << std::endl;
    std::cout << "Arguments: ";
    for (int i = 0; i < msg.getArgCount(); ++i) {
        std::cout << msg.getArgs()[i] << " ";
    }
    std::cout << std::endl;
    std::cout << "Trailing: " << msg.getTrailing() << std::endl;

    return 0;
}
