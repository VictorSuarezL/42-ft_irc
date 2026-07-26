from tests.irc_test_case import IRCIntegrationTest

class InviteOnlyTests(IRCIntegrationTest):
    CHANNEL = "#inviteonly"

    def test_invite_only_rejects_uninvited_user_473(self):
        operator = self.register_client("operator")
        uninvited = self.register_client("uninvited")

        self.join_channel(operator, "operator", self.CHANNEL)

        operator.send_command("MODE " + self.CHANNEL + " +i")
        operator.send_command("PING :mode invite only set")
        self.expect(operator, "PONG :mode invite only set")

        uninvited.send_command("JOIN " + self.CHANNEL)

        self.expect(
            uninvited,
            f":{self.server_name} 473 uninvited #inviteonly "
            ":Cannot join channel (+i)",
        )

    def test_invite_only_accepts_invited_user(self):
        operator = self.register_client("operator")
        bob = self.register_client("bob")

        self.join_channel(
            operator,
            "operator",
            self.CHANNEL,
        )

        operator.send_command(
            "MODE " + self.CHANNEL + " +i"
        )
        operator.send_command("PING :mode-enabled")
        self.expect(operator, "PONG :mode-enabled")

        operator.send_command(
            "INVITE bob " + self.CHANNEL
        )

        # También sincroniza que INVITE ya fue procesado.
        self.expect(
            bob,
            " INVITE bob :" + self.CHANNEL,
        )

        bob.send_command("JOIN " + self.CHANNEL)

        self.expect(
            bob,
            f":bob!bob@{self.server_name} JOIN "
            + self.CHANNEL,
        )

    def test_non_operator_cannot_enable_invite_only(self):
        operator = self.register_client("operator")
        bob = self.register_client("bob")
        charlie = self.register_client("charlie")

        self.join_channel(
            operator,
            "operator",
            self.CHANNEL,
        )
        self.join_channel(
            bob,
            "bob",
            self.CHANNEL,
        )

        bob.send_command(
            "MODE " + self.CHANNEL + " +i"
        )

        self.expect(
            bob,
            f":{self.server_name} 482 bob #inviteonly "
            ":You're not channel operator",
        )

        # Verifica que el modo no se modificó pese al error.
        charlie.send_command("JOIN " + self.CHANNEL)

        self.expect(
            charlie,
            f":charlie!charlie@{self.server_name} JOIN "
            + self.CHANNEL,
        )

    def test_disabling_invite_only_allows_regular_join(self):
        operator = self.register_client("operator")
        bob = self.register_client("bob")

        self.join_channel(
            operator,
            "operator",
            self.CHANNEL,
        )

        operator.send_command(
            "MODE " + self.CHANNEL + " +i"
        )
        operator.send_command("PING :mode-enabled")
        self.expect(operator, "PONG :mode-enabled")

        bob.send_command("JOIN " + self.CHANNEL)
        self.expect(
            bob,
            f":{self.server_name} 473 bob #inviteonly :Cannot join channel (+i)",
        )

        operator.send_command(
            "MODE " + self.CHANNEL + " -i"
        )
        operator.send_command("PING :mode-disabled")
        self.expect(operator, "PONG :mode-disabled")

        bob.send_command("JOIN " + self.CHANNEL)

        self.expect(
            bob,
            f":bob!bob@{self.server_name} JOIN "
            + self.CHANNEL,
        )
