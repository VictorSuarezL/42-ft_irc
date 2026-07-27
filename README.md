*This project has been created as part of the 42 curriculum by francfer and vsanz-su.*

# ft_irc

## Description

ft_irc is an Internet Relay Chat server written in C++98. It accepts multiple TCP clients, authenticates them with a connection password, and lets them communicate through private messages and channels.

The server uses a single `poll()` loop and non-blocking sockets to manage connections, incoming commands, and buffered outgoing messages without forking.

## Features

- Client registration with `PASS`, `NICK`, and `USER`.
- Channel creation and membership with `JOIN` and `PART`.
- Channel and private messages with `PRIVMSG`.
- Connection handling with `PING`, `PONG`, and `QUIT`.
- Channel topics with `TOPIC`.
- Channel operators and the `KICK` and `INVITE` commands.
- Channel modes:
  - `i`: invite-only channel.
  - `t`: operator-restricted topic changes.
  - `k`: channel key.
  - `o`: channel operator privileges.
  - `l`: channel user limit.
- Buffered handling of partial IRC messages and non-blocking output.

## Instructions

### Compilation

Compile the server with:

```sh
make
```

Other available rules are:

```sh
make clean
make fclean
make re
```

### Running the server

Start the executable with a listening port and connection password:

```sh
./ircserv <port> <password>
```

Example:

```sh
./ircserv 6667 testpass
```

IRC commands must end with `\r\n`. A minimal registration sequence is:

```text
PASS testpass
NICK alice
USER alice 0 * :Alice
```

## Resources

- [RFC 2812 - Internet Relay Chat: Client Protocol](https://www.rfc-editor.org/rfc/rfc2812.html)
- [Modern IRC Client Protocol specification](https://modern.ircdocs.horse/)
- [poll(2) manual page](https://man7.org/linux/man-pages/man2/poll.2.html)
- [irssi documentation](https://irssi.org/documentation/)

AI tools were used to assist with repetitive tasks, and checking mechanical consistency across the project. Their output was reviewed and validated by the authors before being incorporated.
