%define		mod_name	gzip
%define 	apxs		/usr/sbin/apxs1
Summary:	Apache module: On-the-fly compression of HTML documents
Summary(pl):	Modu³ do apache: kompresuje dokumenty HTML w locie
Name:		apache1-mod_%{mod_name}
Version:	1.3.26.1a
Release:	2
License:	Apache
Group:		Networking/Daemons
Source0:	http://dl.sourceforge.net/mod-gzip/mod_gzip-%{version}.tgz
# Source0-md5:	080ccc5d789ed5efa0c0a7625e4fa02d
Source1:	%{name}.conf
Source2:	%{name}.logrotate
Patch0:		%{name}-name_clash.patch
Patch1:		%{name}-security.patch
URL:		http://www.schroepl.net/projekte/mod_gzip/
BuildRequires:	%{apxs}
BuildRequires:	apache1-devel >= 1.3.33-2
BuildRequires:	rpmbuild(macros) >= 1.268
BuildRequires:	zlib-devel
Requires(triggerpostun):	%{apxs}
Requires(triggerpostun):	grep
Requires(triggerpostun):	sed >= 4.0
Requires:	apache1 >= 1.3.33-2
Obsoletes:	apache-mod_gzip <= %{version}
BuildRoot:	%{tmpdir}/%{name}-%{version}-root-%(id -u -n)

%define		_pkglibdir	%(%{apxs} -q LIBEXECDIR 2>/dev/null)
%define		_sysconfdir	%(%{apxs} -q SYSCONFDIR 2>/dev/null)
%define		_pkglogdir	%(%{apxs} -q PREFIX 2>/dev/null)/logs

%description
Apache module: On-the-fly compression of HTML documents. Browser will
transparently decompress and display such documents.

%description -l pl
Modu³ do apache: kompresuje dokumenty HTML w locie. Przegl±darki w
sposób przezroczysty dekompresuj± i wy¶wietlaj± takie dokumenty.

%prep
%setup -q -n mod_%{mod_name}-%{version}
%patch0 -p1
%patch1 -p1

%build
%{apxs} -Wc,-Wall,-pipe -c mod_%{mod_name}.c mod_%{mod_name}_debug.c mod_%{mod_name}_compress.c -o mod_%{mod_name}.so

%install
rm -rf $RPM_BUILD_ROOT
install -d $RPM_BUILD_ROOT{%{_pkglibdir},%{_sysconfdir}/conf.d,/etc/logrotate.d,%{_pkglogdir}}

install mod_%{mod_name}.so $RPM_BUILD_ROOT%{_pkglibdir}
install %{SOURCE1} $RPM_BUILD_ROOT%{_sysconfdir}/conf.d/90_mod_%{mod_name}.conf
install %{SOURCE2} $RPM_BUILD_ROOT/etc/logrotate.d/%{name}

> $RPM_BUILD_ROOT%{_pkglogdir}/mod_gzip.log

%clean
rm -rf $RPM_BUILD_ROOT

%post
%service -q apache restart

%postun
if [ "$1" = "0" ]; then
	%service -q apache restart
fi

%triggerpostun -- %{name} < 1.3.26.1a-1.1
if grep -q '^Include conf\.d/\*\.conf' /etc/apache/apache.conf; then
	%{apxs} -e -A -n %{mod_name} %{_pkglibdir}/mod_%{mod_name}.so 1>&2
	sed -i -e '
		/^Include.*mod_%{mod_name}\.conf/d
	' /etc/apache/apache.conf
else
	# they're still using old apache.conf
	sed -i -e '
		s,^Include.*mod_%{mod_name}\.conf,Include %{_sysconfdir}/conf.d/*_mod_%{mod_name}.conf,
	' /etc/apache/apache.conf
fi
%service -q apache restart

%files
%defattr(644,root,root,755)
%doc docs/manual/english/*
%lang(de) %doc docs/manual/deutsch
%attr(755,root,root) %{_pkglibdir}/*
%attr(640,root,root) %config(noreplace) %verify(not md5 mtime size) %{_sysconfdir}/conf.d/*_mod_%{mod_name}.conf
%attr(640,root,root) %config(noreplace) %verify(not md5 mtime size) /etc/logrotate.d/*
%attr(640,root,root) %ghost %{_pkglogdir}/*
