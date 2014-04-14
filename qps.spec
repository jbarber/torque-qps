Name:		qps
Version:	0.0.1
Release:	1%{?dist}
Summary:	Alternative to qstat for the Torque resource manager

Group:		Applications/System
License:	BSD
URL:		https://github.com/jbarber/torque-qps
Source0:	https://github.com/jbarber/torque-qps/qps-%{version}.tar.gz

BuildRequires: gcc make torque-devel

%description
An "improved" version of qstat for the Torque resource manager. Features
include:
* Selectable job attributes - only show interesting information
* One-row per job output - make it easy to grep for relevant jobs
* JSON + XML output
* qstat-like output but without data truncation

%prep
%setup -q

%build
make %{?_smp_mflags}

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}%{_bindir}
mkdir -p %{buildroot}%{_mandir}/man1

make install DESTDIR=%{buildroot}

%files
%attr(755,root,root)
%{_bindir}/qps

%attr(644,root,root)
%doc README.md
%doc %{_mandir}/man1/qps.1*

%changelog
* Tue Apr 1 2014 Jonathan Barber <jonathan.barber@gmail.com> 0.1-1
- Initial specfile