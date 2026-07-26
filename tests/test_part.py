from tests.irc_client import IRCClientTimeout
from tests.irc_test_case import IRCIntegrationTest


class PartTests(IRCIntegrationTest):
    CHANNEL = "#general"

    def expect_exact(self, client, expected):
        received = self.expect(client, expected)
        self.assertEqual(received[-1], expected)
        return received

    def create_channel_with_members(self, *nicknames):
        operator = self.register_client("operator")
        self.join_channel(operator, "operator", self.CHANNEL)

        members = []
        for nickname in nicknames:
            client = self.register_client(nickname)
            self.join_channel(client, nickname, self.CHANNEL)
            members.append(client)

        return operator, members

    def test_part_without_channel_returns_461(self):
        alice = self.register_client("alice")

        alice.send_command("PART")

        self.expect_exact(
            alice,
            ":host 461 alice PART :Not enough parameters",
        )

    def test_part_unknown_channel_returns_403(self):
        alice = self.register_client("alice")

        alice.send_command("PART #missing")

        self.expect_exact(
            alice,
            ":host 403 alice #missing :No such channel",
        )

    def test_part_user_not_in_channel_returns_442(self):
        operator = self.register_client("operator")
        bob = self.register_client("bob")
        self.join_channel(operator, "operator", self.CHANNEL)

        operator.drain()
        bob.send_command(f"PART {self.CHANNEL}")

        self.expect_exact(
            bob,
            f":host 442 bob {self.CHANNEL} "
            ":You're not on that channel",
        )

        with self.assertRaises(IRCClientTimeout):
            operator.read_line(timeout=0.15)

    def test_part_with_reason_is_sent_to_leaver_and_other_members(self):
        operator, members = self.create_channel_with_members("bob")
        bob = members[0]

        operator.drain()
        bob.drain()

        expected = (
            f":bob!bob@host PART {self.CHANNEL} "
            ":Leaving for lunch"
        )
        bob.send_command(
            f"PART {self.CHANNEL} :Leaving for lunch"
        )

        self.expect_exact(bob, expected)
        self.expect_exact(operator, expected)

    def test_part_is_not_sent_to_users_outside_channel(self):
        operator, members = self.create_channel_with_members("bob")
        bob = members[0]
        outsider = self.register_client("outsider")

        operator.drain()
        bob.drain()
        outsider.drain()

        expected = (
            f":bob!bob@host PART {self.CHANNEL} :goodbye"
        )
        bob.send_command(f"PART {self.CHANNEL} :goodbye")

        self.expect_exact(bob, expected)
        self.expect_exact(operator, expected)

        with self.assertRaises(IRCClientTimeout):
            outsider.read_line(timeout=0.15)

    def test_part_without_reason_uses_nickname_as_default_reason(self):
        operator, members = self.create_channel_with_members("bob")
        bob = members[0]

        bob.send_command(f"PART {self.CHANNEL}")

        expected = (
            f":bob!bob@host PART {self.CHANNEL} :bob"
        )
        self.expect_exact(bob, expected)
        self.expect_exact(operator, expected)

    def test_part_accepts_single_word_reason_without_colon(self):
        operator, members = self.create_channel_with_members("bob")
        bob = members[0]

        bob.send_command(f"PART {self.CHANNEL} goodbye")

        expected = (
            f":bob!bob@host PART {self.CHANNEL} :goodbye"
        )
        self.expect_exact(bob, expected)
        self.expect_exact(operator, expected)

    def test_part_preserves_explicit_empty_trailing_reason(self):
        operator, members = self.create_channel_with_members("bob")
        bob = members[0]

        bob.send_command(f"PART {self.CHANNEL} :")

        expected = f":bob!bob@host PART {self.CHANNEL} :"
        self.expect_exact(bob, expected)
        self.expect_exact(operator, expected)

    def test_part_removes_membership_but_keeps_server_connection(self):
        operator, members = self.create_channel_with_members("bob")
        bob = members[0]

        bob.send_command(f"PART {self.CHANNEL} :leaving")
        self.expect_exact(
            bob,
            f":bob!bob@host PART {self.CHANNEL} :leaving",
        )

        bob.send_command(
            f"PRIVMSG {self.CHANNEL} :should be rejected"
        )
        self.expect_exact(
            bob,
            f":host 404 bob {self.CHANNEL} "
            ":Cannot send to channel",
        )

        bob.send_command("PING :still-connected")
        self.expect_exact(bob, "PONG :still-connected")
        self.assert_server_running()

    def test_user_can_rejoin_channel_after_part(self):
        operator, members = self.create_channel_with_members("bob")
        bob = members[0]

        bob.send_command(f"PART {self.CHANNEL} :temporary")
        self.expect_exact(
            bob,
            f":bob!bob@host PART {self.CHANNEL} :temporary",
        )

        self.join_channel(bob, "bob", self.CHANNEL)
        self.expect_exact(
            operator,
            f":bob!bob@host JOIN {self.CHANNEL}",
        )

    def test_parting_one_channel_preserves_other_memberships(self):
        alice = self.register_client("alice")
        bob = self.register_client("bob")

        self.join_channel(alice, "alice", "#one")
        self.join_channel(alice, "alice", "#two")
        self.join_channel(bob, "bob", "#one")
        self.join_channel(bob, "bob", "#two")

        alice.drain()
        bob.drain()

        alice.send_command("PART #one :only leaving one")

        expected = (
            ":alice!alice@host PART #one :only leaving one"
        )
        self.expect_exact(alice, expected)
        self.expect_exact(bob, expected)

        alice.send_command("PRIVMSG #one :not allowed")
        self.expect_exact(
            alice,
            ":host 404 alice #one :Cannot send to channel",
        )

        alice.send_command("PRIVMSG #two :still here")
        self.expect_exact(
            bob,
            ":alice!alice@host PRIVMSG #two :still here",
        )

    def test_last_member_parting_removes_channel(self):
        alice = self.register_client("alice")
        self.join_channel(alice, "alice", "#temporary")

        alice.send_command("PART #temporary :last member")
        self.expect_exact(
            alice,
            ":alice!alice@host PART #temporary :last member",
        )

        bob = self.register_client("bob")
        replies = self.join_channel(bob, "bob", "#temporary")

        self.assertTrue(
            any(
                " 353 bob = #temporary :@bob " in line
                for line in replies
            ),
            "recreated channel did not make Bob operator: {!r}".format(
                replies
            ),
        )

    def test_part_frees_slot_in_limited_channel(self):
        operator, members = self.create_channel_with_members("bob")
        bob = members[0]
        charlie = self.register_client("charlie")

        operator.send_command(f"MODE {self.CHANNEL} +l 2")
        self.expect_exact(
            operator,
            f":operator!operator@host MODE {self.CHANNEL} +l 2",
        )

        charlie.send_command(f"JOIN {self.CHANNEL}")
        self.expect_exact(
            charlie,
            f":host 471 charlie {self.CHANNEL} "
            ":Cannot join channel (+l)",
        )

        bob.send_command(f"PART {self.CHANNEL} :freeing slot")
        self.expect_exact(
            bob,
            f":bob!bob@host PART {self.CHANNEL} :freeing slot",
        )

        self.join_channel(charlie, "charlie", self.CHANNEL)

    def test_parted_user_needs_new_invitation_to_rejoin_invite_only(self):
        operator = self.register_client("operator")
        bob = self.register_client("bob")
        self.join_channel(operator, "operator", self.CHANNEL)

        operator.send_command(f"MODE {self.CHANNEL} +i")
        self.expect_exact(
            operator,
            f":operator!operator@host MODE {self.CHANNEL} +i",
        )

        operator.send_command(f"INVITE bob {self.CHANNEL}")
        self.expect_exact(
            bob,
            f":operator!operator@host INVITE bob :{self.CHANNEL}",
        )
        self.join_channel(bob, "bob", self.CHANNEL)

        bob.send_command(f"PART {self.CHANNEL} :leaving")
        self.expect_exact(
            bob,
            f":bob!bob@host PART {self.CHANNEL} :leaving",
        )

        bob.send_command(f"JOIN {self.CHANNEL}")
        self.expect_exact(
            bob,
            f":host 473 bob {self.CHANNEL} "
            ":Cannot join channel (+i)",
        )

    def test_parted_user_still_needs_channel_key_to_rejoin(self):
        operator = self.register_client("operator")
        bob = self.register_client("bob")
        self.join_channel(operator, "operator", self.CHANNEL)

        operator.send_command(f"MODE {self.CHANNEL} +k secret")
        self.expect_exact(
            operator,
            f":operator!operator@host MODE "
            f"{self.CHANNEL} +k secret",
        )

        self.join_channel(
            bob,
            "bob",
            self.CHANNEL,
            key="secret",
        )

        bob.send_command(f"PART {self.CHANNEL} :leaving")
        self.expect_exact(
            bob,
            f":bob!bob@host PART {self.CHANNEL} :leaving",
        )

        bob.send_command(f"JOIN {self.CHANNEL}")
        self.expect_exact(
            bob,
            f":host 475 bob {self.CHANNEL} "
            ":Cannot join channel (+k)",
        )

        self.join_channel(
            bob,
            "bob",
            self.CHANNEL,
            key="secret",
        )

    def test_part_removes_channel_operator_status(self):
        alice = self.register_client("alice")
        bob = self.register_client("bob")
        self.join_channel(alice, "alice", self.CHANNEL)
        self.join_channel(bob, "bob", self.CHANNEL)

        alice.send_command(f"MODE {self.CHANNEL} +o bob")
        self.expect_exact(
            alice,
            f":alice!alice@host MODE {self.CHANNEL} +o bob",
        )

        alice.send_command(f"PART {self.CHANNEL} :leaving")
        self.expect_exact(
            alice,
            f":alice!alice@host PART {self.CHANNEL} :leaving",
        )

        self.join_channel(alice, "alice", self.CHANNEL)

        alice.send_command(f"MODE {self.CHANNEL} +i")
        self.expect_exact(
            alice,
            f":host 482 alice {self.CHANNEL} "
            ":You're not channel operator",
        )

    def test_part_multiple_channels_with_shared_reason(self):
        alice = self.register_client("alice")
        bob = self.register_client("bob")

        self.join_channel(alice, "alice", "#one")
        self.join_channel(alice, "alice", "#two")
        self.join_channel(bob, "bob", "#one")
        self.join_channel(bob, "bob", "#two")

        alice.drain()
        bob.drain()

        alice.send_command("PART #one,#two :leaving both")

        expected_one = (
            ":alice!alice@host PART #one :leaving both"
        )
        expected_two = (
            ":alice!alice@host PART #two :leaving both"
        )

        self.expect_exact(alice, expected_one)
        self.expect_exact(alice, expected_two)
        self.expect_exact(bob, expected_one)
        self.expect_exact(bob, expected_two)

        alice.send_command("PRIVMSG #one :rejected")
        self.expect_exact(
            alice,
            ":host 404 alice #one :Cannot send to channel",
        )
        alice.send_command("PRIVMSG #two :rejected")
        self.expect_exact(
            alice,
            ":host 404 alice #two :Cannot send to channel",
        )

    def test_part_channel_list_continues_after_invalid_channel(self):
        alice = self.register_client("alice")
        bob = self.register_client("bob")
        self.join_channel(alice, "alice", "#valid")
        self.join_channel(bob, "bob", "#valid")

        alice.drain()
        bob.drain()

        alice.send_command(
            "PART #missing,#valid :process every channel"
        )

        self.expect_exact(
            alice,
            ":host 403 alice #missing :No such channel",
        )

        expected = (
            ":alice!alice@host PART #valid "
            ":process every channel"
        )
        self.expect_exact(alice, expected)
        self.expect_exact(bob, expected)

        alice.send_command("PRIVMSG #valid :rejected")
        self.expect_exact(
            alice,
            ":host 404 alice #valid :Cannot send to channel",
        )
