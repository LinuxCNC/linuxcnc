#!/bin/bash
# Re-express the classic Python hal.stream overrun/underrun/sampleno test on the
# filestream cmod.  gomc removed the embedded Python hal.stream binding; the ring
# semantics it exercised (mixed-type round-trip, sample counting, underrun when
# clocked empty, no overrun) are now HAL pins.  Replay 9 bfsu samples through a
# depth-10 ring clocked for 12 ticks and verify them.
cat > in.txt <<DATA
0 0 0 0
1 1 1 1
0 2 2 2
1 3 3 3
0 4 4 4
1 5 5 5
0 6 6 6
1 7 7 7
0 8 8 8
DATA
rm -f out.txt server.log
gomc-server -r -f halmodule1.hal --serve >server.log 2>&1 &
SRV=$!
trap '[ -n "$SRV" ] && kill $SRV 2>/dev/null; wait 2>/dev/null' EXIT
for i in $(seq 100); do halcmd show comp 2>/dev/null | grep -q filestream && break; sleep 0.1; done
halcmd start
for i in $(seq 300); do [ "$(halcmd getp filestream.done 2>/dev/null | awk '{print $NF}')" = TRUE ] && break; sleep 0.02; done
sn=$(halcmd getp filestream.sample-num | awk '{print $NF}')
un=$(halcmd getp filestream.underruns  | awk '{print $NF}')
ov=$(halcmd getp filestream.overruns   | awk '{print $NF}')
halcmd stop
kill $SRV 2>/dev/null; wait $SRV 2>/dev/null; SRV=""

# sampleno counts every clocked sample; underruns once per empty clock (12-9=3);
# overruns never (the reader keeps up); and the ring round-trips all 9 samples.
if [ "$sn" = 12 ] && [ "$un" = 3 ] && [ "$ov" = 0 ] && diff -q out.txt capture.golden >/dev/null; then
    echo pass
    exit 0
fi
echo "FAIL sampleno=$sn underruns=$un overruns=$ov"
diff out.txt capture.golden
exit 1
