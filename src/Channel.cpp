#include <Channel.hpp>

Channel::Channel(){
    _name = "";
    _topic = "";
    _users = std::map<int, User>();
    _operators = std::map<int, User>();
    _inviteOnly = false;
    _topicRestricted = false;
    _channelKey = "";
    _userLimit = -1;
}

Channel::~Channel() {}

void Channel::setName(const std::string& name) {
    _name = name;
}

std::string Channel::getName() const {
    return _name;
}

void Channel::setTopic(const std::string& topic) {
    _topic = topic;
}

std::string Channel::getTopic() const {
    return _topic;
}

void Channel::addUser(const User& user) {
    if (!isFull()) {
        _users[user.getFd()] = user;
    } else {
       Logger::info("Cannot add user " + user.getNickname() + " to channel " + _name + ": channel is full.");
    }
}

void Channel::removeUser(const User& user) {
    _users.erase(user.getFd());
}

std::map<int, User> Channel::getUsers() const {
    return _users;
}

int Channel::getUserCount() const {
    return _users.size();
}

void Channel::addOperator(const User& user) {
    _operators[user.getFd()] = user;
}

void Channel::removeOperator(const User& user) {
    _operators.erase(user.getFd());
}

std::map<int, User> Channel::getOperators() const {
    return _operators;
}

int Channel::getOperatorCount() const {
    return _operators.size();
}

void Channel::inviteUser(const User& user) {
    _invitedUsers[user.getFd()] = user;
}

void Channel::removeInvite(const User& user) {
    _invitedUsers.erase(user.getFd());
}

bool Channel::isInvited(const User& user) const {
    return _invitedUsers.find(user.getFd()) != _invitedUsers.end();
}

std::map<int, User> Channel::getInvitedUsers() const {
    return _invitedUsers;
}

void Channel::removeAllInvites() {
    _invitedUsers.clear();
}

bool Channel::isInviteOnly() const {
    return _inviteOnly;
}

void Channel::setInviteOnly(bool inviteOnly) {
    _inviteOnly = inviteOnly;
}

bool Channel::isTopicRestricted() const {
    return _topicRestricted;
}

void Channel::setTopicRestricted(bool topicRestricted) {
    _topicRestricted = topicRestricted;
}

std::string Channel::getChannelKey() const {
    return _channelKey;
}

void Channel::setChannelKey(const std::string& key) {
    _channelKey = key;
}

int Channel::getUserLimit() const {
    return _userLimit;
}

void Channel::setUserLimit(int limit) {
    _userLimit = limit;
}

bool Channel::isFull() const {
    return _userLimit > 0 && _users.size() == static_cast<size_t>(_userLimit);
}

void Channel::broadcastMessage(const std::string& message, const User& sender) {
    // Implementation for broadcasting a message to all users in the channel except the sender
    std::cout << "Broadcasting message from " << sender.getNickname() << ": " << message << std::endl;
}

void Channel::printChannelInfo() const {
    std::cout << "Channel Name: " << _name << std::endl;
    std::cout << "Topic: " << _topic << std::endl;
    std::cout << "Users (" << getUserCount() << "): ";
    for (std::map<int, User>::const_iterator it = _users.begin(); it != _users.end(); ++it) {
        std::cout << it->second.getNickname() << " ";
    }
    std::cout << std::endl;
    std::cout << "Operators (" << getOperatorCount() << "): ";
    for (std::map<int, User>::const_iterator it = _operators.begin(); it != _operators.end(); ++it) {
        std::cout << it->second.getNickname() << " ";
    }
    std::cout << "Invite Only: " << (_inviteOnly ? "Yes" : "No") << std::endl;
    std::cout << "Topic Restricted: " << (_topicRestricted ? "Yes" : "No") << std::endl;
    std::cout << "Channel Key: " << (_channelKey.empty() ? "None" : _channelKey) << std::endl;
    std::cout << "User Limit: " << (_userLimit > 0 ? std::to_string(_userLimit) : "None") << std::endl;
    std::cout << std::endl;
}