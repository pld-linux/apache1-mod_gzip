%define		mod_name	gzip
%define 	apxs		/usr/sbin/apxs
Summary:	Apache module: On-the-fly compression of HTML documents
Summary(pl):	Modu³ do apache: kompresuje dokumenty HTML w locie
Name:		apache-mod_%{mod_name}
Version:	1.3.26.1a
Release:	2
License:	Apache
Group:		Networking/Daemons
Source0:	http://dl.sourceforge.net/mod-gzip/mod_gzip-%{version}.tgz
# Source0-md5:	080ccc5d789ed5efa0c0a7625e4fa02d
Source1:	mod_%{mod_name}.logrotate
Patch0:		mod_%{mod_name}-name_clash.patch
Patch1:		mod_%{mod_name}-security.patch
URL:		http://sourceforge.net/projects/mod-gzip/
BuildRequires:	%{apxs}
BuildRequires:	apache(EAPI)-devel
BuildRequires:	zlib-devel
Requires(post,preun):	%{apxs}
Requires(post,preun):	grep
Requires(preun):	fileutils
Requires:	apache(EAPI)
BuildRoot:	%{tmpdir}/%{name}-%{version}-root-%(id -u -n)

%define		_pkglibdir	%(%{apxs} -q LIBEXECDIR)
%define         _sysconfdir     /etc/httpd

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
install -d $RPM_BUILD_ROOT{%{_pkglibdir},%{_sysconfdir},/etc/logrotate.d}

install mod_%{mod_name}.so $RPM_BUILD_ROOT%{_pkglibdir}
sed -e 's#logs/mod_gzip.log#/var/log/httpd/mod_gzip.log#g' docs/mod_gzip.conf.sample > $RPM_BUILD_ROOT%{_sysconfdir}/mod_gzip.conf

install %{SOURCE1} $RPM_BUILD_ROOT/etc/logrotate.d/%{name}

%clean
rm -rf $RPM_BUILD_ROOT

%post
%{apxs} -e -a -n %{mod_name} %{_pkglibdir}/mod_%{mod_name}.so 1>&2
if [ -f /etc/httpd/httpd.conf ] && ! grep -q "^Include.*mod_%{mod_name}.conf" /etc/httpd/httpd.conf; then
        echo "Include /etc/httpd/mod_%{mod_name}.conf" >> /etc/httpd/httpd.conf
fi
if [ -f /var/lock/subsys/httpd ]; then
	/etc/rc.d/init.d/httpd restart 1>&2
fi

%preun
if [ "$1" = "0" ]; then
	%{apxs} -e -A -n %{mod_name} %{_pkglibdir}/mod_%{mod_name}.so 1>&2
	umask 027
	grep -v "^Include.*mod_%{mod_name}.conf" /etc/httpd/httpd.conf > \
                /etc/httpd/httpd.conf.tmp
        mv -f /etc/httpd/httpd.conf.tmp /etc/httpd/httpd.conf
	if [ -f /var/lock/subsys/httpd ]; then
		/etc/rc.d/init.d/httpd restart 1>&2
	fi
fi

%files
%defattr(644,root,root,755)
%doc docs/manual/english/*
%lang(de) %doc docs/manual/deutsch
%attr(755,root,root) %{_pkglibdir}/*
%attr(640,root,root) %config(noreplace) %verify(not size mtime md5) %{_sysconfdir}/mod_gzip.conf
%attr(640,root,root) %config(noreplace) /etc/logrotate.d/*
