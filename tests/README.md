# Integration tests

The tests start `ircserv`, connect real TCP clients, exercise IRC commands and stop
the server automatically after each test.

Run the complete suite from the repository root:

```sh
make test
```

Run one module or one test while developing:

```sh
python3 -m unittest -v tests.test_topic
python3 -m unittest -v \
    tests.test_topic.TopicTests.test_operator_can_set_and_query_topic
```

Each test uses a new server process and an available local port, so state is not
shared between tests. A failed test includes the server log in its error message.
