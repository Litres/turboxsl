# $Id$
#
# This is free software, you may use it and distribute it under the same terms as
# Perl itself.
#
# Copyright 2014 LitRes.
#
#
package TurboXSLT;

use strict;
use vars qw($VERSION @ISA);


BEGIN {
use Carp;

require Exporter;

$VERSION = "1.0";

require DynaLoader;

@ISA = qw(DynaLoader);

# avoid possible shared library name conflict on Win32
# not using this trick on 5.10.0 (suffering from DynaLoader bug)
local $DynaLoader::dl_dlext = "xs.$DynaLoader::dl_dlext" if (($^O eq 'MSWin32') && ($] ne '5.010000'));

bootstrap TurboXSLT $VERSION;

# the following magic lets XML::LibXSLTMT internals know
# where to register XML::LibXMLMT proxy nodes
#INIT_THREAD_SUPPORT()
}


sub new {
    my $class = shift;
    my %options = @_;
    my $self = bless \%options, $class;
    $self->{TURBOXSL_GLOBAL_CONTEXT} = new XSLTGLOBALDATAPtr;
    return $self;
}

sub LoadStylesheet {
  my $self = shift;
  my $file = shift;
  return new TRANSFORM_CONTEXTPtr($self->{TURBOXSL_GLOBAL_CONTEXT}, $file);
}

sub Parse {
  my $self = shift;
  my $text = shift;
  return _parse_str($self->{TURBOXSL_GLOBAL_CONTEXT},$text);
}

sub ParseFile {
  my $self = shift;
  my $file = shift;
  return _parse_file($self->{TURBOXSL_GLOBAL_CONTEXT},$file);
}

sub Output {
  my $self = shift;
  my $ctx = shift;
  my $doc = shift;
  return _output_str($ctx, $doc);
}

sub OutputFile {
  my $self = shift;
  my $ctx = shift;
  my $doc = shift;
  my $file = shift;
  return _output_file($ctx,$doc,$file);
}

sub SetVar {
  my $self = shift;
  my $name = shift;
  if(defined $name && $name ne '') {
    my $value = shift || '';
    setvarg($self->{TURBOXSL_GLOBAL_CONTEXT},$name,$value);
  } else {
    warn "usage: TurboXSLT->SetVar(name[,value]";
  }
}

sub AddHashVar {
  my $self = shift;
  my $name = shift;
  my $index = shift;
  my $value = shift;
  
  $name = '@'.$name.'@'.$index;
  $self->SetVar($name, $value);
}

1;

###########################################################################

__END__

=head1 NAME

XML::LibXSLTMT - Interface to the GNOME libxslt library

=head1 SYNOPSIS

  use XML::LibXSLTMT;
  use XML::LibXMLMT;

  my $xslt = XML::LibXSLTMT->new();

  my $source = XML::LibXMLMT->load_xml(location => 'foo.xml');
  my $style_doc = XML::LibXMLMT->load_xml(location=>'bar.xsl', no_cdata=>1);

  my $stylesheet = $xslt->parse_stylesheet($style_doc);

  my $results = $stylesheet->transform($source);

  print $stylesheet->output_as_bytes($results);

=head1 DESCRIPTION

This module is an interface to the GNOME project's libxslt. This is an
extremely good XSLT engine, highly compliant and also very fast. I have
tests showing this to be more than twice as fast as Sablotron.

=head1 OPTIONS

XML::LibXSLTMT has some global options. Note that these are probably not
thread or even fork safe - so only set them once per process. Each one
of these options can be called either as class methods, or as instance
methods. However either way you call them, it still sets global options.

Each of the option methods returns its previous value, and can be called
without a parameter to retrieve the current value.

=over

=item max_depth

  XML::LibXSLTMT->max_depth(1000);

This option sets the maximum recursion depth for a stylesheet. See the
very end of section 5.4 of the XSLT specification for more details on
recursion and detecting it. If your stylesheet or XML file requires
seriously deep recursion, this is the way to set it. Default value is
250.

=item debug_callback

  XML::LibXSLTMT->debug_callback($subref);

