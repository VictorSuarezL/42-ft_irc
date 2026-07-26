import re

from tests.irc_test_case import IRCIntegrationTest


class RegistrationTests(IRCIntegrationTest):
    WELCOME_NUMERICS = (" 001 ", " 002 ", " 003 ", " 004 ", " 422 ")

    def receive_welcome_sequence(self, client, nickname):
        return self.expect(client, " 422 " + nickname + " ")

    def assert_no_welcome_replies(self, lines):
        unexpected = [
            line
            for line in lines
            if any(numeric in line for numeric in self.WELCOME_NUMERICS)
        ]
        self.assertEqual(
            unexpected,
            [],
            "Unexpected welcome replies:\n" + "\n".join(unexpected),
        )

    def test_successful_registration_sends_complete_welcome_sequence(self):
        client = self.new_client()

        client.register("alice", self.PASSWORD, username="alice_user")

        lines = self.receive_welcome_sequence(client, "alice")

        self.assertEqual(
            len(lines),
            5,
            "Expected exactly five welcome lines (001-004 and 422), got:\n"
            + "\n".join(repr(line) for line in lines),
        )
        self.assertEqual(
            lines[0],
            ":host 001 alice :Welcome to the Internet Relay Network "
            "alice!alice_user@host",
        )
        self.assertEqual(
            lines[1],
            ":host 002 alice :Your host is host, running version ft_irc-1.0",
        )
        self.assertRegex(
            lines[2],
            r"^:host 003 alice :This server was created \S.*$",
        )
        self.assertEqual(
            lines[3],
            ":host 004 alice host ft_irc-1.0 - itkol",
        )
        self.assertEqual(
            lines[4],
            ":host 422 alice :MOTD File is missing",
        )

    def test_003_creation_date_is_shared_by_all_registrations(self):
        alice = self.new_client()
        alice.register("alice", self.PASSWORD)
        alice_lines = self.receive_welcome_sequence(alice, "alice")

        bob = self.new_client()
        bob.register("bob", self.PASSWORD)
        bob_lines = self.receive_welcome_sequence(bob, "bob")

        alice_003 = next(line for line in alice_lines if " 003 alice " in line)
        bob_003 = next(line for line in bob_lines if " 003 bob " in line)
        alice_date = re.sub(r"^:host 003 alice :This server was created ", "", alice_003)
        bob_date = re.sub(r"^:host 003 bob :This server was created ", "", bob_003)

        self.assertTrue(alice_date, "The 003 creation date must not be empty")
        self.assertEqual(alice_date, bob_date)

    def test_welcome_is_not_sent_before_registration_is_complete(self):
        client = self.new_client()
        client.send_raw(
            "PASS {0}\r\n"
            "NICK alice\r\n"
            "PING incomplete-registration\r\n".format(self.PASSWORD)
        )

        lines = self.expect(
            client,
            ":host 451 alice :You have not registered",
        )

        self.assert_no_welcome_replies(lines)

    def test_welcome_is_not_sent_when_password_is_invalid(self):
        client = self.new_client()
        client.send_raw(
            "PASS wrongpassword\r\n"
            "NICK alice\r\n"
            "USER alice 0 * :Alice\r\n"
            "PING invalid-registration\r\n"
        )

        lines = self.expect(
            client,
            ":host 451 alice :You have not registered",
        )

        self.assertIn(":host 464 * :Password incorrect", lines)
        self.assert_no_welcome_replies(lines)

    def test_welcome_sequence_is_sent_only_once(self):
        client = self.new_client()
        client.register("alice", self.PASSWORD)
        self.receive_welcome_sequence(client, "alice")

        client.send_raw(
            "USER alice 0 * :Alice Again\r\n"
            "PING welcome-not-repeated\r\n"
        )
        lines = self.expect(client, "PONG :welcome-not-repeated")

        self.assert_no_welcome_replies(lines)

    def test_valid_registration_accepts_registered_command(self):
        client = self.register_client("alice")

        client.send_command("PING still-registered")

        self.expect(client, "PONG :still-registered")
        self.assert_server_running()

    def test_invalid_password_returns_464(self):
        client = self.new_client()

        client.send_command("PASS wrongpassword")

        self.expect(client, ":host 464 * :Password incorrect")

    def test_command_before_registration_returns_451(self):
        client = self.new_client()

        client.send_command("JOIN #general")

        self.expect(client, ":host 451 * :You have not registered")

    def test_duplicate_nickname_returns_433(self):
        self.register_client("alice")
        second = self.new_client()
        second.send_command("PASS " + self.PASSWORD)

        second.send_command("NICK alice")

        self.expect(
            second,
            ":host 433 * alice :Nickname is already in use",
        )

    def test_registration_commands_can_arrive_in_one_packet(self):
        client = self.new_client()
        client.send_raw(
            "PASS {0}\r\n"
            "NICK bundled\r\n"
            "USER bundled 0 * :Bundled User\r\n"
            "PING bundle-complete\r\n".format(self.PASSWORD)
        )

        self.expect(client, "PONG :bundle-complete")
        self.assert_server_running()
