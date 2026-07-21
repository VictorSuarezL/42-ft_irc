#include "Server.hpp"
#include "Logger.hpp"

bool Server::parsePort(const std::string &portStr)
{
    std::stringstream stream(portStr);
    char extra;
    bool invalidPort = false;

    // Try to parse the port number

    if (!(stream >> _port))
        invalidPort = true;
    else if (stream >> extra)
        invalidPort = true;
    else if (_port < 1024 || _port > 65535)
        invalidPort = true;

    return !invalidPort;
}

bool Server::parsePassword(const std::string &password) {
    // Alphanumeric passwords only
    for (size_t i = 0; i < password.size(); ++i) {
        if (!std::isalnum(password[i])) {
            Logger::error("Invalid password: " + password + ". Password must be alphanumeric.");
            return false;
        }
    }
    return true;
}

Server::Server(const std::string &port, const std::string &password) : _port(0), _password(password), _serverName("definitely_not_discord")
{
    if (!parsePort(port))
    {
        Logger::error("Invalid port: " + port);
        exit(EXIT_FAILURE);
    }
    if (!parsePassword(password)) {
        Logger::error("Invalid password: " + password);
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
        if (errno == EINTR)
            return;
        Logger::error("poll failed.");
        
        setServerStop(true);
        return;
    }

    size_t polledFdCount = _fds.size();

    for(size_t i = 0; i < polledFdCount; ++i)
    {
        int fd = _fds[i].fd;
        short revents = _fds[i].revents;

        if(revents == 0)
            continue; // No events for this fd, skip to the next one
        
        if(fd == _socket)
        {
            if(revents & (POLLERR | POLLHUP | POLLNVAL))
            {
                Logger::error("Server socket error. Stopping server.");
                setServerStop(true);
                break;
            }
            
            if(revents & POLLIN)
                acceptClient();
            
            continue; // Skip to the next fd after handling the server socket
        }

        if(revents & (POLLERR | POLLHUP | POLLNVAL))
        {
            Logger::info("Client disconnected from socket " + numberToString(fd) + ".");
            scheduleDisconnection(fd);
            continue; // Skip to the next fd after scheduling disconnection
        }

        if(revents & POLLIN)
            receiveFromClient(i);

        if(_clientsToDisconnect.find(fd) != _clientsToDisconnect.end())
        {
            Logger::info("Client on socket " + numberToString(fd) + " scheduled for disconnection.");
            continue; // Skip to the next fd after disconnecting
        }

        if(revents & POLLOUT)
            sendPendingData(i);
    }

    processDisconnections(); // Process any scheduled disconnections after handling all fds

}

void Server::sendPendingData(size_t index)
{
    int fd = _fds[index].fd;
    User &user = _users[fd];

    while(user.hasPendingOutput())
    {
        const std::string &buffer = user.getOutputBuffer();
        ssize_t sent = send(fd, buffer.c_str(), buffer.size(), 0);

        if(sent > 0)
        {
            Logger::debug("Sent " + numberToString(sent) + " bytes to socket " + numberToString(fd) + ".");
            user.consumeOutputBuffer(sent);
        }
        else if(sent < 0 && errno == EINTR)
        {
            Logger::warning("Send interrupted by signal for socket " + numberToString(fd) + ". Retrying...");
            continue; // Retry sending
        }
        else if(sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            Logger::warning("Send would block for socket " + numberToString(fd) + ". Will retry later.");
            return; // Exit the loop and try again later
        }
        else
        {
            Logger::error("Failed to send data to socket " + numberToString(fd) + ": " + std::string(std::strerror(errno)));
            scheduleDisconnection(fd);
            return;
        }
    }

    _fds[index].events &= ~POLLOUT; // Disable POLLOUT if there's no more data to send
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
    int fd = _fds[index].fd;
    char buffer[512]; // 1024??
    // memset(buffer, 0, sizeof(buffer));
    ssize_t bytesRead = recv(fd, buffer, sizeof(buffer), 0);

    if(bytesRead == 0)
    {
        Logger::info("Client closed socket " + numberToString(fd) + ".");
        scheduleDisconnection(fd);
        return;
    }

    if (bytesRead < 0)
    {
        if(errno == EINTR)
        return;
        if(errno == EAGAIN || errno == EWOULDBLOCK)
        return;
        Logger::error("recv failed on socket " + numberToString(fd) + ": " + std::string(std::strerror(errno)));
        
        scheduleDisconnection(fd);
        return;
    }
    std::string data(buffer, bytesRead);

    User &user = _users[fd];
    user.appendToInputBuffer(data);
    // buffer[bytesRead] = '\0';

    std::vector<std::string> rawMessages = user.extractCompleteMessages();
    for (size_t i = 0; i < rawMessages.size(); ++i)
    {
        Message msg = Message().parse(rawMessages[i]);
        dispatchMessage(user, msg);
    }

    // Logger::debug("Received from socket " + numberToString(_fds[index].fd) + ": " + std::string(buffer));
}

