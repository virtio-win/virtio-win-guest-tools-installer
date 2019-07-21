#!/bin/bash -xe

# cleanup leftovers from previous builds
rm -rf exported-artifacts
rm -rf tmp.repos
rm -f *.tar.gz

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

if git describe --exact-match --tags --match "[0-9]*" > /dev/null 2>&1 ; then
    SUFFIX=""
else
    SUFFIX=".$(date -u +%Y%m%d%H%M%S).git$(git rev-parse --short HEAD)"
fi

rpmbuild \
    -D "_topdir $PWD/tmp.repos" \
    -D "_sourcedir $PWD" \
    ${SUFFIX:+-D "release_suffix ${SUFFIX}"} \
    -ba ovirt-wgt-wix.spec

mv *.tar.gz exported-artifacts

find \
    "$PWD/tmp.repos/"{RPMS,SRPMS} \
    -iname \*.rpm \
    -exec mv {} exported-artifacts/ \;
