from tests.irc_test_case import IRCIntegrationTest


class RegistrationTests(IRCIntegrationTest):
    def test_valid_registration_accepts_registered_command(self):
        client = self.register_client("alice")

        client.send_command("PING still-registered")

        self.expect(client, "PONG :still-registered")
        self.assert_server_running()

    def test_invalid_password_returns_464(self):
        client = self.new_client()

        client.send_command("PASS wrongpassword")

        self.expect(client, " 464 ")

    def test_command_before_registration_returns_451(self):
        client = self.new_client()

        client.send_command("JOIN #general")

        self.expect(client, " 451 ")

    def test_duplicate_nickname_returns_433(self):
        self.register_client("alice")
        second = self.new_client()
        second.send_command("PASS " + self.PASSWORD)

        second.send_command("NICK alice")

        self.expect(second, " 433 ")

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
