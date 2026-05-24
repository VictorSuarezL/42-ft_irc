#include <string>
#include <vector>
class User {
    public:
        User();
        ~User();
        int getFd() const;
        void setFd(int fd);
        std::string getNickname() const;
        void setNickname(const std::string& nickname);
        std::string getUsername() const;
        void setUsername(const std::string& username);
        bool hasNickname() const;
        bool hasUsername() const;
        bool isRegistered() const;
        void setIsRegistered(bool isRegistered);
        std::string getInputBuffer() const;
        void setInputBuffer(const std::string& inputBuffer);
        std::string getOutputBuffer() const;
        void setOutputBuffer(const std::string& outputBuffer);
        void appendToInputBuffer(const std::string& data);
        std::vector<std::string> extractCompleteMessages();
        void appendToOutputBuffer(const std::string& data);
        bool hasPendingOutput() const;
        bool getToDelete() const;
        void setToDelete(bool toDelete);
        void registerUser();
    private:
        int _fd;
        std::string _nickname;
        std::string _username;
        bool _isRegistered;
        std::string inputBuffer;
        std::string outputBuffer;
        bool toDelete;
};