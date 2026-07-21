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
        volatile static bool _serverStop;
        std::map<std::string, Channel> _channels;
        std::set<int> _clientsToDisconnect;

        bool parsePort(const std::string &port);
        bool parsePassword(const std::string &password);
        void acceptClient(void);
        void receiveFromClient(size_t index);
        void disconnectClient(int fd);
        void scheduleDisconnection(int fd);
        void processDisconnections();

    public:
        Server(const std::string &port, const std::string &password);
        ~Server();
        void createSocket();
        void run(void);
        void dispatchMessage(User& user, const Message& msg);
        void sendToUser(User &user, const std::string &message);
        void sendPendingData(size_t index);
        void enablePollOut(int fd);
        void handlePass(User &user, const Message& msg);
        void handleNick(User& user, const Message& msg);
        void handleUser(User& user, const Message& msg);
        void handleJoin(User& user, const Message& msg);
        void handlePart(const Message& msg);
        void handlePing(User& user, const Message& msg);
        void handleMode(User& user, const Message& msg);
        void handleKick(const Message& msg);
        void handleInvite(const Message& msg);
        void handleTopic(User& user, const Message& msg);
        void handlePrivMsg(User& user, const Message& msg);
        void handleUnknown(const Message& msg);
        void errorBuilder(User& user, const std::string& errorCode);
        bool isNicknameInUse(const std::string& nickname) const;
        static void signalHandler(int signal);
        static void setServerStop(bool value);
        static bool getServerStop();
        void serverShutdown();
        void broadcastMessage(const std::string& message, int senderFd, const std::string& channelName);
        User *getUserByNickname(const std::string& nickname);


        // int checkConnections(void);
        
};

#endif
