%define		mod_name	gzip
%define 	apxs		/usr/sbin/apxs
Summary:	Apache module: On-the-fly compression of HTML documents
Summary(pl):	Modu³ do apache: kompresuje dokumenty HTML w locie
Name:		apache-mod_%{mod_name}
Version:	1.3.19.1a
Release:	2
License:	Apache Software License
Group:		Networking/Daemons
Source0:	http://www.remotecommunications.com/apache/mod_%{mod_name}/src/%{version}/mod_%{mod_name}.c
Source1:	http://www.remotecommunications.com/apache/mod_%{mod_name}/src/%{version}/changes.txt
Source2:	http://www.remotecommunications.com/apache/mod_%{mod_name}/src/%{version}/commands.txt
Source3:	http://www.remotecommunications.com/apache/mod_%{mod_name}/src/%{version}/samples.txt
BuildRequires:	%{apxs}
BuildRequires:	apache(EAPI)-devel
BuildRequires:	zlib-devel
Prereq:		%{_sbindir}/apxs
Requires:	apache(EAPI)
BuildRoot:	%{tmpdir}/%{name}-%{version}-root-%(id -u -n)

%define		_pkglibdir	%(%{apxs} -q LIBEXECDIR)

%description
Apache module: On-the-fly compression of HTML documents. Browser will
transparently decompress and display such documents.

%description -l pl
Modu³ do apache: kompresuje dokumenty HTML w locie. Przegl±darki w
sposób przezroczysty dekompresuj± i wy¶wietlaj± takie dokumenty.

%prep
%setup -q -T -c
cp %{SOURCE0} .
cp %{SOURCE1} .
cp %{SOURCE2} .
cp %{SOURCE3} .

%build
%{apxs} -c mod_%{mod_name}.c -o mod_%{mod_name}.so -lz

%install
rm -rf $RPM_BUILD_ROOT
install -d $RPM_BUILD_ROOT%{_pkglibdir}

install mod_%{mod_name}.so $RPM_BUILD_ROOT%{_pkglibdir}

gzip -9nf *.txt

%post
%{_sbindir}/apxs -e -a -n %{mod_name} %{_pkglibdir}/mod_%{mod_name}.so 1>&2
if [ -f /var/lock/subsys/httpd ]; then
	%{_sysconfdir}/rc.d/init.d/httpd restart 1>&2
fi

%preun
if [ "$1" = "0" ]; then
	%{_sbindir}/apxs -e -A -n %{mod_name} %{_pkglibdir}/mod_%{mod_name}.so 1>&2
	if [ -f /var/lock/subsys/httpd ]; then
		/etc/rc.d/init.d/httpd restart 1>&2
	fi
fi

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(644,root,root,755)
%doc *.gz
%attr(755,root,root) %{_pkglibdir}/*
