#include "Server.hpp"
#include "Logger.hpp"
#include <ctime>

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

Server::Server(const std::string &port, const std::string &password) : _port(0), _password(password), _serverName("host")
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
    std::time_t now = std::time(NULL);
    char *date = std::ctime(&now);

    if (date != NULL)
    {
        _creationDate = date;

        // std::ctime termina con '\n'
        if (!_creationDate.empty()
            && _creationDate[_creationDate.size() - 1] == '\n')
        {
            _creationDate.erase(_creationDate.size() - 1);
        }
    }
    else
    {
        _creationDate = "unknown";
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

        if(_clientsToDisconnect.find(fd) != _clientsToDisconnect.end())
            break;
    }

    // Logger::debug("Received from socket " + numberToString(_fds[index].fd) + ": " + std::string(buffer));
}

void Server::handlePass(User& user, const Message& msg) {
    Logger::info("Handling command " + msg.getCommand());

    if (msg.getArgCount() < 1)
    {
        Logger::warning("PASS command received with insufficient arguments.");
        errorBuilder(user, "ERR_NEEDMOREPARAMS", msg.getCommand());
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
        errorBuilder(user, "ERR_NEEDMOREPARAMS", msg.getCommand());
        return;
    }
    std::string nickname = msg.getArgs()[0];
    Logger::debug("Nick name ---> " + nickname);
    // Check if the nickname is already in use
    if (isNicknameInUse(nickname)) {
        Logger::warning("NICK command received with a nickname that is already in use: " + nickname);
        errorBuilder(user, "ERR_NICKNAMEINUSE", nickname);
        return;
    } else {
        Logger::debug("Received nickname: " + nickname + " for user on socket " + numberToString(user.getFd()));
        if(user.isRegistered())
            sendToUser(user, ":" + user.getNickname() + "!" + user.getUsername() + "@" + _serverName + " NICK :" + nickname);
        user.setNickname(nickname);
    }
}

void Server::handleUser(User& user, const Message& msg) {
    Logger::info("Handling command " + msg.getCommand() + " for user on socket " + numberToString(user.getFd()));
    // Implement USER command handling logic here
    if (msg.getArgCount() < 1)
    {
        Logger::warning("USER command received with insufficient arguments.");
        errorBuilder(user, "ERR_NEEDMOREPARAMS", msg.getCommand());
        return;
    }
    std::string username = msg.getArgs()[0];
    Logger::debug("Received username: " + msg.getArgs()[0] + " for user on socket " + numberToString(user.getFd()));
    Logger::debug(msg.getArgs()[0]);
    user.setUsername(msg.getArgs()[0]);
}

