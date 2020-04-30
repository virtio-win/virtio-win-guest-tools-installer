SHELL=/bin/bash

# Parameters that we must pass to make
VERSION=$(VERSION)
ARCH=$(ARCH)
VIRTIO_WIN_DRIVERS_PATH=$(VIRTIO_WIN_DRIVERS_PATH)
SPICE_VDAGENT_PATH=$(SPICE_VDAGENT_PATH)
SPICE_DRIVER_PATH=$(SPICE_DRIVER_PATH)
QEMU_GA_86_MSI_PATH=$(QEMU_GA_86_MSI_PATH)
QEMU_GA_64_MSI_PATH=$(QEMU_GA_64_MSI_PATH)

NAME=virtio-win-guest-tools-installer
ARCHIVE=$(NAME)-$(VERSION).tar.gz
# Location of installed RPMS that we package:
# ovirt-guest-agent-windows.rpm
# wix311-binaries
OVIRTGA_PATH=/usr/share/ovirt-guest-agent-windows
VDA32BIN=/usr/i686-w64-mingw32/sys-root/mingw/bin/
VDA64BIN=/usr/x86_64-w64-mingw32/sys-root/mingw/bin/
WIX_BINARIES_FILES=/usr/share/wix-toolset-binaries

# Project Paths #
WIX_BINARIES_LINK=$(CURDIR)/wix311-binaries
SSO_PATH=$(CURDIR)/3rdParty/SSO
OVIRTGA_LINK=$(CURDIR)/ovirt-guest-agent

# Windows Paths 
VIRTIO_WIN_PATH=$(shell winepath -w $(VIRTIO_WIN_DRIVERS_PATH)|sed 's|\\|\\\\\\\\|g')
OVIRT_GA_WIN_PATH=$(shell winepath -w $(OVIRTGA_LINK)|sed 's|\\|\\\\\\\\|g')
SSO_WIN_PATH=$(shell winepath -w $(SSO_PATH)|sed 's|\\|\\\\\\\\|g')
WIX_BINARIES_WIN_PATH=$(shell winepath -w $(WIX_BINARIES_LINK)|sed 's|\\|\\\\\\\\|g')
INSTALLER_WIN_PATH=$(shell winepath -w $(CURDIR)/virtio-win-drivers-installer|sed 's|\\|\\\\\\\\|g')

GENERATED = \
	ovirt-agent-installer/constants.wxi \
	virtio-win-installers-bundler/Bundle.wxs \
	virtio-win-drivers-installer/constants.wxi \
	virtio-win-drivers-installer/build_args/candle_argsx64.txt \
	virtio-win-drivers-installer/build_args/candle_argsx86.txt \
	virtio-win-drivers-installer/build_args/light_argsx64.txt \
	virtio-win-drivers-installer/build_args/light_argsx86.txt \
	$(NULL)


all: init-files $(GENERATED) ovirt-guest-agent create-installer


init-files: wix


ovirt-guest-agent: $(OVIRTGA_PATH)
	ln -s "$(OVIRTGA_PATH)" "$(OVIRTGA_LINK)"
	pushd ovirt-agent-installer/ ;\
	wine cmd.exe /c "$(WIX_BINARIES_LINK)/candle.exe *.wxs -nologo -v -arch $(ARCH) -ext $(WIX_BINARIES_WIN_PATH)/WixUtilExtension.dll";\
	wine cmd.exe /c "$(WIX_BINARIES_LINK)/light.exe -sval *.wixobj -nologo -out ovirt-guest-agent-$(ARCH).msi -ext $(WIX_BINARIES_WIN_PATH)/WixUtilExtension.dll";\
	popd


wix: $(WIX_BINARIES_FILES)
	ln -s "$(WIX_BINARIES_FILES)" $(WIX_BINARIES_LINK)


create-installer: $(GENERATED) wix ovirt-guest-agent
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
	rm -f $(OVIRTGA_LINK)
	rm -f $(GENERATED)


.SUFFIXES:
.SUFFIXES: .in 


.in:
	@echo $<
	sed \
	-e "s|@@VIRTIO-WIN-PATH@@|${VIRTIO_WIN_PATH}|g" \
	-e "s|@@OVIRT-GA-PATH@@|${OVIRT_GA_WIN_PATH}|g" \
	-e "s|@@SSO-WIN-PATH@@|${SSO_WIN_PATH}|g" \
	-e "s|@@WIX_BIN_PATH@@|${WIX_BINARIES_WIN_PATH}\\\\|g" \
	-e "s|@@INSTALLER_PATH@@|${INSTALLER_WIN_PATH}|g" \
	-e "s|@@SPICE_VDAGENT_PATH@@|${SPICE_VDAGENT_PATH}|g" \
	-e "s|@@SPICE_DRIVER_PATH@@|${SPICE_DRIVER_PATH}|g" \
	-e "s|@@QEMU_GA_64_MSI_PATH@@|${QEMU_GA_64_MSI_PATH}|g" \
	-e "s|@@QEMU_GA_86_MSI_PATH@@|${QEMU_GA_86_MSI_PATH}|g" \
	-e "s|@@VERSION@@|${VERSION}|g" \
	$< > $@


dist:
	tar -cvf "$(ARCHIVE)" --owner=root --group=root ./*


.PHONY : all init-files ovirt-guest-agent wix create-installer dist test bundle
