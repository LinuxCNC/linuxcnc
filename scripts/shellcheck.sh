#!/bin/bash -e

if ! command -v shellcheck > /dev/null; then
    echo "E: Please install the program 'shellcheck' prior to executing this script."
    exit 1
fi

echo Shellcheck
find . -name "*.sh" -exec shellcheck {} \;
