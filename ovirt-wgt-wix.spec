%global rel 0

Name:		ovirt-guest-tools
Version:	4.4
Release:	%{rel}%{?release_suffix}%{?dist}
Summary:	oVirt Windows Guest Tools
License:	GPLv2 and GPLv2+ and ASL 2.0 and Zlib and MIT and Python and Platform SDK Redistributable EULA and Microsoft DDK Redistributable EULA
Source:     ovirt-wgt-%{version}.tar.gz
URL:		https://github.com/oVirt/ovirt-wgt-wix
BuildArch:	noarch

BuildRequires:	genisoimage
BuildRequires:	p7zip
BuildRequires:	p7zip-plugins
BuildRequires:	hardlink
BuildRequires:	mingw32-spice-vdagent >= 0.9.0
BuildRequires:	mingw64-spice-vdagent >= 0.9.0
BuildRequires:	ovirt-guest-agent-windows
BuildRequires:	vcredist-x86
BuildRequires:	virtio-win >= 0.1.171
BuildRequires:	wine
BuildRequires:	wget
Obsoletes:	ovirt-guest-tools
Obsoletes:	ovirt-guest-tools-iso

%description
Windows Guest tools ISO for oVirt Virtualization Manager.

%global make_common_opts \\\
	PREFIX=%{_prefix} \\\
    NAME=ovirt-wgt \\\
    VERSION=%{version} \\\
	DISPLAYED_VERSION='%{version}-%{release}' \\\
	ISO_LABEL='oVirt-WGT-%{version}-%{rel}'

%prep
#TODO: Change that to be a macro from the outside
%setup -q -n ovirt-wgt-%{version}

%build
make %{make_common_opts}

%install
make %{make_common_opts} install DESTDIR="%{buildroot}"

%files
%{_datadir}/%{name}

%changelog
* Thu Jul 18 2019 - Gal Zaidman <gzaidman@redhat.com> - 4.4-0
	- Started the project

