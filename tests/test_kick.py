from tests.irc_test_case import IRCIntegrationTest

class KickTests(IRCIntegrationTest):
    CHANNEL = "#general"

    def test_kick_without_enough_parameters_returns_461(self):
        alice = self.register_client("alice")
        self.join_channel(alice, "alice", self.CHANNEL)

        alice.send_command("KICK")

        self.expect(
            alice,
            f":{self.server_name} 461 alice KICK :Not enough parameters",
        )

    def test_kick_nonexistent_channel_returns_403(self):
        alice = self.register_client("alice")

        alice.send_command("KICK #nonexistent bob")

        self.expect(
            alice,
            f":{self.server_name} 403 alice #nonexistent :No such channel",
        )

    def test_kicker_not_in_channel_returns_442(self):
        operator = self.register_client("operator")
        alice = self.register_client("alice")
        bob = self.register_client("bob")

        self.join_channel(
            operator,
            "operator",
            self.CHANNEL,
        )

        alice.send_command(
            "KICK " + self.CHANNEL + " bob"
        )

        self.expect(
            alice,
            f":{self.server_name} 442 alice #general :You're not on that channel",
        )

    def test_kick_from_nooperator_returns_482(self):
        alice = self.register_client("alice")
        bob = self.register_client("bob")
        self.join_channel(alice, "alice", self.CHANNEL)
        self.join_channel(bob, "bob", self.CHANNEL)

        bob.send_command("KICK " + self.CHANNEL + " alice")

        self.expect(
            bob,
            f":{self.server_name} 482 bob #general :You're not channel operator",
        )

    def test_kick_target_not_in_channel_returns_441(self):
        alice = self.register_client("alice")
        bob = self.register_client("bob")
        self.join_channel(alice, "alice", self.CHANNEL)

        alice.send_command("KICK " + self.CHANNEL + " bob")

        self.expect(
            alice,
            f":{self.server_name} 441 alice bob #general "
            ":They aren't on that channel",
        )

    def test_kick_broadcasts_to_channel_members(self):
        operator = self.register_client("operator")
        alice = self.register_client("alice")
        bob = self.register_client("bob")

        self.join_channel(operator, "operator", self.CHANNEL)
        self.join_channel(alice, "alice", self.CHANNEL)
        self.join_channel(bob, "bob", self.CHANNEL)

        operator.send_command("KICK " + self.CHANNEL + " alice :You are being kicked!")

        self.expect(bob, f":operator!operator@{self.server_name} KICK {self.CHANNEL} alice :You are being kicked!")

    def test_kick_without_reason_uses_kicker_nickname_as_reason(self):
        operator = self.register_client("operator")
        alice = self.register_client("alice")
        self.join_channel(operator, "operator", self.CHANNEL)
        self.join_channel(alice, "alice", self.CHANNEL)

        operator.send_command("KICK " + self.CHANNEL + " alice")

        self.expect(alice, f":operator!operator@{self.server_name} KICK {self.CHANNEL} alice :operator")

    def test_kicked_user_is_removed_from_channel(self):
        operator = self.register_client("operator")
        alice = self.register_client("alice")

        self.join_channel(operator, "operator", self.CHANNEL,)
        self.join_channel(alice, "alice", self.CHANNEL,)

        operator.send_command("KICK " + self.CHANNEL + " alice :You are being kicked!")

        # Alice receives the KICK notification.
        self.expect(alice, " KICK #general alice :You are being kicked!",)

        # Alice can no longer send messages to the channel.
        alice.send_command("PRIVMSG " + self.CHANNEL + " :Hello everyone!")
        self.expect(
            alice,
            f":{self.server_name} 404 alice #general :Cannot send to channel",
        )

        # KICK did not disconnect Alice from the server.
        alice.send_command("PING :after-kick")
        self.expect(alice, "PONG :after-kick")

        self.assert_server_running()
