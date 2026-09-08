#!/bin/bash
# Set up a Python virtual environment so that it can build CRADLE.
echo "Setting up Python environment in .venv..."
set -x -e
virtualenv "$@" --prompt="(cradle) " .venv
source .venv/bin/activate
python --version
pip install gcovr pytest requests websocket-client msgpack pyyaml
# Install the CRADLE Python package (provides "import cradle" for integration tests).
pip install -e python
