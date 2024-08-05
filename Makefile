SHELL=/bin/bash

# Parameters that we must pass to make
VERSION=$(VERSION)
ARCH=$(ARCH)
VIRTIO_WIN_DRIVERS_PATH=$(VIRTIO_WIN_DRIVERS_PATH)
SPICE_VDAGENT_64_MSI_PATH=$(SPICE_VDAGENT_64_MSI_PATH)
SPICE_VDAGENT_86_MSI_PATH=$(SPICE_VDAGENT_86_MSI_PATH)
SPICE_DRIVER_64_MSI_PATH=$(SPICE_DRIVER_64_MSI_PATH)
SPICE_DRIVER_86_MSI_PATH=$(SPICE_DRIVER_86_MSI_PATH)
QEMU_GA_86_MSI_PATH=$(QEMU_GA_86_MSI_PATH)
QEMU_GA_64_MSI_PATH=$(QEMU_GA_64_MSI_PATH)

NAME=virtio-win-guest-tools-installer
ARCHIVE=$(NAME)-$(VERSION).tar.gz
# Location of wix311-binaries RPM
WIX_BINARIES_FILES=/usr/share/wix-toolset-binaries

# Project Paths #
WIX_BINARIES_LINK=$(CURDIR)/wix311-binaries
WIX_LIBRARIES_LINK=$(CURDIR)/Libraries

# Windows Paths 
VIRTIO_WIN_PATH=$(shell winepath -w $(VIRTIO_WIN_DRIVERS_PATH)|sed 's|\\|\\\\\\\\|g')
WIX_BINARIES_WIN_PATH=$(shell winepath -w $(WIX_BINARIES_LINK)|sed 's|\\|\\\\\\\\|g')
WIX_LIBRARIES_WIN_PATH=$(shell winepath -w $(WIX_LIBRARIES_LINK)|sed 's|\\|\\\\\\\\|g')
INSTALLER_WIN_PATH=$(shell winepath -w $(CURDIR)/virtio-win-drivers-installer|sed 's|\\|\\\\\\\\|g')

GENERATED = \
	virtio-win-installers-bundler/Bundle.wxs \
	virtio-win-drivers-installer/constants.wxi \
	virtio-win-drivers-installer/build_args/candle_argsx64.txt \
	virtio-win-drivers-installer/build_args/candle_argsx86.txt \
	virtio-win-drivers-installer/build_args/light_argsx64.txt \
	virtio-win-drivers-installer/build_args/light_argsx86.txt \
	$(NULL)


all: init-files $(GENERATED) create-installer


init-files: wix


wix: $(WIX_BINARIES_FILES)
	ln -s "$(WIX_BINARIES_FILES)" $(WIX_BINARIES_LINK)


create-installer: $(GENERATED) wix
	pushd virtio-win-drivers-installer/ ;\
	wine cmd.exe /c "$(WIX_BINARIES_LINK)/candle.exe @build_args/candle_args$(ARCH).txt" ;\
	wine cmd.exe /c "$(WIX_BINARIES_LINK)/light.exe -sval @build_args/light_args$(ARCH).txt" ;\
	rm -rf wixobjx*; \
	popd


test:
	python3 -m pytest test/test.py

bundle:
	pushd virtio-win-installers-bundler/ ;\
	wine cmd.exe /c "$(WIX_BINARIES_LINK)/candle.exe *.wxs -ext $(WIX_BINARIES_WIN_PATH)/WixBalExtension.dll" ;\
	wine cmd.exe /c "$(WIX_BINARIES_LINK)/light.exe *.wixobj -o ../virtio-win-guest-tools.exe -ext $(WIX_BINARIES_WIN_PATH)/WixBalExtension.dll" ;\
	popd

clean:
	rm -rf exported-artifacts tmp
	rm -rf .wine .local .config .cache
	rm -f wix311-binaries.zip
	rm -rf *.tar.gz
	rm -rf $(WIX_BINARIES_LINK)
	rm -f $(GENERATED)


.SUFFIXES:
.SUFFIXES: .in


.in:
	@echo $<
	sed \
	-e "s|@@VIRTIO-WIN-PATH@@|${VIRTIO_WIN_PATH}|g" \
	-e "s|@@WIX_BIN_PATH@@|${WIX_BINARIES_WIN_PATH}\\\\|g" \
	-e "s|@@WIX_LIB_PATH@@|${WIX_LIBRARIES_WIN_PATH}\\\\|g" \
	-e "s|@@INSTALLER_PATH@@|${INSTALLER_WIN_PATH}|g" \
	-e "s|@@SPICE_VDAGENT_64_MSI_PATH@@|${SPICE_VDAGENT_64_MSI_PATH}|g" \
	-e "s|@@SPICE_VDAGENT_86_MSI_PATH@@|${SPICE_VDAGENT_86_MSI_PATH}|g" \
	-e "s|@@SPICE_DRIVER_64_MSI_PATH@@|${SPICE_DRIVER_64_MSI_PATH}|g" \
	-e "s|@@SPICE_DRIVER_86_MSI_PATH@@|${SPICE_DRIVER_86_MSI_PATH}|g" \
	-e "s|@@QEMU_GA_64_MSI_PATH@@|${QEMU_GA_64_MSI_PATH}|g" \
	-e "s|@@QEMU_GA_86_MSI_PATH@@|${QEMU_GA_86_MSI_PATH}|g" \
	-e "s|@@VERSION@@|${VERSION}|g" \
	$< > $@


dist:
	tar -cvf "$(ARCHIVE)" --owner=root --group=root ./*


.PHONY : all init-files wix create-installer dist test bundle
