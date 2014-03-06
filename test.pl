#!/usr/bin/perl -w
use strict;
use TurboXSLT;
use Time::HiRes;
use utf8;

my $engine = new TurboXSLT;
my $Start = Time::HiRes::time();
my $ctx = $engine->LoadStylesheet("xsl/for_readers/index.xsl");
warn sprintf "Style ini:\t%1.6fs per action\n",(Time::HiRes::time() - $Start);

$Start = Time::HiRes::time();
my $doc = $engine->ParseFile("xml.xml");
warn sprintf "Parse XML:\t%1.6fs per action\n",(Time::HiRes::time() - $Start);

$Start = Time::HiRes::time();
my $res = $ctx->Transform($doc);
warn sprintf "Transform:\t%1.6fs per action\n",(Time::HiRes::time() - $Start);

$Start = Time::HiRes::time();
print $ctx->OutputFile($res, "outperl.html");
warn sprintf "Output:\t%1.6fs per action\n",(Time::HiRes::time() - $Start);
print "ok\n";
1;