void Server::handlePass(User& user, const Message& msg) {
    Logger::info("Handling command " + msg.getCommand());

    if (msg.getArgCount() < 1)
    {
        Logger::warning("PASS command received with insufficient arguments.");
        errorBuilder(user, "ERR_NEEDMOREPARAMS");
        return;
    }

    std::string password = msg.getArgs()[0];
    Logger::debug("Received password: " + password);
    if (password != _password)
    {
        Logger::warning("User on socket " + numberToString(user.getFd()) + " provided an invalid password.");
        // Send an error message back to the user here
        errorBuilder(user, "ERR_PASSWDMISMATCH");
        return;
    }
    user.setHasValidPassword(true);
}

void Server::handleNick(User& user, const Message& msg) {
    Logger::info("Handling command " + msg.getCommand());
    // Implement NICK command handling logic here
    if (msg.getArgCount() < 1)
    {
        Logger::warning("NICK command received with insufficient arguments.");
        errorBuilder(user, "ERR_NEEDMOREPARAMS");
        return;
    }
    std::string nickname = msg.getArgs()[0];
    // Check if the nickname is already in use
    if (isNicknameInUse(nickname)) {
        Logger::warning("NICK command received with a nickname that is already in use: " + nickname);
        errorBuilder(user, "ERR_NICKNAMEINUSE");
        return;
    } else {
        Logger::debug("Received nickname: " + nickname + " for user on socket " + numberToString(user.getFd()));
        user.setNickname(nickname);
    }
}

void Server::handleUser(User& user, const Message& msg) {
    Logger::info("Handling command " + msg.getCommand() + " for user on socket " + numberToString(user.getFd()));
    // Implement USER command handling logic here
    if (msg.getArgCount() < 1)
    {
        Logger::warning("USER command received with insufficient arguments.");
        errorBuilder(user, "ERR_NEEDMOREPARAMS");
        return;
    }
    std::string username = msg.getArgs()[0];
    Logger::debug("Received username: " + msg.getArgs()[0] + " for user on socket " + numberToString(user.getFd()));
    user.setUsername(msg.getArgs()[0]);
}

