// selftest: echo every change of /test/ping back with /pout. Run: chuck --silent selftest.ck
// (DemiurgeParams.ck must be added first: the selftest.sh does `chuck --silent DemiurgeParams.ck selftest.ck`)
DemiurgeParams p;
p.init("test", 0);
p.add("/test/ping", 0.1);
p.get("/test/ping") => float last;
p.out("/test/ping", last);
while( true )
{
    p.get("/test/ping") => float v;
    if( v != last ) { v => last; p.out("/test/ping", v); }
    10::ms => now;
}
