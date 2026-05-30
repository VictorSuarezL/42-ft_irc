#ifndef ERRORS_HPP
#define ERRORS_HPP

#include <Common.hpp>

extern std::map<std::string, std::pair<int, std::string> > errorMessages;

void initErrorMessages();
std::pair<int, std::string> getErrorMessage(const std::string& errorCode);

#endif
