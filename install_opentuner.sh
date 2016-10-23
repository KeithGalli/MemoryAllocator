#!/usr/bin/env bash

################################################################################
# Documentation
#
# Installs the OpenTuner Framework. This script is idempotent.
#
# To run this script on AWS machines:
#
# cp install_opentuner.sh ~
# sudo ~/install_opentuner.sh
#
# The copy step is necessary because sudo shell may not have AFS tokens.
#
# The install process takes ~10 minutes.


################################################################################
# Helper Functions

create_swapfile() {
    # Assume swap file doesn't exist.
    if [ -e "$SWAPFILE" ]
    then
        echo "Delete swap file $SWAPFILE before creating one."
        exit 1
    fi

    # Create swap file.
    dd if=/dev/zero of="$SWAPFILE" bs=1024 count="$(($SWAPSIZE * 1024))"

    # Set permissions.
    chown root:root "$SWAPFILE"
    chmod 0600 "$SWAPFILE"

    # Set up swap file.
    mkswap "$SWAPFILE"

    # Turn on swap file.
    swapon "$SWAPFILE"
}

remove_swapfile() {
    # Turn off swapfile if it's on.
    swapon -s | grep -qFe "$SWAPFILE" && swapoff "$SWAPFILE"

    # Remove swapfile if it exists.
    [ -e "$SWAPFILE" ] && rm -f "$SWAPFILE"
}


################################################################################
# Swap File Parameters

# Full path of swap file for pip install.
SWAPFILE=/swapfile_opentuner

# Swap file size in MB.
SWAPSIZE=1024


################################################################################
# Main script

# Check if root.
ROOT_UID=0
if [ "$UID" -ne "$ROOT_UID" ]
then
    echo "Must be root to install."
    echo "Remember to copy the script to a local (non-AFS) folder before running."
    exit 1
fi

# Remove any existing swap file.
remove_swapfile

# Do nothing if opentuner is already installed.
if ! pip show opentuner >/dev/null
then
    # Install necessary apt packages (removing these breaks installation).
    apt-get -y install pkg-config g++ python-dev libsqlite3-dev libpng-dev libfreetype6-dev

    # These packages are listed as dependencies, but are either already
    # installed on AWS or have been observed to not be necessary.
    apt-get -y install sqlite3 gnuplot

    # This requirement should be satisfied by installing g++.
    # apt-get -y install build-essential

    # Install opentuner package and dependencies using pip.
    create_swapfile
    pip install opentuner
    remove_swapfile
fi
