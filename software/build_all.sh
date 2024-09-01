#!/bin/bash

# Ensure the script exits on any error
set -e

# Get the list of configure presets from the CMakePresets.json file
PRESETS=$(cmake --list-presets | grep -Eo '"[^"]+"' | tr -d '"')

echo $PRESETS
# Loop through each preset and run configuration and build
for PRESET in $PRESETS; do
    echo "Configuring project with preset: $PRESET"
    cmake --preset "$PRESET"
    
    echo "Building project with preset: $PRESET"
    cmake --build --preset "$PRESET"
    
    echo "Finished building with preset: $PRESET"
    echo "-----------------------------------"
done

echo "All presets have been configured and built."
