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

### Building for ARM64:

The installer can also be built for ARM64 Windows (`virtio-win-gt-arm64.msi`) with the same WiX 6 project, by selecting the `arm64` platform:

```
dotnet build virtio-win-drivers-installer\virtio-win-drivers-installer.wixproj -c Release -p:Platform=arm64 -p:VirtioWinDriversPath=C:\path\to\drivers
```

- The driver directory uses the `ARM64` architecture folders as they are on the virtio-win ISO, e.g. `Balloon\w11\ARM64`.
- The MSI needs `Libraries\arm64\DrvInstCA.dll`. It links against the WiX 3.14.1 SDK (`sdk\vs2017\lib\arm64`; the 3.11 SDK has no arm64 libraries) and needs the Visual Studio ARM64 C++ tools (`Microsoft.VisualStudio.Component.VC.Tools.ARM64`). The installer itself is built with WiX 6, as for x64 and x86. To rebuild the DLL, from a Developer Command Prompt at the root of the repository, with the [WiX 3.14.1 binaries](https://github.com/wixtoolset/wix3/releases/tag/wix3141rtm) extracted to `C:\wix314`:

  ```
  msbuild src\DrvInst\DrvInstCA\DrvInstCA.vcxproj /p:Configuration=Release /p:Platform=ARM64 ^
    /p:WIX=C:\wix314\ /p:WixPlatformToolset=VS2017 /p:WindowsTargetPlatformVersion=<SDK version> ^
    /p:PlatformTarget=arm64 /p:SolutionDir=%CD%\src\DrvInst\
  copy /y src\DrvInst\Install\Release\arm64\DrvInstCA.dll Libraries\arm64\
  ```

  `<SDK version>` is an installed Windows SDK, e.g. `10.0.26100.0` (a folder name under `C:\Program Files (x86)\Windows Kits\10\Include`). The project's default `10.0.10240.0` has no ARM64 libraries; the committed DLL was built with the newest SDK on the build machine.
- Packaged for arm64: Balloon, NetKVM, vioinput, viorng, vioscsi, vioserial, viostor, viogpudo and viomem.
- Not packaged for arm64: fwcfg, qemupciserial and viofs (no arm64 build of these exists) and pvpanic (it has arm64 configurations in the driver source, but no arm64 build is published in the virtio-win driver tree yet; the guards in `features.wxs` and `directories.wxs` are the only thing to remove once one is). Spice has no arm64 build either, so nothing from it is installed on arm64.
- The bundle picks the arm64 MSIs on arm64 Windows using the built-in `NativeMachine` Burn variable, which reports the real machine even when the bundle itself runs as an emulated x64 process. The arm64 packages are included when `Libraries\arm64\DrvInstCA.dll` exists (override with `/p:IncludeArm64=true|false`), and the arm64 qemu-guest-agent MSI is added when `/p:QemuGaArm64MsiPath=...` is set. The bundle project builds the arm64 driver MSI along with x64 and x86. A bundle built without the arm64 packages refuses to start on arm64 Windows, rather than finishing without installing anything.
- The package selection only applies once the bundle is running. The bundle is an x64 executable by default: Windows 11 on Arm runs it under x64 emulation, but Windows 10 on Arm only emulates x86, so it cannot start an x64 bundle at all. For Windows 10 on Arm, build a native arm64 bundle (with the same MSI path properties as for x64):

  ```
  dotnet build virtio-win-installers-bundler\virtio-win-installers-bundler.wixproj -c Release -p:Platform=arm64
  ```

  The output is `virtio-win-installers-bundler\bin\arm64\Release\virtio-win-guest-tools.exe`; it runs only on arm64 Windows.
- `test/test.py` also checks the arm64 MSI if it is present (`../tmp/virtio-win-drivers-installer/virtio-win-gt-arm64.msi`); the x64 and x86 MSIs are still required.

## Contributions:

Contributions are more than welcome, please fork the repository and create a PR.
