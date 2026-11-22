#!/bin/sh
# usage: ./sweep.sh "-DLPC_X=.. -DLPC_Y=.."
# one line: mean envelope-fit err (partials/grid) over 6 strings x 2 notes; mean steady env err
# (two notes) on string 4 and string 0; attack on string 4: mean rise delta ms, mean |peak dB|, mean |level dB|
OPTS="$1"
make -s sweep OPTS="$OPTS" >/dev/null 2>&1 || { echo "build fail: $OPTS"; exit 1; }
F=$(./lb_sweep fit | grep "^FIT string" | awk '{p+=$7; g+=$10; n++} END {printf "fit %.2f/%.2f", p/n, g/n}')
S=$(./lb_sweep steady 4 | awk '/MEAN/ {printf "%s ", $6}')
S2=$(./lb_sweep steady 0 | awk '/MEAN/ {printf "%s ", $6}')
H=$(./lb_sweep fithi | grep "^FIT string" | awk "{p+=\$7; g+=\$10; n++} END {printf \"hi %.2f/%.2f\", p/n, g/n}")
A=$(./lb_sweep attack 4 | awk "/MEAN delta/ {print \$6, \$12, \$17}")
C=$(./lb_sweep converge 4 | awk "/MEAN\(/ {print \$2, \$3}")
echo "$OPTS | $F | steady s4 $S| s0 $S2| atk(d,pk,lvl) $A | $H | conv0-80ms $C"
