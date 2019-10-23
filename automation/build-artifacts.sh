#!/bin/bash -e

# Silence wine 'fixme' warnings
export WINEDEBUG=fixme-all

# Define VARS for make
# Due to https://github.com/wixtoolset/issues/issues/5314
# It is that the path will be under 260 chars
# If we get an light.exe error "The system cannot find the file..."
# This is probably the cause
VIRTIO_WIN_DRIVERS_PATH=${1:-"$PWD/vwi"}

if [ ! -e "${VIRTIO_WIN_DRIVERS_PATH}" -o \
     ! -e "${VIRTIO_WIN_DRIVERS_PATH}/viorng" ] ; then
    echo "Must pass in a directory containing virtio-win iso content. "
    echo "If using virtio-win-pkg-scripts, this is something like "
    echo "tmp*/make-driver-dir-output-*"
    exit 1
fi

VERSION=${2:?"Must pass in the version of the installer"}

PACKAGES="$(cat automation/build-artifacts.packages | xargs)"
if ! rpm -q --quiet $PACKAGES ; then
    echo "ERROR: Not all of the required packages are installed: $PACKAGES"
    exit 1
fi

# Add a /tmp link to the drivers directory, to try and avoid length limitations
VIRTIO_WIN_DRIVERS_LINK=$(mktemp -t vwi.XXXX)
ln -sf ${VIRTIO_WIN_DRIVERS_PATH} ${VIRTIO_WIN_DRIVERS_LINK}

# Enable command tracing
set -x

# cleanup leftovers from previous builds
make clean

# Create dirs
mkdir -p exported-artifacts
mkdir -p tmp

# Pack all files is tar.gz file
make dist VERSION=${VERSION}

# Extract tar.gz to tmp-build-dir (to build in a clean dir)
find \
    ./ \
    -iname \*.tar.gz \
    -exec tar -C tmp/ -xvf {} \;
mv *.tar.gz exported-artifacts/

# Build the installer
pushd tmp
make ARCH=x64 VERSION=${VERSION} VIRTIO_WIN_DRIVERS_PATH=${VIRTIO_WIN_DRIVERS_LINK}
make clean
make ARCH=x86 VERSION=${VERSION} VIRTIO_WIN_DRIVERS_PATH=${VIRTIO_WIN_DRIVERS_LINK}

unlink ${VIRTIO_WIN_DRIVERS_LINK}
make test
popd

find \
    tmp/ \
    -iname virtio*msi \
    -exec mv {} exported-artifacts/ \;
