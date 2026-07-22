from tests.irc_test_case import IRCIntegrationTest, IRCClientTimeout

class InviteTests(IRCIntegrationTest):
    CHANNEL = "#general"


    def test_invite_without_engough_parameters_returns_461(self):
        alice = self.register_client("alice")
        self.join_channel(alice, "alice", self.CHANNEL)

        alice.send_command("INVITE bob")

        self.expect(alice, " 461 ")

    def test_invite_nonexistent_user_returns_401(self):
        alice = self.register_client("alice")
        self.join_channel(alice, "alice", self.CHANNEL)

        alice.send_command("INVITE charlie " + self.CHANNEL)

        self.expect(alice, " 401 ")

    def test_invite_nonexistent_channel_returns_403(self):
        alice = self.register_client("alice")

        alice.send_command("INVITE bob #nonexistent")

        self.expect(alice, " 403 ")

    def test_inviter_not_in_channel_returns_442(self):
        operator = self.register_client("operator")
        alice = self.register_client("alice")
        bob = self.register_client("bob")

        self.join_channel(
            operator,
            "operator",
            self.CHANNEL,
        )

        alice.send_command(
            "INVITE bob " + self.CHANNEL
        )

        self.expect(alice, " 442 ")

    def test_invite_user_not_operator_returns_482(self):
        alice = self.register_client("alice")
        bob = self.register_client("bob")
        self.join_channel(alice, "alice", self.CHANNEL)
        self.join_channel(bob, "bob", self.CHANNEL)

        bob.send_command("INVITE alice " + self.CHANNEL)

        self.expect(bob, " 482 ")

    def test_invite_user_already_in_channel_returns_443(self):
        alice = self.register_client("alice")
        bob = self.register_client("bob")
        self.join_channel(alice, "alice", self.CHANNEL)
        self.join_channel(bob, "bob", self.CHANNEL)

        alice.send_command("INVITE bob " + self.CHANNEL)

        self.expect(alice, " 443 ")

    def test_invite_broadcasts_to_target_user(self):
        alice = self.register_client("alice")
        bob = self.register_client("bob")
        self.join_channel(alice, "alice", self.CHANNEL)

        alice.send_command("INVITE bob " + self.CHANNEL)

        self.expect(bob, f":alice!alice@definitely_not_discord INVITE bob :{self.CHANNEL}")

        self.expect(alice, f" 341 alice bob {self.CHANNEL}")

    def test_invite_is_not_broadcast_to_channel(self):
        alice = self.register_client("alice")
        bob = self.register_client("bob")
        charlie = self.register_client("charlie")

        self.join_channel(alice, "alice", self.CHANNEL)
        self.join_channel(charlie, "charlie", self.CHANNEL)
        charlie.drain()

        alice.send_command("INVITE bob " + self.CHANNEL)

        self.expect(bob, " INVITE bob :#general")

        with self.assertRaises(IRCClientTimeout):
            charlie.read_line(timeout=0.15)

    def test_mode_invite_only_allows_invited_users_to_join(self):
        alice = self.register_client("alice")
        bob = self.register_client("bob")
        charlie = self.register_client("charlie")

        self.join_channel(alice, "alice", self.CHANNEL)

        alice.send_command("MODE " + self.CHANNEL + " +i")

        bob.send_command("JOIN " + self.CHANNEL)

        self.expect(bob, " 473 ")

        alice.send_command("INVITE bob " + self.CHANNEL)

        bob.send_command("JOIN " + self.CHANNEL)

        self.expect(bob, f":bob!bob@definitely_not_discord JOIN {self.CHANNEL}")

    # def test_invitation_is_removed_after_user_joins(self):
    #     alice = self.register_client("alice")
    #     bob = self.register_client("bob")

    #     self.join_channel(alice, "alice", self.CHANNEL)

    #     alice.send_command("MODE " + self.CHANNEL + " +i")

    #     alice.send_command("INVITE bob " + self.CHANNEL)

    #     bob.send_command("JOIN " + self.CHANNEL)

    #     self.expect(bob, f":bob!bob@definitely_not_discord JOIN {self.CHANNEL}")

    #     bob.send_command("PART " + self.CHANNEL)

    #     bob.send_command("JOIN " + self.CHANNEL)

    #     self.expect(bob, " 473 ")
