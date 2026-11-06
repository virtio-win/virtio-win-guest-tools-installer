# Virtio-win guest tools installer

## Introduction:

Virtio-win guest tools installer is a msi (Microsoft installer) created with [Wix Toolset](https://wixtoolset.org/releases/) which installs:

<u>Virtio-win drivers</u> - KVM/QEMUY windows guest drivers for both paravirtual and emulated hardware, more on [Github Repo](https://github.com/virtio-win/kvm-guest-drivers-windows).

<u>Spice guest agent and driver</u> - Spice is an open remote computing solution, providing client access to remote machine display and devices, more info on the [Offical website](https://www.spice-space.org).


The installer is distributed as part of virtio-win package on [Fedora](https://fedorapeople.org/groups/virt/virtio-win/repo/rpms/).

## Building from sources:

The installer can be built in two ways, using [oVirt CI mock runner tool](https://ovirt-infra-docs.readthedocs.io/en/latest/CI/Using_mock_runner/index.html) and a manual build using make.

### Prerequsits:

Before buidling the installer you must have the virtio-win drivers localy on the format which they are distributed on the iso:
```
|-Balloon['2k16', '2k19', '2k22', '2k25', 'w10', 'w11']
|-NetKVM['2k16', '2k19', '2k22', '2k25', 'w10', 'w11']
|-pvpanic['2k16', '2k19', '2k22', '2k25', 'w10', 'w11']
|-fwcfg['2k16', '2k19', '2k22', '2k25', 'w10', 'w11]
|-qemupciserial['2k16', '2k19', '2k22', '2k25', 'w10', 'w11']
|-vioinput['2k16', '2k19', '2k22', '2k25', 'w10', 'w11']
|-viorng['2k16', '2k19', '2k22', '2k25', 'w10', 'w11']
|-vioscsi['2k16', '2k19', '2k22', '2k25', 'w10', 'w11']
|-vioserial['2k16', '2k19', '2k22', '2k25', 'w10', 'w11']
|-viostor['2k16', '2k19', '2k22', '2k25', 'w10', 'w11']
|-viofs['2k16', '2k19', '2k22', '2k25', 'w10', 'w11']
|-viogpu['2k16', '2k19', '2k22', '2k25', 'w10', 'w11']
|-viomem['2k16', '2k19', '2k22', '2k25', 'w10', 'w11']
```
You can get it by:

- Extracting the virtio-win iso content (availabe on virtio-win rpm package).

- Run make-driver-dir.py from [virtio-win-pkg-scripts github repo](https://github.com/crobinso/virtio-win-pkg-scripts).

### Building with Mock Runner:

1. Follow the instructions on [oVirt CI mock runner docs](https://ovirt-infra-docs.readthedocs.io/en/latest/CI/Using_mock_runner/index.html) for setting up and installing mock runner.

2. Create a soft link to the drivers directory:
   
   ```bash
   ln -sf ${VIRTIO_WIN_DRIVERS_PATH} $(pwd)/vwi 
   ```

3. Once mock_runner is setup, from the project root directory run:
   
   ```bash
   path/to/jenkins/mock_configs/mock_runner.sh -b fc30
   ```

4. The build artifacts which are 2 msis (x64, x86) and one .exe file will be on the exported artifacts directory, which was created during the build process.

### Building with make:

1. Make sure all the packages on automation/build-artifacts.packages are installed on the machine, relevant repos can be found on automation/build-artifacts.repos.

2. Run the build script ./automation/build-artifacts.sh VIRTIO_WIN_DRIVERS_PATH SPICE_VDAGENT_64_MSI_PATH SPICE_VDAGENT_86_MSI_PATH SPICE_DRIVER_64_MSI_PATH SPICE_DRIVER_86_MSI_PATH QEMU_GA_64_MSI_PATH QEMU_GA_86_MSI_PATH VERSION

   - VIRTIO_WIN_DRIVERS_PATH - dir containing the virtio-win drivers
   - SPICE_VDAGENT_64_MSI_PATH - path to the spice ga x64 MSI
   - SPICE_VDAGENT_86_MSI_PATH - path to the spice ga x86 MSI
   - SPICE_DRIVER_64_MSI_PATH - path to the spice driver x64 MSI
   - SPICE_DRIVER_86_MSI_PATH - path to the spice driver x86 MSI
   - QEMU_GA_64_MSI_PATH - path to the qemu ga x64 MSI
   - QEMU_GA_86_MSI_PATH - path to the qemu ga x86 MSI
   - VERSION - version on the installer

3. The build artifacts which are 2 msis (x64, x86) and one .exe file will be on the exported artifacts directory, which was created during the build process.

### Building with Visual Studio 2022:

#### Prerequisites:

1. [Visual Studio 2022](https://visualstudio.microsoft.com/)
2. [WiX Toolset v3.11](https://github.com/wixtoolset/wix3/releases)
3. [WiX Toolset Visual Studio 2022 Extension](https://marketplace.visualstudio.com/items?itemName=WixToolset.WixToolsetVisualStudio2022Extension)
4. Virtio-win drivers (extracted from the virtio-win ISO)

#### Building from Visual Studio:

1. Open `VirtioWinInstaller.sln` in Visual Studio 2022.

2. Edit the default driver path in `virtio-win-drivers-installer\virtio-win-drivers-installer.wixproj` - set `VirtioWinDriversPath` to the location of your extracted virtio-win ISO content (see [Prerequisites](#prerequsits) for the expected driver directory structure).

3. Select the desired platform (x64 or x86) from the toolbar.

4. Build the solution (Ctrl+Shift+B).

5. The MSI output will be in `virtio-win-drivers-installer\bin\<platform>\<configuration>\`.

6. The Bundle (.exe) requires additional MSI paths for Spice and QEMU Guest Agent. Edit the default paths in `virtio-win-installers-bundler\virtio-win-installers-bundler.wixproj` or pass them via command line using `msbuild /p:SpiceDriver64MsiPath=... /p:QemuGa64MsiPath=...` etc. The bundle project builds both x64 and x86 virtio-win driver MSIs automatically. To use prebuilt driver MSIs instead, set `VirtioWinGtX64MsiPath`, `VirtioWinGtX86MsiPath`, and `/p:BuildBundledDriverMsis=false`.

#### Build outputs:

- MSI installers: `virtio-win-drivers-installer\bin\<platform>\<configuration>\virtio-win-gt-<platform>.msi`
- Bundle (.exe): `virtio-win-installers-bundler\bin\<platform>\<configuration>\virtio-win-guest-tools.exe`

### QGA VSS package choice (draft)

The WiX 6 bundle requires two genuine QGA variants for each architecture. Keep the existing full MSI paths and additionally supply:

- `/p:QemuGaNoVss64MsiPath=C:\msi\qemu-ga-x86_64-no-vss.msi`
- `/p:QemuGaNoVss86MsiPath=C:\msi\qemu-ga-i386-no-vss.msi`

The no-VSS packages must omit the VSS DLL/TLB and provider registration actions, not merely have different filenames. The package builder can use QEMU's existing `qga_vss=disabled` configuration. `ReadQgaPackageMetadata.ps1` reads the four media MSIs through Windows Installer's read-only COM API and generates their ProductCode constants under `obj`; no installed/cached package is edited. The inputs must have distinct ProductCodes and one shared QGA UpgradeCode family; the helper emits both identities so the bundle can also detect older related registrations. The build host needs Windows PowerShell and must permit that local build script under its existing execution policy.

The Install and Modify pages expose **Install QEMU Guest Agent VSS support**. A fresh install defaults to the full package; unchecking selects the Agent-only package, which does not provide Windows filesystem freezing. For noninteractive installation, use `virtio-win-guest-tools.exe /quiet QgaVssEnabled=0`. The supported explicit values are `0` and `1`; `-1` is the internal auto sentinel. Explicit CLI values override saved choices. Otherwise the saved same-bundle choice is retained, or a known installed no-VSS ProductCode supplies the default before falling back to full QGA.

SENS startup configuration is read during detection only. Disabled or unreadable SENS prevents installing/repairing full QGA; no service policy is changed. Interactive users can uncheck VSS before continuing. Quiet/passive invalid selections fail before apply. The native license-acceptance gate remains separate. Removing a known full QGA also requires the prerequisite; no-VSS removal does not.

This is an experimental integration proposal, not a validated migration path. Switching an installed full/no-VSS flavor is deliberately blocked, even at the same version. Both supplied flavors installed together, or an unrecognized existing QGA service/related MSI registration, are refused. A missing service does not make an older MSI registration a fresh install. Use the original installer, with its required dependencies available, to remove that installation first. Exact ProductCode detection identifies the supplied flavors, not a general inventory or migration policy for arbitrary versions; a SENS startup value is neither a VSS health check nor an immediate pre-plan recheck.

The Windows SDK-6 bundle built with the real ten payloads with zero warnings/errors. An isolated QGA-only bundle using the same selection/theme installed the correct no-VSS MSI on x64/x86 with SENS disabled, passed guest-ping/info and uninstalled; quiet full-VSS and invalid-value requests were rejected before Apply. A real older registered QGA with its service deleted was also rejected before Apply. This is not a full companion-driver bundle end-to-end result. Interactive EULA/keyboard/accessibility behavior remains unverified: the SSH-session UI probe could not obtain an accessible window. Maintenance, rollback and related-bundle upgrades still require validation. The legacy Makefile/automation recipes still target WiX 3 and are not adapted by this WiX 6 proposal; the distribution pipeline must supply both extra artifacts and adapt its entry point before integration.

## Contributions:

Contributions are more than welcome, please fork the repository and create a PR.
