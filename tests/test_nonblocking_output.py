import socket
import time

from tests.irc_client import IRCClientTimeout
from tests.irc_test_case import IRCIntegrationTest


class NonBlockingOutputTests(IRCIntegrationTest):
    MESSAGE_COUNT = 20000
    STRESS_TIMEOUT = 10.0

    def test_slow_client_does_not_block_or_lose_messages(self):
        slow_client = self.register_client("slow")
        fast_client = self.register_client("fast")

        # Elimina respuestas anteriores.
        slow_client.drain()
        fast_client.drain()

        # Reduce el buffer de recepción del cliente lento.
        slow_client.socket.setsockopt(
            socket.SOL_SOCKET,
            socket.SO_RCVBUF,
            1024,
        )

        payload = "".join(
            "PING slow-{0:05d}\r\n".format(index)
            for index in range(self.MESSAGE_COUNT)
        )

        # Permitimos más tiempo para enviar el bloque de comandos.
        slow_client.socket.settimeout(self.STRESS_TIMEOUT)

        # Envía miles de comandos, pero todavía no lee respuestas.
        slow_client.send_raw(payload)

        # El otro cliente debe continuar funcionando inmediatamente.
        fast_client.send_command("PING fast-client")

        self.expect(
            fast_client,
            "PONG :fast-client",
            timeout=2.0,
        )

        self.assert_server_running()

        # Ahora comenzamos a leer las respuestas del cliente lento.
        deadline = time.monotonic() + self.STRESS_TIMEOUT

        for index in range(self.MESSAGE_COUNT):
            remaining = deadline - time.monotonic()

            if remaining <= 0:
                self.fail(
                    "timed out after receiving {0}/{1} responses\n"
                    "Server output:\n{2}".format(
                        index,
                        self.MESSAGE_COUNT,
                        self.server.output(),
                    )
                )

            try:
                line = slow_client.read_line(remaining)
            except (IRCClientTimeout, ConnectionError) as error:
                self.fail(
                    "failed after receiving {0}/{1} responses: {2}\n"
                    "Server output:\n{3}".format(
                        index,
                        self.MESSAGE_COUNT,
                        error,
                        self.server.output(),
                    )
                )

            expected = "PONG :slow-{0:05d}".format(index)

            self.assertEqual(
                line,
                expected,
                "responses were lost, duplicated or reordered",
            )

        self.assert_server_running()
