#ifndef LOGGER_HPP
# define LOGGER_HPP

#include "Common.hpp"

class Logger
{
    public:
        static void info(const std::string& message);
        static void warning(const std::string& message);
        static void error(const std::string& message);
        static void debug(const std::string& message);
};

#endif
