from tests.irc_client import IRCClientTimeout
from tests.irc_test_case import IRCIntegrationTest


class PartialMessageTests(IRCIntegrationTest):
    def test_command_is_not_processed_before_newline(self):
        client = self.register_client("alice")

        client.send_raw(b"PI")
        client.send_raw(b"NG fragmented")

        with self.assertRaises(IRCClientTimeout):
            client.read_line(timeout=0.15)

        client.send_raw(b"\r\n")
        self.expect(client, "PONG :fragmented")

    def test_multiple_commands_in_one_packet_are_all_processed(self):
        client = self.register_client("alice")

        client.send_raw(b"PING first\r\nPING second\r\n")

        replies = self.expect(client, "PONG :second")
        self.assertTrue(
            any("PONG :first" in line for line in replies),
            "first PING was not processed: {!r}".format(replies),
        )
        self.assert_server_running()
