#!/usr/bin/perl

$ENV{"LC_ALL"} = "C";

use Cwd;
my $cwd = &Cwd::cwd();

my $svn_exe = `which svn`;

my $target = "local";

$target = $ARGV[0] if $ARGV[0];

# Don't muck with things if you can't find svn
exit(0) if ( ! $svn_exe );

# Don't muck with thinks if you aren't in an svn repository
if ( $target eq "local" && ! -d ".svn" ) {
  exit(0);
}

# Read the svn revision number
my $svn_info;
if ( $target eq "local" ) {
  $svn_info  = `svn info`;
} else {
  $svn_info  = `svn info $target`;
}

if ( $svn_info =~ /Last Changed Rev: (\d+)/ ) {
  my $rev = $1;
  open(OUT,">$cwd/svnrevision.h");
  print OUT "#define SVNREV $rev\n";
  close(OUT);
}
else {
  die "Unable to find revision in svn info\n";
}
