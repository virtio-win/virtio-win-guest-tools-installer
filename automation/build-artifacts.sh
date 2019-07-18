#!/bin/bash -xe

# cleanup leftovers from previous builds
rm -rf exported-artifacts
rm -rf tmp.repos

# Create dirs
mkdir -p exported-artifacts
mkdir -p tmp.repos
# mkdir -p vdagent
# mkdir -p virtio-win
# mkdir wix311-binaries

# init wine
chown $(whoami) $(pwd)
winecfg

if [ -e /etc/fedora-release ]; then
	dnf builddep --spec ovirt-wgt-wix.spec
else
	yum-builddep ovirt-wgt-wix.spec
fi

make dist
#Define windows paths
# VIRTIO_WIN_PATH=$(winepath -w $(pwd)/virtio-win|sed 's|\\|\\\\|g')
# OVIRT_GA_WIN_PATH=$(winepath -w $(pwd)/ovirt-guest-agent|sed 's|\\|\\\\|g')
# VDAGENT_WIN_PATH=$(winepath -w $(pwd)/vdagent|sed 's|\\|\\\\|g')

# Unpack virtio driver files
# 7z -o
# mkdir tmp
# mount /usr/share/virtio-win/virtio-win.iso ./tmp
# cp -r tmp/* virtio-win/
# umount ./tmp
# rm -rfd ./tmp
# sed "s:@@VIRTIO-WIN-PATH@@:$(VIRTIO_WIN_PATH):" -i installer/constants.wxi
# link ovirt-guest-agent to the original folder
# ln -s /usr/share/ovirt-guest-agent-windows/ ovirt-guest-agent
# sed "s:@@OVIRT-GA-PATH@@:$(OVIRT_GA_WIN_PATH):" -i installer/constants.wxi
# spice agent
# ln -s /usr/i686-w64-mingw32/sys-root/mingw/bin/ vdagent/x64
# ln -s /usr/x86_64-w64-mingw32/sys-root/mingw/bin/ vdagent/x86
# sed "s:@@VDAGENT-WIN-PATH@@:$(VDAGENT_WIN_PATH):" -i installer/constants.wxi


# Get Wix 
# wget https://github.com/wixtoolset/wix3/releases/download/wix3111rtm/wix311-binaries.zip
# unzip wix311-binaries.zip -d wix311-binaries
# sed "s:@@WIX_BIN_PATH@@:$(pwd)/wix311-binaries:" -i installer/build_args/*
# sed "s:@@INSTALLER_PATH@@:$(pwd)/installer:" -i installer/build_args/*
# Compile the files
# pushd installer/
# wine cmd.exe /c "../wix311-binaries/candle.exe @build_args/candle_argsx64.txt"
# popd

if git describe --exact-match --tags --match "[0-9]*" > /dev/null 2>&1 ; then
    SUFFIX=""
else
    SUFFIX=".$(date -u +%Y%m%d%H%M%S).git$(git rev-parse --short HEAD)"
fi

if ! rpm --eval "%dist" | grep -qFi 'el6'; then
    yum-builddep -y ovirt-wgt-wix.spec
    rpmbuild \
        -D "_topdir $PWD/tmp.repos" \
        -D "_sourcedir $PWD" \
        ${SUFFIX:+-D "release_suffix ${SUFFIX}"} \
        -ba ovirt-wgt-wix.spec
fi

if rpm --eval "%dist" | grep -qFi 'el6'; then
    yum-builddep -y ovirt-guest-agent.rhel6.spec
    rpmbuild \
        -D "_topdir $PWD/tmp.repos" \
        -D "_sourcedir $PWD" \
        ${SUFFIX:+-D "release_suffix ${SUFFIX}"} \
        -ba ovirt-guest-agent.rhel6.spec
else
    yum-builddep -y ovirt-guest-agent.spec
    rpmbuild \
        -D "_topdir $PWD/tmp.repos" \
        -D "_sourcedir $PWD" \
        ${SUFFIX:+-D "release_suffix ${SUFFIX}"} \
        -ba ovirt-guest-agent.spec
fi

mv *.tar.bz2 exported-artifacts
find \
    "$PWD/tmp.repos" \
    -iname \*.rpm \
    -exec mv {} exported-artifacts/ \;
