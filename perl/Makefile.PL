# -------------------------------------------------------------------------- #
# $Id$
# -------------------------------------------------------------------------- #
# Makefile.PL for XML::LibXMLMT.
# This file is required to generate a localized Makefile
# -------------------------------------------------------------------------- #


BEGIN {
  if ($] < 5.008_001) {
    warn "\nSorry, at least Perl 5.8.1 is required for this module!\n\n";
    exit;
  }
}

use ExtUtils::MakeMaker;
use Config;


$|=0;

my %config;

while($_ = shift) {
    my ($key, $val) = split(/=/, $_, 2);
    $config{$key} = $val;
}

my $DEBUG = delete $config{DEBUG};

unless ( $is_Win32 ) { # cannot get config in W32
    my $xsltcfg = "turboxsl-config";
    my $libprefix = $ENV{XSLTPREFIX} || $config{XSLTPREFIX};

    delete $config{XSLTPREFIX}; # delete if exists, otherwise MakeMaker gets confused

    if ( defined $libprefix ) {
        $xsltcfg = $libprefix . '/bin/' . $xsltcfg;
    }

    # if a user defined INC and LIBS on the command line we must not
    # override them
    if ( not defined $config{LIBS} and not defined $config{INC} ) {
        # get libs and inc from gnome-config
        eval {
            print "running turboxsl-config... ";
#            my $ver = backtick("$xsltcfg --version");
#            my ($major, $minor, $point) = $ver =~ /(\d+)\.(\d+)\.(\d+)/g;
#            if (not
#                (
#                    ($major > 1)
#                        or
#                    (($major == 1) && ($minor > 1))
#                        or
#                    (($major == 1) && ($minor == 1) && ($point >= 28))
#                )
#            )
#            {
#                die <<'EOF';
#libxslt versions before 1.1.28 are buggy. Please install libxslt-1.1.28.
#EOF
#            }
            $config{LIBS} ||= backtick("$xsltcfg --libs");
            $config{INC} ||= backtick("$xsltcfg --cflags");
            print "ok\n";
        };
        if ($@) {
            print "failed\n";
            warn "*** ", $@ if $DEBUG;
            warn "using fallback values for LIBS and INC\n";
            print <<OPT;
options:
  LIBS='$config{LIBS}'
  INC='$config{INC}'
If this is wrong, Re-run as:
  \$ $^X Makefile.PL LIBS='-L/path/to/lib' INC='-I/path/to/include'

OPT
        }
    }
}

if ($config{LIBS} !~ /\-l(lib)?turboxsl\b/) {
    # in this case we are not able to run xml2-config. therefore we need to
    # expand the libz as well.
  if ($is_Win32) {
    if( $ENV{ACTIVEPERL_MINGW} ) {
      $config{LIBS} .= ' -llibturboxsl.lib';
    } else {
      $config{LIBS} .= ' -llibturboxsl';
    }
  } else {
    $config{LIBS} .= ' ';
  }
}

if ($config{LIBS} !~ /\-lm\b/) {
    # math support is important, but is not available separately in W32
    $config{LIBS} .= $is_Win32 ? '' :' -lm';
}

if (!have_library($is_Win32 ? "libturboxsl" : "turboxsl")) {
    print STDERR <<DEATH;
libturboxsl not found

If you install via RPMs, make sure you also install the -devel
RPMs, as this is where the headers (.h files) are.
DEATH
exit 0; # 0 recommended by http://cpantest.grango.org (Notes for CPAN Authors)
}


$config{LIBS}.=' '.$Config{libs};

if ($DEBUG) {
  print "LIBS: $config{LIBS}\n";
  print "INC: $config{INC}\n";
}

my $ldflags = delete $config{LDFLAGS};
if ($ldflags) {
    $config{dynamic_lib} = { OTHERLDFLAGS => " $ldflags " };
}

