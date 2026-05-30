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
        void addUser(const User& user);
        void removeUser(const User& user);
        std::map<int, User> getUsers() const;
        int getUserCount() const;
        void addOperator(const User& user);
        void removeOperator(const User& user);
        std::map<int, User> getOperators() const;
        int getOperatorCount() const;
        void inviteUser(const User& user);
        void removeInvite(const User& user);
        std::map<int, User> getInvitedUsers() const;
        void removeAllInvites();
        bool isInvited(const User& user) const;
        bool isInviteOnly() const;
        void setInviteOnly(bool inviteOnly);
        bool isTopicRestricted() const;
        void setTopicRestricted(bool topicRestricted);
        std::string getChannelKey() const;
        void setChannelKey(const std::string& key);
        int getUserLimit() const;
        void setUserLimit(int limit);
        bool isFull() const;
        void broadcastMessage(const std::string& message, const User& sender);
        void printChannelInfo() const;
    private:
        std::string _name;
        std::string _topic;
        std::map<int, User> _users;
        std::map<int, User> _operators;
        bool _inviteOnly;
        std::map<int, User> _invitedUsers;
        bool _topicRestricted;
        std::string _channelKey;
        int _userLimit;
    };

#endif