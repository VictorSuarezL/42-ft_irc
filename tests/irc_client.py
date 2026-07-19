import socket
import time


class IRCClientTimeout(TimeoutError):
    """Raised when an IRC response is not received before the deadline."""


class IRCClient:
    def __init__(self, host, port, timeout=1.0):
        self.socket = socket.create_connection((host, port), timeout=timeout)
        self.socket.settimeout(timeout)
        self._buffer = b""

    def close(self):
        if self.socket is None:
            return
        try:
            self.socket.shutdown(socket.SHUT_RDWR)
        except OSError:
            pass
        self.socket.close()
        self.socket = None

    def send_command(self, command):
        if "\r" in command or "\n" in command:
            raise ValueError("send_command expects one command without CR/LF")
        self.send_raw((command + "\r\n").encode("utf-8"))

    def send_raw(self, data):
        if isinstance(data, str):
            data = data.encode("utf-8")
        self.socket.sendall(data)

    def register(self, nickname, password, username=None):
        if username is None:
            username = nickname
        payload = (
            "PASS {password}\r\n"
            "NICK {nickname}\r\n"
            "USER {username} 0 * :{nickname}\r\n"
        ).format(
            password=password,
            nickname=nickname,
            username=username,
        )
        self.send_raw(payload)

    def read_line(self, timeout=1.0):
        deadline = time.monotonic() + timeout

        while b"\n" not in self._buffer:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise IRCClientTimeout("timed out waiting for an IRC line")

            self.socket.settimeout(remaining)
            try:
                data = self.socket.recv(4096)
            except socket.timeout as error:
                raise IRCClientTimeout(
                    "timed out waiting for an IRC line"
                ) from error

            if not data:
                raise ConnectionError("the IRC server closed the connection")
            self._buffer += data

        raw_line, self._buffer = self._buffer.split(b"\n", 1)
        return raw_line.rstrip(b"\r").decode("utf-8", errors="replace")

    def receive_until(self, expected, timeout=1.0):
        deadline = time.monotonic() + timeout
        received = []

        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise IRCClientTimeout(
                    "did not receive {!r}; received: {!r}".format(
                        expected,
                        received,
                    )
                )

            try:
                line = self.read_line(remaining)
            except IRCClientTimeout as error:
                raise IRCClientTimeout(
                    "did not receive {!r}; received: {!r}".format(
                        expected,
                        received,
                    )
                ) from error
            received.append(line)
            if expected in line:
                return received

    def drain(self, timeout=0.05):
        received = []
        while True:
            try:
                received.append(self.read_line(timeout))
            except IRCClientTimeout:
                return received

    def has_complete_line_buffered(self):
        return b"\n" in self._buffer

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        self.close()
