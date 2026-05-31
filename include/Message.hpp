#include <string>
#include <vector>
#include <iostream>
#include <sstream>

class Message {
    public:
        Message();
        ~Message();
        void setCommand(const std::string& cmd);
        std::string getCommand() const;
        void setTrailing(const std::string& trail);
        std::string getTrailing() const;
        void addArg(const std::string& arg);
        std::vector<std::string> getArgs() const;
        std::string getArgsAsString() const;
        int getArgCount() const;
        void printMessage() const;
        Message parse(const std::string& raw);
    private:
        std::string command;
        std::vector<std::string> args;
        std::string trailing;
};