# Avoid possible shared library name conflict. On Win32 systems
# the name of system DLL libxlst.dll clashes with module's LibXSLT.dll.
# To handle this we are gonna rename module's DLL to LibXSLT.xs.dll.
if ($is_Win32) {
  if ($] eq '5.010000') {
    print STDERR <<5100BUG;
******************************** !!!WARNING!!! ********************************
According to the version string you are running a win32 perl 5.10.0 that is
known to be suffering from dynaloader bug that prevents using non-default
module's DLL name. Therefore we have to use LibXSLT.dll that will clash with the
standard name of xslt library DLL. Please consider renaming your xslt library
to e.g. libxslt_win32.dll and then make the appropriate changes to this module's
build process to look for that instead of standard libxslt.dll. If you do not
make the proposed changes you will got during the tests many error popups with
slightly confusing message about "xsltApplyOneTemplate" function.
This bug is not present in perl 5.8.* and will be fixed in 5.10.1.
*******************************************************************************
5100BUG
  }
  else {
    $config{DLEXT} = 'xs.dll' if ($is_Win32);
  }
}

WriteMakefile(
    'NAME'	=> 'TurboXSLT',
    'VERSION_FROM' => 'TurboXSLT.pm', # finds $VERSION
    'AUTHOR'    => 'Egor Voznessenski',
    'ABSTRACT'  => 'Interface to turboxsl xslt conversion library',
    'PREREQ_PM' =>
    {
        'strict' => 0,
        'warnings' => 0,
    },
    'OBJECT'     => '$(O_FILES)',
    %config,
);

###################################################################
# Functions
#  - these should really be in MakeMaker... But &shrug;
###################################################################

use Config;
use Cwd;
use Symbol;
use File::Spec;

use vars qw/$DEVNULL $is_Win32/;

BEGIN {
    $is_Win32 = ($^O =~ /Win32/);
    if ($is_Win32) {
        $DEVNULL = 'DEVNULL';
    }
    else {
        $DEVNULL = eval { File::Spec->devnull };
        if ($@) { $DEVNULL = '/dev/null' }
    }
}

sub rm_f {
    my @files = @_;
    my @realfiles;
    foreach (@files) {
        push @realfiles, glob($_);
    }
    if (@realfiles) {
        chmod(0777, @realfiles);
        unlink(@realfiles);
    }
}

sub rm_fr {
    my @files = @_;
    my @realfiles;
    foreach (@files) {
        push @realfiles, glob($_);
    }
    foreach my $file (@realfiles) {
        if (-d $file) {
            # warn("$file is a directory\n");
            rm_fr("$file/*");
            rm_fr("$file/.exists");
            rmdir($file) || die "Couldn't remove $file: $!";
        }
        else {
            # warn("removing $file\n");
            chmod(0777, $file);
            unlink($file);
        }
    }
}

sub xsystem {
  my (@command)=@_;
    if ($DEBUG) {
        print "@command\n";
        if (system(@command) != 0) {
            die "system call to '@command' failed";
        }
        return 1;
    }
    open(OLDOUT, ">&STDOUT");
    open(OLDERR, ">&STDERR");
    open(STDOUT, ">$DEVNULL");
    open(STDERR, ">$DEVNULL");
    my $retval = system(@command);
    open(STDOUT, ">&OLDOUT");
    open(STDERR, ">&OLDERR");
    if ($retval != 0) {
        die "system call to '@command' failed";
    }
    return 1;
}

sub backtick {
    my $command = shift;
    if ($DEBUG) {
        print $command, "\n";
        my $results = `$command`;
        chomp $results;
        if ($? != 0) {
            die "backticks call to '$command' failed";
        }
        return $results;
    }
    open(OLDOUT, ">&STDOUT");
    open(OLDERR, ">&STDERR");
    open(STDOUT, ">$DEVNULL");
    open(STDERR, ">$DEVNULL");
    my $results = `$command`;
    my $retval = $?;
    open(STDOUT, ">&OLDOUT");
    open(STDERR, ">&OLDERR");
    if ($retval != 0) {
        die "backticks call to '$command' failed";
    }
    chomp $results;
    return $results;
}

