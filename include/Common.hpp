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

inline std::string numberToString(int number)
{
    std::ostringstream stream;

    stream << number;
    return stream.str();
}

#endif
