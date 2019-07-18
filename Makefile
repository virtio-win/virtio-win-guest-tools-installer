.ONESHELL:
SHELL=/bin/bash

NAME=ovirt-wgt
VERSION=4.4
DISPLAYED_VERSION=$(VERSION)
ARCHIVE=$(NAME)-$(VERSION).tar.gz
RELEASE_SUFFIX=_master

###################################
# Project Paths #
VIRTIO_WIN_DRIVERS_PATH=$(CURDIR)/virtio-win
VDAGENT_PATH=$(CURDIR)/vdagent
WIX_BINARIES_PATH=$(CURDIR)/wix311-binaries
OVIRT_GA_PATH=$(CURDIR)/ovirt-guest-agent
QEMU_GA_PATH=$(CURDIR)/linux/{debian,el6,el7,fc28,fc29,lp151}
ISO_PATH=$(CURDIR)/ISO
# Windows Paths #
VIRTIO_WIN_PATH=$(shell winepath -w $(VIRTIO_WIN_DRIVERS_PATH)|sed 's|\\|\\\\\\\\|g')
OVIRT_GA_WIN_PATH=$(shell winepath -w $(OVIRT_GA_PATH)|sed 's|\\|\\\\\\\\|g')
VDAGENT_WIN_PATH=$(shell winepath -w $(VDAGENT_PATH)|sed 's|\\|\\\\\\\\|g')
WIX_BINARIES_WIN_PATH=$(shell winepath -w $(WIX_BINARIES_PATH)|sed 's|\\|\\\\\\\\|g')
INSTALLER_WIN_PATH=$(shell winepath -w $(CURDIR)/installer|sed 's|\\|\\\\\\\\|g')
#
VIRTIO_WIN_ISO=/usr/share/virtio-win/virtio-win.iso
OVIRTGA_PATH=/usr/share/ovirt-guest-agent-windows
VDA32BIN=/usr/i686-w64-mingw32/sys-root/mingw/bin/
VDA64BIN=/usr/x86_64-w64-mingw32/sys-root/mingw/bin/
WIX_BINARIES_URL="https://github.com/wixtoolset/wix3/releases/download/wix3111rtm/wix311-binaries.zip"
QEMU_DEB_URL="http://mirror.isoc.org.il/pub/debian/pool/main/q/qemu/qemu-guest-agent_2.8%2bdfsg-6%2bdeb9u7_amd64.deb"
QEMU_EL6_URL="http://mirror.isoc.org.il/pub/centos/6/os/x86_64/Packages/qemu-guest-agent-0.12.1.2-2.503.el6_9.6.x86_64.rpm"
QEMU_EL7_URL="http://mirror.isoc.org.il/pub/centos/7/os/x86_64/Packages/qemu-guest-agent-2.12.0-2.el7.x86_64.rpm"
QEMU_FC28_URL="http://mirror.isoc.org.il/pub/fedora/releases/28/Everything/x86_64/os/Packages/q/qemu-guest-agent-2.11.1-2.fc28.x86_64.rpm"
QEMU_FC29_URL="http://mirror.isoc.org.il/pub/fedora/releases/29/Everything/x86_64/os/Packages/q/qemu-guest-agent-3.0.0-1.fc29.x86_64.rpm"
# install targets
PREFIX=/usr/local
DATAROOT_DIR=$(PREFIX)/share
INSTALL_NAME=ovirt-guest-tools-iso
INSTALL_DATA_DIR=$(DATAROOT_DIR)/$(INSTALL_NAME)
ISO_IMAGE=oVirt-toolsSetup_$(DISPLAYED_VERSION).iso
ISO_P_TEXT=oVirt - KVM Virtualization Manager Project (www.ovirt.org)
ISO_LABEL=oVirt-WGT-$(DISPLAYED_VERSION)
###################################


# Available from http://www.microsoft.com/en-us/download/details.aspx?id=5582
# RPM wrapping this available from http://resources.ovirt.org/pub/ovirt-3.6/rpm/fc22
VCREDIST=/usr/share/vcredist-x86/vcredist_x86.exe

# Common definitions for targets


all: | make-dirs init-files edit-project-paths create-iso

