// selftest: no server is booted. Echo every change of /test/ping back as /pout.
(
(thisProcess.nowExecutingPath.dirname +/+ "DemiurgeParams.scd").load;
~dparamInit.("test", 0);
~dparam.("/test/ping", 0.1);
~last = 0.1;
~dpout.("/test/ping", 0.1);
fork {
	loop {
		var v = ~dparamVal.("/test/ping");
		if(v != ~last) { ~last = v; ~dpout.("/test/ping", v) };
		0.01.wait;
	}
};
)
