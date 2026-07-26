from tests.irc_test_case import IRCIntegrationTest


class UnknownCommandTests(IRCIntegrationTest):
    def test_registered_user_receives_421_for_unknown_command(self):
        alice = self.register_client("alice")

        alice.send_command("UNKNOWN")

        self.expect(
            alice,
            f":{self.server_name} 421 alice UNKNOWN :Unknown command",
        )
