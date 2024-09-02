#!/bin/bash

# Check if a project name was provided as an argument
if [ -z "$1" ]; then
  echo "Error: Project name is missing."
  echo "Usage: $0 <project_name>"
  exit 1
fi

# Store the project name from the first argument
PROJECT_NAME="$1"

# Remove the old build directory
rm -rf out/"$PROJECT_NAME"

# Run CMake with the specified preset
cmake --preset "$PROJECT_NAME"

# Build the project using the specified preset
cmake --build --preset "$PROJECT_NAME"

