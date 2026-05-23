#include "Logger.hpp"

void Logger::info(const std::string& message)
{
    std::cout << GREEN
              << "[INFO] "
              << RESET
              << message
              << std::endl;
}

void Logger::warning(const std::string& message)
{
    std::cout << YELLOW
              << "[WARNING] "
              << RESET
              << message
              << std::endl;
}

void Logger::error(const std::string& message)
{
    std::cerr << RED
              << "[ERROR] "
              << RESET
              << message
              << std::endl;
}

void Logger::debug(const std::string& message)
{
    std::cout << BLUE
              << "[DEBUG] "
              << RESET
              << message
              << std::endl;
}