void Server::handleJoin(User& user, const Message& msg) {
    bool wasCreated = false;
    Logger::info("Handling command " + msg.getCommand());
    if (msg.getArgCount() < 1)
    {
        Logger::warning("JOIN command received with insufficient arguments.");
        errorBuilder(user, "ERR_NEEDMOREPARAMS");
        return;
    }
    // Implement JOIN command handling logic here
    std::string channelName = msg.getArgs()[0];

    // Check if the channel name is valid (e.g., starts with #)
    if (channelName.empty() 
        || channelName[0] != '#' 
        || (channelName.find(' ') != std::string::npos) 
        || channelName.size() < 2
        || channelName.find(',') != std::string::npos
        || channelName.find('\r') != std::string::npos
        || channelName.find('\n') != std::string::npos)
    {
        Logger::warning("Invalid channel name: " + channelName);
        errorBuilder(user, "ERR_BADCHANMASK");
        return;
        
    } 
    if (_channels.find(channelName) == _channels.end()) {
        // Channel does not exist, create it
        wasCreated = true;
        Channel newChannel;
        newChannel.setName(channelName);
        _channels[channelName] = newChannel;
        Logger::info("Channel " + channelName + " created.");
    } 
    
    Channel &channel = _channels[channelName];

    if(channel.hasUser(user.getFd()))
    {
        Logger::warning("User " + user.getNickname() + " is already in channel " + channelName);
        errorBuilder(user, "ERR_USERONCHANNEL");
        return;
    }

    if (channel.isInviteOnly() && !channel.isInvited(user.getFd()))
    {
        Logger::warning("User " + user.getNickname() + " is not invited to join channel " + channelName);
        errorBuilder(user, "ERR_INVITEONLYCHAN");
        return;
    }

    if(!channel.getChannelKey().empty() && channel.getChannelKey() != msg.getArgs()[1])
    {
        Logger::warning("User " + user.getNickname() + " provided incorrect channel key for channel " + channelName);
        errorBuilder(user, "ERR_BADCHANNELKEY");
        return;
    }

    if(!channel.addUser(user.getFd()))
    {
        Logger::warning("Failed to add user " + user.getNickname() + " to channel " + channelName + " because the channel is full.");
        errorBuilder(user, "ERR_CHANNELISFULL");
        return;
    }
    Logger::info("User " + user.getNickname() + " joined channel " + channelName);

    // Send a JOIN message to the user
    std::string joinResponseMessage = ":" + user.getNickname() + "!" + user.getUsername() + "@" + _serverName + " JOIN " + channelName;
    sendToUser(user, joinResponseMessage);

    // Broadcast the JOIN message to all users in the channel
    std::string joinToChannelMessage = ":" + user.getNickname() + "!" + user.getUsername() + "@" + _serverName + " JOIN " + channelName;
    broadcastMessage(joinToChannelMessage, user.getFd(), channelName);
    
    // Send the topic of the channel to the user
    std::string topic = channel.getTopic();
    if (topic.empty())
    {
        std::string noTopicMessage = ":" + _serverName + " 331 " + user.getNickname() + " " + channelName + " :No topic is set";
        sendToUser(user, noTopicMessage);
    }
    else
    {
        std::string topicMessage = ":" + _serverName + " 332 " + user.getNickname() + " " + channelName + " :" + topic;
        sendToUser(user, topicMessage);
    }

    // Make user operator if they are the first user in the channel
    if (wasCreated)
    { 
        if(!channel.addOperator(user.getFd()))
        {
        Logger::warning("Failed to add user " + user.getNickname() + " as operator to channel " + channelName);
        errorBuilder(user, "ERR_CHANNELISFULL");
        return;
        }
        Logger::info("User " + user.getNickname() + " is now an operator in channel " + channelName);
    }
    else
    {
        // If the channel already exists, check if the user is invited and remove the invite
        if (channel.isInvited(user.getFd()))
        {
            channel.removeInvite(user);
            Logger::info("User " + user.getNickname() + " was invited to channel " + channelName + " and has now joined.");
        }
    }

    // Send the list of users in the channel to the user
    std::set<int> usersInChannel = channel.getUsers();
    std::string userList = "";
    for (std::set<int>::iterator it = usersInChannel.begin(); it != usersInChannel.end(); ++it)
    {
        int userFd = *it;
        if (_users.find(userFd) != _users.end())
        {
            User& channelUser = _users[userFd];
            if(channel.isOperator(userFd))
                userList += "@" + channelUser.getNickname() + " ";
            else
                userList += channelUser.getNickname() + " ";
        }
    }
    std::string userListMessage = ":" + _serverName + " 353 " + user.getNickname() + " = " + channelName + " :" + userList;
    sendToUser(user, userListMessage);

    // Send End of NAMES list message to the user
    std::string endOfNamesMessage = ":" + _serverName + " 366 " + user.getNickname() + " " + channelName + " :End of /NAMES list";
    sendToUser(user, endOfNamesMessage);

    return;
}

void Server::handlePart(const Message& msg) {
    Logger::info("Handling command " + msg.getCommand());
    // Implement PART command handling logic here
}

void Server::handlePing(User& user, const Message& msg) {
    Logger::info("Handling command " + msg.getCommand());
    if ((msg.getArgCount() + (msg.getTrailing().empty() ? 0 : 1)) < 1)
    {
        Logger::warning("PING command received with insufficient arguments.");
        errorBuilder(user, "ERR_NEEDMOREPARAMS");
        return;
    }

    std::string pongResponse = msg.getArgsAsString();
    if (!msg.getTrailing().empty())
    {
        if (!pongResponse.empty())
            pongResponse += " ";
        pongResponse += msg.getTrailing();
    }

    sendToUser(user, "PONG :" + pongResponse);
}

