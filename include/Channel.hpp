#ifndef CHANNEL_HPP
# define CHANNEL_HPP

#include <User.hpp>
#include <Common.hpp>
#include <Logger.hpp>

class Channel 
{
    public:
        Channel();
        ~Channel();
        void setName(const std::string& name);
        std::string getName() const;
        void setTopic(const std::string& topic);
        std::string getTopic() const;
        bool addUser(int userFd);
        void removeUser(const User& user);
        std::set<int> getUsers() const;
        int getUserCount() const;
        bool addOperator(int userFd);
        void removeOperator(const User& user);
        std::set<int> getOperators() const;
        int getOperatorCount() const;
        bool isOperator(int userFd) const;
        void inviteUser(const User& user);
        void removeInvite(const User& user);
        std::set<int> getInvitedUsers() const;
        void removeAllInvites();
        bool isInvited(int userFd) const;
        bool isInviteOnly() const;
        void setInviteOnly(bool inviteOnly);
        bool isTopicRestricted() const;
        void setTopicRestricted(bool topicRestricted);
        std::string getChannelKey() const;
        void setChannelKey(const std::string& key);
        int getUserLimit() const;
        void setUserLimit(int limit);
        bool isFull() const;
        void printChannelInfo() const;
        bool hasUser(int userFd) const;
        bool isModerated() const;
        void setModerated(bool moderated);
    private:
        std::string _name;
        std::string _topic;
        std::set<int> _users;
        std::set<int> _operators;
        bool _inviteOnly;
        std::set<int> _invitedUsers;
        bool _topicRestricted;
        std::string _channelKey;
        int _userLimit;
        bool _isModerated;
    };

#endif