void Server::handleJoin(User& user, const Message& msg) {
    bool wasCreated = false;
    Logger::info("Handling command " + msg.getCommand());
    int msgArgCount = msg.getArgCount();
    if (msgArgCount < 1)
    {
        Logger::warning("JOIN command received with insufficient arguments.");
        errorBuilder(user, "ERR_NEEDMOREPARAMS", msg.getCommand());
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
        errorBuilder(user, "ERR_BADCHANMASK", channelName);
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
        errorBuilder(user, "ERR_USERONCHANNEL", user.getNickname() + " " + channelName);
        return;
    }
    // +i
    if (channel.isInviteOnly() && !channel.isInvited(user.getFd()))
    {
        Logger::warning("User " + user.getNickname() + " is not invited to join channel " + channelName);
        errorBuilder(user, "ERR_INVITEONLYCHAN", channelName);
        return;
    }
    // +k
    std::string providedKey;

    if (msgArgCount >= 2)
        providedKey = msg.getArgs()[1];

    if(!channel.getChannelKey().empty() && providedKey != channel.getChannelKey())
    {
        Logger::warning("User " + user.getNickname() + " provided incorrect channel key for channel " + channelName);
        sendToUser(user, ":" + _serverName+ " 475 "+ user.getNickname()+ " "+ channelName+ " :Cannot join channel (+k)");
        return;
    }
    // +l
    if(channel.isFull())
    {
        Logger::warning("User " + user.getNickname() + " cannot join channel " + channelName + " because it is full.");
        errorBuilder(user, "ERR_CHANNELISFULL", channelName);
        return;
    }

    if(!channel.addUser(user.getFd()))
    {
        Logger::warning("Failed to add user " + user.getNickname() + " to channel " + channelName + " because the channel is full.");
        errorBuilder(user, "ERR_CHANNELISFULL", channelName);
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
        errorBuilder(user, "ERR_CHANNELISFULL", channelName);
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

void Server::handlePart(User& user, const Message& msg) {
    Logger::info("Handling command " + msg.getCommand());
    int argSize = msg.getArgCount();
    
    if (argSize < 1)
    {
        Logger::warning("PART command received with insufficient arguments.");
        errorBuilder(user, "ERR_NEEDMOREPARAMS", msg.getCommand());
        return;
    }
    
    std::string nickname = user.getNickname();
    std::string channelList = msg.getArgs()[0];
    size_t start = 0;
    std::string reason = "";
    
    if(msg.hasTrailing())
        reason = msg.getTrailing();
    else if(msg.getArgCount() >= 2)
        reason = msg.getArgs()[1];
    else
        reason = nickname;

    while (start < channelList.size())
    {
        size_t comma = channelList.find(',', start);
        std::string channelName;

        if (comma == std::string::npos)
        {
            channelName = channelList.substr(start);
            start = channelList.size();
        }
        else
        {
            channelName = channelList.substr(
                start,
                comma - start
            );
            start = comma + 1;
        }

        Logger::debug("channelName = " + channelName);

        if (_channels.find(channelName) == _channels.end())
        {
            Logger::warning(
                "PART command received for non-existent channel: "
                + channelName
            );
            errorBuilder(
                user,
                "ERR_NOSUCHCHANNEL",
                channelName
            );
            continue;
        }

        Channel& channel = _channels[channelName];

        if (!channel.hasUser(user.getFd()))
        {
            Logger::warning(
                "User " + nickname
                + " is not in channel " + channelName
            );
            errorBuilder(
                user,
                "ERR_NOTONCHANNEL",
                channelName
            );
            continue;
        }

        Logger::debug(
            "PART command received with channel: " + channelName
        );
        if(channel.isOperator(user.getFd()))
        {
            channel.removeOperator(user);
            Logger::info("User " + nickname + " was an operator in channel " + channelName + " and has been removed from the operator list.");
        }
        
        if(channel.isInvited(user.getFd()))
        {
            channel.removeInvite(user);
            Logger::info("User " + nickname + " was invited to channel " + channelName + " and has now been kicked.");
        }

        channel.removeUser(user);
        std::string response =
                ":" + user.getNickname() +
                "!" + user.getUsername() +
                "@" + _serverName +
                " PART " + channelName +
                " :" + reason;

        broadcastMessage(response, user.getFd(), channelName);
        sendToUser(user, response);

        if (channel.getUserCount() == 0)
        {
            Logger::info("Channel " + channelName + " is empty, removing it");
            _channels.erase(channelName);
        }
    }
}

void Server::handlePing(User& user, const Message& msg) {
    Logger::info("Handling command " + msg.getCommand());
    if ((msg.getArgCount() + (msg.getTrailing().empty() ? 0 : 1)) < 1)
    {
        Logger::warning("PING command received with insufficient arguments.");
        errorBuilder(user, "ERR_NEEDMOREPARAMS", msg.getCommand());
        return;
    }

    std::string pongResponse = msg.getArgsAsString();
    if (!msg.getTrailing().empty())
    {
        if (!pongResponse.empty())
            pongResponse += " ";
        pongResponse += msg.getTrailing();
    }
    Logger::debug(":" + _serverName + " PONG " + _serverName + " :" + pongResponse);
    sendToUser(user, ":" + _serverName + " PONG " + _serverName + " :" + pongResponse);
}

void Server::handleMode(User& user, const Message& msg) {
    Logger::info("Handling command " + msg.getCommand());
    if (msg.getArgCount() < 1)
    {
        Logger::warning("MODE command received with insufficient arguments.");
        errorBuilder(user, "ERR_NEEDMOREPARAMS", msg.getCommand());
        return;
    }
    if (msg.getArgCount() == 1) {
        std::string channelName = msg.getArgs()[0];
        if (_channels.find(channelName) == _channels.end()) {
            Logger::warning("MODE command received for non-existent channel: " + channelName);
            errorBuilder(user, "ERR_NOSUCHCHANNEL", channelName);
            return;
        }
        Channel& channel = _channels[channelName];
        std::string modeString = "+";
        std::string parameters = "";

        if (channel.isInviteOnly())
            modeString += "i";

        if (channel.isTopicRestricted())
            modeString += "t";

        if (!channel.getChannelKey().empty())
        {
            modeString += "k";
            parameters += " " + channel.getChannelKey();
        }

        if (channel.getUserLimit() > 0)
        {
            modeString += "l";
            parameters += " " + numberToString(channel.getUserLimit());
        }

        std::string response = ":" + _serverName + " 324 " + user.getNickname() + " " + channelName + " " + modeString + parameters;
        sendToUser(user, response);
        return;    
    }

    std::vector<std::string> args = msg.getArgs();
    std::string target = args[0];

    if (target[0] == '#') 
    {
        if (_channels.find(target) == _channels.end()) {
            Logger::warning("MODE command received for non-existent channel: " + target);
            errorBuilder(user, "ERR_NOSUCHCHANNEL", target);
            return;
        }

        Channel& channel = _channels[target];
        std::string channelName = channel.getName();

        if (!channel.hasUser(user.getFd()) || !channel.isOperator(user.getFd())) {
            Logger::warning("User " + user.getNickname() + " tried to change channel mode without operator privileges on " + target);
            errorBuilder(user, "ERR_CHANOPRIVSNEEDED", channelName);
            return;
        }

        std::string modeChanges = args[1];
        bool adding = true;
        std::string appliedModes = "";
        std::string appliedParameters = "";
        size_t parameterIndex = 2;
        char currentSign;
        char lastSign = '\0';

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
            currentSign = adding ? '+' : '-';

            if (lastSign != currentSign) {
                appliedModes += currentSign;
                lastSign = currentSign;
            }

            switch (mode) {
                case 'i':
                    channel.setInviteOnly(adding);
                    appliedModes += mode;
                    break;
                case 't':
                    channel.setTopicRestricted(adding);
                    appliedModes += mode;
                    break;
                case 'k':
                    if (adding) {
                        if (args.size() < parameterIndex + 1) {
                            Logger::warning("MODE command received with insufficient arguments for +k mode.");
                            errorBuilder(user, "ERR_NEEDMOREPARAMS", msg.getCommand());
                            continue;
                        }
                        std::string providedKey = args[parameterIndex++];
                        channel.setChannelKey(providedKey);
                        if (!appliedParameters.empty())
                        {
                            appliedParameters += " ";
                        }
                        appliedParameters += providedKey;
                        } else {
                            channel.setChannelKey("");
                        }
                        
                        appliedModes += mode;
                    break;
                case 'o': {
                    if (args.size() < parameterIndex + 1) {
                        Logger::warning("MODE command received with insufficient arguments for +o/-o mode.");
                        errorBuilder(user, "ERR_NEEDMOREPARAMS", msg.getCommand());
                        continue;
                    }

                    std::string targetNickname = args[parameterIndex++];
                    int targetFd = -1;

                    for (std::map<int, User>::iterator it = _users.begin(); it != _users.end(); ++it) {
                        if (it->second.getNickname() == targetNickname) {
                            targetFd = it->first;
                            break;
                        }
                    }

                    if (targetFd == -1) {
                        Logger::warning("MODE command received with non-existent user: " + targetNickname);
                        errorBuilder(user, "ERR_NOSUCHNICK", targetNickname);
                        continue;
                    }

                    if (!channel.hasUser(targetFd)) {
                        Logger::warning("MODE command received for user not in channel: " + targetNickname);
                        errorBuilder(user, "ERR_USERNOTINCHANNEL", targetNickname + " " + channelName);
                        continue;
                    }

                    if (adding) {
                        channel.addOperator(targetFd);
                    } else {
                        channel.removeOperator(_users[targetFd]);
                    }
                    if (!appliedParameters.empty())
                        appliedParameters += " ";
                    appliedParameters += targetNickname;
                    appliedModes += mode;
                    break;
                }
                case 'l': {
                    if (adding) {
                        if (args.size() < parameterIndex + 1) {
                            Logger::warning("MODE command received with insufficient arguments for +l mode.");
                            errorBuilder(user, "ERR_NEEDMOREPARAMS", msg.getCommand());
                            continue;
                        }
                        // Check userLimit is numeric
                        std::string parameter = args[parameterIndex++];
                        int userLimit = std::atoi(parameter.c_str());

                        if (userLimit <= 0 || !isNumber(parameter)) {
                            Logger::warning("MODE command received with invalid user limit: " + parameter);
                            errorBuilder(user, "ERR_INVALIDMODEPARAM");
                            continue;
                        }
                        channel.setUserLimit(userLimit);
                        if (!appliedParameters.empty())
                            appliedParameters += " ";
                        appliedParameters += parameter;
                    } else {
                        channel.setUserLimit(0);
                    }
                    appliedModes += mode;
                    break;
                }
            default:
                Logger::warning("MODE command received with unknown mode: " + std::string(1, mode));
                errorBuilder(user, "ERR_UNKNOWNMODE", std::string(1, mode));
                continue;
            }
        }
        if (!appliedModes.empty()) 
        {
            std::string modeMessage =
                ":" + user.getNickname() +
                "!" + user.getUsername() +
                "@" + _serverName +
                " MODE " + target +
                " " + appliedModes;

            if (!appliedParameters.empty())
                modeMessage += " " + appliedParameters;

            broadcastMessage(modeMessage, user.getFd(), target);
            sendToUser(user, modeMessage);
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
        errorBuilder(user, "ERR_NEEDMOREPARAMS", msg.getCommand());
        return;
    }

    std::string channelName = msg.getArgs()[0];
    std::string targetNickname = msg.getArgs()[1];

    
    std::map<std::string, Channel>::iterator channelIt = _channels.find(channelName);
    if(channelIt == _channels.end())
    {
        Logger::warning("KICK command received for non-existent channel: " + channelName);
        errorBuilder(user, "ERR_NOSUCHCHANNEL", channelName);
        return;
    }
    
    User* targetUser = getUserByNickname(targetNickname);
    if(!targetUser)
    {
        Logger::warning("KICK command received for non-existent user: " + targetNickname);
        errorBuilder(user, "ERR_NOSUCHNICK",targetNickname);
        return;
    }
    
    Channel& channel = channelIt->second;

    if(!channel.hasUser(user.getFd()))
    {
        Logger::warning("User " + user.getNickname() + " is not in channel " + channelName + " and cannot invite others.");
        errorBuilder(user, "ERR_NOTONCHANNEL", channelName);
        return;
    }

    if(!channel.isOperator(user.getFd()))
    {
        Logger::warning("User " + user.getNickname() + " is not an operator in channel " + channelName + " and cannot invite others.");
        errorBuilder(user, "ERR_CHANOPRIVSNEEDED", channelName);
        return;
    }

    if(!channel.hasUser(targetUser->getFd()))
    {
        Logger::warning("User " + targetNickname + " is not in channel " + channelName + " and cannot be invited.");
        errorBuilder(user, "ERR_USERNOTINCHANNEL", targetNickname + " " + channelName);
        return;
    }

    std::string reason;

    if(msg.hasTrailing())
        reason = msg.getTrailing();
    else if(msg.getArgCount() >= 3)
        reason = msg.getArgs()[2];
    else
        reason = user.getNickname();

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

    sendToUser(user, notification);
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
        errorBuilder(user, "ERR_NEEDMOREPARAMS", msg.getCommand());
        return;
    }

    std::string targetNickname = msg.getArgs()[0];
    std::string channelName = msg.getArgs()[1];

    
    std::map<std::string, Channel>::iterator channelIt = _channels.find(channelName);
    if(channelIt == _channels.end())
    {
        Logger::warning("INVITE command received for non-existent channel: " + channelName);
        errorBuilder(user, "ERR_NOSUCHCHANNEL", channelName);
        return;
    }
    
    User* targetUser = getUserByNickname(targetNickname);
    if(!targetUser)
    {
        Logger::warning("INVITE command received for non-existent user: " + targetNickname);
        errorBuilder(user, "ERR_NOSUCHNICK",targetNickname);
        return;
    }

    Channel& channel = channelIt->second;

    if(!channel.hasUser(user.getFd()))
    {
        Logger::warning("User " + user.getNickname() + " is not in channel " + channelName + " and cannot invite others.");
        errorBuilder(user, "ERR_NOTONCHANNEL", channelName);
        return;
    }

    if(!channel.isOperator(user.getFd()))
    {
        Logger::warning("User " + user.getNickname() + " is not an operator in channel " + channelName + " and cannot invite others.");
        errorBuilder(user, "ERR_CHANOPRIVSNEEDED", channelName);
        return;
    }

    if(channel.hasUser(targetUser->getFd()))
    {
        Logger::warning("User " + targetNickname + " is already in channel " + channelName + " and cannot be invited.");
        errorBuilder(user, "ERR_USERONCHANNEL", targetNickname + " " + channelName);
        return;
    }

    channel.inviteUser(*targetUser);
    Logger::info("User " + user.getNickname() + " invited " + targetNickname + " to channel " + channelName);

    channel.removeInvite(user);
    
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
    // Check if the number of arguments is sufficient
    if (msg.getArgCount() < 1) 
    {
        Logger::warning("TOPIC command received with insufficient arguments.");
        errorBuilder(user, "ERR_NEEDMOREPARAMS", msg.getCommand());
        return;
    }
    // Check if the channel exists
    std::string channelName = msg.getArgs()[0];
    if (_channels.find(channelName) == _channels.end()) 
    {
        Logger::warning("TOPIC command received for non-existent channel: " + channelName);
        errorBuilder(user, "ERR_NOSUCHCHANNEL", channelName);
        return;
    }
    // Check if the user is in the channel
    Channel& channel = _channels[channelName];
    if (!channel.hasUser(user.getFd()))
    {
        Logger::warning("User " + user.getNickname() + " is not in channel "+ channelName + " and cannot set the topic.");
        errorBuilder(user, "ERR_NOTONCHANNEL", channelName);
        return;
    }
    // Check if the user is an operator if the channel is topic restricted
    if (channel.isTopicRestricted() && !channel.isOperator(user.getFd()))
    {
        Logger::warning("User " + user.getNickname() + " is not an operator in channel " + channelName + " and cannot set the topic.");
        errorBuilder(user, "ERR_CHANOPRIVSNEEDED", channelName);
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
        errorBuilder(user, "ERR_CHANOPRIVSNEEDED", channelName);
        return;
    }
    // Set the topic for the channel
    channel.setTopic(msg.getTrailing());
    Logger::info("User " + user.getNickname() + " set the topic for channel " + channelName + " to: " + msg.getTrailing());
    std::string topicSetMessage = ":" + user.getNickname() + "!" + user.getUsername() + "@" + _serverName + " TOPIC " + channelName + " :" + msg.getTrailing();
    broadcastMessage(topicSetMessage, user.getFd(), channelName);
    sendToUser(user, topicSetMessage);
}

void Server::handlePrivMsg(User& user, const Message& msg) {
    Logger::info("Handling command " + msg.getCommand());
    // Implement PRIVMSG command handling logic here
    if (msg.getArgCount() < 1)
    {
        Logger::warning("PRIVMSG command received with insufficient arguments.");
        errorBuilder(user, "ERR_NEEDMOREPARAMS", msg.getCommand());
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
            errorBuilder(user, "ERR_NOSUCHCHANNEL", target);
            return;
        }
        Channel& channel = _channels[target];
        if (!channel.hasUser(user.getFd()))
        {
            Logger::warning("User " + user.getNickname() + " is not in channel " + target + " and cannot send messages to it.");
            errorBuilder(user, "ERR_CANNOTSENDTOCHAN", target);
            return;
        }
        if(channel.isModerated() && !channel.isOperator(user.getFd()))
        {
            Logger::warning("User " + user.getNickname() + " is not an operator in moderated channel " + target + " and cannot send messages to it.");
            errorBuilder(user, "ERR_CANNOTSENDTOCHAN", target);
            return;
        }
        std::string formattedMessage = ":" + user.getNickname() + "!" + user.getUsername() + "@" + _serverName + " PRIVMSG " + target + " :" + message;
        broadcastMessage(formattedMessage, user.getFd(), target);
    } else {
        // Private message to a user
        if(!isNicknameInUse(target))
        {
            Logger::warning("PRIVMSG command received for non-existent user: " + target);
            errorBuilder(user, "ERR_NOSUCHNICK",target);
            return;
        }
        std::string formattedMessage = ":" + user.getNickname() + "!" + user.getUsername() + "@" + _serverName + " PRIVMSG " + target + " :" + message;
        User* targetUser = getUserByNickname(target);
        if(targetUser == NULL)
        {
            Logger::warning("PRIVMSG command received for non-existent user: " + target);
            errorBuilder(user, "ERR_NOSUCHNICK",target);
            return;
        }
        sendToUser(*targetUser, formattedMessage);
    }
}

