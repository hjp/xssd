Summary: xssd - extremely simple sudo
Name: xssd
Version: 0.7
Release: 1
Copyright: GPL
Source: %{name}.tar.gz
Group: Utilities/Luga
BuildRoot: /var/tmp/%{name}-root
Prefix: %{_prefix}

%description
xssd allows execution of commands as another user similar to sudo or
setuid bits. Unlike sudo, however, it is intended to be extremely simple
and therefore easy to verify for correctness. Unlike setuid bits, it
logs each invocation to syslog, filters environment variables and is
more flexible in granting access to different users.

%prep
%setup -c %{name}

%build

%install
make PREFIX=%{_prefix}
make PREFIX=%{_prefix} BUILD_ROOT=${RPM_BUILD_ROOT} SUIDPERM="" install

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(0644,root,root,0755)
%attr(4711,root,root) /usr/bin/xssd
%dir /etc/xssd
%{_mandir}/man1/xssd.1.gz

%changelog
* Mon Nov 25 2002 Peter J. Holzer <hjp@hjp.at
- allow building RPM as non-root				0.7-1

* Fri Apr 26 2002 Peter J. Holzer <hjp@hjp.at
- Close config file after use.
- Make sure stdin/out/err are open.

* Wed Jan 23 2002 Peter J. Holzer <hjp@hjp.at>
- rpmified							0.4-1