// TODO - Refactor this handleMode
// TODO - Make sure it works as expected
void Server::handleMode(User& user, const Message& msg) {
    Logger::info("Handling command " + msg.getCommand());
    if (msg.getArgCount() < 1)
    {
        Logger::warning("MODE command received with insufficient arguments.");
        errorBuilder(user, "ERR_NEEDMOREPARAMS");
        return;
    }
    if (msg.getArgCount() == 1) {
        std::string channelName = msg.getArgs()[0];
        if (_channels.find(channelName) == _channels.end()) {
            Logger::warning("MODE command received for non-existent channel: " + channelName);
            errorBuilder(user, "ERR_NOSUCHCHANNEL");
            return;
        }
        Channel& channel = _channels[channelName];
        std::string modeString = "+";
        if (channel.isInviteOnly())
            modeString += "i";
        if (channel.isTopicRestricted())
            modeString += "t";
        if (!channel.getChannelKey().empty())
            modeString += "k";
        if (channel.getOperatorCount() > 0)
            modeString += "o";
        if (channel.getUserLimit() > 0)
                modeString += "l";
        std::string response = ":" + _serverName + " MODE " + channelName + " :" + modeString;
        sendToUser(user, response);
        return;    
    }

    std::vector<std::string> args = msg.getArgs();
    std::string target = args[0];

    if (target[0] == '#') {
        if (_channels.find(target) == _channels.end()) {
            Logger::warning("MODE command received for non-existent channel: " + target);
            errorBuilder(user, "ERR_NOSUCHCHANNEL");
            return;
        }

        Channel& channel = _channels[target];

        if (!channel.hasUser(user.getFd()) || !channel.isOperator(user.getFd())) {
            Logger::warning("User " + user.getNickname() + " tried to change channel mode without operator privileges on " + target);
            errorBuilder(user, "ERR_CHANOPRIVSNEEDED");
            return;
        }

        std::string modeChanges = args[1];
        bool adding = true;

        for (size_t i = 0; i < modeChanges.size(); ++i)
        {            
            char mode = modeChanges[i];
            if (mode == '+') {
                adding = true;
                continue;
            } else if (mode == '-') {
                adding = false;
                continue;
            }

            switch (mode) {
                case 'i':
                    channel.setInviteOnly(adding);
                    break;
                case 't':
                    channel.setTopicRestricted(adding);
                    break;
                case 'k':
                    if (adding) {
                        if (args.size() < 3) {
                            Logger::warning("MODE command received with insufficient arguments for +k mode.");
                            errorBuilder(user, "ERR_NEEDMOREPARAMS");
                            return;
                        }
                        channel.setChannelKey(args[2]);
                    } else {
                        channel.setChannelKey("");
                    }
                    break;
                case 'o': {
                    if (args.size() < 3) {
                        Logger::warning("MODE command received with insufficient arguments for +o/-o mode.");
                        errorBuilder(user, "ERR_NEEDMOREPARAMS");
                        return;
                    }

                    std::string targetNickname = args[2];
                    int targetFd = -1;

                    for (std::map<int, User>::iterator it = _users.begin(); it != _users.end(); ++it) {
                        if (it->second.getNickname() == targetNickname) {
                            targetFd = it->first;
                            break;
                        }
                    }

                    if (targetFd == -1) {
                        Logger::warning("MODE command received with non-existent user: " + targetNickname);
                        errorBuilder(user, "ERR_NOSUCHNICK");
                        return;
                    }

                    if (!channel.hasUser(targetFd)) {
                        Logger::warning("MODE command received for user not in channel: " + targetNickname);
                        errorBuilder(user, "ERR_USERNOTINCHANNEL");
                        return;
                    }

                    if (adding) {
                        channel.addOperator(targetFd);
                    } else {
                        channel.removeOperator(_users[targetFd]);
                    }
                    break;
                }
                case 'l': {
                    if (adding) {
                        if (args.size() < 3) {
                            Logger::warning("MODE command received with insufficient arguments for +l mode.");
                            errorBuilder(user, "ERR_NEEDMOREPARAMS");
                            return;
                        }
                        int userLimit = std::atoi(args[2].c_str());
                        if (userLimit <= 0) {
                            Logger::warning("MODE command received with invalid user limit: " + args[2]);
                            errorBuilder(user, "ERR_INVALIDMODEPARAM");
                            return;
                        }
                        channel.setUserLimit(userLimit);
                    } else {
                        channel.setUserLimit(0);
                    }
                    break;
                }
            default:
                Logger::warning("MODE command received with unknown mode: " + std::string(1, mode));
                errorBuilder(user, "ERR_UNKNOWNMODE");
                return;
            }
        }

    } else {
        Logger::warning("User mode changes are not supported yet.");
        errorBuilder(user, "ERR_UMODEUNKNOWNFLAG");
        return;
    }
}

