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
int Message::getArgCount() const {
    return args.size();
}