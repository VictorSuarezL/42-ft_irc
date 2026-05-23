#include "Server.hpp"
#include "Logger.hpp"

Server::Server(int port, const std::string &password) : _port(port), _password(password) {
    Logger::info("Server created on port " + std::to_string(_port) + " with password: " + _password);
}

Server::~Server() {
    Logger::info("Server on port " + std::to_string(_port) + " is shutting down.");
}
