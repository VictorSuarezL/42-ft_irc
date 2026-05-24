#include <Message.hpp>

enum CommandType {
    /*
    ** =========================
    ** REGISTRATION
    ** =========================
    */

    // PASS <password>
    // Server password before registration
    PASS,

    // NICK <nickname>
    // Change or set nickname
    NICK,

    // USER <username> <hostname> <servername> :<realname>
    // Initial user registration
    USER,

    /*
    ** =========================
    ** MESSAGING
    ** =========================
    */

    // PRIVMSG <target> :<message>
    // Private message to a user or channel
    // target can be:
    //   - user
    //   - #channel
    // multiple targets separated by ','
    PRIVMSG,

    // NOTICE <target> :<message>
    // Same as PRIVMSG but without automatic replies
    NOTICE,

    /*
    ** =========================
    ** CHANNELS
    ** =========================
    */

    // JOIN <#channel>
    // JOIN <#a,#b,#c>
    // Join channels
    JOIN,

    // PART <#channel> :<reason>
    // Leave channel
    PART,

    // TOPIC <#channel>
    // TOPIC <#channel> :<new topic>
    // Get or change topic
    TOPIC,

    // MODE <target> <flags> [args]
    // Typical flags:
    //   +i  invite only
    //   +t  topic protected
    //   +k  password
    //   +o  operator
    //   +l  user limit
    MODE,

    // KICK <#channel> <user> :<reason>
    // Kick user
    KICK,

    // INVITE <user> <#channel>
    // Invite user to channel
    INVITE,

    /*
    ** =========================
    ** INFO
    ** =========================
    */

    // NAMES <#channel>
    // List users in the channel
    NAMES,

    // LIST
    // List channels
    LIST,

    // WHO <mask>
    // Basic information about users
    WHO,

    // WHOIS <nickname>
    // Detailed user information
    WHOIS,

    /*
    ** =========================
    ** CONTROL
    ** =========================
    */

    // PING <token>
    // Client-server keepalive
    PING,

    // PONG <token>
    // Response to PING
    PONG,

    // QUIT :<reason>
    // Disconnection
    QUIT,

    UNKNOWN
};

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