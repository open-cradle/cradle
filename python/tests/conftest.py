import os
import subprocess
import sys
import time

import pytest
import websocket  # type: ignore

import cradle


# Also in config.yml
SERVER_URL = "ws://localhost:41071"


def server_is_running():
    try:
        websocket.create_connection(SERVER_URL)
    except ConnectionRefusedError:
        return False
    else:
        return True


def start_servers():
    deploy_dir = os.environ["CRADLE_DEPLOY_DIR"]
    executables = [
        deploy_dir + '/websocket_server',
        # websocket_server will start rpclib_server when needed.
        # Doing it here would require testing for a successful ping-pong.
        # deploy_dir + '/rpclib_server',
    ]
    procs = []
    for executable in executables:
        procs.append(subprocess.Popen(
            [executable], cwd=deploy_dir,
            stdout=sys.stdout, stderr=subprocess.STDOUT))
    return procs


def stop_servers(procs):
    for proc in procs:
        proc.kill()


def wait_until_servers_running():
    """
    Attempt to connect to a WebSocket server, retrying for a brief period if it
    doesn't initially work.
    """
    attempts_left = 100
    while True:
        try:
            return websocket.create_connection(SERVER_URL)
        except ConnectionRefusedError:
            time.sleep(0.01)
            attempts_left -= 1
            if attempts_left == 0:
                raise


@pytest.fixture(scope='session')
def server():
    procs = []
    if not server_is_running():
        procs = start_servers()
        wait_until_servers_running()

    yield

    stop_servers(procs)


@pytest.fixture(scope='session')
def session(server):
    return cradle.Session()
