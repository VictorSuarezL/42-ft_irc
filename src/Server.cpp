#include "Server.hpp"
#include "Logger.hpp"

bool Server::parsePort(const std::string &portStr)
{
    std::stringstream stream(portStr);
    char extra;
    bool invalidPort = false;

    if (!(stream >> _port))
        invalidPort = true;
    else if (stream >> extra)
        invalidPort = true;
    else if (_port <= 0 || _port > 65535)
        invalidPort = true;

    return !invalidPort;
}

Server::Server(const std::string &port, const std::string &password) : _port(0), _password(password), _serverName("definitely_not_discord")
{
    if (!parsePort(port))
    {
        Logger::error("Invalid port: " + port);
        exit(EXIT_FAILURE);
    }
    createSocket();
    Logger::info("Server created on port " + numberToString(_port) + " with password: " + _password);
}

Server::~Server()
{
    for (size_t i = 0; i < fds.size(); ++i)
        close(fds[i].fd);
    Logger::info("Server on port " + numberToString(_port) + " is shutting down.");
}

void Server::createSocket()
{
    std::memset(&_address, 0, sizeof(_address));

    // Create a socket
    // AF_INET: IPv4, SOCK_STREAM: TCP, 0: default protocol
    _socket = socket(AF_INET, SOCK_STREAM, 0);
    if (_socket < 0)
    {
        Logger::error("Failed to create socket.");
        exit(EXIT_FAILURE);
    }
    int opt = 1;
    // Set socket options to allow reuse of the address
    // SOL_SOCKET: Level for socket options
    // SO_REUSEADDR: Allow reuse of local addresses
    if (setsockopt(_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        Logger::error("Failed to set socket options.");
        exit(EXIT_FAILURE);
    }
    _address.sin_family = AF_INET;
    _address.sin_addr.s_addr = INADDR_ANY; // Listen on all interfaces
    _address.sin_port = htons(_port);

    fcntl(_socket, F_SETFL, O_NONBLOCK); // Set socket to non-blocking mode

    if (bind(_socket, (struct sockaddr *)&_address, sizeof(_address)) < 0)
    {
        Logger::error("Failed to bind socket to port " + numberToString(_port) + ": " + std::string(std::strerror(errno)));
        exit(EXIT_FAILURE);
    }

    if (listen(_socket, 5) < 0)
    {
        Logger::error("Failed to listen on socket.");
        exit(EXIT_FAILURE);
    }

    struct pollfd pfd;
    pfd.fd = _socket;
    pfd.events = POLLIN; // Monitor for incoming connections
    pfd.revents = 0;     // Initialize revents to 0
    fds.push_back(pfd);  // Add the server socket to the list of file descriptors to monitor

    Logger::info("Setting up server socket on port " + numberToString(_port) + "...");
}

void Server::run(void)
{
    int ready = poll(&fds[0], fds.size(), -1);

    if (ready < 0)
    {
        Logger::error("poll failed.");
        return;
    }

    for (size_t i = 0; i < fds.size(); ++i)
    {
        if (fds[i].revents & POLLIN)
        {
            if (fds[i].fd == _socket)
                acceptClient();
            else
                receiveFromClient(i);
        }
    }
}

void Server::acceptClient(void)
{
    sockaddr_in clientAddress;
    socklen_t clientLength = sizeof(clientAddress);

    std::memset(&clientAddress, 0, sizeof(clientAddress));
    int clientSocket = accept(_socket, (struct sockaddr *)&clientAddress, &clientLength);

    if (clientSocket < 0)
    {
        Logger::error("Failed to accept new client.");
        return;
    }
    if (fcntl(clientSocket, F_SETFL, O_NONBLOCK) < 0)
    {
        Logger::error("Failed to set client socket to non-blocking mode.");
        close(clientSocket);
        return;
    }

    struct pollfd pfd;
    pfd.fd = clientSocket;
    pfd.events = POLLIN;
    pfd.revents = 0;
    fds.push_back(pfd);

    Logger::info("Client connected on socket " + numberToString(clientSocket) + ".");
}

void Server::receiveFromClient(size_t index)
{
    char buffer[512];
    int bytesRead = recv(fds[index].fd, buffer, sizeof(buffer) - 1, 0);

    if (bytesRead <= 0)
    {
        Logger::info("Client disconnected from socket " + numberToString(fds[index].fd) + ".");
        close(fds[index].fd);
        fds.erase(fds.begin() + index);
        return;
    }

    buffer[bytesRead] = '\0';
    Logger::info("Received from socket " + numberToString(fds[index].fd) + ": " + std::string(buffer));
}
