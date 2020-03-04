# Virtio-win guest tools installer

## Introduction

Virtio-win guest tools installer is a MSI (Microsoft installer) created with [Wix Toolset](https://wixtoolset.org/releases/) which installs:

 * virtio-win drivers

   KVM/QEMU Windows guest drivers for both para-virtual and emulated hardware, more on [GitHub Repo](https://github.com/virtio-win/kvm-guest-drivers-windows).

 * SPICE guest agent and driver

   SPICE is an open remote computing solution, providing client access to remote machine display and devices, more info on the [offical website](https://www.spice-space.org/).

 * oVirt guest agent (optional)

   Enables the oVirt engine to receive internal machine events and information, SSO support, and enables the engine to execute specific commands on the Windows virtual machine, more on [oVirt docs](https://www.ovirt.org/develop/internal/guest-agent/guest-agent.html) and [GitHub Repo](https://github.com/oVirt/ovirt-guest-agent).

The installer is distributed as part of virtio-win package on [Fedora](https://fedorapeople.org/groups/virt/virtio-win/repo/rpms/).

## Build from source

The installer can be built in two ways, using [oVirt CI mock runner tool](https://ovirt-infra-docs.readthedocs.io/en/latest/CI/Using_mock_runner/index.html) or make.

### Prerequisites

Before building the installer you need pre-built virtio-win drivers, conforming to the layout as the virtio-win ISO image:
```
|-Balloon['2k12', '2k12R2', '2k16', '2k19', '2k3', '2k8', '2k8R2', 'w10', 'w7', 'w8', 'w8.1', 'xp']
|-NetKVM['2k12', '2k12R2', '2k16', '2k19', '2k3', '2k8', '2k8R2', 'w10', 'w7', 'w8', 'w8.1', 'xp']
|-pvpanic['2k12', '2k12R2', '2k16', '2k19', '2k8', '2k8R2', 'w10', 'w7', 'w8', 'w8.1']
|-qemufwcfg['2k16', '2k19', 'w10']
|-qemupciserial['2k12', '2k12R2', '2k16', '2k19', '2k8', '2k8R2', 'w10', 'w7', 'w8', 'w8.1']
|-vioinput['2k12', '2k12R2', '2k16', '2k19', '2k8R2', 'w10', 'w7', 'w8', 'w8.1']
|-viorng['2k12', '2k12R2', '2k16', '2k19', '2k8', '2k8R2', 'w10', 'w7', 'w8', 'w8.1']
|-vioscsi['2k12', '2k12R2', '2k16', '2k19', '2k8', '2k8R2', 'w10', 'w7', 'w8', 'w8.1']
|-vioserial['2k12', '2k12R2', '2k16', '2k19', '2k3', '2k8', '2k8R2', 'w10', 'w7', 'w8', 'w8.1', 'xp']
|-viostor['2k12', '2k12R2', '2k16', '2k19', '2k3', '2k8', '2k8R2', 'w10', 'w7', 'w8', 'w8.1', 'xp']
```

You can get it by either extract the virtio-win ISO content (available in virtio-win rpm package) or run `make-driver-dir.py` from [virtio-win-pkg-scripts GitHub repo](https://github.com/crobinso/virtio-win-pkg-scripts).

### Build with Mock Runner:

1. Follow the instructions on [oVirt CI mock runner docs](https://ovirt-infra-docs.readthedocs.io/en/latest/CI/Using_mock_runner/index.html) for setting up and installing mock runner.

2. Create a soft link to the drivers directory:
   
   ```bash
   ln -sf ${VIRTIO_WIN_DRIVERS_PATH} $(pwd)/vwi 
   ```

3. Once mock_runner is setup, from the project root directory run:
   
   ```bash
   path/to/jenkins/mock_configs/mock_runner.sh -b fc30
   ```

4. The build artifacts which are 2 MSIs (x64, x86) will be in the exported artifacts directory, which was created during the build process.

### Build with make

1. Make sure all the packages on `automation/build-artifacts.packages` are installed on the machine, relevant repos can be found on `automation/build-artifacts.repos`.

2. Run the build script `automation/build-artifacts.sh VIRTIO_WIN_DRIVERS_PATH INSTALLER_VERSION`

3. The build artifacts which are 2 MSIs (x64, x86) will be in the exported artifacts directory, which was created during the build process.

## Contribution

Contributions are more than welcome, please fork the repository and create a PR.
