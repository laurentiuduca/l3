#!/usr/bin/perl

if (scalar(@ARGV) < 1) {
    die("Error: $0 <sim> <gccoptions>, called with ".join(" ",@ARGV));
}
my $sim = $ARGV[0];
my $opt = $ARGV[1];
$sim =~ s/.exe$//;
$sim =~ s/^.*\///;
my $simopt = "";

if (!($sim eq 'tsim-leon2' ||
      $sim eq 'tsim-leon3' ||
      $sim eq 'tsim-erc32')) {
    die("Error: expecting tsim-erc32 tsim-leon tsim-leon3 as simulators\n");
}

if($opt =~ /\@msoft-float/) {
    if($sim eq 'tsim-leon2') {
	$simopt .= " -nofpu 1 ";
    } elsif($sim eq 'tsim-leon3') {
	$simopt .= " -nofpu 1 ";
    } elsif($sim eq 'tsim-erc32') {
	$simopt .= " -nofpu 1 ";
    }
}

if(!($opt =~ /\@mcpu=v8/ || $opt =~ /\@mcpu=leon/)) {
    if($sim eq 'tsim-leon2') {
	$simopt .= " -nov8 ";
    } elsif($sim eq 'tsim-leon3') {
 	$simopt .= " -nov8 ";
    } elsif($sim eq 'tsim-erc32') {
	#print(STDERR "warning: running with \@mcpu=v8 option for tsim-erc32:\n".join(" ",@ARGV));
    }
} else {
}

if($opt =~ /\@mflat/) {
    if($sim eq 'tsim-leon2') {
	die("error: \@mflat option not supported for tsim-leon\n");
    } elsif($sim eq 'tsim-leon3') {
	$simopt .= " -nwin 2 ";
    } elsif($sim eq 'tsim-erc32') {
	die("error: \@mflat option not supported for tsim-erc32\n");
    }
}

if($opt =~ /\@qsvt/) {
    if($sim eq 'tsim-leon2') {
	die("error: \@qsvt option not supported for tsim-leon\n");
    } elsif($sim eq 'tsim-leon3') {
    } elsif($sim eq 'tsim-erc32') {
	die("error: \@qsvt option not supported for tsim-erc32\n");
    }
}

#print(STDERR "Options for $sim: $simopt\n");
print($simopt);


