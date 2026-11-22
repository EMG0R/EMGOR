# Offline drum/synth measurement (runs on a PC, no board needed)

    cd tools
    echo 'struct LaneSet; int seqPickOctaveOffset(const LaneSet&){return 0;}' > stub.cpp
    g++ -O2 -std=gnu++14 -I.. -I../pmor_core loud.cpp ../pmor_core/drums.cpp ../pmor_core/drum_sounds.cpp ../pmor_core/scale.cpp -o loud && ./loud

Prints, per sound: summed L+R peak, A-weighted RMS ("loud"), plain 150 ms RMS.
Gains in drum_sounds.cpp were set from these (kicks by rms, others by loud, peak cap 1.30).