void Server::handleKick(User &user, const Message& msg) {
    Logger::info("Handling command " + msg.getCommand());

    if(msg.getArgCount() < 2)
    {
        Logger::warning("KICK command received with insufficient arguments.");
        errorBuilder(user, "ERR_NEEDMOREPARAMS");
        return;
    }

    std::string channelName = msg.getArgs()[0];
    std::string targetNickname = msg.getArgs()[1];

    
    std::map<std::string, Channel>::iterator channelIt = _channels.find(channelName);
    if(channelIt == _channels.end())
    {
        Logger::warning("KICK command received for non-existent channel: " + channelName);
        errorBuilder(user, "ERR_NOSUCHCHANNEL");
        return;
    }
    
    User* targetUser = getUserByNickname(targetNickname);
    if(!targetUser)
    {
        Logger::warning("KICK command received for non-existent user: " + targetNickname);
        errorBuilder(user, "ERR_NOSUCHNICK");
        return;
    }
    
    Channel& channel = channelIt->second;

    if(!channel.hasUser(user.getFd()))
    {
        Logger::warning("User " + user.getNickname() + " is not in channel " + channelName + " and cannot invite others.");
        errorBuilder(user, "ERR_NOTONCHANNEL");
        return;
    }

    if(!channel.isOperator(user.getFd()))
    {
        Logger::warning("User " + user.getNickname() + " is not an operator in channel " + channelName + " and cannot invite others.");
        errorBuilder(user, "ERR_CHANOPRIVSNEEDED");
        return;
    }

    if(!channel.hasUser(targetUser->getFd()))
    {
        Logger::warning("User " + targetNickname + " is not in channel " + channelName + " and cannot be invited.");
        errorBuilder(user, "ERR_USERNOTINCHANNEL");
        return;
    }

    std::string reason;

    if(msg.hasTrailing())
        reason = msg.getTrailing();
    else if(msg.getArgCount() >= 3)
        reason = msg.getArgs()[2];
    else
        reason = user.getNickname();

    channel.removeUser(*targetUser);
    Logger::info("User " + user.getNickname() + " kicked " + targetNickname + " from channel " + channelName + " for reason: " + reason);
    
    std::string notification =
        ":" + user.getNickname()
        + "!" + user.getUsername()
        + "@" + _serverName
        + " KICK "
        + channelName
        + " "
        + targetNickname
        + " :"
        + reason;

    sendToUser(*targetUser, notification);
    broadcastMessage(notification, user.getFd(), channelName);

    if(channel.isOperator(targetUser->getFd()))
    {
        channel.removeOperator(*targetUser);
        Logger::info("User " + targetNickname + " was an operator in channel " + channelName + " and has been removed from the operator list.");
    }
    
    if(channel.isInvited(targetUser->getFd()))
    {
        channel.removeInvite(*targetUser);
        Logger::info("User " + targetNickname + " was invited to channel " + channelName + " and has now been kicked.");
    }

    channel.removeUser(*targetUser);
}

