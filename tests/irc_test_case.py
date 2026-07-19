import signal
import socket
import subprocess
import tempfile
import time
import unittest
from pathlib import Path

from tests.irc_client import IRCClient, IRCClientTimeout


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SERVER_BINARY = PROJECT_ROOT / "ircserv"


def find_available_port():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as probe:
        probe.bind(("127.0.0.1", 0))
        return probe.getsockname()[1]


class IRCServerProcess:
    def __init__(self, password="testpass"):
        self.password = password
        self.port = find_available_port()
        self.process = None
        self._log = tempfile.TemporaryFile(mode="w+b")

    def start(self, timeout=2.0):
        if not SERVER_BINARY.exists():
            raise RuntimeError(
                "ircserv does not exist; run make before the tests"
            )

        self.process = subprocess.Popen(
            [str(SERVER_BINARY), str(self.port), self.password],
            cwd=str(PROJECT_ROOT),
            stdout=self._log,
            stderr=subprocess.STDOUT,
        )

        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if self.process.poll() is not None:
                server_output = self.output()
                self.stop()
                raise RuntimeError(
                    "ircserv exited during startup:\n" + server_output
                )
            try:
                with socket.create_connection(
                    ("127.0.0.1", self.port), timeout=0.05
                ):
                    return
            except OSError:
                time.sleep(0.02)

        self.stop()
        raise RuntimeError("ircserv did not start before the timeout")

    def stop(self):
        if self.process is not None and self.process.poll() is None:
            self.process.send_signal(signal.SIGINT)
            try:
                self.process.wait(timeout=2.0)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait(timeout=1.0)

        self.process = None
        if not self._log.closed:
            self._log.close()

    def output(self):
        self._log.flush()
        position = self._log.tell()
        self._log.seek(0)
        data = self._log.read().decode("utf-8", errors="replace")
        self._log.seek(position)
        return data

    def is_running(self):
        return self.process is not None and self.process.poll() is None


class IRCIntegrationTest(unittest.TestCase):
    PASSWORD = "testpass"
    RESPONSE_TIMEOUT = 1.0

    def setUp(self):
        self.server = IRCServerProcess(self.PASSWORD)
        self.server.start()
        self.clients = []

    def tearDown(self):
        for client in self.clients:
            client.close()
        self.server.stop()

    def new_client(self):
        client = IRCClient("127.0.0.1", self.server.port)
        self.clients.append(client)
        return client

    def register_client(self, nickname, password=None, username=None):
        client = self.new_client()
        client.register(
            nickname,
            self.PASSWORD if password is None else password,
            username,
        )
        token = "registration-" + nickname
        client.send_command("PING " + token)
        self.expect(client, "PONG :" + token)
        return client

    def join_channel(self, client, nickname, channel, key=None):
        command = "JOIN " + channel
        if key is not None:
            command += " " + key
        client.send_command(command)
        return self.expect(client, " 366 " + nickname + " " + channel + " ")

    def expect(self, client, expected, timeout=None):
        if timeout is None:
            timeout = self.RESPONSE_TIMEOUT
        try:
            return client.receive_until(expected, timeout)
        except (IRCClientTimeout, ConnectionError) as error:
            self.fail(
                "{}\n\nServer output:\n{}".format(
                    error,
                    self.server.output(),
                )
            )

    def assert_server_running(self):
        self.assertTrue(
            self.server.is_running(),
            "ircserv stopped unexpectedly:\n" + self.server.output(),
        )
