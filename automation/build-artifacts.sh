#!/bin/bash -e

# Silence wine 'fixme' warnings
export WINEDEBUG=fixme-all

# Define VARS for make
# Due to https://github.com/wixtoolset/issues/issues/5314
# It is that the path will be under 260 chars
# If we get an light.exe error "The system cannot find the file..."
# This is probably the cause
VIRTIO_WIN_DRIVERS_PATH=${1:?"Must pass in a directory containing virtio-win iso content."}
SPICE_VDAGENT_64_MSI_PATH=${2:?"Must pass path to spice agent x64 msi"}
SPICE_VDAGENT_86_MSI_PATH=${3:?"Must pass path to spice agent x86 msi"}
SPICE_DRIVER_64_MSI_PATH=${4:?"Must pass path to spice driver x64 msi"}
SPICE_DRIVER_86_MSI_PATH=${5:?"Must pass path to spice driver x86 msi"}
QEMU_GA_64_MSI_PATH=${6:?"Must pass path to qemu guest agent x64 msi"}
QEMU_GA_86_MSI_PATH=${7:?"Must pass path to qemu guest agent x86 msi"}
VERSION=${8:?"Must pass in the version of the installer"}

if [ ! -e "${VIRTIO_WIN_DRIVERS_PATH}" -o \
     ! -e "${VIRTIO_WIN_DRIVERS_PATH}/viorng" ] ; then
    echo "Must pass in a directory containing virtio-win iso content. "
    echo "If using virtio-win-pkg-scripts, this is something like "
    echo "tmp*/make-driver-dir-output-*"
    exit 1
fi

PACKAGES="$(cat automation/build-artifacts.packages | xargs)"
if ! rpm -q --quiet $PACKAGES ; then
    echo "ERROR: Not all of the required packages are installed: $PACKAGES"
    exit 1
fi

# Add a /tmp link to the drivers directory, to try and avoid length limitations
VIRTIO_WIN_DRIVERS_LINK=$(mktemp -t vwi.XXXX)
SPICE_VDAGENT_64_MSI_LINK=$(mktemp -t vwi.XXXX)
SPICE_VDAGENT_86_MSI_LINK=$(mktemp -t vwi.XXXX)
SPICE_DRIVER_64_MSI_LINK=$(mktemp -t vwi.XXXX)
SPICE_DRIVER_86_MSI_LINK=$(mktemp -t vwi.XXXX)
QEMU_GA_64_MSI_LINK=$(mktemp -t vwi.XXXX)
QEMU_GA_86_MSI_LINK=$(mktemp -t vwi.XXXX)

ln -sf ${VIRTIO_WIN_DRIVERS_PATH} ${VIRTIO_WIN_DRIVERS_LINK}
ln -sf ${SPICE_VDAGENT_64_MSI_PATH} ${SPICE_VDAGENT_64_MSI_LINK}
ln -sf ${SPICE_VDAGENT_86_MSI_PATH} ${SPICE_VDAGENT_86_MSI_LINK}
ln -sf ${SPICE_DRIVER_64_MSI_PATH} ${SPICE_DRIVER_64_MSI_LINK}
ln -sf ${SPICE_DRIVER_86_MSI_PATH} ${SPICE_DRIVER_86_MSI_LINK}
ln -sf ${QEMU_GA_64_MSI_PATH} ${QEMU_GA_64_MSI_LINK}
ln -sf ${QEMU_GA_86_MSI_PATH} ${QEMU_GA_86_MSI_LINK}

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
make \
    ARCH=x64\
    VERSION=${VERSION}\
    VIRTIO_WIN_DRIVERS_PATH=${VIRTIO_WIN_DRIVERS_LINK}\
    SPICE_VDAGENT_64_MSI_PATH=${SPICE_VDAGENT_64_MSI_LINK}\
    SPICE_VDAGENT_86_MSI_PATH=${SPICE_VDAGENT_86_MSI_LINK}\
    SPICE_DRIVER_64_MSI_PATH=${SPICE_DRIVER_64_MSI_LINK}\
    SPICE_DRIVER_86_MSI_PATH=${SPICE_DRIVER_86_MSI_LINK}\
    QEMU_GA_86_MSI_PATH=${QEMU_GA_86_MSI_LINK}\
    QEMU_GA_64_MSI_PATH=${QEMU_GA_64_MSI_LINK}

make clean

make \
    ARCH=x86\
    VERSION=${VERSION}\
    VIRTIO_WIN_DRIVERS_PATH=${VIRTIO_WIN_DRIVERS_LINK}\
    SPICE_VDAGENT_64_MSI_PATH=${SPICE_VDAGENT_64_MSI_LINK}\
    SPICE_VDAGENT_86_MSI_PATH=${SPICE_VDAGENT_86_MSI_LINK}\
    SPICE_DRIVER_64_MSI_PATH=${SPICE_DRIVER_64_MSI_LINK}\
    SPICE_DRIVER_86_MSI_PATH=${SPICE_DRIVER_86_MSI_LINK}\
    QEMU_GA_86_MSI_PATH=${QEMU_GA_86_MSI_LINK}\
    QEMU_GA_64_MSI_PATH=${QEMU_GA_64_MSI_LINK}

make bundle

unlink ${VIRTIO_WIN_DRIVERS_LINK}
unlink ${SPICE_VDAGENT_64_MSI_LINK}
unlink ${SPICE_VDAGENT_86_MSI_LINK}
unlink ${SPICE_DRIVER_64_MSI_LINK}
unlink ${SPICE_DRIVER_86_MSI_LINK}
unlink ${QEMU_GA_64_MSI_LINK}
unlink ${QEMU_GA_86_MSI_LINK}

make test
popd

find \
    tmp/virtio-win-drivers-installer/ \
    -iname virtio*msi \
    -exec mv {} exported-artifacts/ \;

find \
    tmp/ \
    -iname virtio-win-guest-tools.exe \
    -exec mv {} exported-artifacts/ \;
