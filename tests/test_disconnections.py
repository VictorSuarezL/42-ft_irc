from tests.irc_test_case import IRCIntegrationTest


class DisconnectionTests(IRCIntegrationTest):
    CHANNEL = "#temporary"

    def test_closed_connections_do_not_poison_poll(self):
        """
        Fuerza varias conexiones y desconexiones para favorecer
        la reutilización de file descriptors.
        """
        for _ in range(5):
            client = self.new_client()
            client.close()

        survivor = self.register_client("survivor")

        survivor.send_command("PING server-alive")
        self.expect(survivor, "PONG :server-alive")
        self.assert_server_running()

    def test_disconnected_user_releases_nickname(self):
        alice = self.register_client("alice")
        observer = self.register_client("observer")

        alice.close()

        # Sincroniza con el servidor sin utilizar sleep().
        observer.send_command("PING cleanup-finished")
        self.expect(observer, "PONG :cleanup-finished")

        replacement = self.register_client("alice")

        replacement.send_command("PING nickname-reused")
        self.expect(replacement, "PONG :nickname-reused")
        self.assert_server_running()

    def test_disconnection_removes_empty_channel(self):
        alice = self.register_client("alice")
        observer = self.register_client("observer")

        self.join_channel(alice, "alice", self.CHANNEL)
        alice.close()

        # Cuando recibamos este PONG, la desconexión anterior
        # ya debería haber sido procesada.
        observer.send_command("PING cleanup-finished")
        self.expect(observer, "PONG :cleanup-finished")

        replies = self.join_channel(
            observer,
            "observer",
            self.CHANNEL,
        )

        self.assertTrue(
            any(
                " 353 observer = #temporary :@observer "
                in line
                for line in replies
            ),
            "the recreated channel did not assign operator status: {!r}"
            .format(replies),
        )

    def test_disconnection_does_not_affect_other_clients(self):
        alice = self.register_client("alice")
        bob = self.register_client("bob")

        alice.close()

        bob.send_command("PING bob-survives")
        self.expect(bob, "PONG :bob-survives")
        self.assert_server_running()
