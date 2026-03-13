#!/bin/bash

# Usage: ./scripts/update_version.sh <version> <tag>
# Example: ./scripts/update_version.sh 1.0.2 "Main Branch"

if [ "$#" -ne 2 ]; then
    echo "Error: Missing arguments."
    echo "Usage: $0 <version> <tag>"
    exit 1
fi

VERSION=$1
TAG=$2
INI_FILE="platformio.ini"

if [ ! -f "$INI_FILE" ]; then
    echo "Error: $INI_FILE not found in the current directory."
    exit 1
fi

# Use sed to replace the version and tag lines
# We need to be careful with escaped quotes in the ini file
sed -i "s/-D APP_VERSION=\\\".*\\\"/-D APP_VERSION=\\\"$VERSION\\\"/" "$INI_FILE"
sed -i "s/-D BUILD_TAG=\\\".*\\\"/-D BUILD_TAG=\\\"$TAG\\\"/" "$INI_FILE"

echo "Successfully updated $INI_FILE:"
echo "  APP_VERSION -> $VERSION"
echo "  BUILD_TAG   -> $TAG"
