#!/bin/bash
set -e

BUILD_TYPE="$1"

command -v npm >/dev/null 2>&1 || { echo >&2 "npm is required but not installed.  Aborting."; exit 1; }
command -v cmake >/dev/null 2>&1 || { echo >&2 "cmake is required but not installed.  Aborting."; exit 1; }

# Build protocol server and mods using cmake
function build_cpp {
    mkdir build | :
    cd build
    if [ "$BUILD_TYPE" = "debug" ]; then
        cmake -DCMAKE_BUILD_TYPE=Debug ..
    else
        cmake ..
    fi
    make -j`nproc`
    cd ..
}

# Build contacts mod which has uses TS/JS+webpack
# TODO make a separate cmake module for it and run npm commands from it
function build_contacts_mod {
    if [[ "$SKIP_CONTACTS" -eq 1 ]] || [[ "$NO_JS" -eq 1 ]]; then
        return 0
    fi
    cd mods/contacts/frontend
    npm install
    npm run build
    cd ../../..
}

# Minify portal_frontend assets
function minify_portal_frontend {
    : # TODO
}

# Run build tasks in parallel
build_cpp & CPP_PID=$!
build_contacts_mod & CONTACTS_PID=$!
wait $CPP_PID || exit 1
wait $CONTACTS_PID || exit 2
