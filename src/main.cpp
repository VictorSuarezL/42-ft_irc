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

// int main() {
//     // Chema && Sara testing purposes
//     std::string input;
//     while (true) {
//         std::cout << "Enter a raw IRC message: ";
//         std::getline(std::cin, input);
//         Message msg;
//         msg = msg.parse(input);
//         msg.printMessage();
//     }
//     return 0;
// }

// int main()
// {
//     /*
//     ** Expected:
//     ** Command  = NICK
//     ** Args     = ["john"]
//     ** Trailing = ""
//     */
//     Message m1 = Message().parse("NICK john");
//     m1.printMessage();

//     /*
//     ** Expected:
//     ** Command  = USER
//     ** Args     = ["john", "localhost", "server"]
//     ** Trailing = "John Doe"
//     */
//     Message m2 = Message().parse("USER john localhost server :John Doe");
//     m2.printMessage();

//     /*
//     ** Expected:
//     ** Command  = PRIVMSG
//     ** Args     = ["#general"]
//     ** Trailing = "Hello everyone!"
//     */
//     Message m3 = Message().parse("PRIVMSG #general :Hello everyone!");
//     m3.printMessage();

//     /*
//     ** Expected:
//     ** Command  = JOIN
//     ** Args     = ["#42"]
//     ** Trailing = ""
//     */
//     Message m4 = Message().parse("JOIN #42");
//     m4.printMessage();

//     /*
//     ** Expected:
//     ** Command  = PART
//     ** Args     = ["#42"]
//     ** Trailing = "Goodbye guys"
//     */
//     Message m5 = Message().parse("PART #42 :Goodbye guys");
//     m5.printMessage();

//     /*
//     ** Expected:
//     ** Command  = QUIT
//     ** Args     = []
//     ** Trailing = "Connection closed"
//     */
//     Message m6 = Message().parse("QUIT :Connection closed");
//     m6.printMessage();

//     /*
//     ** Expected:
//     ** Command  = PING
//     ** Args     = ["server1"]
//     ** Trailing = ""
//     */
//     Message m7 = Message().parse("PING server1");
//     m7.printMessage();

//     /*
//     ** Expected:
//     ** Command  = MODE
//     ** Args     = ["#general", "+i"]
//     ** Trailing = ""
//     */
//     Message m8 = Message().parse("MODE #general +i");
//     m8.printMessage();

//     /*
//     ** Expected:
//     ** Command  = KICK
//     ** Args     = ["#general", "john"]
//     ** Trailing = "Spamming"
//     */
//     Message m9 = Message().parse("KICK #general john :Spamming");
//     m9.printMessage();

//     /*
//     ** Expected:
//     ** Command  = TOPIC
//     ** Args     = ["#general"]
//     ** Trailing = "New channel topic"
//     */
//     Message m10 = Message().parse("TOPIC #general :New channel topic");
//     m10.printMessage();

//     /*
//     ** Expected:
//     ** Command  = INVITE
//     ** Args     = ["john", "#general"]
//     ** Trailing = ""
//     */
//     Message m11 = Message().parse("INVITE john #general");
//     m11.printMessage();

//     /*
//     ** Expected:
//     ** Command  = PRIVMSG
//     ** Args     = ["user1,user2"]
//     ** Trailing = "Hello both!"
//     */
//     Message m12 = Message().parse("PRIVMSG user1,user2 :Hello both!");
//     m12.printMessage();

//     /*
//     ** Expected:
//     ** Command  = ""
//     ** Args     = []
//     ** Trailing = ""
//     **
//     ** Empty input test
//     */
//     Message m13 = Message().parse("");
//     m13.printMessage();

//     /*
//     ** Expected:
//     ** Command  = WHOIS
//     ** Args     = ["john"]
//     ** Trailing = ""
//     */
//     Message m14 = Message().parse("WHOIS john");
//     m14.printMessage();

//     /*
//     ** Expected:
//     ** Command  = NOTICE
//     ** Args     = ["john"]
//     ** Trailing = "Auto-reply disabled"
//     */
//     Message m15 = Message().parse("NOTICE john :Auto-reply disabled");
//     m15.printMessage();

//     return 0;
// }