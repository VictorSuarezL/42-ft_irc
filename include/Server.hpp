#ifndef SERVER_HPP
#define SERVER_HPP

#include "Common.hpp"
#include <poll.h>

class Server
{
    private:
        int _port;
        std::string _password;
        int _socket;
        sockaddr_in _address;
        std::string _serverName;
        std::vector<pollfd> fds; // List of file descriptors to monitor for incoming connections
        bool parsePort(const std::string &port);
        void acceptClient(void);
        void receiveFromClient(size_t index);

    public:
        Server(const std::string &port, const std::string &password);
        ~Server();
        void createSocket();
        void run(void);

        // int checkConnections(void);
        
};

#endif
