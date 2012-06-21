#!/usr/bin/perl -w

use IO::File;

my $IN_HTML = "src/Data_values.html";
my $OUT_PNG = "img";

if (! -d $OUT_PNG) { mkdir $OUT_PNG,0777; }
my $fd = new IO::File($IN_HTML);
die "$IN_HTML : $!\n" if (! defined $fd);

while (my $line = <$fd>) {
    if ($line =~ /^<td.+src="\/images\/thumb\/.\/([a-f0-9]{2})\/(.+\.png)\/25px\-.+\/td>/ ) {
        my ($ha,$name,$id) = ($1,$2,undef);
        my $idstr = <$fd>;
        if ($idstr =~ /<td>\s+(?:<span[^<]+>)?(\d+)(?:<\/span>)?\s+<\/td>/) {
            $id = $1;
        }

        my $url = "http://www.minecraftwiki.net/images/".substr($ha,0,1)."/$ha/$name";
        print "wget -nv -O \"$OUT_PNG/$id.png\" \"$url\"\n";
    }
}

