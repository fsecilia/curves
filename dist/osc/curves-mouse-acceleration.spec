Name:           curves-mouse-acceleration
Version:        0.0.0
Release:        0
Summary:        Curves mouse acceleration driver
License:        MIT
URL:            https://github.com/fsecilia/curves
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  cmake
BuildRequires:  dkms
BuildRequires:  gcc
BuildRequires:  gcc-c++
BuildRequires:  systemd
BuildRequires:  systemd-rpm-macros

Requires:       dkms
Requires:       kernel-devel
Requires:       gcc
Requires:       gcc-c++
Requires:       make
Requires:       systemd

Requires(post): dkms
Requires(preun): dkms

%description
Kernel module for custom mouse acceleration curves, managed via DKMS.

%prep
%setup -q

%build
cmake -B build -DCMAKE_INSTALL_PREFIX=%{_prefix} -DCMAKE_BUILD_TYPE=Release
cmake --build build

%install
DESTDIR=%{buildroot} cmake --install build

%post
dkms add -m %{name} -v %{version} || :
dkms build -m %{name} -v %{version} || :
dkms install -m %{name} -v %{version} || :
%systemd_post curves-mouse-acceleration-restore.service

%preun
%systemd_preun curves-mouse-acceleration-restore.service
dkms remove -m %{name} -v %{version} --all || :

%files
%{_bindir}/curves-mouse-acceleration-config
%dir %{_modulesloaddir}
%{_modulesloaddir}/curves-mouse-acceleration.conf
%dir %{_prefix}/src/%{name}-%{version}
%{_prefix}/src/%{name}-%{version}/*
%dir %{_udevrulesdir}
%{_udevrulesdir}/99-curves-mouse-acceleration.rules
%{_unitdir}/curves-mouse-acceleration-restore.service
