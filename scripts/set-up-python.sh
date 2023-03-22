#!/bin/bash
# Set up a Python virtual environment so that it can build CRADLE.
echo "Setting up Python environment in .venv..."
set -x -e
virtualenv "$@" --prompt="(cradle) " .venv
source .venv/bin/activate
python --version
pip install conan==1.59.0 gcovr pytest websocket-client msgpack
