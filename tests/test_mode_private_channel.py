from tests.irc_test_case import IRCIntegrationTest, IRCClientTimeout

class PrivateChannelTests(IRCIntegrationTest):
    CHANNEL = "#privatechannel"

    def test_key_mode_non_members_cannot_join_without_password(self):
        """ Verifica que un canal privado no permite que los usuarios que no son miembros se unan sin la contraseña correcta. """
        operator = self.register_client("operator")
        alice = self.register_client("alice")

        self.join_channel(operator, "operator", self.CHANNEL)

        operator.send_command("MODE " + self.CHANNEL + " +k 123pass")
        operator.send_command("PING :mode private set")
        self.expect(operator, "PONG :mode private set")

        alice.send_command("JOIN " + self.CHANNEL)
        self.expect(alice, ":host 475 alice " + self.CHANNEL + " :Cannot join channel (+k)")

    def test_key_mode_join_with_wrong_key_returns_475(self):
        """ Verifica que un canal privado devuelve el código 475 cuando un usuario intenta unirse con una clave incorrecta. """
        operator = self.register_client("operator")
        bob = self.register_client("bob")

        self.join_channel(
            operator,
            "operator",
            self.CHANNEL,
        )

        operator.send_command(
            f"MODE {self.CHANNEL} +k correct-key"
        )
        operator.send_command("PING :key-set")
        self.expect(operator, "PONG :key-set")

        bob.send_command(
            f"JOIN {self.CHANNEL} wrong-key"
        )

        self.expect(bob, f":host 475 bob {self.CHANNEL} :Cannot join channel (+k)")
        self.assert_server_running()
   
    def test_key_mode_setting_key_without_parameter_returns_461(self):
        """ Verifica que un canal privado devuelve el código 461 cuando se intenta establecer una clave sin proporcionar un parámetro. """
        operator = self.register_client("operator")
        bob = self.register_client("bob")

        self.join_channel(
            operator,
            "operator",
            self.CHANNEL,
        )

        operator.send_command(
            f"MODE {self.CHANNEL} +k"
        )

        self.expect(operator, " 461 ")
        self.assert_server_running()

        bob.send_command(f"JOIN {self.CHANNEL}")

        self.expect(
            bob,
            f":bob!bob@host JOIN {self.CHANNEL}",
        )

    def test_key_mode_non_operator_cannot_set_channel_key(self):
        """ Verifica que un canal privado devuelve el código 482 cuando un usuario que no es operador intenta establecer una clave. """
        operator = self.register_client("operator")
        bob = self.register_client("bob")

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
            f"MODE {self.CHANNEL} +k secret"
        )

        self.expect(bob, " 482 ")
        self.assert_server_running()

    def test_key_mode_removing_key_allows_join_without_key(self):
        """ Verifica que un canal privado permite que los usuarios se unan sin una clave después de que se haya eliminado la clave. """
        operator = self.register_client("operator")
        bob = self.register_client("bob")

        self.join_channel(
            operator,
            "operator",
            self.CHANNEL,
        )

        operator.send_command(
            f"MODE {self.CHANNEL} +k 123pass"
        )
        operator.send_command("PING :key-set")
        self.expect(operator, "PONG :key-set")

        bob.send_command(f"JOIN {self.CHANNEL}")
        self.expect(bob, " 475 ")

        operator.send_command(
            f"MODE {self.CHANNEL} -k 123pass"
        )
        operator.send_command("PING :key-removed")
        self.expect(operator, "PONG :key-removed")

        bob.send_command(f"JOIN {self.CHANNEL}")

        self.expect(
            bob,
            f":bob!bob@host JOIN {self.CHANNEL}",
        )
        
    def test_key_mode_returns_mode_info_324(self):
        """ Verifica que el comando MODE para un canal privado devuelve la información de modo correcta (código 324). """
        operator = self.register_client("operator")

        self.join_channel(operator, "operator", self.CHANNEL)

        operator.send_command("MODE " + self.CHANNEL + " +k 123pass")
        operator.send_command("PING :mode private set")
        self.expect(operator, "PONG :mode private set")
        operator.send_command("MODE " + self.CHANNEL)

        self.expect(operator, f":host 324 operator {self.CHANNEL} +k 123pass")

    def test_key_mode_change_is_broadcast_to_channel_members(self):
        """ 
        Verifica que un canal privado no notifica a los usuarios que no son miembros
        cuando un nuevo usuario se une al canal.
        """
        operator = self.register_client("operator", username="username")
        bob = self.register_client("bob")

        # self.join_channel(operator, "operator", self.CHANNEL)
        self.join_channel(operator, "operator", self.CHANNEL)
        self.join_channel(bob, "bob", self.CHANNEL)

        operator.send_command("MODE " + self.CHANNEL + " +k 123pass")
        operator.send_command("PING :mode private set")
        self.expect(operator, "PONG :mode private set")

        self.expect(bob, ":operator!username@host MODE " + self.CHANNEL + " +k 123pass")

        charlie = self.register_client("charlie")
        charlie.drain()

        operator.send_command(
            f"MODE {self.CHANNEL} +k 123pass"
        )

        self.expect(
            bob,
            f":operator!username@host MODE "
            f"{self.CHANNEL} +k 123pass",
        )

        with self.assertRaises(IRCClientTimeout):
            charlie.read_line(timeout=0.15)

    def test_key_mode_non_members_can_join_with_password(self):
        """ Verifica que un canal privado permite que los usuarios que no son miembros se unan con la contraseña correcta. """
        operator = self.register_client("operator")
        bob = self.register_client("bob")

        self.join_channel(operator, "operator", self.CHANNEL)

        operator.send_command("MODE " + self.CHANNEL + " +k 123pass")
        operator.send_command("PING :mode private set")
        self.expect(operator, "PONG :mode private set")

        bob.send_command("JOIN " + self.CHANNEL + " 123pass")
        self.expect(bob, ":bob!bob@host JOIN " + self.CHANNEL)

    def test_non_operator_cannot_set_channel_key(self):
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
            f"MODE {self.CHANNEL} +k secret"
        )

        self.expect(bob, " 482 ")

        charlie.send_command(f"JOIN {self.CHANNEL}")

        self.expect(
            charlie,
            f":charlie!charlie@host JOIN {self.CHANNEL}",
        )

    def test_removing_key_allows_join_without_key(self):
        operator = self.register_client("operator")
        bob = self.register_client("bob")

        self.join_channel(
            operator,
            "operator",
            self.CHANNEL,
        )

        operator.send_command(
            f"MODE {self.CHANNEL} +k 123pass"
        )
        operator.send_command("PING :key-set")
        self.expect(operator, "PONG :key-set")

        bob.send_command(f"JOIN {self.CHANNEL}")
        self.expect(bob, " 475 ")

        operator.send_command(
            f"MODE {self.CHANNEL} -k 123pass"
        )
        operator.send_command("PING :key-removed")
        self.expect(operator, "PONG :key-removed")

        bob.send_command(f"JOIN {self.CHANNEL}")

        self.expect(
            bob,
            f":bob!bob@host JOIN {self.CHANNEL}",
        )

    def test_user_can_retry_join_with_correct_key(self):
        operator = self.register_client("operator")
        bob = self.register_client("bob")

        self.join_channel(
            operator,
            "operator",
            self.CHANNEL,
        )

        operator.send_command(
            f"MODE {self.CHANNEL} +k correct-key"
        )
        operator.send_command("PING :key-set")
        self.expect(operator, "PONG :key-set")

        bob.send_command(
            f"JOIN {self.CHANNEL} wrong-key"
        )
        self.expect(bob, " 475 ")

        bob.send_command(
            f"JOIN {self.CHANNEL} correct-key"
        )

        self.expect(
            bob,
            f":bob!bob@host JOIN {self.CHANNEL}",
        )

    def test_invitation_does_not_bypass_channel_key(self):
        operator = self.register_client("operator")
        bob = self.register_client("bob")

        self.join_channel(
            operator,
            "operator",
            self.CHANNEL,
        )

        operator.send_command(
            f"MODE {self.CHANNEL} +ik secret"
        )
        operator.send_command("PING :modes-set")
        self.expect(operator, "PONG :modes-set")

        operator.send_command(
            f"INVITE bob {self.CHANNEL}"
        )
        self.expect(
            bob,
            f" INVITE bob :{self.CHANNEL}",
        )

        bob.send_command(f"JOIN {self.CHANNEL}")
        self.expect(bob, " 475 ")

        bob.send_command(
            f"JOIN {self.CHANNEL} secret"
        )

        self.expect(
            bob,
            f":bob!bob@host JOIN {self.CHANNEL}",
        )
