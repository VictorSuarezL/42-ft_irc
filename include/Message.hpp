#include <string>
#include <vector>
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
        int getArgCount() const;
    private:
        std::string command;
        std::vector<std::string> args;
        std::string trailing;
};