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
    Logger::info("Server on port " + numberToString(_port) + " is shutting down.");
}

// int Server::checkConnections(void) {
//     // Placeholder for connection checking logic
//     Logger::info("Checking for new connections...");
//     return 0; // Return 0 for now, indicating no new connections
// }

void Server::createSocket()
{
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
        Logger::error("Failed to bind socket to port " + numberToString(_port) + ".");
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
    // Placeholder for server's main loop logic
    // Logger::info("Running server on port " + numberToString(_port) + "...");
    // Here you would typically use poll() to wait for events on the file descriptors in fds
    poll(&fds[0], fds.size(), 0); // Non-blocking poll to check for events
    
    for(size_t i = 0; i < fds.size(); ++i) {
        if (fds[i].revents & POLLIN) {
            if (fds[i].fd == _socket) {
                // Handle new incoming connection
                Logger::info("New connection detected on server socket.");
                // Accept the new connection and add it to fds
            } else {
                // Handle data from an existing client
                Logger::info("Data available from client socket: " + numberToString(fds[i].fd));
                // Read data from the client and process it
            }
        }
    }
}