void Server::handleInvite(User &user, const Message& msg) {
    Logger::info("Handling command " + msg.getCommand());

    if(msg.getArgCount() < 2)
    {
        Logger::warning("INVITE command received with insufficient arguments.");
        errorBuilder(user, "ERR_NEEDMOREPARAMS");
        return;
    }

    std::string targetNickname = msg.getArgs()[0];
    std::string channelName = msg.getArgs()[1];

    
    std::map<std::string, Channel>::iterator channelIt = _channels.find(channelName);
    if(channelIt == _channels.end())
    {
        Logger::warning("INVITE command received for non-existent channel: " + channelName);
        errorBuilder(user, "ERR_NOSUCHCHANNEL");
        return;
    }
    
    User* targetUser = getUserByNickname(targetNickname);
    if(!targetUser)
    {
        Logger::warning("INVITE command received for non-existent user: " + targetNickname);
        errorBuilder(user, "ERR_NOSUCHNICK");
        return;
    }

    Channel& channel = channelIt->second;

    if(!channel.hasUser(user.getFd()))
    {
        Logger::warning("User " + user.getNickname() + " is not in channel " + channelName + " and cannot invite others.");
        errorBuilder(user, "ERR_NOTONCHANNEL");
        return;
    }

    if(!channel.isOperator(user.getFd()))
    {
        Logger::warning("User " + user.getNickname() + " is not an operator in channel " + channelName + " and cannot invite others.");
        errorBuilder(user, "ERR_CHANOPRIVSNEEDED");
        return;
    }

    if(channel.hasUser(targetUser->getFd()))
    {
        Logger::warning("User " + targetNickname + " is already in channel " + channelName + " and cannot be invited.");
        errorBuilder(user, "ERR_USERONCHANNEL");
        return;
    }

    channel.inviteUser(*targetUser);
    Logger::info("User " + user.getNickname() + " invited " + targetNickname + " to channel " + channelName);
    
    std::string confirmation =
        ":" + _serverName
        + " 341 "
        + user.getNickname()
        + " "
        + targetNickname
        + " "
        + channelName;

    sendToUser(user, confirmation);
    
    std::string notification =
        ":" + user.getNickname()
        + "!" + user.getUsername()
        + "@" + _serverName
        + " INVITE "
        + targetNickname
        + " :"
        + channelName;

    sendToUser(*targetUser, notification);
}

void Server::handleTopic(User& user, const Message& msg) {
    Logger::info("Handling command " + msg.getCommand());
    if (msg.getArgCount() < 1) {
        Logger::warning("TOPIC command received with insufficient arguments.");
        errorBuilder(user, "ERR_NEEDMOREPARAMS");
        return;
    }

    const std::string channelName = msg.getArgs()[0];
    if (_channels.find(channelName) == _channels.end()) {
        Logger::warning("TOPIC command received for non-existent channel: " + channelName);
        errorBuilder(user, "ERR_NOSUCHCHANNEL");
        return;
    }

    Channel& channel = _channels[channelName];
    if (!channel.hasUser(user.getFd())) {
        Logger::warning("TOPIC command received from user not in channel: " + user.getNickname());
        errorBuilder(user, "ERR_NOTONCHANNEL");
        return;
    }

    if (!msg.getTrailing().empty()) {
        if (channel.isTopicRestricted() && !channel.isOperator(user.getFd())) {
            Logger::warning("TOPIC change denied for non-operator user: " + user.getNickname());
            errorBuilder(user, "ERR_CHANOPRIVSNEEDED");
            return;
        }
        channel.setTopic(msg.getTrailing());
        return;
    }

    std::string response = ":" + _serverName + " 332 " + user.getNickname() + " " + channelName + " :" + channel.getTopic();
    sendToUser(user, response);
}

