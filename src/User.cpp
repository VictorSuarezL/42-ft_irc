#include <User.hpp>
User::User() {
    _fd = -1;
    _nickname = nullptr;
    _username = nullptr;
    _isRegistered = false;
    inputBuffer = nullptr;
    outputBuffer = nullptr;
    toDelete = false;
}

User::~User() {}

int User::getFd() const {
    return _fd;
}

void User::setFd(int fd) {
    _fd = fd;
}

std::string User::getNickname() const {
    return _nickname;
}

void User::setNickname(const std::string& nickname) {
    _nickname = nickname;
}

std::string User::getUsername() const {
    return _username;
}

void User::setUsername(const std::string& username) {
    _username = username;
}

bool User::hasNickname() const {
    return !_nickname.empty();
}

bool User::hasUsername() const {
    return !_username.empty();
}

bool User::isRegistered() const {
    return _isRegistered;
}

void User::setIsRegistered(bool isRegistered) {
    _isRegistered = isRegistered;
}

std::string User::getInputBuffer() const {
    return inputBuffer;
}

void User::setInputBuffer(const std::string& inputBuffer) {
    this->inputBuffer = inputBuffer;
}

std::string User::getOutputBuffer() const {
    return outputBuffer;
}

void User::setOutputBuffer(const std::string& outputBuffer) {
    this->outputBuffer = outputBuffer;
}

void User::appendToInputBuffer(const std::string& data) {
    inputBuffer += data;
}

std::vector<std::string> User::extractCompleteMessages() {
    std::vector<std::string> messages;
    size_t pos;
    while ((pos = inputBuffer.find("\r\n")) != std::string::npos) {
        messages.push_back(inputBuffer.substr(0, pos));
        inputBuffer.erase(0, pos + 2);
    }
    return messages;
}

void User::appendToOutputBuffer(const std::string& data) {
    outputBuffer += data;
}

bool User::hasPendingOutput() const {
    return !outputBuffer.empty();
}

bool User::getToDelete() const {
    return toDelete;
}

void User::setToDelete(bool toDelete) {
    this->toDelete = toDelete;
}

void User::registerUser() {
    if (hasNickname() && hasUsername()) {
        setIsRegistered(true);
    }
}