make-dirs:
	mkdir -p $(VDAGENT_PATH)
	mkdir -p $(VIRTIO_WIN_DRIVERS_PATH)
	mkdir -p $(WIX_BINARIES_PATH)
	mkdir -p $(ISO_PATH)
	mkdir -p $(QEMU_GA_PATH)

init-files: | virtio-win ovirt-guest-agent vdagent qemu-ga wix;

# extract the iso to get the drivers
virtio-win:
	7z -o"$(VIRTIO_WIN_DRIVERS_PATH)" x "$(VIRTIO_WIN_ISO)"
	hardlink -vv "$(VIRTIO_WIN_DRIVERS_PATH)"

# link ovirt-guest-agent to the original folder from the rpm
ovirt-guest-agent:
	ln -s "$(OVIRTGA_PATH)" "$(OVIRT_GA_PATH)"

vdagent:
	ln -s "$(VDA32BIN)" $(VDAGENT_PATH)/x64
	ln -s "$(VDA64BIN)" $(VDAGENT_PATH)/x86

qemu-ga:
	wget -P linux/debian/ "$(QEMU_DEB_URL)"
	wget -P linux/el6/ "$(QEMU_EL6_URL)"
	wget -P linux/el7/ "$(QEMU_EL7_URL)"
	wget -P linux/fc28/ "$(QEMU_FC28_URL)"
	wget -P linux/fc29/ "$(QEMU_FC29_URL)"

wix:
	wget $(WIX_BINARIES_URL)
	unzip wix311-binaries.zip -d $(WIX_BINARIES_PATH)

edit-project-paths:
	sed "s|@@VIRTIO-WIN-PATH@@|${VIRTIO_WIN_PATH}|" -i installer/constants.wxi
	sed "s|@@OVIRT-GA-PATH@@|${OVIRT_GA_WIN_PATH}|" -i installer/constants.wxi
	sed "s|@@VDAGENT-WIN-PATH@@|${VDAGENT_WIN_PATH}|" -i installer/constants.wxi
	sed "s|@@WIX_BIN_PATH@@|${WIX_BINARIES_WIN_PATH}|" -i installer/build_args/*
	sed "s|@@INSTALLER_PATH@@|${INSTALLER_WIN_PATH}|" -i installer/build_args/*

create-iso: create-installer iso

create-installer:
	pushd installer/
	wine cmd.exe /c "$(WIX_BINARIES_PATH)\candle.exe @build_args/candle_argsx64.txt"
	wine cmd.exe /c "$(WIX_BINARIES_PATH)\light.exe -sval @build_args/light_argsx64.txt"
	popd

iso:
	mkisofs -J \
			-rational-rock \
			-full-iso9660-filenames \
			-verbose \
			-V "$(ISO_LABEL)" \
			-preparer "$(ISO_P_TEXT)" \
			-publisher "$(ISO_P_TEXT)" \
			-o "$(ISO_IMAGE)" \
			-graft-points \
			"/linux/"="/linux/ \
			"/Drivers/"="/$(VIRTIO_WIN_DRIVERS_PATH)/" \
			./manifest.txt
			./ovirt-wgtx64.msi

install: iso
	mkdir -p "$(DESTDIR)$(INSTALL_DATA_DIR)"
	cp "$(ISO_IMAGE)" "$(DESTDIR)$(INSTALL_DATA_DIR)"
	ln -s "$(ISO_IMAGE)" "$(DESTDIR)$(INSTALL_DATA_DIR)/$(ISO_GENERIC)"

# clean:
# 	rm -rf *.exe bin drivers "$(TEMP_DIR)" "$(ISO_IMAGE)" $(GENERATED)

# .SUFFIXES:
# .SUFFIXES: .in

# .in:
# 	sed \
# 	-e "s|@VERSION@|$(VERSION)|g" \
# 	-e "s|@RELEASE_SUFFIX@|$(RELEASE_SUFFIX)|g" \
# 	$< > $@

dist:
	tar -cvf "$(ARCHIVE)" --owner=root --group=root --transform 's,^,$(NAME)-$(VERSION)/,S' ./*

.PHONY : all make-dirs init-files virtio-win ovirt-guest-agent \
		vdagent qemu-ga wix edit-project-paths create-iso \
		create-installer iso install dist