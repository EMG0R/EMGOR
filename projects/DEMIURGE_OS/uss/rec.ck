// rec.ck -- offline recorder for uss/check.sh. Taps dac into a wav for N seconds, then exits the VM.
// usage: chuck --silent DemiurgeParams.ck <instrument>.ck rec.ck:<seconds>:<out.wav>
Std.atof( me.arg(0) ) => float secs;
dac => WvOut2 w => blackhole;
me.arg(1) => w.wavFilename;
secs::second => now;
w.closeFile();
Machine.removeAllShreds();