sub try_link0 {
    my ($src, $opt) = @_;
    my $cfile = gensym();
    # local $config{LIBS};
    # $config{LIBS} .= $opt;
    unless (mkdir(".testlink", 0777)) {
        rm_fr(".testlink");
        mkdir(".testlink", 0777) || die "Cannot create .testlink dir: $!";
    }
    chdir(".testlink");
    open($cfile, ">Conftest.xs") || die "Cannot write to file Conftest.xs: $!";
print $cfile <<EOT;
#ifdef __cplusplus
extern "C" {
#endif
#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>
#ifdef __cplusplus
}
#endif

EOT
    print $cfile $src;
    print $cfile <<EOT;

MODULE = Conftest          PACKAGE = Conftest

PROTOTYPES: DISABLE

EOT
    close($cfile);
    open($cfile, ">Conftest.pm") || die "Cannot write to file Conftest.pm: $!";
    print $cfile <<'EOT';
package Conftest;
$VERSION = 1.0;
require DynaLoader;
@ISA = ('DynaLoader');
bootstrap Conftest $VERSION;
1;
EOT
    close($cfile);
    open($cfile, ">Makefile.PL") || die "Cannot write to file Makefile.PL: $!";
    print $cfile <<'EOT';
use ExtUtils::MakeMaker;
my %config;
while($_ = shift @ARGV) {
    my ($k, $v) = split /=/, $_, 2;
    warn("$k = $v\n");
    $config{$k} = $v;
}
WriteMakefile(NAME => "Conftest", VERSION_FROM => "Conftest.pm", %config);
EOT
    close($cfile);
    open($cfile, ">test.pl") || die "Cannot write to file test.pl: $!";
    print $cfile <<EOT;
use Test; BEGIN { plan tests => 1; } END { ok(\$loaded) }
use Conftest; \$loaded++;
EOT
    close($cfile);
    xsystem($^X,'Makefile.PL',map "$_=$config{$_}", keys %config);
    xsystem($Config{make},
	    ($config{MAKEAPERL} ? qw(-f Makefile.aperl FIRST_MAKEFILE=Makefile.aperl) : ()),
	    'test'); #,"OTHERLDFLAGS=".$opt);
}

sub try_link {
    my $start_dir = cwd();
    my $result = eval {
        try_link0(@_);
    };
    warn $@ if $DEBUG && $@;
    chdir($start_dir);
    rm_fr(".testlink");
    return $result;
}

sub have_library {
    my ($lib, $func) = (@_, "blank");
    printf("checking for %s() in -l%s... ", $func, $lib) if $func ne "blank";
    printf("looking for -l%s... ", $lib) if $func eq "blank";

    my $result;
    if ($func) {
        my $libs = $is_Win32 ? " $lib.lib  " : "-l$lib";
        if ($is_Win32) {
            $result = try_link(<<"SRC", $libs);
#include <windows.h>
#include <winsock.h>
blank() { return 0; }
int t() { ${func}(); return 0; }
SRC
            unless ($result) {
                $result = try_link(<<"SRC", $libs);
#include <windows.h>
#include <winsock.h>
blank() { return 0; }
int t() { void ((*p)()); p = (void ((*)()))${func}; return 0; }
SRC
            }
        }
        else {

            $result = try_link(<<"SRC", $libs);
blank() { return 0; }
int t() { ${func}(); return 0; }
SRC
        }
    }

    unless ($result) {
        print "no\n";
        return 0;
    }

    if ($func ne "main") {
        $config{DEFINE} .= uc(" -Dhave_$func");
    }

    print "yes\n";
    return 1;
}

sub MY::postamble {
  return <<'MAKE_FRAG';

runtest: pure_all
	perl -MFile::Spec -MTest::Run::CmdLine::Iface -e \
        "local @INC = @INC;	unshift @INC, map { File::Spec->rel2abs(\$$_) } ('$(INST_LIB)', '$(INST_ARCHLIB)'); Test::Run::CmdLine::Iface->new({test_files => [glob(q{t/*.t})]})->run();"

distruntest: distdir
	cd $(DISTVNAME) && $(ABSPERLRUN) Makefile.PL
	cd $(DISTVNAME) && $(MAKE) $(PASTHRU)
	cd $(DISTVNAME) && $(MAKE) runtest $(PASTHRU)

MAKE_FRAG
}

