#include <Message.hpp>

// Message constructor and destructor
Message::Message() {}
Message::~Message() {}

// Setters and getters for command
void Message::setCommand(const std::string& cmd) {
    command = cmd;
}
std::string Message::getCommand() const {
    return command;
}

// Setters and getters for trailing
void Message::setTrailing(const std::string& trail) {
    trailing = trail;
}
std::string Message::getTrailing() const {
    return trailing;
}

// Setters and getters for arguments
void Message::addArg(const std::string& arg) {
    args.push_back(arg);
}
std::vector<std::string> Message::getArgs() const {
    return args;
}

std::string Message::getArgsAsString() const {
    std::string result;
    for (size_t i = 0; i < args.size(); ++i) {
        result += args[i];
        if (i < args.size() - 1)
            result += " ";
    }
    return result;
}

int Message::getArgCount() const {
    return args.size();
}

// Print the message content (for debugging)
void Message::printMessage() const {
    std::cout << "*** MESSAGE CONTENT ***" << std::endl;
    std::cout << "COMMAND: " << command << std::endl;
    std::cout << "ARGUMENTS: ";
    for (size_t i = 0; i < args.size(); ++i) {
        std::cout << args[i] << " ";
    }
    std::cout << std::endl;
    std::cout << "TRAILING: " << trailing << std::endl;
    std::cout << "*** END MESSAGE ***" << std::endl;
}