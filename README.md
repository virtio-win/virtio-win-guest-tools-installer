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
|-Balloon['2k16', '2k19', '2k22', 'w10', 'w11']
|-NetKVM['2k16', '2k19', '2k22', 'w10', 'w11']
|-pvpanic['2k16', '2k19', '2k22', 'w10', 'w11']
|-fwcfg['2k16', '2k19', '2k22', 'w10', 'w11]
|-qemupciserial['2k16', '2k19', '2k22', 'w10', 'w11']
|-vioinput['2k16', '2k19', '2k22', 'w10', 'w11']
|-viorng['2k16', '2k19', '2k22', 'w10', 'w11']
|-vioscsi['2k16', '2k19', '2k22', 'w10', 'w11']
|-vioserial['2k16', '2k19', '2k22', 'w10', 'w11']
|-viostor['2k16', '2k19', '2k22', 'w10', 'w11']
|-viofs['2k16', '2k19', '2k22', 'w10', 'w11']
|-viogpu['2k16', '2k19', '2k22', 'w10', 'w11']
|-viomem['2k16', '2k19', '2k22', 'w10', 'w11']
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

2. Run the build script ./automation/build-artifacts.sh VIRTIO_WIN_DRIVERS_PATH QEMU_GA_64_MSI_PATH QEMU_GA_86_MSI_PATH VERSION

   - VIRTIO_WIN_DRIVERS_PATH - dir containing the virtio-win drivers
   - QEMU_GA_64_MSI_PATH - path to the qemu ga x64 MSI
   - QEMU_GA_86_MSI_PATH - path to the qemu ga x86 MSI
   - VERSION - version on the installer

3. The build artifacts which are 2 msis (x64, x86) and one .exe file will be on the exported artifacts directory, which was created during the build process.

## Contributions:

Contributions are more than welcome, please fork the repository and create a PR.
