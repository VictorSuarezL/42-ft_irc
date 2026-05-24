#include <Message.hpp>

// Basic parsing of raw message into command, args, and trailing
Message Message::parse(const std::string& raw)
{
    std::string line = raw;

    if (!line.empty() && line[line.size() - 1] == '\n')
        line.erase(line.size() - 1);

    if (!line.empty() && line[line.size() - 1] == '\r')
        line.erase(line.size() - 1);
    
    Message msg;

    std::istringstream iss(line);
    std::string token;

    /*
    ** COMMAND
    */
    if (!(iss >> token))
        return msg;

    msg.setCommand(token);

    /*
    ** ARGS + TRAILING
    */
    while (iss >> token)
    {
        /*
        ** Trailing starts with :
        ** Everything after belongs to trailing
        */
        if (token[0] == ':')
        {
            std::string trailing;

            trailing = token.substr(1);

            std::string rest;
            std::getline(iss, rest);

            if (!rest.empty())
                trailing += rest;

            msg.setTrailing(trailing);
            break;
        }

        msg.addArg(token);
    }

    return msg;
}