Sets a callback to be used for debug messages. If you don't set this,
debug messages will be ignored.

=item register_function

  XML::LibXSLTMT->register_function($uri, $name, $subref);
  $stylesheet->register_function($uri, $name, $subref);

Registers an XSLT extension function mapped to the given URI. For example:

  XML::LibXSLTMT->register_function("urn:foo", "bar",
    sub { scalar localtime });

Will register a C<bar> function in the C<urn:foo> namespace (which you
have to define in your XSLT using C<xmlns:...>) that will return the
current date and time as a string:

  <xsl:stylesheet version="1.0"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:foo="urn:foo">
  <xsl:template match="/">
    The time is: <xsl:value-of select="foo:bar()"/>
  </xsl:template>
  </xsl:stylesheet>

Parameters can be in whatever format you like. If you pass in a nodelist
it will be a XML::LibXMLMT::NodeList object in your perl code, but ordinary
values (strings, numbers and booleans) will be ordinary perl scalars. If
you wish them to be C<XML::LibXMLMT::Literal>, C<XML::LibXMLMT::Number> and
C<XML::LibXMLMT::Number> values respectively then set the variable
C<$XML::LibXSLTMT::USE_LIBXML_DATA_TYPES> to a true value. Return values can
be a nodelist or a plain value - the code will just do the right thing.
But only a single return value is supported (a list is not converted to
a nodelist).

=item register_element

	$stylesheet->register_element($uri, $name, $subref)

Registers an XSLT extension element $name mapped to the given URI. For example:

  $stylesheet->register_element("urn:foo", "hello", sub {
	  my $name = $_[2]->getAttribute( "name" );
	  return XML::LibXMLMT::Text->new( "Hello, $name!" );
  });

Will register a C<hello> element in the C<urn:foo> namespace that returns a "Hello, X!" text node. You must define this namespace in your XSLT and include its prefix in the C<extension-element-prefixes> list:

  <xsl:stylesheet version="1.0"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:foo="urn:foo"
	extension-element-prefixes="foo">
  <xsl:template match="/">
    <foo:hello name="bob"/>
  </xsl:template>
  </xsl:stylesheet>

The callback is passed the input document node as $_[1] and the stylesheet node as $_[2]. $_[0] is reserved for future use.

=back

=head1 API

The following methods are available on the new XML::LibXSLTMT object:

=over

=item parse_stylesheet($stylesheet_doc)

C<$stylesheet_doc> here is an XML::LibXMLMT::Document object (see L<XML::LibXMLMT>)
representing an XSLT file. This method will return a
XML::LibXSLTMT::Stylesheet object, or undef on failure. If the XSLT is
invalid, an exception will be thrown, so wrap the call to
parse_stylesheet in an eval{} block to trap this.

IMPORTANT: C<$stylesheet_doc> should not contain CDATA sections,
otherwise libxslt may misbehave. The best way to assure this is to
load the stylesheet with no_cdata flag, e.g.

  my $stylesheet_doc = XML::LibXMLMT->load_xml(location=>"some.xsl", no_cdata=>1);

=item parse_stylesheet_file($filename)

Exactly the same as the above, but parses the given filename directly.

=back

=head1 Input Callbacks

To define XML::LibXSLTMT or XML::LibXSLT::Stylesheet specific input
callbacks, reuse the XML::LibXMLMT input callback API as described in
L<XML::LibXMLMT::InputCallback(3)>.

=head1 Security Callbacks

To create security preferences for the transformation see
L<XML::LibXSLTMT::Security>. Once the security preferences have been defined you
can apply them to an XML::LibXSLTMT or XML::LibXSLTMT::Stylesheet instance using
the C<security_callbacks()> method.

=head1 XML::LibXSLTMT::Stylesheet

The main API is on the stylesheet, though it is fairly minimal.

One of the main advantages of XML::LibXSLTMT is that you have a generic
stylesheet object which you call the transform() method passing in a
document to transform. This allows you to have multiple transformations
happen with one stylesheet without requiring a reparse.

=over

=item transform(doc, %params)

  my $results = $stylesheet->transform($doc, foo => "'bar'");
  print $stylesheet->output_as_bytes($results);

