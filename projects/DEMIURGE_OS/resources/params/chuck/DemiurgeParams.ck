// DemiurgeParams.ck -- join the Demiurge parameter pool from ChucK.
// Contract: docs/parameters.md (§1 0..1 floats, §11 two-way).
//
//   Machine.add("DemiurgeParams.ck");     // or: chuck DemiurgeParams.ck yourpatch.ck
//   DemiurgeParams p;
//   p.init("mystage", 0);                 // 0 = port from env DEMIURGE_PARAM_PORT
//   p.add("/mystage/cutoff", 0.5);        // declare + default
//   p.get("/mystage/cutoff") => float c;  // 0..1, latest /p from the pool
//   p.out("/mystage/level", 0.3);         // report back: /pout to the pool
//
// Env: DEMIURGE_PARAM_PORT (listen), DEMIURGE_POOL_HOST (127.0.0.1), DEMIURGE_POOL_PORT (9102).
// An explicit port != 0 in init() wins over the env.
public class DemiurgeParams
{
    float vals[0];          // path -> value (associative)
    OscIn in;
    OscMsg msg;
    OscOut outp;
    string stage;
    int port;
    0 => int ready;

    fun int envInt(string name, int dflt)
    {
        Std.getenv(name) => string s;
        if( s == "" ) return dflt;
        return Std.atoi(s);
    }

    fun void init(string stg, int prt)
    {
        stg => stage;
        if( prt <= 0 ) envInt("DEMIURGE_PARAM_PORT", 0) => prt;
        prt => port;
        Std.getenv("DEMIURGE_POOL_HOST") => string host;
        if( host == "" ) "127.0.0.1" => host;
        envInt("DEMIURGE_POOL_PORT", 9102) => int poolPort;
        outp.dest(host, poolPort);
        if( port > 0 )
        {
            port => in.port;
            in.addAddress("/p, s f");
            spork ~ listen();
        }
        else <<< "DemiurgeParams: no port (init arg or DEMIURGE_PARAM_PORT)" >>>;
        1 => ready;
    }

    fun void add(string path, float dflt) { dflt => vals[path]; }

    fun float get(string path)
    {
        if( vals.isInMap(path) ) return vals[path];
        return 0.0;
    }

    fun void out(string path, float v)
    {
        outp.start("/pout");
        outp.add(path);
        outp.add(v);
        outp.send();
    }

    fun void listen()
    {
        while( true )
        {
            in => now;
            while( in.recv(msg) )
            {
                msg.getString(0) => string p;
                msg.getFloat(1) => float v;
                if( v < 0 ) 0 => v;
                if( v > 1 ) 1 => v;
                // only declared parameters are stored; unknown paths are ignored
                if( vals.isInMap(p) ) v => vals[p];
            }
        }
    }
}
