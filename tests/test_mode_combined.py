from tests.irc_test_case import IRCIntegrationTest


class CombinedChannelModeTests(IRCIntegrationTest):
    CHANNEL = "#combined"

    def create_channel_with_members(self, *nicknames):
        operator = self.register_client("operator")
        self.join_channel(operator, "operator", self.CHANNEL)

        members = []
        for nickname in nicknames:
            client = self.register_client(nickname)
            self.join_channel(client, nickname, self.CHANNEL)
            members.append(client)

        return operator, members

    def expect_exact(self, client, expected):
        received = self.expect(client, expected)
        self.assertEqual(received[-1], expected)

    def assert_channel_modes(self, client, nickname, expected_modes):
        expected = (
            f":{self.server_name} 324 {nickname} {self.CHANNEL} {expected_modes}"
        )
        client.send_command(f"MODE {self.CHANNEL}")
        self.expect_exact(client, expected)

    def send_modes_and_expect_broadcast(
        self,
        operator,
        members,
        mode_expression,
        parameters="",
    ):
        command = f"MODE {self.CHANNEL} {mode_expression}"
        expected = (
            f":operator!operator@{self.server_name} MODE "
            f"{self.CHANNEL} {mode_expression}"
        )

        if parameters:
            command += " " + parameters
            expected += " " + parameters

        operator.send_command(command)
        self.expect_exact(operator, expected)

        for member in members:
            self.expect_exact(member, expected)

    def test_combined_modes_without_parameters_are_broadcast_together(self):
        operator, members = self.create_channel_with_members("bob")

        self.send_modes_and_expect_broadcast(
            operator,
            members,
            "+it",
        )

        self.assert_channel_modes(operator, "operator", "+it")

    def test_combined_modes_with_multiple_parameters_keep_their_order(self):
        operator, members = self.create_channel_with_members("bob")

        self.send_modes_and_expect_broadcast(
            operator,
            members,
            "+itkl",
            "secret 3",
        )

        self.assert_channel_modes(
            operator,
            "operator",
            "+itkl secret 3",
        )

    def test_noncanonical_mode_order_consumes_matching_parameters(self):
        operator, members = self.create_channel_with_members("bob")

        self.send_modes_and_expect_broadcast(
            operator,
            members,
            "+ltik",
            "4 secret",
        )

        # MODE queries use the server's canonical i, t, k, l order.
        self.assert_channel_modes(
            operator,
            "operator",
            "+itkl secret 4",
        )

    def test_combined_removal_clears_all_channel_modes(self):
        operator, members = self.create_channel_with_members("bob")

        self.send_modes_and_expect_broadcast(
            operator,
            members,
            "+itkl",
            "secret 3",
        )
        self.send_modes_and_expect_broadcast(
            operator,
            members,
            "-itkl",
        )

        self.assert_channel_modes(operator, "operator", "+")

    def test_combined_mode_message_preserves_sign_transitions(self):
        operator, members = self.create_channel_with_members("bob")

        self.send_modes_and_expect_broadcast(
            operator,
            members,
            "+itkl",
            "secret 3",
        )
        self.send_modes_and_expect_broadcast(
            operator,
            members,
            "-ik+l",
            "5",
        )

        self.assert_channel_modes(operator, "operator", "+tl 5")

    def test_same_mode_can_be_added_and_removed_in_one_command(self):
        operator, members = self.create_channel_with_members("bob")

        self.send_modes_and_expect_broadcast(
            operator,
            members,
            "+it-i",
        )

        self.assert_channel_modes(operator, "operator", "+t")

    def test_repeated_operator_mode_consumes_one_nickname_per_mode(self):
        operator, members = self.create_channel_with_members(
            "bob",
            "charlie",
        )
        bob, charlie = members

        self.send_modes_and_expect_broadcast(
            operator,
            members,
            "+too",
            "bob charlie",
        )

        bob.send_command(
            f"TOPIC {self.CHANNEL} :topic set by bob"
        )
        self.expect_exact(
            operator,
            f":bob!bob@{self.server_name} TOPIC {self.CHANNEL} :topic set by bob",
        )

        charlie.send_command(
            f"TOPIC {self.CHANNEL} :topic set by charlie"
        )
        self.expect_exact(
            operator,
            f":charlie!charlie@{self.server_name} TOPIC "
            f"{self.CHANNEL} :topic set by charlie",
        )

    def test_operator_modes_with_different_signs_target_correct_users(self):
        operator, members = self.create_channel_with_members(
            "bob",
            "charlie",
        )
        bob, charlie = members

        self.send_modes_and_expect_broadcast(
            operator,
            members,
            "+o",
            "charlie",
        )

        operator.drain()
        bob.drain()
        charlie.drain()

        self.send_modes_and_expect_broadcast(
            operator,
            members,
            "+to-o",
            "bob charlie",
        )

        bob.send_command(f"TOPIC {self.CHANNEL} :allowed")
        self.expect_exact(
            operator,
            f":bob!bob@{self.server_name} TOPIC {self.CHANNEL} :allowed",
        )

        charlie.send_command(f"TOPIC {self.CHANNEL} :forbidden")
        self.expect(
            charlie,
            f":{self.server_name} 482 charlie {self.CHANNEL} "
            ":You're not channel operator",
        )

    def test_valid_modes_surrounding_unknown_mode_are_still_announced(self):
        operator, members = self.create_channel_with_members("bob")
        bob = members[0]

        operator.send_command(f"MODE {self.CHANNEL} +izt")
        self.expect(
            operator,
            f":{self.server_name} 472 operator z :is unknown mode char to me",
        )

        self.expect_exact(
            bob,
            f":operator!operator@{self.server_name} MODE {self.CHANNEL} +it",
        )
        self.assert_channel_modes(operator, "operator", "+it")

    def test_modes_applied_before_invalid_parameter_are_announced(self):
        operator, members = self.create_channel_with_members("bob")
        bob = members[0]

        operator.send_command(
            f"MODE {self.CHANNEL} +ikl secret invalid"
        )
        operator.send_command("PING :invalid-combined-mode")
        self.expect(operator, "PONG host :invalid-combined-mode")

        self.expect_exact(
            bob,
            f":operator!operator@{self.server_name} MODE "
            f"{self.CHANNEL} +ik secret",
        )
        self.assert_channel_modes(
            operator,
            "operator",
            "+ik secret",
        )

    def test_missing_parameter_does_not_announce_unapplied_mode(self):
        operator, members = self.create_channel_with_members("bob")
        bob = members[0]

        operator.send_command(
            f"MODE {self.CHANNEL} +ikl secret"
        )
        self.expect(
            operator,
            f":{self.server_name} 461 operator MODE :Not enough parameters",
        )

        self.expect_exact(
            bob,
            f":operator!operator@{self.server_name} MODE "
            f"{self.CHANNEL} +ik secret",
        )
        self.assert_channel_modes(
            operator,
            "operator",
            "+ik secret",
        )
