# **Ovirt-WGT-wix**
Ovirt widows guest tools project creates an iso which contins
a Wix toolset based Microsoft Installer for installing:
* [virtio-win drivers](https://github.com/virtio-win/kvm-guest-drivers-windows) 
* [spice guest agent](https://www.spice-space.org/) 
* [qemu guest agent](https://github.com/qemu/qemu) 
* [ovirt guest agent](https://github.com/oVirt/ovirt-guest-agent)

## **Install**
---
1. Attach the iso to the windows VM as a CD.
2. Run the ovirt-wgtx{arch}.msi.
3. Run the and qemu-ga.msi.
4. Reboot.
5. Check in device manager that devices are recognized.
6. Check in task manager that the services are running.

## **Build**
---
**Using mock runner**\
Probably the easyest way to build WGT iso is by using mock runner
follow the docs on:
https://ovirt-infra-docs.readthedocs.io/en/latest/CI/Using_mock_runner/index.html
after you have mock runner setup, from the project root dir run:
>%{path_to_mock_runner_dir}/mock_runner -b %{required_os}

**Using make**

make sure that all the packages listed on automation/build-artifacts.packages
are installed in your build env.
from the project root run:
> make ARCH={x64/x86}




