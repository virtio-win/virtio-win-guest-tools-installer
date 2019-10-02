#!/bin/bash -xe

# Silence wine 'fixme' warnings
export WINEDEBUG=fixme-all

# cleanup leftovers from previous builds
make clean

# Create dirs
mkdir -p exported-artifacts
mkdir -p tmp

# init wine
chown $(whoami) $(pwd)
winecfg

# Define VARS for make
# Due to https://github.com/wixtoolset/issues/issues/5314
# It is that the path will be under 260 chars
# If we get an light.exe error "The system cannot find the file..."
# This is probably the cause
VIRTIO_WIN_DRIVERS_PATH=${1:-"$PWD/vwi"}
VERSION=${2:?"Must pass in the version of the installer"}
# Install dependencies
if [ -e /etc/fedora-release ]; then
    dnf -y install $(cat automation/build-artifacts.packages)
else
    yum -y install $(cat automation/build-artifacts.packages)
fi

# Pack all files is tar.gz file
make dist VERSION=${VERSION}

# Extract tar.gz to tmp-build-dir (to build in a clean dir)
find \
    ./ \
    -iname \*.tar.gz \
    -exec tar -C tmp/ -xvf {} \;

# Build the installer
pushd tmp
make ARCH=x64 VERSION=${VERSION} VIRTIO_WIN_DRIVERS_PATH=${VIRTIO_WIN_DRIVERS_PATH}
make clean
make ARCH=x86 VERSION=${VERSION} VIRTIO_WIN_DRIVERS_PATH=${VIRTIO_WIN_DRIVERS_PATH}

make test
popd

find \
    "$PWD/" \
    -iname virtio*tar.gz \
    -exec mv {} exported-artifacts/ \;

find \
    "$PWD/" \
    -iname virtio*msi \
    -exec mv {} exported-artifacts/ \;
