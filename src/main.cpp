#include <Message.hpp>
#include <iostream>
int main() {
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