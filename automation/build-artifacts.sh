#!/bin/bash -xe

# cleanup leftovers from previous builds
make clean

# Create dirs
mkdir -p exported-artifacts
mkdir -p tmp

# init wine
chown $(whoami) $(pwd)
winecfg

# Define VARS for make
VERSION=4.4
VIRTIO_WIN_DRIVERS_PATH=/home/gzaidman/workspace/upstream/virtio-win-guest-tools-installer/vwi/
# Install dependencies
if [ -e /etc/fedora-release ]; then
    dnf -y install $(cat build-artifacts.packages)
else
    yum -y install $(cat build-artifacts.packages)
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
    -regex ".*\.\(virtio*\.msi\|tar\.gz\)" \
    -exec mv {} exported-artifacts/ \;
