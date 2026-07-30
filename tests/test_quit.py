from tests.irc_client import IRCClientTimeout
from tests.irc_test_case import IRCIntegrationTest


class QuitTests(IRCIntegrationTest):
    CHANNEL = "#general"

    def expect_exact(self, client, expected):
        received = self.expect(client, expected)
        self.assertEqual(received[-1], expected)
        return received

    def assert_no_message(self, client, timeout=0.15):
        with self.assertRaises(IRCClientTimeout):
            client.read_line(timeout=timeout)

    def assert_connection_closed(self, client, timeout=1.0):
        try:
            line = client.read_line(timeout=timeout)
        except ConnectionError:
            return
        except IRCClientTimeout:
            self.fail("QUIT did not close the client connection")

        self.fail(
            "received IRC line instead of connection close: {!r}".format(
                line
            )
        )

    def join_clients(self, channel, *clients):
        for nickname, client in clients:
            self.join_channel(client, nickname, channel)

    def test_quit_without_reason_uses_client_quit_and_closes_connection(self):
        alice = self.register_client("alice")
        bob = self.register_client("bob")
        self.join_clients(
            self.CHANNEL,
            ("alice", alice),
            ("bob", bob),
        )
        alice.drain()
        bob.drain()

        alice.send_command("QUIT")

        self.expect_exact(
            bob,
            f":alice!alice@{self.server_name} QUIT :Client Quit",
        )
        self.assert_connection_closed(alice)

    def test_quit_preserves_trailing_reason_with_spaces(self):
        alice = self.register_client("alice")
        bob = self.register_client("bob")
        self.join_clients(
            self.CHANNEL,
            ("alice", alice),
            ("bob", bob),
        )
        alice.drain()
        bob.drain()

        alice.send_command("QUIT :Leaving for lunch")

        self.expect_exact(
            bob,
            f":alice!alice@{self.server_name} QUIT :Leaving for lunch",
        )
        self.assert_connection_closed(alice)

    def test_quit_accepts_single_word_reason_without_colon(self):
        alice = self.register_client("alice")
        bob = self.register_client("bob")
        self.join_clients(
            self.CHANNEL,
            ("alice", alice),
            ("bob", bob),
        )
        alice.drain()
        bob.drain()

        alice.send_command("QUIT goodbye")

        self.expect_exact(
            bob,
            f":alice!alice@{self.server_name} QUIT :goodbye",
        )
        self.assert_connection_closed(alice)

    def test_quit_preserves_explicit_empty_trailing_reason(self):
        alice = self.register_client("alice")
        bob = self.register_client("bob")
        self.join_clients(
            self.CHANNEL,
            ("alice", alice),
            ("bob", bob),
        )
        alice.drain()
        bob.drain()

        alice.send_command("QUIT :")

        self.expect_exact(
            bob,
            f":alice!alice@{self.server_name} QUIT :",
        )
        self.assert_connection_closed(alice)

    def test_quit_uses_complete_user_prefix(self):
        alice = self.register_client(
            "alice",
            username="alice_user",
        )
        bob = self.register_client("bob")
        self.join_clients(
            self.CHANNEL,
            ("alice", alice),
            ("bob", bob),
        )
        alice.drain()
        bob.drain()

        alice.send_command("QUIT :bye")

        self.expect_exact(
            bob,
            f":alice!alice_user@{self.server_name} QUIT :bye",
        )

    def test_quit_is_not_sent_to_users_without_shared_channels(self):
        alice = self.register_client("alice")
        bob = self.register_client("bob")
        outsider = self.register_client("outsider")
        self.join_clients(
            self.CHANNEL,
            ("alice", alice),
            ("bob", bob),
        )
        alice.drain()
        bob.drain()
        outsider.drain()

        alice.send_command("QUIT :bye")

        self.expect_exact(
            bob,
            f":alice!alice@{self.server_name} QUIT :bye",
        )
        self.assert_no_message(outsider)

    def test_quit_is_sent_once_when_users_share_multiple_channels(self):
        alice = self.register_client("alice")
        bob = self.register_client("bob")
        self.join_clients(
            "#one",
            ("alice", alice),
            ("bob", bob),
        )
        self.join_clients(
            "#two",
            ("alice", alice),
            ("bob", bob),
        )
        alice.drain()
        bob.drain()

        alice.send_command("QUIT :shared channels")

        self.expect_exact(
            bob,
            f":alice!alice@{self.server_name} QUIT :shared channels",
        )
        self.assert_no_message(bob)

    def test_quit_reaches_each_user_from_different_shared_channels(self):
        alice = self.register_client("alice")
        bob = self.register_client("bob")
        charlie = self.register_client("charlie")
        self.join_clients(
            "#one",
            ("alice", alice),
            ("bob", bob),
        )
        self.join_clients(
            "#two",
            ("alice", alice),
            ("charlie", charlie),
        )
        alice.drain()
        bob.drain()
        charlie.drain()

        alice.send_command("QUIT :bye everyone")

        expected = f":alice!alice@{self.server_name} QUIT :bye everyone"
        self.expect_exact(bob, expected)
        self.expect_exact(charlie, expected)
        self.assert_no_message(bob)
        self.assert_no_message(charlie)

    def test_quit_does_not_include_channel_name(self):
        alice = self.register_client("alice")
        bob = self.register_client("bob")
        self.join_clients(
            self.CHANNEL,
            ("alice", alice),
            ("bob", bob),
        )
        alice.drain()
        bob.drain()

        alice.send_command("QUIT :bye")

        received = self.expect_exact(
            bob,
            f":alice!alice@{self.server_name} QUIT :bye",
        )
        self.assertNotIn(self.CHANNEL, received[-1])

    def test_quit_does_not_disconnect_remaining_clients(self):
        alice = self.register_client("alice")
        bob = self.register_client("bob")
        self.join_clients(
            self.CHANNEL,
            ("alice", alice),
            ("bob", bob),
        )
        alice.drain()
        bob.drain()

        alice.send_command("QUIT :bye")
        self.expect_exact(
            bob,
            f":alice!alice@{self.server_name} QUIT :bye",
        )

        bob.send_command("PING :bob-survives")
        self.expect_exact(bob, ":host PONG host :bob-survives")
        self.assert_server_running()

    def test_quit_releases_nickname(self):
        alice = self.register_client("alice")
        observer = self.register_client("observer")
        self.join_clients(
            self.CHANNEL,
            ("alice", alice),
            ("observer", observer),
        )
        alice.drain()
        observer.drain()

        alice.send_command("QUIT :bye")
        self.expect_exact(
            observer,
            f":alice!alice@{self.server_name} QUIT :bye",
        )
        self.assert_connection_closed(alice)

        replacement = self.register_client("alice")
        replacement.send_command("PING :nickname-reused")
        self.expect_exact(
            replacement,
            ":host PONG host :nickname-reused",
        )

    def test_quit_removes_channel_when_last_member_leaves(self):
        alice = self.register_client("alice")
        observer = self.register_client("observer")
        self.join_channel(alice, "alice", "#temporary")
        alice.drain()
        observer.drain()

        alice.send_command("QUIT :last member")
        self.assert_connection_closed(alice)

        replies = self.join_channel(
            observer,
            "observer",
            "#temporary",
        )
        self.assertTrue(
            any(
                " 353 observer = #temporary :@observer " in line
                for line in replies
            ),
            "recreated channel did not make observer operator: {!r}"
            .format(replies),
        )

    def test_quit_frees_slot_in_limited_channel(self):
        operator = self.register_client("operator")
        alice = self.register_client("alice")
        charlie = self.register_client("charlie")
        self.join_clients(
            self.CHANNEL,
            ("operator", operator),
            ("alice", alice),
        )

        operator.send_command(f"MODE {self.CHANNEL} +l 2")
        self.expect_exact(
            operator,
            f":operator!operator@{self.server_name} MODE {self.CHANNEL} +l 2",
        )

        charlie.send_command(f"JOIN {self.CHANNEL}")
        self.expect_exact(
            charlie,
            f":{self.server_name} 471 charlie {self.CHANNEL} "
            ":Cannot join channel (+l)",
        )

        alice.send_command("QUIT :freeing slot")
        self.expect_exact(
            operator,
            f":alice!alice@{self.server_name} QUIT :freeing slot",
        )

        self.join_channel(charlie, "charlie", self.CHANNEL)

    def test_quit_removes_channel_operator_status(self):
        alice = self.register_client("alice")
        bob = self.register_client("bob")
        self.join_clients(
            self.CHANNEL,
            ("alice", alice),
            ("bob", bob),
        )

        alice.send_command(f"MODE {self.CHANNEL} +o bob")
        self.expect_exact(
            alice,
            f":alice!alice@{self.server_name} MODE {self.CHANNEL} +o bob",
        )

        alice.send_command("QUIT :leaving")
        self.expect_exact(
            bob,
            f":alice!alice@{self.server_name} QUIT :leaving",
        )
        self.assert_connection_closed(alice)

        replacement = self.register_client("alice")
        self.join_channel(
            replacement,
            "alice",
            self.CHANNEL,
        )
        replacement.send_command(f"MODE {self.CHANNEL} +i")
        self.expect_exact(
            replacement,
            f":{self.server_name} 482 alice {self.CHANNEL} "
            ":You're not channel operator",
        )

    def test_quit_clears_pending_channel_invitations(self):
        operator = self.register_client("operator")
        bob = self.register_client("bob")
        self.join_channel(operator, "operator", self.CHANNEL)

        operator.send_command(f"MODE {self.CHANNEL} +i")
        self.expect_exact(
            operator,
            f":operator!operator@{self.server_name} MODE {self.CHANNEL} +i",
        )
        operator.send_command(f"INVITE bob {self.CHANNEL}")
        self.expect_exact(
            bob,
            f":operator!operator@{self.server_name} INVITE bob :{self.CHANNEL}",
        )

        bob.send_command("QUIT :before joining")
        self.assert_connection_closed(bob)

        replacement = self.register_client("bob")
        replacement.send_command(f"JOIN {self.CHANNEL}")
        self.expect_exact(
            replacement,
            f":{self.server_name} 473 bob {self.CHANNEL} "
            ":Cannot join channel (+i)",
        )

    def test_commands_after_quit_in_same_packet_are_not_processed(self):
        alice = self.register_client("alice")
        bob = self.register_client("bob")
        self.join_clients(
            self.CHANNEL,
            ("alice", alice),
            ("bob", bob),
        )
        alice.drain()
        bob.drain()

        alice.send_raw(
            "QUIT :leaving\r\n"
            "PRIVMSG bob :must not be delivered\r\n"
        )

        self.expect_exact(
            bob,
            f":alice!alice@{self.server_name} QUIT :leaving",
        )
        self.assert_no_message(bob)

    def test_unregistered_client_can_quit_without_error(self):
        client = self.new_client()

        client.send_command("QUIT :registration cancelled")

        self.assert_connection_closed(client)
