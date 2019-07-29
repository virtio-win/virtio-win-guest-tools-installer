SHELL=/bin/bash

NAME=ovirt-wgt-wix
VERSION=4.4
DISPLAYED_VERSION=$(DISPLAYED_VERSION)
ARCHIVE=$(NAME)-$(VERSION).tar.gz
RELEASE_SUFFIX=_master
ARCH=x64
# Project Paths #
VIRTIO_WIN_DRIVERS_PATH=$(CURDIR)/virtio-win
VDAGENT_PATH=$(CURDIR)/vdagent
WIX_BINARIES_PATH=$(CURDIR)/wix311-binaries
OVIRT_GA_PATH=$(CURDIR)/ovirt-guest-agent
ISO_PATH=$(CURDIR)/ISO
# Windows Paths #
VIRTIO_WIN_PATH=$(shell winepath -w $(VIRTIO_WIN_DRIVERS_PATH)|sed 's|\\|\\\\\\\\|g')
OVIRT_GA_WIN_PATH=$(shell winepath -w $(OVIRT_GA_PATH)|sed 's|\\|\\\\\\\\|g')
VDAGENT_WIN_PATH=$(shell winepath -w $(VDAGENT_PATH)|sed 's|\\|\\\\\\\\|g')
WIX_BINARIES_WIN_PATH=$(shell winepath -w $(WIX_BINARIES_PATH)|sed 's|\\|\\\\\\\\|g')
INSTALLER_WIN_PATH=$(shell winepath -w $(CURDIR)/installer|sed 's|\\|\\\\\\\\|g')
#Paths come from the location of virtio-win ovirt-guest-agent rpm files
VIRTIO_WIN_ISO=/usr/share/virtio-win/virtio-win.iso
OVIRTGA_PATH=/usr/share/ovirt-guest-agent-windows
VDA32BIN=/usr/i686-w64-mingw32/sys-root/mingw/bin/
VDA64BIN=/usr/x86_64-w64-mingw32/sys-root/mingw/bin/
WIX_BINARIES_URL="https://github.com/wixtoolset/wix3/releases/download/wix3111rtm/wix311-binaries.zip"
# install targets
PREFIX=/usr/local
DATAROOT_DIR=$(PREFIX)/share
INSTALL_DATA_DIR=$(DATAROOT_DIR)/$(NAME)
ISO_IMAGE=oVirt-toolsSetup_Wix_$(ARCH)_$(DISPLAYED_VERSION).iso
ISO_GENERIC=ovirt-wgt-wix-setup-$(ARCH).iso
ISO_P_TEXT=oVirt - Open Virtualization Project (www.ovirt.org)
# Up to 16 digits will be displayed in windows, to fit in the iso label
ISO_LABEL=oVirt-WGT-$(ARCH)-$(DISPLAYED_VERSION)
# Available from http://www.microsoft.com/en-us/download/details.aspx?id=5582
# RPM wrapping this available from http://resources.ovirt.org/
VCREDIST=/usr/share/vcredist-x86/vcredist_x86.exe

# GENERATED=$(shell find ./* -name *.in)

GENERATED = \
	installer/constants.wxi \
	installer/build_args/candle_argsx64.txt \
	installer/build_args/candle_argsx86.txt \
	installer/build_args/light_argsx64.txt \
	installer/build_args/light_argsx86.txt \
	$(NULL)

all: make-dirs init-files $(GENERATED) create-iso

make-dirs:
	mkdir -p $(VDAGENT_PATH)
	mkdir -p $(VIRTIO_WIN_DRIVERS_PATH)
	mkdir -p $(WIX_BINARIES_PATH)
	mkdir -p $(ISO_PATH)

init-files: virtio-win ovirt-guest-agent vdagent qemu-ga wix

# extract the iso to get the drivers
virtio-win: make-dirs
	7z -o"$(VIRTIO_WIN_DRIVERS_PATH)" x "$(VIRTIO_WIN_ISO)"
	hardlink -vv "$(VIRTIO_WIN_DRIVERS_PATH)"

# link ovirt-guest-agent to the original folder from the rpm
ovirt-guest-agent:
	ln -s "$(OVIRTGA_PATH)" "$(OVIRT_GA_PATH)"

vdagent: make-dirs
	ln -s "$(VDA32BIN)" $(VDAGENT_PATH)/x86
	ln -s "$(VDA64BIN)" $(VDAGENT_PATH)/x64


# Download the wix binaries
# TODO: package wix as rpm
wix: make-dirs
	wget $(WIX_BINARIES_URL)
	unzip wix311-binaries.zip -d $(WIX_BINARIES_PATH)


create-iso: create-installer clean-installer-dir iso


create-installer: $(GENERATED) wix qemu-ga vdagent ovirt-guest-agent virtio-win
	pushd installer/ ;\
	wine cmd.exe /c "$(WIX_BINARIES_PATH)/candle.exe @build_args/candle_args$(ARCH).txt" ;\
	wine cmd.exe /c "$(WIX_BINARIES_PATH)/light.exe -sval @build_args/light_args$(ARCH).txt" ;\
	popd


clean-installer-dir: create-installer
	rm -rf installer/wixobjx*


iso: create-installer clean-installer-dir
	mkisofs -J \
			-rational-rock \
			-full-iso9660-filenames \
			-verbose \
			-V "$(ISO_LABEL)" \
			-preparer "$(ISO_P_TEXT)" \
			-publisher "$(ISO_P_TEXT)" \
			-o "$(ISO_IMAGE)" \
			-graft-points \
			"linux/"="./linux/" \
			"Drivers/"="$(VIRTIO_WIN_DRIVERS_PATH)/" \
			"Sources/"="./installer/" \
			./manifest.txt \
			./ovirt-wgt$(ARCH).msi


install:
	mkdir -p "$(DESTDIR)$(INSTALL_DATA_DIR)"
	cp "$(ISO_IMAGE)" "$(DESTDIR)$(INSTALL_DATA_DIR)"
	ln -s "$(ISO_IMAGE)" "$(DESTDIR)$(INSTALL_DATA_DIR)/$(ISO_GENERIC)"


clean:
	rm -rf exported-artifacts tmp 
	rm -rf .wine .local .config .cache
	rm -rf *.tar.gz
	rm -rf $(VDAGENT_PATH)
	rm -rf $(VIRTIO_WIN_DRIVERS_PATH)
	rm -rf $(WIX_BINARIES_PATH)
	rm -rf $(ISO_PATH)
	rm -f $(OVIRT_GA_PATH)
	rm -f $(GENERATED)


.SUFFIXES:
.SUFFIXES: .in 


.in:
	@echo $<
	sed \
	-e "s|@@VIRTIO-WIN-PATH@@|${VIRTIO_WIN_PATH}|g" \
	-e "s|@@OVIRT-GA-PATH@@|${OVIRT_GA_WIN_PATH}|g" \
	-e "s|@@VDAGENT-WIN-PATH@@|${VDAGENT_WIN_PATH}|g" \
	-e "s|@@WIX_BIN_PATH@@|${WIX_BINARIES_WIN_PATH}|g" \
	-e "s|@@INSTALLER_PATH@@|${INSTALLER_WIN_PATH}|g" \
	$< > $@


dist:
	tar -cvf "$(ARCHIVE)" --owner=root --group=root --transform 's,^,$(NAME)-$(VERSION)/,S' ./*

.PHONY : all make-dirs init-files virtio-win ovirt-guest-agent \
		vdagent qemu-ga wix create-iso create-installer iso install dist