Transforms the passed in XML::LibXMLMT::Document object, and returns a
new XML::LibXMLMT::Document. Extra hash entries are used as parameters.
Be sure to keep in mind the caveat with regard to quotes explained in
the section on L</"Parameters"> below.

=item transform_file(filename, %params)

  my $results = $stylesheet->transform_file($filename, bar => "'baz'");

Note the string parameter caveat, detailed in the section on
L</"Parameters"> below.

=item output_as_bytes(result)

Returns a scalar that is the XSLT rendering of the
XML::LibXMLMT::Document object using the desired output format
(specified in the xsl:output tag in the stylesheet). Note that you can
also call $result->toString, but that will *always* output the
document in XML format which may not be what you asked for in the
xsl:output tag. The scalar is a byte string encoded in the output
encoding specified in the stylesheet.

=item output_as_chars(result)

Like C<output_as_bytes(result)>, but always return the output as (UTF-8
encoded) string of characters.

=item output_string(result)

DEPRECATED: This method is something between
C<output_as_bytes(result)> and C<output_as_bytes(result)>: The scalar
returned by this function appears to Perl as characters (UTF8 flag is
on) if the output encoding specified in the XSLT stylesheet was UTF-8
and as bytes if no output encoding was specified or if the output
encoding was other than UTF-8. Since the behavior of this function
depends on the particular stylesheet, it is deprecated in favor of
C<output_as_bytes(result)> and C<output_as_chars(result)>.

=item output_fh(result, fh)

Outputs the result to the filehandle given in C<$fh>.

=item output_file(result, filename)

Outputs the result to the file named in C<$filename>.

=item output_encoding()

Returns the output encoding of the results. Defaults to "UTF-8".

=item output_method()

Returns the value of the C<method> attribute from C<xsl:output>
(usually C<xml>, C<html> or C<text>). If this attribute is
unspecified, the default value is initially C<xml>. If the
L<transform> method is used to produce an HTML document, as per the
L<XSLT spec|http://www.w3.org/TR/xslt#output>, the default value will
change to C<html>. To override this behavior completely, supply an
C<xsl:output> element in the stylesheet source document.

=item media_type()

Returns the value of the C<media-type> attribute from
C<xsl:output>. If this attribute is unspecified, the default media
type is initially C<text/xml>. This default changes to C<text/html>
under the same conditions as L<output_method>.

=back

=cut

=head1 Parameters

LibXSLTMT expects parameters in XPath format. That is, if you wish to pass
a string to the XSLT engine, you actually have to pass it as a quoted
string:

  $stylesheet->transform($doc, param => "'string'");

Note the quotes within quotes there!

Obviously this isn't much fun, so you can make it easy on yourself:

  $stylesheet->transform($doc, XML::LibXSLTMT::xpath_to_string(
        param => "string"
        ));

The utility function does the right thing with respect to strings in XPath,
including when you have quotes already embedded within your string.


=head1 XML::LibXSLTMT::Security

Provides an interface to the libxslt security framework by allowing callbacks
to be defined that can restrict access to various resources (files or URLs)
during a transformation.

The libxslt security framework allows callbacks to be defined for certain
actions that a stylesheet may attempt during a transformation. It may be
desirable to restrict some of these actions (for example, writing a new file
using exsl:document). The actions that may be restricted are:

=over

=item read_file

Called when the stylesheet attempts to open a local file (ie: when using the
document() function).

=item write_file

Called when an attempt is made to write a local file (ie: when using the
exsl:document element).

=item create_dir

Called when a directory needs to be created in order to write a file.

NOTE: By default, create_dir is not allowed. To enable it a callback must be
registered.

=item read_net

Called when the stylesheet attempts to read from the network.

=item write_net

Called when the stylesheet attempts to write to the network.

=back

=head2 Using XML::LibXSLTMT::Security

