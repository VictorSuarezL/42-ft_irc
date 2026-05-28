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
    for (size_t i = 0; i < _fds.size(); ++i)
        close(_fds[i].fd);
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
    _fds.push_back(pfd);  // Add the server socket to the list of file descriptors to monitor

    Logger::info("Setting up server socket on port " + numberToString(_port) + "...");
}

void Server::run(void)
{
    int ready = poll(&_fds[0], _fds.size(), -1);

    if (ready < 0)
    {
        Logger::error("poll failed.");
        return;
    }

    for (size_t i = 0; i < _fds.size(); ++i)
    {
        if (_fds[i].revents & POLLIN)
        {
            if (_fds[i].fd == _socket)
                acceptClient();
            else
                receiveFromClient(i);
        }
    }
}

void Server::acceptClient(void)
{
    sockaddr_in clientAddress; // Structure to hold client address information
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

    // Add the new client socket to the list of file descriptors to monitor
    struct pollfd pfd;
    pfd.fd = clientSocket;
    pfd.events = POLLIN;
    pfd.revents = 0;
    _fds.push_back(pfd);

    Logger::debug("Client connected on socket " + numberToString(clientSocket) + ".");

    User user;
    user.setFd(clientSocket);
    _users[clientSocket] = user;

    Logger::debug("Debug set fd passed");
}

void Server::receiveFromClient(size_t index)
{
    char buffer[512]; // 1024??
    memset(buffer, 0, sizeof(buffer));
    int bytesRead = recv(_fds[index].fd, buffer, sizeof(buffer) - 1, 0);

    if (bytesRead <= 0)
    {
        Logger::info("Client disconnected from socket " + numberToString(_fds[index].fd) + ".");
        close(_fds[index].fd);
        _users.erase(_fds[index].fd);
        _fds.erase(_fds.begin() + index);
        return;
    }
    std::string data(buffer, bytesRead);

    User &user = _users[_fds[index].fd];
    user.appendToInputBuffer(data);

    buffer[bytesRead] = '\0';
    Logger::debug("Received from socket " + numberToString(_fds[index].fd) + ": " + std::string(buffer));
}

void Server::handlePass(const Message& msg) {
    std::cout << "Handling command ";
    std::cout << msg.getCommand() << std::endl;
    // Implement PASS command handling logic here
}

void Server::handleNick(const Message& msg) {
    std::cout << "Handling command ";
    std::cout << msg.getCommand() << std::endl;
    // Implement NICK command handling logic here
}

void Server::handleUser(const Message& msg) {
    std::cout << "Handling command ";
    std::cout << msg.getCommand() << std::endl;
    // Implement USER command handling logic here
}

void Server::handleJoin(const Message& msg) {
    std::cout << "Handling command ";
    std::cout << msg.getCommand() << std::endl;
    // Implement JOIN command handling logic here
}

void Server::handlePart(const Message& msg) {
    std::cout << "Handling command ";
    std::cout << msg.getCommand() << std::endl;
    // Implement PART command handling logic here
}

void Server::handlePing(const Message& msg) {
    std::cout << "Handling command ";
    std::cout << msg.getCommand() << std::endl;
    // Implement PING command handling logic here
}

void Server::handleMode(const Message& msg) {
    std::cout << "Handling command ";
    std::cout << msg.getCommand() << std::endl;
    // Implement MODE command handling logic here
}

void Server::handleKick(const Message& msg) {
    std::cout << "Handling command ";
    std::cout << msg.getCommand() << std::endl;
    // Implement KICK command handling logic here
}

void Server::handleInvite(const Message& msg) {
    std::cout << "Handling command ";
    std::cout << msg.getCommand() << std::endl;
    // Implement INVITE command handling logic here
}

void Server::handleTopic(const Message& msg) {
    std::cout << "Handling command ";
    std::cout << msg.getCommand() << std::endl;
    // Implement TOPIC command handling logic here
}

void Server::handlePrivMsg(const Message& msg) {
    std::cout << "Handling command ";
    std::cout << msg.getCommand() << std::endl;
    // Implement PRIVMSG command handling logic here
}

void Server::handleUnknown(const Message& msg) {
    std::cout << "Unknown command: " << msg.getCommand() << std::endl;
    // Implement handling for unknown commands here
}

void Server::dispatchMessage(const Message& msg) {
    std::string cmd = msg.getCommand();
    // remove \n and \r if present
    if (!cmd.empty() && cmd[cmd.size() - 1] == '\n')
        cmd.erase(cmd.size() - 1);
    if (!cmd.empty() && cmd[cmd.size() - 1] == '\r')
        cmd.erase(cmd.size() - 1);
    toLowerCase(cmd);
    if (cmd == PASS_STR)
        handlePass(msg);
    else if (cmd == NICK_STR)
        handleNick(msg);
    else if (cmd == USER_STR)
        handleUser(msg);
    else if (cmd == JOIN_STR)
        handleJoin(msg);
    else if (cmd == PART_STR)
        handlePart(msg);
    else if (cmd == PING_STR)
        handlePing(msg);
    else if (cmd == MODE_STR)
        handleMode(msg);
    else if (cmd == KICK_STR)
        handleKick(msg);
    else if (cmd == INVITE_STR)
        handleInvite(msg);
    else if (cmd == TOPIC_STR)
        handleTopic(msg);
    else if (cmd == PRIVMSG_STR)
        handlePrivMsg(msg);
    else
        handleUnknown(msg);
}
