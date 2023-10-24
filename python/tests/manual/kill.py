import pytest


# fails as there is no response
def test_kill(session):
    session.kill()
