// DemiurgeParams.scd -- join the Demiurge parameter pool from SuperCollider.
// Contract: docs/parameters.md (§1 0..1 floats, §11 two-way).
//
//   "DemiurgeParams.scd".loadRelative;     // or (thisProcess.nowExecutingPath.dirname +/+ "...").load
//   ~dparamInit.("mystage", 0);            // 0 = port from env DEMIURGE_PARAM_PORT
//   ~cut = ~dparam.("/mystage/cutoff", 0.5);   // Bus (server running) or Function
//   ~dparamVal.("/mystage/cutoff")         // ALWAYS works: latest 0..1 value
//   ~dpout.("/mystage/level", 0.3);        // report back: /pout to the pool
//
// ~dparam returns a Function (call .value) when no server is running, and a control Bus
// when Server.default is running (kept updated with .set so you can .map a synth to it).
// In both cases ~dparamVal.(path) reads the value.
// Env: DEMIURGE_PARAM_PORT (listen), DEMIURGE_POOL_HOST (127.0.0.1), DEMIURGE_POOL_PORT (9102).
// Listening uses thisProcess.openUDPPort, so it works without booting a server.
(
~dparamVals = ~dparamVals ? IdentityDictionary.new;
~dparamBuses = ~dparamBuses ? IdentityDictionary.new;

~dparamInit = { |stage, port = 0|
	var envp = "DEMIURGE_PARAM_PORT".getenv;
	var host = "DEMIURGE_POOL_HOST".getenv ? "127.0.0.1";
	var pp = ("DEMIURGE_POOL_PORT".getenv ? "9102").asInteger;
	if(port <= 0) { port = (envp ? "0").asInteger };
	~dparamStage = stage;
	~dparamPool = NetAddr(host, pp);
	~dparamPort = port;
	if(port > 0) {
		thisProcess.openUDPPort(port);
		OSCdef(\demiurgeParam, { |msg|
			var p = msg[1].asSymbol, v = msg[2].asFloat.clip(0, 1);
			if(~dparamVals.includesKey(p)) {   // unknown paths ignored
				~dparamVals[p] = v;
				~dparamBuses[p] !? { |b| b.set(v) };
			};
		}, '/p', recvPort: port);
	} {
		"DemiurgeParams: no port (arg or DEMIURGE_PARAM_PORT)".warn;
	};
};

~dparam = { |path, dflt = 0.0|
	var p = path.asSymbol, bus;
	~dparamVals[p] = dflt;
	if(Server.default.serverRunning) {
		bus = Bus.control(Server.default, 1);
		bus.set(dflt);
		~dparamBuses[p] = bus;
		bus
	} {
		{ ~dparamVals[p] }      // a Function: .value is always the latest 0..1
	}
};

~dparamVal = { |path| ~dparamVals[path.asSymbol] };

~dpout = { |path, v|
	~dparamPool !? { |a| a.sendMsg('/pout', path.asString, v.asFloat) };
};
)
