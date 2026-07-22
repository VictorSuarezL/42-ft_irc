import re
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]


class IOArchitectureTests(unittest.TestCase):
    def test_send_is_only_called_from_send_pending_data(self):
        source = (
            PROJECT_ROOT / "src" / "Server.cpp"
        ).read_text(encoding="utf-8")

        send_calls = []

        for line_number, line in enumerate(
            source.splitlines(),
            start=1,
        ):
            if line.lstrip().startswith("//"):
                continue

            if re.search(r"\bsend\s*\(", line):
                send_calls.append((line_number, line.strip()))

        self.assertEqual(
            len(send_calls),
            1,
            "send() must have exactly one call site: {!r}"
            .format(send_calls),
        )

        function_start = source.find(
            "void Server::sendPendingData"
        )
        send_position = source.find("send(", function_start)
        next_function = source.find(
            "\nvoid Server::",
            function_start + 1,
        )

        self.assertNotEqual(function_start, -1)
        self.assertNotEqual(send_position, -1)
        self.assertTrue(
            next_function == -1
            or send_position < next_function,
            "send() is not inside sendPendingData()",
        )
