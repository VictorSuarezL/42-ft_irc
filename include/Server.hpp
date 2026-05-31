#ifndef SERVER_HPP
#define SERVER_HPP

#include <Common.hpp>
#include <poll.h>
#include <User.hpp>
#include <Message.hpp>
#include <Errors.hpp>
#include <Logger.hpp>
#include <iostream>
#include <Channel.hpp>

class Server
{
    private:
        int _port;
        std::string _password;
        int _socket;
        sockaddr_in _address;
        std::string _serverName;
        std::vector<pollfd> _fds; // List of file descriptors to monitor for incoming connections
        std::map<int, User> _users;
        std::map<std::string, Channel> _channels;

        bool parsePort(const std::string &port);
        bool parsePassword(const std::string &password);
        void acceptClient(void);
        void receiveFromClient(size_t index);

    public:
        Server(const std::string &port, const std::string &password);
        ~Server();
        void createSocket();
        void run(void);
        void dispatchMessage(User& user, const Message& msg);
        void sendToUser(User &user, const std::string &message);
        void handlePass(User &user, const Message& msg);
        void handleNick(User& user, const Message& msg);
        void handleUser(User& user, const Message& msg);
        void handleJoin(User& user, const Message& msg);
        void handlePart(const Message& msg);
        void handlePing(User& user, const Message& msg);
        void handleMode(const Message& msg);
        void handleKick(const Message& msg);
        void handleInvite(const Message& msg);
        void handleTopic(const Message& msg);
        void handlePrivMsg(const Message& msg);
        void handleUnknown(const Message& msg);
        void errorBuilder(User& user, const std::string& errorCode);

        // int checkConnections(void);
        
};

#endif
