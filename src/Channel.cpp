#include <Channel.hpp>

Channel::Channel(){
    _name = "";
    _topic = "";
    _inviteOnly = false;
    _topicRestricted = false;
    _channelKey = "";
    _userLimit = -1;
    _isModerated = false;
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

bool Channel::addUser(int userFd) {
    if (isFull())
        return false;
    _users.insert(userFd);
    return true;
}

void Channel::removeUser(const User& user) {
    _users.erase(user.getFd());
}

std::set<int> Channel::getUsers() const {
    return _users;
}

int Channel::getUserCount() const {
    return _users.size();
}

bool Channel::hasUser(int userFd) const {
    return _users.find(userFd) != _users.end();
}

bool Channel::isOperator(int userFd) const {
    return _operators.find(userFd) != _operators.end();
}

bool Channel::addOperator(int userFd) {
        if (!hasUser(userFd))
        return false;
    _operators.insert(userFd);
    return true;
}

void Channel::removeOperator(const User& user) {
    _operators.erase(user.getFd());
}

std::set<int> Channel::getOperators() const {
    return _operators;
}

int Channel::getOperatorCount() const {
    return _operators.size();
}

void Channel::inviteUser(const User& user) {
    _invitedUsers.insert(user.getFd());
}

void Channel::removeInvite(const User& user) {
    _invitedUsers.erase(user.getFd());
}

bool Channel::isInvited(int userFd) const {
    return _invitedUsers.find(userFd) != _invitedUsers.end();
}

std::set<int> Channel::getInvitedUsers() const {
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
    return _userLimit > 0 && _users.size() >= static_cast<size_t>(_userLimit);
}

void Channel::printChannelInfo() const {
    std::cout << "Channel Name: " << _name << std::endl;
    std::cout << "Topic: " << _topic << std::endl;
    std::cout << "Users (" << getUserCount() << "): ";
    for (std::set<int>::const_iterator it = _users.begin(); it != _users.end(); ++it) {
        std::cout << "User with FD: " << *it << " ";
    }
    std::cout << std::endl;
    std::cout << "Operators (" << getOperatorCount() << "): ";
    for (std::set<int>::const_iterator it = _operators.begin(); it != _operators.end(); ++it) {
        std::cout << "Operator with FD: " << *it << " ";
    }
    std::cout << "Invite Only: " << (_inviteOnly ? "Yes" : "No") << std::endl;
    std::cout << "Topic Restricted: " << (_topicRestricted ? "Yes" : "No") << std::endl;
    std::cout << "Channel Key: " << (_channelKey.empty() ? "None" : _channelKey) << std::endl;
    std::cout << "User Limit: " << (_userLimit > 0 ? std::to_string(_userLimit) : "None") << std::endl;
    std::cout << std::endl;
}

bool Channel::isModerated() const {
    return _isModerated;
}

void Channel::setModerated(bool moderated) {
    _isModerated = moderated;
}

bool Channel::isOperator(int userFd) const {
    return _operators.find(userFd) != _operators.end();
}
