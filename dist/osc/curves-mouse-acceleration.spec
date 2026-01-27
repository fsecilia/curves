Name:           curves-mouse-acceleration
Version:        0.0.0
Release:        0
Summary:        Curves mouse acceleration input handler
License:        MIT AND (GPL-2.0-or-later OR MIT)
URL:            https://github.com/fsecilia/curves
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  cmake
BuildRequires:  dkms
BuildRequires:  ((gcc >= 14.0.0 and gcc-c++ >= 14.0.0) or clang >= 18.0.0)
BuildRequires:  pkgconfig(Qt6Core)
BuildRequires:  pkgconfig(Qt6Gui)
BuildRequires:  pkgconfig(Qt6Widgets)
BuildRequires:  systemd
BuildRequires:  systemd-rpm-macros

Requires:       dkms
Requires:       kernel-devel
Requires:       ((gcc >= 14.0.0 and gcc-c++ >= 14.0.0) or clang >= 18.0.0)
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

%systemd_post %{name}-restore.service

udevadm control --reload || :

%preun
%systemd_preun %{name}-restore.service

if [ $1 -eq 0 ]; then
    %{_sbindir}/modprobe -r curves_mouse_acceleration >/dev/null 2>&1 || :
fi

dkms remove -m %{name} -v %{version} --all --rpm_safe_upgrade || :

%postun
%systemd_postun_with_restart %{name}-restore.service

%files
%license COPYING
%license LICENSE
%{_bindir}/%{name}-config
%{_modulesloaddir}/%{name}.conf
%{_prefix}/src/%{name}-%{version}/
%{_udevrulesdir}/99-%{name}.rules
%{_unitdir}/%{name}-restore.service
