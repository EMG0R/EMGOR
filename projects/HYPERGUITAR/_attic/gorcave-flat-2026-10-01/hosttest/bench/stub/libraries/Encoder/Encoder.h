#pragma once
class Encoder {
public:
	enum Rotation { NONE, CW, CCW };
	enum Mode { ANY, RISING_EDGE, FALLING_EDGE };
	void setup(int, Mode){}
	Rotation process(int, int){ return NONE; }
	int get(){ return 0; }
};
