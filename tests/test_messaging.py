from tests.irc_test_case import IRCIntegrationTest


class MessagingTests(IRCIntegrationTest):
    CHANNEL = "#general"

    def two_users_in_channel(self):
        alice = self.register_client("alice")
        bob = self.register_client("bob")
        self.join_channel(alice, "alice", self.CHANNEL)
        self.join_channel(bob, "bob", self.CHANNEL)
        alice.drain()
        return alice, bob

    def test_channel_privmsg_reaches_other_members(self):
        alice, bob = self.two_users_in_channel()

        alice.send_command("PRIVMSG #general :Hello channel")

        self.expect(
            bob,
            ":alice!alice@definitely_not_discord PRIVMSG #general :Hello channel",
        )

    def test_private_message_reaches_target_user(self):
        alice = self.register_client("alice")
        bob = self.register_client("bob")

        alice.send_command("PRIVMSG bob :Hello Bob")

        self.expect(
            bob,
            ":alice!alice@definitely_not_discord PRIVMSG bob :Hello Bob",
        )

    def test_privmsg_to_unknown_user_returns_401(self):
        alice = self.register_client("alice")

        alice.send_command("PRIVMSG nobody :Hello")

        self.expect(alice, " 401 ")

    def test_non_member_cannot_send_to_channel(self):
        alice = self.register_client("alice")
        outsider = self.register_client("outsider")
        self.join_channel(alice, "alice", self.CHANNEL)

        outsider.send_command("PRIVMSG #general :Not allowed")

        self.expect(outsider, " 404 ")
