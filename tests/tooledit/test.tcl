source ../../tcl/tooledit.tcl

set infile [lindex $argv 0]
set outfile [lindex $argv 1]

# get rid of the default window
wm withdraw .
# run tooledit using the test.tbl
tooledit::tooledit $infile {x y z a b c u v w diam front back orien}
# write the test.tbl to result.tbl
tooledit::writefile $outfile
# finish up
exit 0