void Server::handleUnknown(User& user, const Message& msg) {
    Logger::error("Handling command " + msg.getCommand());
    errorBuilder(user, "ERR_UNKNOWNCOMMAND", msg.getCommand());
}

void Server::handleQuit(User& user, const Message& msg)
{
    Logger::info("Handling command " + msg.getCommand() + " with " + numberToString(msg.getArgCount()) + " args.");

    std::string nickname = user.getNickname();
    std::map<std::string, Channel>::iterator it = _channels.begin();
    std::set<int> usersToNotify;
    std::string reason = "";

    if(msg.hasTrailing())
    {
        // Logger::debug("has trailing");
        reason = msg.getTrailing();
    }
    else if(msg.getArgCount() >= 1)
    {
        reason = msg.getArgs()[0];
        // Logger::debug("reason = " + reason);
    }
    else
        reason = "Client Quit";
    
    if(!user.isRegistered())
    {
        scheduleDisconnection(user.getFd());
        return;
    }

    while (it != _channels.end()) 
    {
        std::map<std::string, Channel>::iterator current = it;
        ++it;

        Channel& channel = current->second;

        if (channel.hasUser(user.getFd())) 
        {
            std::set<int> channelUsers = channel.getUsers();

            for(std::set<int>::const_iterator it = channelUsers.begin(); it != channelUsers.end(); ++it)
            {
                int userFd = *it;

                if(userFd != user.getFd())
                    usersToNotify.insert(userFd);

            }
            channel.removeUser(user);

            if (channel.getUserCount() == 0)
                _channels.erase(current);
        }
    }

    std::string quitMessage = ":" + user.getNickname() +
            "!" + user.getUsername() +
            "@" + _serverName +
            " QUIT " + 
            ":" + reason;

    for(std::set<int>::const_iterator it = usersToNotify.begin(); it != usersToNotify.end(); ++it)
    {
        int userFd = *it;

        std::map<int, User>::iterator userIt = _users.find(userFd);

        if(userIt != _users.end())
            sendToUser(userIt->second, quitMessage);
    }

    scheduleDisconnection(user.getFd());
}

