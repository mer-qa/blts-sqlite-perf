Name: blts-sqlite-perf
Summary: BLTS sqlite performance test suite
Version: 0.0.1
Release: 1
License: GPLv2
Group: Development/Testing
URL: https://github.com/mer-qa/blts-sqlite-perf
Source0: %{name}-%{version}.tar.gz
BuildRequires: libbltscommon-devel
BuildRequires: sqlite-devel

%description
This package contains performance tests for sqlite

%prep
%setup -q

%build
./autogen.sh
%configure
make

%install
make install DESTDIR=$RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
/opt/tests/blts-sqlite-perf/*
