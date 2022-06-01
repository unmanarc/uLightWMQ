%define name uLightWMQ
%define version 1.0.4
%define build_timestamp %{lua: print(os.date("%Y%m%d"))}

Name:           %{name}
Version:        %{version}
Release:        %{build_timestamp}.git%{?dist}
Summary:        Unmanarc Lightweight HTTPS Web Message Queue
License:        LGPLv3
URL:            https://github.com/unmanarc/uLightWMQ
Source0:        https://github.com/unmanarc/uLightWMQ/archive/main.tar.gz#/%{name}-%{version}-%{build_timestamp}.tar.gz
Group:          Applications/Internet

%define cmake cmake

%if 0%{?rhel} == 6
%define cmake cmake3
%endif

%if 0%{?rhel} == 7
%define cmake cmake3
%endif

%if 0%{?rhel} == 8
%define debug_package %{nil}
%endif

%if 0%{?rhel} == 9
%define debug_package %{nil}
%endif

%if 0%{?fedora} >= 33
%define debug_package %{nil}
%endif

BuildRequires: libMantids-devel >= 2.5.11
BuildRequires:  %{cmake} systemd libMantids-sqlite openssl-devel zlib-devel boost-devel gcc-c++ sqlite-devel
Requires: libMantids >= 2.5.11
Requires: libMantids-sqlite zlib openssl boost-regex sqlite

%description
This package contains a very efficient and simple web server for WEB Messages Queue with TLS/SSL Common Name peer authentication

%prep
%autosetup -n %{name}-main

%build
%{cmake} -DCMAKE_INSTALL_PREFIX:PATH=/usr -DCMAKE_BUILD_TYPE=MinSizeRel -DWITH_SSL_SUPPORT=ON
%{cmake} -DCMAKE_INSTALL_PREFIX:PATH=/usr -DCMAKE_BUILD_TYPE=MinSizeRel -DWITH_SSL_SUPPORT=ON
make %{?_smp_mflags}

%clean
rm -rf $RPM_BUILD_ROOT

%install
rm -rf $RPM_BUILD_ROOT

%if 0%{?fedora} >= 33
ln -s . %{_host}
%endif

%if 0%{?rhel} >= 9
ln -s . %{_host}
ln -s . redhat-linux-build
%endif

%if 0%{?fedora} == 35
ln -s . redhat-linux-build
%endif

%if "%{_host}" == "powerpc64le-redhat-linux-gnu"
ln -s . ppc64le-redhat-linux-gnu
%endif

%if "%{_host}" == "s390x-ibm-linux-gnu"
ln -s . s390x-redhat-linux-gnu
%endif

%if "%{cmake}" == "cmake3"
%cmake3_install
%else
%cmake_install
%endif

mkdir -vp $RPM_BUILD_ROOT/var/lib/ulightwmq
chmod 700 $RPM_BUILD_ROOT/var/lib/ulightwmq

mkdir -vp $RPM_BUILD_ROOT/etc
cp -a etc/uLightWMQ $RPM_BUILD_ROOT/etc/
chmod 600 $RPM_BUILD_ROOT/etc/uLightWMQ/keys/web_snakeoil.key

mkdir -vp $RPM_BUILD_ROOT/usr/lib/systemd/system
cp usr/lib/systemd/system/ulightwmq.service $RPM_BUILD_ROOT/usr/lib/systemd/system/%{name}.service
chmod 644 $RPM_BUILD_ROOT/usr/lib/systemd/system/%{name}.service


%files
%doc
%{_bindir}/uLightWMQ
%config(noreplace) /etc/uLightWMQ/keys/ca.crt
%config(noreplace) /etc/uLightWMQ/keys/web_snakeoil.crt
%config(noreplace) /etc/uLightWMQ/keys/web_snakeoil.key
%config(noreplace) /etc/uLightWMQ/config.ini
%dir /var/lib/ulightwmq
/usr/lib/systemd/system/%{name}.service

%post
%systemd_post %{name}.service

%preun
%systemd_preun %{name}.service

%postun
%systemd_postun_with_restart %{name}.service

%changelog