void Server::dispatchMessage(User& user, const Message& msg) {
    std::string cmd = msg.getCommand();
    // remove \n and \r if present
    if (!cmd.empty() && cmd[cmd.size() - 1] == '\n')
        cmd.erase(cmd.size() - 1);
    if (!cmd.empty() && cmd[cmd.size() - 1] == '\r')
        cmd.erase(cmd.size() - 1);
    toLowerCase(cmd);
    if (cmd == QUIT_STR && !user.isRegistered())
    {
        handleQuit(user, msg);
        return;
    }

    // if (cmd == CAP_STR)
    //     handleCap(user, msg);
    // else if (cmd == PASS_STR && !user.getHasValidPassword() && !user.isRegistered())
    if (cmd == PASS_STR && !user.getHasValidPassword() && !user.isRegistered())
        handlePass(user, msg);
    else if (cmd == NICK_STR)
    {
        handleNick(user, msg);
        return;
    }
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
        std::string reply =
            ":" + _serverName + " 001 " + user.getNickname()
            + " :Welcome to the Internet Relay Network "
            + user.getNickname() + "!"
            + user.getUsername() + "@"
            + _serverName;

        sendToUser(user, reply);

        reply =
            ":" + _serverName + " 002 " + user.getNickname()
            + " :Your host is " + _serverName
            + ", running version ft_irc-1.0";

        sendToUser(user, reply);

        reply =
            ":" + _serverName + " 003 " + user.getNickname()
            + " :This server was created " + _creationDate;

        sendToUser(user, reply);

        reply =
            ":" + _serverName
            + " 004 " + user.getNickname()
            + " " + _serverName
            + " ft_irc-1.0 - itkol";

        sendToUser(user, reply);

        reply =
            ":" + _serverName + " 422 " + user.getNickname()
            + " :MOTD File is missing";

        sendToUser(user, reply);
        return;
    }
    if(user.isRegistered())
    {
        if (cmd == CAP_STR)
            return;
        else if (cmd == JOIN_STR)
            handleJoin(user, msg);
        else if (cmd == PART_STR)
            handlePart(user, msg);
        else if (cmd == PING_STR)
            handlePing(user, msg);
        else if (cmd == PONG_STR)
            return;
        // else if (cmd == WHO_STR)
        //     handleWho(user, msg);
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
        else if (cmd == QUIT_STR)
            handleQuit(user, msg);
        else if (cmd == USER_STR)
            handleUser(user, msg);
        else
            handleUnknown(user, msg);
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

void Server::errorBuilder(User& user, const std::string& errorCode, const std::string& parameters) 
{
    std::pair<int, std::string> errorMessage = getErrorMessage(errorCode);
    std::string recipient = user.getNickname();

    if(recipient.empty())
        recipient = "*";

    // int errorCodeInt = errorMessage.first;
    // const std::string& errorMessageStr = errorMessage.second;
    std::string response = ":" + _serverName + " " + numberToString(errorMessage.first) + " " + recipient;

    if(!parameters.empty())
        response += " " + parameters;
    
    response += " " + errorMessage.second;

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
