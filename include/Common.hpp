#ifndef COMMON_HPP
# define COMMON_HPP

# define RED     "\033[31m"
# define GREEN   "\033[32m"
# define YELLOW  "\033[33m"
# define BLUE    "\033[34m"
# define RESET   "\033[0m"

# include <iostream>
# include <cerrno>
# include <cstdlib>
# include <cstring>
# include <sstream>
# include <string>
# include <vector>
# include <sys/socket.h>
# include <netinet/in.h>
# include <unistd.h>
# include <fcntl.h>


const std::string PASS_STR = "pass";
const std::string NICK_STR = "nick";
const std::string USER_STR = "user";
const std::string JOIN_STR = "join";
const std::string PART_STR = "part";
const std::string TOPIC_STR = "topic";
const std::string MODE_STR = "mode";
const std::string KICK_STR = "kick";
const std::string INVITE_STR = "invite";
const std::string NAMES_STR = "names";
const std::string LIST_STR = "list";
const std::string WHO_STR = "who";
const std::string WHOIS_STR = "whois";
const std::string PING_STR = "ping";
const std::string PONG_STR = "pong";
const std::string QUIT_STR = "quit";
const std::string PRIVMSG_STR = "privmsg";
const std::string NOTICE_STR = "notice";

inline std::string numberToString(int number)
{
    std::ostringstream stream;

    stream << number;
    return stream.str();
}

inline void toLowerCase(std::string& str) {
    for (size_t i = 0; i < str.size(); ++i) {
        str[i] = std::tolower(str[i]);
    }
}

#endif
