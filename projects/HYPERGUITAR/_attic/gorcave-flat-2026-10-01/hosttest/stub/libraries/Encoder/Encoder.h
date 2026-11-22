/* Host-test stub - type only, never instantiated in tests. */
#ifndef ENCODER_STUB_H
#define ENCODER_STUB_H
class Encoder {
public:
	void setup(unsigned int, unsigned int) {}
	int getPosition() { return 0; }
};
#endif
