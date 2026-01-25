Name:           curves-mouse-acceleration
Version:        0.0.0
Release:        0
Summary:        Curves mouse acceleration driver
License:        MIT AND (GPL-2.0-or-later OR MIT)
URL:            https://github.com/fsecilia/curves
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  cmake
BuildRequires:  dkms
BuildRequires:  gcc
BuildRequires:  gcc-c++
BuildRequires:  pkgconfig(Qt6Core)
BuildRequires:  pkgconfig(Qt6Gui)
BuildRequires:  pkgconfig(Qt6Widgets)
BuildRequires:  systemd
BuildRequires:  systemd-rpm-macros

Requires:       dkms
Requires:       kernel-devel
Requires:       gcc
Requires:       gcc-c++
Requires:       make
Requires:       systemd
Requires:       udev

%{?systemd_requires}

%description
Kernel module for custom mouse acceleration curves, managed via DKMS.

%prep
%setup -q

%build
cmake -B build \
    -DCMAKE_INSTALL_PREFIX=%{_prefix} \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -Dmodprobe_path=%{_sbindir}/modprobe \
    -Dudevadm_path=%{_bindir}/udevadm

cmake --build build

%install
DESTDIR=%{buildroot} cmake --install build

%post
dkms add -m %{name} -v %{version} --rpm_safe_upgrade || :
dkms build -m %{name} -v %{version} || :
dkms install -m %{name} -v %{version} || :

%systemd_post curves-mouse-acceleration-restore.service

udevadm control --reload || :

%preun
%systemd_preun curves-mouse-acceleration-restore.service

if [ $1 -eq 0 ]; then
    %{_sbindir}/modprobe -r curves_mouse_acceleration >/dev/null 2>&1 || :
fi

dkms remove -m %{name} -v %{version} --all --rpm_safe_upgrade || :

%postun
%systemd_postun_with_restart curves-mouse-acceleration-restore.service

%files
%license LICENSE
%license src/driver/crv/COPYING
%{_bindir}/curves-mouse-acceleration-config
%{_modulesloaddir}/curves-mouse-acceleration.conf
%{_prefix}/src/%{name}-%{version}/
%{_udevrulesdir}/99-curves-mouse-acceleration.rules
%{_unitdir}/curves-mouse-acceleration-restore.service
