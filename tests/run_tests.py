import sys
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
TESTS_DIR = PROJECT_ROOT / "tests"

if str(PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(PROJECT_ROOT))

from tests.irc_test_case import IRCExpectationFailure


class ConciseIRCResult(unittest.TextTestResult):
    def _exc_info_to_string(self, error, test):
        exception = error[1]

        if isinstance(exception, IRCExpectationFailure):
            return str(exception).lstrip()

        return super()._exc_info_to_string(error, test)


def load_suite():
    loader = unittest.defaultTestLoader

    if len(sys.argv) > 1:
        return loader.loadTestsFromNames(sys.argv[1:])

    return loader.discover(
        start_dir=str(TESTS_DIR),
        top_level_dir=str(PROJECT_ROOT),
    )


def main():
    runner = unittest.TextTestRunner(
        verbosity=2,
        resultclass=ConciseIRCResult,
    )
    result = runner.run(load_suite())
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    raise SystemExit(main())