void Server::handlePrivMsg(const Message& msg) {
    Logger::info("Handling command " + msg.getCommand());
    // Implement PRIVMSG command handling logic here
    if (msg.getArgCount() < 1)
    {
        Logger::warning("PRIVMSG command received with insufficient arguments.");
        errorBuilder(user, "ERR_NEEDMOREPARAMS");
        return;
    }
    std::string target = msg.getArgs()[0];
    std::string message = msg.getTrailing();

    // msg.printMessage();

    if(target[0] == '#')
    {
        // Message to a channel
        if(_channels.find(target) == _channels.end())
        {
            Logger::warning("PRIVMSG command received for non-existent channel: " + target);
            errorBuilder(user, "ERR_NOSUCHCHANNEL");
            return;
        }
        Channel& channel = _channels[target];
        if (!channel.hasUser(user.getFd()))
        {
            Logger::warning("User " + user.getNickname() + " is not in channel " + target + " and cannot send messages to it.");
            errorBuilder(user, "ERR_CANNOTSENDTOCHAN");
            return;
        }
        if(channel.isModerated() && !channel.isOperator(user.getFd()))
        {
            Logger::warning("User " + user.getNickname() + " is not an operator in moderated channel " + target + " and cannot send messages to it.");
            errorBuilder(user, "ERR_CANNOTSENDTOCHAN");
            return;
        }
        std::string formattedMessage = ":" + user.getNickname() + "!" + user.getUsername() + "@" + _serverName + " PRIVMSG " + target + " :" + message;
        broadcastMessage(formattedMessage, user.getFd(), target);
    } else {
        // Private message to a user
        if(!isNicknameInUse(target))
        {
            Logger::warning("PRIVMSG command received for non-existent user: " + target);
            errorBuilder(user, "ERR_NOSUCHNICK");
            return;
        }
        std::string formattedMessage = ":" + user.getNickname() + "!" + user.getUsername() + "@" + _serverName + " PRIVMSG " + target + " :" + message;
        User* targetUser = getUserByNickname(target);
        if(targetUser == NULL)
        {
            Logger::warning("PRIVMSG command received for non-existent user: " + target);
            errorBuilder(user, "ERR_NOSUCHNICK");
            return;
        }
        sendToUser(*targetUser, formattedMessage);
    }
}

void Server::handleUnknown(const Message& msg) {
    Logger::info("Handling command " + msg.getCommand());
    // Implement handling for unknown commands here
}

void Server::dispatchMessage(User& user, const Message& msg) {
    std::string cmd = msg.getCommand();
    // remove \n and \r if present
    if (!cmd.empty() && cmd[cmd.size() - 1] == '\n')
        cmd.erase(cmd.size() - 1);
    if (!cmd.empty() && cmd[cmd.size() - 1] == '\r')
        cmd.erase(cmd.size() - 1);
    toLowerCase(cmd);

    // if (!user.isRegistered() 
    //     && cmd != PASS_STR 
    //     && cmd != NICK_STR 
    //     && cmd != USER_STR 
    //     && cmd != PING_STR)
    // {
    //     Logger::warning("User on socket " + numberToString(user.getFd()) + " is not registered and sent command: " + cmd);
    //     // Send an error message back to the user here
    //     sendToUser(user, ":" + _serverName + " 451 * :You have not registered");
    //     return;
    // }
    if (cmd == PASS_STR && !user.getHasValidPassword() && !user.isRegistered())
        handlePass(user, msg);
    else if (cmd == NICK_STR)
        handleNick(user, msg);
    else if (cmd == USER_STR && !user.isRegistered())
        handleUser(user, msg);
    else if (!user.isRegistered())
    {
        Logger::warning("User on socket " + numberToString(user.getFd()) + " is not registered and sent command: " + cmd);
        errorBuilder(user, "ERR_NOTREGISTERED");
        return;
    }
    if (user.getHasValidPassword() && user.hasNickname() && user.hasUsername() && !user.isRegistered())
    {
        user.setIsRegistered(true);
    }
    if(user.isRegistered())
    {
        if (cmd == JOIN_STR)
            handleJoin(user, msg);
        else if (cmd == PART_STR)
            handlePart(msg);
        else if (cmd == PING_STR)
            handlePing(user, msg);
        else if (cmd == MODE_STR)
            handleMode(user, msg);
        else if (cmd == KICK_STR)
            handleKick(user, msg);
        else if (cmd == INVITE_STR)
            handleInvite(user, msg);
        else if (cmd == TOPIC_STR)
            handleTopic(user, msg);
        else if (cmd == PRIVMSG_STR)
            handlePrivMsg(user, msg);
        else
            handleUnknown(msg);
    }
}

void Server::sendToUser(User &user, const std::string &message)
{
    user.appendToOutputBuffer(message + "\r\n");
    enablePollOut(user.getFd());
}
    
void Server::enablePollOut(int fd)
{
    for (size_t i = 0; i < _fds.size(); ++i)
    {
        if (_fds[i].fd == fd)
        {
            _fds[i].events |= POLLOUT;
            return;
        }
    }
}

