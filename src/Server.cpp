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
    // Implement MODE command handling logic here
    if (msg.getArgCount() < 1)
    {
        Logger::warning("MODE command received with insufficient arguments.");
        errorBuilder(user, "ERR_NEEDMOREPARAMS");
        return;
    }
    if (msg.getArgCount() == 1) {
        // Show channel modes
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
        if (channel.getUserLimit() > 0)
            modeString += "l";
        std::string response = ":" + _serverName + " MODE " + channelName + " :" + modeString;
        sendToUser(user, response);
        return;    
    }
    std::vector<std::string> args = msg.getArgs();
    std::string target = args[0];
    // Check if the target is a channel
    if (target[0] == '#') {
        // Validate channel exists
        if (_channels.find(target) == _channels.end()) {
            Logger::warning("MODE command received for non-existent channel: " + target);
            errorBuilder(user, "ERR_NOSUCHCHANNEL");
            return;
        }
        Channel& channel = _channels[target];
        std::string modeChanges = args[1];
        // Process mode changes ONLY i, t, k, o, l for now
        for (size_t i = 0; i < modeChanges.size(); ++i)
        {            
            char mode = modeChanges[i];
            bool adding = true;
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
                    }
                    // Find the user by nickname
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
                errorBuilder(user, "ERR_UMODEUNKNOWNFLAG");
                return;
            }
        }

    } else {
        // Handle user mode changes (not implemented in this version)
        Logger::warning("User mode changes are not supported yet.");
        errorBuilder(user, "ERR_UMODEUNKNOWNFLAG");
        return;
    }


}

void Server::handleKick(const Message& msg) {
    Logger::info("Handling command " + msg.getCommand());
    // Implement KICK command handling logic here
}

void Server::handleInvite(const Message& msg) {
    Logger::info("Handling command " + msg.getCommand());
    // Implement INVITE command handling logic here
}

void Server::handleTopic(User& user, const Message& msg) {
    Logger::info("Handling command " + msg.getCommand());
    // Check if the number of arguments is sufficient
    if (msg.getArgCount() < 1) 
    {
        Logger::warning("TOPIC command received with insufficient arguments.");
        errorBuilder(user, "ERR_NEEDMOREPARAMS");
        return;
    }
    // Check if the channel exists
    std::string channelName = msg.getArgs()[0];
    if (_channels.find(channelName) == _channels.end()) 
    {
        Logger::warning("TOPIC command received for non-existent channel: " + channelName);
        errorBuilder(user, "ERR_NOSUCHCHANNEL");
        return;
    }
    // Check if the user is in the channel
    Channel& channel = _channels[channelName];
    if (!channel.hasUser(user.getFd()))
    {
        Logger::warning("User " + user.getNickname() + " is not in channel "+ channelName + " and cannot set the topic.");
        errorBuilder(user, "ERR_NOTONCHANNEL");
        return;
    }
    // Check if the user is an operator if the channel is topic restricted
    if (channel.isTopicRestricted() && !channel.isOperator(user.getFd()))
    {
        Logger::warning("User " + user.getNickname() + " is not an operator in channel " + channelName + " and cannot set the topic.");
        errorBuilder(user, "ERR_CHANOPRIVSNEEDED");
        return;
    }
    // If the message does not have a trailing part, return the current topic
    if(!msg.hasTrailing())
    {
        Logger::info("User " + user.getNickname() + " requested the current topic for channel " + channelName);
        if(channel.getTopic().empty())
        {
            std::string noTopicMessage = ":" + _serverName + " 331 " + user.getNickname() + " " + channelName + " :No topic is set";
            sendToUser(user, noTopicMessage);
        }
        else
        {
            std::string topicMessage = ":" + _serverName + " 332 " + user.getNickname() + " " + channelName + " :" + channel.getTopic();
            sendToUser(user, topicMessage);
        }
        return;
    }
    // If the message has a trailing part, set the topic; otherwise, return the current topic
    if(channel.isTopicRestricted() && !channel.isOperator(user.getFd()))
    {
        Logger::warning("User " + user.getNickname() + " is not an operator in channel " + channelName + " and cannot set the topic.");
        errorBuilder(user, "ERR_CHANOPRIVSNEEDED");
        return;
    }
    // Set the topic for the channel
    channel.setTopic(msg.getTrailing());
    Logger::info("User " + user.getNickname() + " set the topic for channel " + channelName + " to: " + msg.getTrailing());
    std::string topicSetMessage = ":" + user.getNickname() + "!" + user.getUsername() + "@" + _serverName + " TOPIC " + channelName + " :" + msg.getTrailing();
    broadcastMessage(topicSetMessage, user.getFd(), channelName);
}

void Server::handlePrivMsg(User& user, const Message& msg) {
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
        const User* targetUser = getUserByNickname(target);
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
            handleKick(msg);
        else if (cmd == INVITE_STR)
            handleInvite(msg);
        else if (cmd == TOPIC_STR)
            handleTopic(user, msg);
        else if (cmd == PRIVMSG_STR)
            handlePrivMsg(user, msg);
        else
            handleUnknown(msg);
    }
}

void Server::sendToUser(const User &user, const std::string &message)
{
    std::string response = message + "\r\n";
    send(user.getFd(), response.c_str(), response.size(), 0);
}

void Server::errorBuilder(User& user, const std::string& errorCode) {
    std::pair<int, std::string> errorMessage = getErrorMessage(errorCode);
    int errorCodeInt = errorMessage.first;
    const std::string& errorMessageStr = errorMessage.second;
    // :<server_name> <error_code> <nickname> :<error_message>
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

const User *Server::getUserByNickname(const std::string &nickname) const
{
    for (std::map<int, User>::const_iterator it = _users.begin(); it != _users.end(); ++it)
    {
        if (it->second.getNickname() == nickname)
            return &(it->second);
    }
    return NULL;
}
