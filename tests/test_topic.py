from tests.irc_test_case import IRCIntegrationTest


class TopicTests(IRCIntegrationTest):
    CHANNEL = "#general"

    def create_channel(self):
        operator = self.register_client("alice")
        self.join_channel(operator, "alice", self.CHANNEL)
        return operator

    def test_query_channel_without_topic_returns_331(self):
        operator = self.create_channel()

        operator.send_command("TOPIC " + self.CHANNEL)

        self.expect(operator, " 331 alice " + self.CHANNEL + " ")

    def test_operator_can_set_and_query_topic(self):
        operator = self.create_channel()

        operator.send_command("TOPIC " + self.CHANNEL + " :Automated topic")
        operator.send_command("TOPIC " + self.CHANNEL)

        self.expect(
            operator,
            " 332 alice " + self.CHANNEL + " :Automated topic",
        )

        operator.send_command("TOPIC " + self.CHANNEL)
        self.expect(
            operator,
            " 332 alice " + self.CHANNEL + " :Automated topic",
        )

    def test_empty_trailing_clears_topic(self):
        operator = self.create_channel()
        operator.send_command("TOPIC " + self.CHANNEL + " :Temporary topic")
        operator.send_command("TOPIC " + self.CHANNEL + " :")

        operator.send_command("TOPIC " + self.CHANNEL)

        self.expect(operator, " 331 alice " + self.CHANNEL + " ")

    def test_topic_restricted_channel_rejects_regular_user_change(self):
        operator = self.create_channel()
        regular = self.register_client("bob")
        self.join_channel(regular, "bob", self.CHANNEL)
        operator.drain()
        operator.send_command("MODE " + self.CHANNEL + " +t")

        regular.send_command("TOPIC " + self.CHANNEL + " :Forbidden")

        self.expect(
            regular,
            f":{self.server_name} 482 bob #general :You're not channel operator",
        )

    def test_topic_for_unknown_channel_returns_403(self):
        client = self.register_client("alice")

        client.send_command("TOPIC #missing")

        self.expect(
            client,
            f":{self.server_name} 403 alice #missing :No such channel",
        )

    def test_topic_restricted_channel_allows_operator_change(self):
        operator = self.create_channel()
        regular = self.register_client("bob")
        self.join_channel(regular, "bob", self.CHANNEL)
        operator.drain()
        operator.send_command("MODE " + self.CHANNEL + " +t")

        operator.send_command("TOPIC " + self.CHANNEL + " :New topic")

        self.expect(
            operator,
            f":alice!alice@{self.server_name} TOPIC {self.CHANNEL} :New topic",
        )

    def test_topic_change_broadcasts_to_all_members(self):
        operator = self.create_channel()
        bob = self.register_client("bob")
        charlie = self.register_client("charlie")

        self.join_channel(bob, "bob", self.CHANNEL)
        self.join_channel(charlie, "charlie", self.CHANNEL)

        operator.drain()
        bob.drain()
        charlie.drain()

        operator.send_command("TOPIC " + self.CHANNEL + " :New topic")

        self.expect(
            operator,
            f":alice!alice@{self.server_name} TOPIC {self.CHANNEL} :New topic",
        )
        self.expect(
            bob,
            f":alice!alice@{self.server_name} TOPIC {self.CHANNEL} :New topic",
        )
        self.expect(
            charlie,
            f":alice!alice@{self.server_name} TOPIC {self.CHANNEL} :New topic",
        )
