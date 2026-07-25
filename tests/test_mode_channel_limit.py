from tests.irc_client import IRCClientTimeout
from tests.irc_test_case import IRCIntegrationTest


class ChannelLimitTests(IRCIntegrationTest):
    CHANNEL = "#limited"

    def create_channel(self, channel=None):
        if channel is None:
            channel = self.CHANNEL

        operator = self.register_client("operator")
        self.join_channel(operator, "operator", channel)
        return operator

    def enable_limit(self, operator, limit, channel=None):
        if channel is None:
            channel = self.CHANNEL

        expected = (
            f":operator!operator@host MODE {channel} +l {limit}"
        )
        operator.send_command(f"MODE {channel} +l {limit}")
        received = self.expect(operator, expected)
        self.assertEqual(received[-1], expected)

    def assert_channel_modes(
        self,
        client,
        nickname,
        expected_modes,
        channel=None,
    ):
        if channel is None:
            channel = self.CHANNEL

        expected = (
            f":host 324 {nickname} {channel} {expected_modes}"
        )
        client.send_command(f"MODE {channel}")
        received = self.expect(client, expected)
        self.assertEqual(received[-1], expected)

    def test_channel_without_limit_does_not_report_limit_or_zero(self):
        operator = self.create_channel()

        self.assert_channel_modes(operator, "operator", "+")

    def test_operator_can_enable_limit_and_query_it(self):
        operator = self.create_channel()

        self.enable_limit(operator, 3)

        self.assert_channel_modes(operator, "operator", "+l 3")

    def test_limit_allows_users_until_capacity_and_then_returns_471(self):
        operator = self.create_channel()
        bob = self.register_client("bob")
        charlie = self.register_client("charlie")

        self.enable_limit(operator, 2)

        self.join_channel(bob, "bob", self.CHANNEL)

        charlie.send_command(f"JOIN {self.CHANNEL}")
        self.expect(
            charlie,
            f":host 471 charlie {self.CHANNEL} "
            ":Cannot join channel (+l)",
        )
        self.assert_server_running()

    def test_non_operator_cannot_enable_limit(self):
        operator = self.create_channel()
        bob = self.register_client("bob")
        charlie = self.register_client("charlie")

        self.join_channel(bob, "bob", self.CHANNEL)

        bob.send_command(f"MODE {self.CHANNEL} +l 2")
        self.expect(
            bob,
            f":host 482 bob {self.CHANNEL} "
            ":You're not channel operator",
        )

        # There are already two users. If Bob's MODE had taken effect,
        # Charlie would receive ERR_CHANNELISFULL.
        self.join_channel(charlie, "charlie", self.CHANNEL)

    def test_enabling_limit_without_parameter_returns_461(self):
        operator = self.create_channel()
        bob = self.register_client("bob")

        operator.send_command(f"MODE {self.CHANNEL} +l")
        self.expect(
            operator,
            ":host 461 operator MODE :Not enough parameters",
        )

        # The failed MODE command must not leave +l enabled.
        self.join_channel(bob, "bob", self.CHANNEL)
        self.assert_channel_modes(operator, "operator", "+")

    def test_invalid_limits_are_rejected_without_changing_channel_mode(self):
        operator = self.register_client("operator")
        invalid_limits = ("0", "-1", "not-a-number", "2users")

        for index, invalid_limit in enumerate(invalid_limits):
            channel = f"{self.CHANNEL}{index}"
            self.join_channel(operator, "operator", channel)

            operator.send_command(
                f"MODE {channel} +l {invalid_limit}"
            )

            # Synchronize without depending on a particular numeric for an
            # invalid MODE parameter.
            token = f"invalid-limit-{index}"
            operator.send_command(f"PING :{token}")
            self.expect(operator, f"PONG :{token}")

            self.assert_channel_modes(
                operator,
                "operator",
                "+",
                channel,
            )
            self.assert_server_running()

    def test_disabling_limit_allows_a_previously_rejected_user_to_join(self):
        operator = self.create_channel()
        bob = self.register_client("bob")

        self.enable_limit(operator, 1)

        bob.send_command(f"JOIN {self.CHANNEL}")
        self.expect(
            bob,
            f":host 471 bob {self.CHANNEL} "
            ":Cannot join channel (+l)",
        )

        expected = (
            f":operator!operator@host MODE {self.CHANNEL} -l"
        )
        operator.send_command(f"MODE {self.CHANNEL} -l")
        received = self.expect(operator, expected)
        self.assertEqual(received[-1], expected)

        self.assert_channel_modes(operator, "operator", "+")
        self.join_channel(bob, "bob", self.CHANNEL)

    def test_increasing_limit_allows_a_previously_rejected_user_to_join(self):
        operator = self.create_channel()
        bob = self.register_client("bob")

        self.enable_limit(operator, 1)

        bob.send_command(f"JOIN {self.CHANNEL}")
        self.expect(
            bob,
            f":host 471 bob {self.CHANNEL} "
            ":Cannot join channel (+l)",
        )

        self.enable_limit(operator, 2)

        self.join_channel(bob, "bob", self.CHANNEL)
        self.assert_channel_modes(operator, "operator", "+l 2")

    def test_lowering_limit_does_not_remove_members_but_blocks_new_joins(self):
        operator = self.create_channel()
        bob = self.register_client("bob")
        charlie = self.register_client("charlie")
        david = self.register_client("david")

        self.enable_limit(operator, 3)
        self.join_channel(bob, "bob", self.CHANNEL)
        self.join_channel(charlie, "charlie", self.CHANNEL)

        self.enable_limit(operator, 2)

        # Existing users remain in the channel even when the new limit is
        # lower than the current member count.
        bob.send_command(
            f"PRIVMSG {self.CHANNEL} :still in the channel"
        )
        self.expect(
            operator,
            f":bob!bob@host PRIVMSG {self.CHANNEL} "
            ":still in the channel",
        )

        david.send_command(f"JOIN {self.CHANNEL}")
        self.expect(
            david,
            f":host 471 david {self.CHANNEL} "
            ":Cannot join channel (+l)",
        )

    def test_kick_frees_a_slot_in_limited_channel(self):
        operator = self.create_channel()
        bob = self.register_client("bob")
        charlie = self.register_client("charlie")

        self.enable_limit(operator, 2)
        self.join_channel(bob, "bob", self.CHANNEL)

        charlie.send_command(f"JOIN {self.CHANNEL}")
        self.expect(
            charlie,
            f":host 471 charlie {self.CHANNEL} "
            ":Cannot join channel (+l)",
        )

        operator.send_command(f"KICK {self.CHANNEL} bob :freeing slot")
        self.expect(
            bob,
            f":operator!operator@host KICK {self.CHANNEL} bob "
            ":freeing slot",
        )

        self.join_channel(charlie, "charlie", self.CHANNEL)

    def test_limit_mode_change_is_sent_to_members_but_not_outsiders(self):
        operator = self.create_channel()
        bob = self.register_client("bob")
        outsider = self.register_client("outsider")

        self.join_channel(bob, "bob", self.CHANNEL)

        operator.drain()
        bob.drain()
        outsider.drain()

        expected = (
            f":operator!operator@host MODE {self.CHANNEL} +l 3"
        )
        operator.send_command(f"MODE {self.CHANNEL} +l 3")

        operator_messages = self.expect(operator, expected)
        bob_messages = self.expect(bob, expected)
        self.assertEqual(operator_messages[-1], expected)
        self.assertEqual(bob_messages[-1], expected)

        with self.assertRaises(IRCClientTimeout):
            outsider.read_line(timeout=0.15)

    def test_combined_kl_modes_consume_parameters_in_order(self):
        operator = self.create_channel()
        bob = self.register_client("bob")
        charlie = self.register_client("charlie")

        operator.send_command(
            f"MODE {self.CHANNEL} +kl secret 2"
        )
        operator.send_command("PING :combined-kl")
        self.expect(operator, "PONG :combined-kl")

        self.assert_channel_modes(
            operator,
            "operator",
            "+kl secret 2",
        )

        self.join_channel(
            bob,
            "bob",
            self.CHANNEL,
            key="secret",
        )

        charlie.send_command(f"JOIN {self.CHANNEL} secret")
        self.expect(
            charlie,
            f":host 471 charlie {self.CHANNEL} "
            ":Cannot join channel (+l)",
        )

    def test_combined_lk_modes_consume_parameters_in_order(self):
        operator = self.create_channel()
        bob = self.register_client("bob")
        charlie = self.register_client("charlie")

        operator.send_command(
            f"MODE {self.CHANNEL} +lk 2 secret"
        )
        operator.send_command("PING :combined-lk")
        self.expect(operator, "PONG :combined-lk")

        # MODE queries normalize the display order to +kl and therefore
        # must also normalize the associated parameter order.
        self.assert_channel_modes(
            operator,
            "operator",
            "+kl secret 2",
        )

        self.join_channel(
            bob,
            "bob",
            self.CHANNEL,
            key="secret",
        )

        charlie.send_command(f"JOIN {self.CHANNEL} secret")
        self.expect(
            charlie,
            f":host 471 charlie {self.CHANNEL} "
            ":Cannot join channel (+l)",
        )
