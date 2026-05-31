#include <Errors.hpp>

std::map<std::string, std::pair<int, std::string> > errorMessages;

void initErrorMessages()
{
    errorMessages["ERR_NOTREGISTERED"] = std::make_pair(451, ":You have not registered");
    errorMessages["ERR_NEEDMOREPARAMS"] = std::make_pair(461, ":Not enough parameters");
    errorMessages["ERR_ALREADYREGISTERED"] = std::make_pair(462, ":You may not reregister");
    errorMessages["ERR_PASSWDMISMATCH"] = std::make_pair(464, ":Password incorrect");
    errorMessages["ERR_NONICKNAMEGIVEN"] = std::make_pair(431, ":No nickname given");
    errorMessages["ERR_ERRONEUSNICKNAME"] = std::make_pair(432, ":Erroneous nickname");
    errorMessages["ERR_NICKNAMEINUSE"] = std::make_pair(433, ":Nickname is already in use");
    errorMessages["ERR_NOSUCHNICK"] = std::make_pair(401, ":No such nick/channel");
    errorMessages["ERR_NOSUCHCHANNEL"] = std::make_pair(403, ":No such channel");
    errorMessages["ERR_CANNOTSENDTOCHAN"] = std::make_pair(404, ":Cannot send to channel");
    errorMessages["ERR_TOOMANYCHANNELS"] = std::make_pair(405, ":You have joined too many channels");
    errorMessages["ERR_UNKNOWNCOMMAND"] = std::make_pair(421, ":Unknown command");
    errorMessages["ERR_USERNOTINCHANNEL"] = std::make_pair(441, ":They aren't on that channel");
    errorMessages["ERR_NOTONCHANNEL"] = std::make_pair(442, ":You're not on that channel");
    errorMessages["ERR_USERONCHANNEL"] = std::make_pair(443, ":is already on channel");
    errorMessages["ERR_CHANNELISFULL"] = std::make_pair(471, ":Cannot join channel (+l)");
    errorMessages["ERR_UNKNOWNMODE"] = std::make_pair(472, ":is unknown mode char to me");
    errorMessages["ERR_INVITEONLYCHAN"] = std::make_pair(473, ":Cannot join channel (+i)");
    errorMessages["ERR_BANNEDFROMCHAN"] = std::make_pair(474, ":Cannot join channel (+b)");
    errorMessages["ERR_BADCHANNELKEY"] = std::make_pair(475, ":Cannot join channel (+k)");
    errorMessages["ERR_BADCHANMASK"] = std::make_pair(476, ":Bad Channel Mask");
    errorMessages["ERR_CHANOPRIVSNEEDED"] = std::make_pair(482, ":You're not channel operator");
    errorMessages["ERR_SERVERSHUTDOWN"] = std::make_pair(666, ":Servers shutting down");
}

std::pair<int, std::string> getErrorMessage(const std::string& errorCode)
{
    if (errorMessages.empty())
        initErrorMessages();
    if (errorMessages.find(errorCode) != errorMessages.end())
        return errorMessages[errorCode];
    return std::make_pair(500, ":Unknown error");
}