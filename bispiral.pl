#!/usr/bin/perl -w

use IO::File;
use Data::Dumper;
use POSIX;

my $PI= 4*atan2(1, 1);

sub hollow_2d {
    my $s = scalar @_;
    my $b;

    my $field = {};

    foreach $b (@_) {
        $$field{$$b{x}}{$$b{z}} = 'a';
    }

    foreach $b (@_) {
        my $x = $$b{x};
        my $z = $$b{z};
        if ( defined ($$field{$x-1}{$z}) && defined ($$field{$x+1}{$z}) &&
             defined ($$field{$x}{$z-1}) && defined ($$field{$x}{$z+1}) ) {
            $$field{$x}{$z} = 'b';
        }
    }

    return grep { $$field{$$_{x}}{$$_{z}} eq 'a' } @_;
}

sub make_disk {
    my ($r) = @_;
    my $r2 = $r*$r;

    my @B = ();

    my ($x,$z);
    my $max = int(ceil($r)); # with the center at x=0, 0<=x<=$max for all points from the center to the edge

    for($x=-$max; $x<=$max; $x++) {
        for($z=-$max; $z<=$max; $z++) {
            if ($x*$x+$z*$z <= $r2) {
                push @B, { x=>$x, z=>$z };
            }
        }
    }

    return @B;
}

sub make_cone {
    my ($h,$r,$hollow) = @_;

    my $dr = $r/($h-1); # radius difference between two layers

    my @B = ();
    for(my $y=0; $y<$h; $y++) {
        my @disk = make_disk($r);
        if ($hollow) { @disk = hollow_2d(@disk); }
        push @B, map { $$_{y} = $y; $_ } @disk;
        $r -= $dr;
    }

    return @B;
}

sub apply_material {
    my ($bid,$meta,@B) = @_;
    foreach (@B) {
        $$_{bid} = $bid;
        $$_{meta} = $meta;
    }
}

sub export {
    my ($fname, @B) = @_;
    my $fh = new IO::File("> $fname") || die "$!\n";

    print $fh "x,y,z,bid,meta\n";

    foreach my $b (@B) {
        print $fh "$$b{x},$$b{y},$$b{z},$$b{bid},$$b{meta}\n";
    }
    $fh->close();
}

sub norm { return $_[0] - floor($_[0]); }

sub polar_cut {
    my ($nr, $wd, $ph, @B) = @_;

    $ph -= $wd/4;

    my @P = map { $_*(1/$nr) } ( 0 .. $nr-1 );
    my @O = ();
    foreach my $b (@B) {
        my $phi = atan2($$b{x},$$b{z})/(2*$PI)-$ph;
        foreach my $p (@P) {
            my $diff = abs(norm($phi-$p));
            if ($diff <= $wd/2) {
                push @O,$b;
                last;
            }
        }
    }

    return @O;
}

my @B=();
for(my $y=28; $y<120; $y++) {
    my $r = (sin(1/($y/70))-0.6)*22+8;
    my @disk = map { $$_{y}=$y-28; $_ } make_disk($r);
    my @hd = hollow_2d(@disk);
    my $wd = $r/30;
    my $ph = ($y-28)/100;
    my @cut = polar_cut(2, $wd, $ph, @hd);
    push @B, @cut;
}

my %J = ();
foreach (@B) {
    my $id = "$$_{x},$$_{z},$$_{y}";
    if (defined $J{$id}) {
        $J{$id}{meta} = 10; # purple
    }
    else {
        $J{$id} = { x=>$$_{x}, z=>$$_{z}, y=>$$_{y}, meta=>11, bid=>0x5f }; # blue
    }

    my $nz = -$$_{z};
    my $id2 = "$$_{x},$nz,$$_{y}";
    if (defined $J{$id2}) {
        $J{$id2}{meta} = 10; # purple
    }
    else {
        $J{$id2} = { x=>$$_{x}, z=>$nz, y=>$$_{y}, meta=>14, bid=>0x5f }; # red
    }
}

export("csv/twirl2.csv",values %J);

__END__

# Example for 2D-hollowing a disk
my @disk = hollow_2d(make_disk(3.3));
foreach (@disk) { print "$$_{x},$$_{z}\n"; }

# big pile of melons :)
my $nr = defined($ARGV[2]) ? $ARGV[2] : 2;
my @cone = polar_cut($nr,$ARGV[0],$ARGV[1],make_cone(50,30,0));
apply_material(0x67,0,@cone);
export("csv/cone.csv",@cone);
