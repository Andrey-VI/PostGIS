#!/usr/bin/perl -w

# $Id$

use Pg;

$VERBOSE = 0;

sub usage
{
	local($me) = `basename $0`;
	chop($me);
	print STDERR "$me [-v] [-bps <max_bps>] <table> <col>\n";
}

$BPS=32;
$TABLE='';
$COLUMN='';
for ($i=0; $i<@ARGV; $i++)
{
	if ( $ARGV[$i] =~ m/^-/ )
	{
		if ( $ARGV[$i] eq '-v' )
		{
			$VERBOSE++;
		}
		elsif ( $ARGV[$i] eq '-bps' )
		{
			$BPS=$ARGV[++$i]
		}
		else
		{
			print STDERR "Unknown option $ARGV[$i]:\n";
			usage();
			exit(1);
		}
	}
	elsif ( ! $TABLE )
	{
		$TABLE = $ARGV[$i];
	}
	elsif ( ! $COLUMN )
	{
		$COLUMN = $ARGV[$i];
	}
	else
	{
		print STDERR "Too many options:\n";
		usage();
		exit(1);
	}
}


$SCHEMA = 'public';
$COLUMN = 'the_geom' if ( $COLUMN eq '' );
if ( $TABLE =~ /(.*)\.(.*)/ ) 
{
	$SCHEMA = $1;
	$TABLE = $2;
}

#connect
$conn = Pg::connectdb("");
if ( $conn->status != PGRES_CONNECTION_OK ) {
        print STDERR $conn->errorMessage;
	exit(1);
}

if ( $VERBOSE )
{
	print "Table: \"$SCHEMA\".\"$TABLE\"\n";
	print "Column: \"$COLUMN\"\n";
}

# Get extent

$query = 'select extent("'.$COLUMN.'"), min(geometrytype("'.$COLUMN.'")) from "'.$SCHEMA.'"."'.$TABLE.'"';
$res = $conn->exec($query);
if ( $res->resultStatus != PGRES_TUPLES_OK )  {
	print STDERR $conn->errorMessage;
	exit(1);
}
$TYPE = $res->getvalue(0, 1);
$EXTENT = $res->getvalue(0, 0);

# find srid
$query = 'select srid("'.$COLUMN.'") from "'.$SCHEMA.'"."'.$TABLE.'"';
$res = $conn->exec($query);
if ( $res->resultStatus != PGRES_TUPLES_OK )  {
	print STDERR $conn->errorMessage;
	exit(1);
}
$SRID = $res->getvalue(0, 0);


# parse extent
$EXTENT =~ /^BOX3D\((.*) (.*) (.*),(.*) (.*) (.*)\)$/;
$ext{xmin} = $1;
$ext{ymin} = $2;
$ext{xmax} = $4;
$ext{ymax} = $5;

# vacuum analyze table
$query = 'vacuum analyze "'.$SCHEMA.'"."'.$TABLE.'"';
$res = $conn->exec($query);
if ( $res->resultStatus != PGRES_COMMAND_OK )  {
	print STDERR $conn->errorMessage;
	exit(1);
}


@extents = ( \%ext );

print "  Type: $TYPE\n";
print "Extent: ".print_extent(\%ext)."\n" if ($VERBOSE);


$ext = \%ext;
#print "BPS: $bps (".@extents." cells)\n";
print "  bps\test\treal\tdelta\tmatch_factor\n";
print "----------------------------------------------------------\n";
for ($bps=1; $bps<=$BPS; $bps*=2)
{
	split_extent($ext, $bps, \@extents);


	$best_match_factor=10000;
	$worst_match_factor=1;
	$sum_match_factors=0;
	$count_match_factors=0;
	while ( ($cell_ext=pop(@extents)) )
	{
		($est,$real) = test_extent($cell_ext);
		$delta = $est-$real;

		print "    $bps\t".$est."\t".$real."\t$delta";
		if ($real)
		{
			$match_factor = $est/$real;
			if ( $match_factor && $match_factor < 1 )
			{
				$match_factor = - (1/$match_factor);
			}
			$match_factor = int($match_factor*1000)/1000;
			print "\t$match_factor";
			$count_match_factors++;

			$abs_match_factor = abs($match_factor);
			$sum_match_factors += $abs_match_factor;
			if ( $abs_match_factor > $worst_match_factor )
			{
				$worst_match_factor = $match_factor;
			}
			if ( $abs_match_factor < $best_match_factor )
			{
				$best_match_factor = $match_factor;
			}
		}
		print "\n";
	}
	$avg_match_factor = $sum_match_factors/$count_match_factors;
	$avg_match_factor = int($avg_match_factor*1000)/1000;
	print "(bps/best/worst/avg):\t".
		$bps."\t".
		$best_match_factor."\t".
		$worst_match_factor."\t".$avg_match_factor."\n";
}


##################################################################

sub print_extent
{
	local($ext) = shift;
	local($s);

	$s = $ext->{'xmin'}." ".$ext->{'ymin'}."  ";
	$s .= $ext->{'xmax'}." ".$ext->{'ymax'};

	return $s;
}

sub split_extent
{
	local($ext) = shift;
	local($bps) = shift;
	local($stack) = shift;

	local($width, $height, $cell_width, $cell_height);
	local($x,$y);

	$width = $ext->{'xmax'} - $ext->{'xmin'};
	$height = $ext->{'ymax'} - $ext->{'ymin'};
	$cell_width = $width / $bps;
	$cell_height = $height / $bps;

	if ($VERBOSE)
	{
		print "cell_w: $cell_width\n";
		print "cell_h: $cell_height\n";
	}

	@$stack = ();
	for ($x=$ext->{'xmin'}; $x<$ext->{'xmax'}; $x += $cell_width)
	{
		for ($y=$ext->{'ymin'}; $y<$ext->{'ymax'}; $y += $cell_height)
		{
			local(%cell);
			$cell{'xmin'} = $x;
			$cell{'ymin'} = $y;
			$cell{'xmax'} = $x+$cell_width;
			$cell{'ymax'} = $y+$cell_height;
			#print "PUSH\n";

			print "cell: ".print_extent(\%cell)."\n" if ($VERBOSE);
			#print "Y: $y\n" if ($VERBOSE);
			push(@$stack, \%cell);
		}
	}
}

sub test_extent
{
	local($ext) = shift;

	# Test whole extent query
	$query = 'explain analyze select oid from "'.
		$SCHEMA.'"."'.$TABLE.'" WHERE "'.$COLUMN.'" && '.
		"'SRID=$SRID;BOX3D(".$ext->{'xmin'}." ".
		$ext->{'ymin'}.", ".$ext->{'xmax'}." ".
		$ext->{'ymax'}.")'";
	$res = $conn->exec($query);
	if ( $res->resultStatus != PGRES_TUPLES_OK )  {
		print STDERR $conn->errorMessage;
		exit(1);
	}
	while ( ($row=$res->fetchrow()) )
	{
		next unless $row =~ /.* rows=([0-9]+) .* rows=([0-9]+) /;
		$est = $1;
		$real = $2;
		last;
	}

	return ($est,$real);
}

# 
# $Log$
# Revision 1.3  2004/03/05 16:01:02  strk
# added -bps switch to set maximun query level. reworked command line parsing
#
# Revision 1.2  2004/03/05 15:29:35  strk
# more verbose output
#
# Revision 1.1  2004/03/05 11:52:24  strk
# initial import
#
#