void Server::errorBuilder(User& user, const std::string& errorCode) {
    std::pair<int, std::string> errorMessage = getErrorMessage(errorCode);
    int errorCodeInt = errorMessage.first;
    const std::string& errorMessageStr = errorMessage.second;
    std::string response = ":" + _serverName + " " + numberToString(errorCodeInt) + " " + user.getNickname() + " " + errorMessageStr;
    sendToUser(user, response);
}

bool Server::isNicknameInUse(const std::string& nickname) const {
    // Find in _users map if any user has the same nickname
    for (std::map<int, User>::const_iterator it = _users.begin(); it != _users.end(); ++it) {
        if (it->second.getNickname() == nickname) {
            return true;
        }
    }
    return false;
}

void Server::signalHandler(int signal)
{
    if (signal == SIGINT)
    {
        // Clean ^C output by moving the cursor to the beginning of the line and clearing it
        std::cout << "\r\033[K";
        Server::setServerStop(true);
    }
}

void Server::setServerStop(bool value) {
    _serverStop = value;
}

bool Server::getServerStop() {
    return _serverStop;
}

volatile bool Server::_serverStop = false;

void Server::serverShutdown() {
    // Send a message to all connected users about the server shutdown
    for (std::map<int, User>::iterator it = _users.begin(); it != _users.end(); ++it) {
        User& user = it->second;
        errorBuilder(user, "ERR_SERVERSHUTDOWN");
    }
}

void Server::broadcastMessage(const std::string& message, int senderFd, const std::string& channelName) {

    Channel& channel = _channels[channelName];
    std::set<int> users = channel.getUsers();

    for (std::set<int>::iterator it = users.begin(); it != users.end(); ++it) {
        if (*it != senderFd) {
            sendToUser(_users[*it], message);
        }
    }
}

User *Server::getUserByNickname(const std::string &nickname)
{
    for (std::map<int, User>::iterator it = _users.begin(); it != _users.end(); ++it)
    {
        if (it->second.getNickname() == nickname)
            return &(it->second);
    }
    return NULL;
}

void Server::disconnectClient(int fd)
{
    std::map<int, User>::iterator userIt = _users.find(fd);

    if (userIt == _users.end())
    {
        Logger::warning(
            "Cannot disconnect unknown client on socket "
            + numberToString(fd) + "."
        );

        for (std::vector<pollfd>::iterator it = _fds.begin();
             it != _fds.end();)
        {
            if (it->fd == fd)
                it = _fds.erase(it);
            else
                ++it;
        }

        return;
    }

    User &user = userIt->second;
    std::string nickname = user.getNickname();

    Logger::info(
        "Disconnecting client "
        + nickname
        + " on socket "
        + numberToString(fd) + "."
    );

    std::map<std::string, Channel>::iterator channelIt =
        _channels.begin();

    while (channelIt != _channels.end())
    {
        Channel &channel = channelIt->second;

        if (channel.isOperator(fd))
            channel.removeOperator(user);

        if (channel.isInvited(fd))
            channel.removeInvite(user);

        if (channel.hasUser(fd))
            channel.removeUser(user);

        if (channel.getUserCount() == 0)
        {
            std::map<std::string, Channel>::iterator toErase =
                channelIt;

            ++channelIt;
            _channels.erase(toErase);
        }
        else
        {
            ++channelIt;
        }
    }

    for (std::vector<pollfd>::iterator it = _fds.begin();
         it != _fds.end();)
    {
        if (it->fd == fd)
            it = _fds.erase(it);
        else
            ++it;
    }

    _users.erase(userIt);

    if (close(fd) < 0)
    {
        Logger::warning(
            "close failed on socket "
            + numberToString(fd)
            + ": "
            + std::string(std::strerror(errno))
        );
    }
}

void Server::scheduleDisconnection(int fd)
{
    if(fd < 0 || fd == _socket)
    {
        Logger::warning("Attempted to schedule disconnection for invalid socket " + numberToString(fd) + ".");
        return;
    }
    Logger::info("Scheduling disconnection for client on socket " + numberToString(fd) + ".");
    _clientsToDisconnect.insert(fd);
}

void Server::processDisconnections()
{
    while (!_clientsToDisconnect.empty())
    {
        std::set<int>::iterator it =
            _clientsToDisconnect.begin();

        int fd = *it;
        _clientsToDisconnect.erase(it);

        disconnectClient(fd);
    }
}

