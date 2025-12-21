#!/bin/bash

# Setup Unity test framework for hash shell

set -e

UNITY_VERSION="2.6.1"
UNITY_URL="https://github.com/ThrowTheSwitch/Unity/archive/refs/tags/v${UNITY_VERSION}.tar.gz"
TEST_DIR="tests"
UNITY_DIR="${TEST_DIR}/unity"

echo "Setting up Unity test framework..."

# Create tests directory if it doesn't exist
mkdir -p "${TEST_DIR}"

# Download Unity if not already present
if [ ! -d "${UNITY_DIR}" ]; then
    echo "Downloading Unity v${UNITY_VERSION}..."

    cd "${TEST_DIR}"
    curl -L "${UNITY_URL}" -o unity.tar.gz
    tar -xzf unity.tar.gz
    mv "Unity-${UNITY_VERSION}" unity
    rm unity.tar.gz
    cd ..

    echo "Unity downloaded successfully!"
else
    echo "Unity already exists at ${UNITY_DIR}"
fi

# Create a simple unity.h wrapper if needed
if [ ! -f "${TEST_DIR}/unity.h" ]; then
    cat > "${TEST_DIR}/unity.h" << 'EOF'
#ifndef UNITY_WRAPPER_H
#define UNITY_WRAPPER_H

#include "unity/src/unity.h"

#endif // UNITY_WRAPPER_H
EOF
    echo "Created unity.h wrapper"
fi

echo ""
echo "Unity setup complete!"
echo "You can now run: make test"
