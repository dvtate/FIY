#!/bin/bash

###
# FIY CLI Tool
###

# This should get set by install script
INSTALL_PATH="/opt/fiy"
cd "$INSTALL_PATH" || {
    echo "Failed to cd into $INSTALL_PATH. Maybe the installation has moved?"
    exit 1
}

if [ "$1" = "admin" ]; then
    if [ "$2" = "add" ]; then
        sqlite3 db.db3 "UPDATE Users SET isAdmin=1 WHERE username='${3//\'/\'\'}' OR email='${3//\'/\'\'}'"
        exit
    fi
    if [ "$2" = "rm" ]; then
        sqlite3 db.db3 "UPDATE Users SET isAdmin=0 WHERE username='${3//\'/\'\'}' OR email='${3//\'/\'\'}'"
        exit
    fi
    if [ "$2" = "list" ]; then
          sqlite3 db.db3 "SELECT username, email FROM Users WHERE isAdmin != 0"
    fi
fi
