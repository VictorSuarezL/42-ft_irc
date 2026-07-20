from tests.irc_test_case import IRCIntegrationTest


class PingTests(IRCIntegrationTest):
    def test_trailing_parameter_is_preserved_in_pong(self):
        client = self.register_client("alice")

        client.send_command("PING :trailing-token")

        self.expect(client, "PONG :trailing-token")