The interface for this module is similar to XML::LibXMLMT::InputCallback. After
creating a new instance you may register callbacks for each of the security
options listed above. Then you apply the security preferences to the
XML::LibXSLTMT or XML::LibXSLTMT::Stylesheet object using C<security_callbacks()>.

  my $security = XML::LibXSLTMT::Security->new();
  $security->register_callback( read_file  => $read_cb );
  $security->register_callback( write_file => $write_cb );
  $security->register_callback( create_dir => $create_cb );
  $security->register_callback( read_net   => $read_net_cb );
  $security->register_callback( write_net  => $write_net_cb );

  $xslt->security_callbacks( $security );
   -OR-
  $stylesheet->security_callbacks( $security );


The registered callback functions are called when access to a resource is
requested. If the access should be allowed the callback should return 1, if
not it should return 0. The callback functions should accept the following
arguments:

=over

=item $tctxt

This is the transform context (XML::LibXSLTMT::TransformContext). You can use
this to get the current XML::LibXSLTMT::Stylesheet object by calling
C<stylesheet()>.

  my $stylesheet = $tctxt->stylesheet();

The stylesheet object can then be used to share contextual information between
different calls to the security callbacks.

=item $value

This is the name of the resource (file or URI) that has been requested.

=back

If a particular option (except for C<create_dir>) doesn't have a registered
callback, then the stylesheet will have full access for that action.

=head2 Interface

=over

=item new()

Creates a new XML::LibXSLTMT::Security object.

=item register_callback( $option, $callback )

Registers a callback function for the given security option (listed above).

=item unregister_callback( $option )

Removes the callback for the given option. This has the effect of allowing all
access for the given option (except for C<create_dir>).

=back

=head1 BENCHMARK

Included in the distribution is a simple benchmark script, which has two
drivers - one for LibXSLTMT and one for Sablotron. The benchmark requires
the testcases files from the XSLTMark distribution which you can find
at http://www.datapower.com/XSLTMark/

Put the testcases directory in the directory created by this distribution,
and then run:

  perl benchmark.pl -h

to get a list of options.

The benchmark requires XML::XPath at the moment, but I hope to factor that
out of the equation fairly soon. It also requires Time::HiRes, which I
could be persuaded to factor out, replacing it with Benchmark.pm, but I
haven't done so yet.

I would love to get drivers for XML::XSLT and XML::Transformiix, if you
would like to contribute them. Also if you get this running on Win32, I'd
love to get a driver for MSXSLT via OLE, to see what we can do against
those Redmond boys!

=head1 LIBRARY VERSIONS

For debugging purposes, XML::LibXSLTMT provides version information
about the libxslt C library (but do not confuse it with the version
number of XML::LibXSLTMT module itself, i.e. with
C<$XML::LibXSLTMT::VERSION>). XML::LibXSLTMT issues a warning if the
runtime version of the library is less then the compile-time version.

=over

=item XML::LibXSLTMT::LIBXSLT_VERSION()

Returns version number of libxslt library which was used to compile
XML::LibXSLTMT as an integer. For example, for libxslt-1.1.18, it will
return 10118.

=item XML::LibXSLTMT::LIBXSLT_DOTTED_VERSION()

Returns version number of libxslt library which was used to compile
XML::LibXSLTMT as a string, e.g. "1.1.18".

=item XML::LibXSLTMT::LIBXSLT_RUNTIME_VERSION()

Returns version number of libxslt library to which XML::LibXSLTMT is
linked at runtime (either dynamically or statically). For example, for
example, for libxslt.so.1.1.18, it will return 10118.

=item XML::LibXSLTMT::HAVE_EXLT()

Returns 1 if the module was compiled with libexslt, 0 otherwise.

=back

=head1 LICENSE

This is free software, you may use it and distribute it under the same terms as
Perl itself.

Copyright 2001-2009, AxKit.com Ltd.

=head1 AUTHOR

Matt Sergeant, matt@sergeant.org

Security callbacks implementation contributed by Shane Corgatelli.

=head1 MAINTAINER

Petr Pajas , pajas@matfyz.org

=head1 BUGS

Please report bugs via

  http://rt.cpan.org/NoAuth/Bugs.html?Dist=XML-LibXSLTMT

=head1 SEE ALSO

XML::LibXMLMT

=cut
