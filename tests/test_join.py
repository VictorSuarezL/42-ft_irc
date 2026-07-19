from tests.irc_test_case import IRCIntegrationTest


class JoinTests(IRCIntegrationTest):
    CHANNEL = "#general"

    def test_first_channel_member_is_operator_in_names_reply(self):
        alice = self.register_client("alice")

        replies = self.join_channel(alice, "alice", self.CHANNEL)

        self.assertTrue(
            any(" 353 alice = #general :@alice " in line for line in replies),
            "operator prefix was not present in replies: {!r}".format(replies),
        )

    def test_join_is_broadcast_to_existing_members(self):
        alice = self.register_client("alice")
        bob = self.register_client("bob")
        self.join_channel(alice, "alice", self.CHANNEL)

        self.join_channel(bob, "bob", self.CHANNEL)

        self.expect(alice, ":bob!bob@definitely_not_discord JOIN #general")

    def test_invalid_channel_name_returns_476(self):
        alice = self.register_client("alice")

        alice.send_command("JOIN general")

        self.expect(alice, " 476 ")

    def test_joining_same_channel_twice_returns_443(self):
        alice = self.register_client("alice")
        self.join_channel(alice, "alice", self.CHANNEL)

        alice.send_command("JOIN " + self.CHANNEL)

        self.expect(alice, " 443 ")
