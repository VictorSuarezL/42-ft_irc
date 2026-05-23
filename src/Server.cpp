#include "Server.hpp"

Server::Server(int port, const std::string &password) : _port(port), _password(password) {
    std::cout << "Server created on port " << _port << " with password: " << _password << std::endl;
}

Server::~Server() {
    std::cout << "Server on port " << _port << " is shutting down." << std::endl;
}
