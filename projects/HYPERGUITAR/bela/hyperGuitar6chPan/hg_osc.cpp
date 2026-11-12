/*
 * hg_osc.cpp - UDP/OSC transport for the NEPTR control link.
 *
 * Deliberately raw BSD sockets + oscpkt rather than Bela's OscReceiver/
 * OscSender wrappers. Those wrappers own their own threads and delivery
 * timing; here the whole point is that receive happens on a Bela auxiliary
 * task at a known rate and NEVER on the audio thread, and that is easier to
 * guarantee by owning the socket than by reasoning about someone else's.
 *
 * MESSAGES IN  (Pi -> Bela, port 7772)
 *   /hg/lane  i i i   lane, on, index     - a NEPTR encoder moved
 *   /hg/pedal f       0..1                - expression pedal position
 *   /hg/sync                              - resend every lane
 *
 * MESSAGES OUT (Bela -> Pi 192.168.7.1:7770, NEPTR's existing control port)
 *   /hg/state i i i   lane, on, index     - a lane changed, whoever moved it
 *   /hg/hello i       1                   - heartbeat, once a second
 *
 * The heartbeat is what NEPTR's menu gating keys off. It is sent from the
 * instrument itself rather than inferred on the Pi from ifconfig or lsusb,
 * because what the menu needs to know is not "is a Bela plugged in" but "is
 * hyperGuitar running and listening" - a Bela that is booting, or running some
 * other project, must not produce a NEPTR menu that sends into a void.
 */
#include "hg_link.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <cerrno>

#include <oscpkt.hh>

static int gSock = -1;
static struct sockaddr_in gPiAddr;
static bool gLinkUp = false;

bool hgOscSetup(void)
{
	gSock = socket(AF_INET, SOCK_DGRAM, 0);
	if(gSock < 0) {
		rt_printf("hg-osc: socket() failed: %s\n", strerror(errno));
		return false;
	}

	/*
	 * NON-BLOCKING is not optional. The aux task drains this socket in a loop
	 * until it runs dry; a blocking recvfrom would park that task on an empty
	 * socket and, because Bela aux tasks are real-time scheduled, starve the
	 * ones behind it.
	 */
	int fl = fcntl(gSock, F_GETFL, 0);
	fcntl(gSock, F_SETFL, fl | O_NONBLOCK);

	struct sockaddr_in me;
	memset(&me, 0, sizeof(me));
	me.sin_family = AF_INET;
	me.sin_addr.s_addr = htonl(INADDR_ANY);
	me.sin_port = htons(HG_LISTEN_PORT);
	if(bind(gSock, (struct sockaddr *)&me, sizeof(me)) < 0) {
		rt_printf("hg-osc: bind(%d) failed: %s\n", HG_LISTEN_PORT, strerror(errno));
		close(gSock);
		gSock = -1;
		return false;
	}

	memset(&gPiAddr, 0, sizeof(gPiAddr));
	gPiAddr.sin_family = AF_INET;
	gPiAddr.sin_port = htons(HG_PI_PORT);
	inet_pton(AF_INET, HG_PI_HOST, &gPiAddr.sin_addr);

	gLinkUp = true;
	rt_printf("hg-osc: listening on %d, echoing to %s:%d\n",
			HG_LISTEN_PORT, HG_PI_HOST, HG_PI_PORT);
	return true;
}

void hgOscClose(void)
{
	if(gSock >= 0)
		close(gSock);
	gSock = -1;
	gLinkUp = false;
}

static void hgSend(oscpkt::Message &msg)
{
	if(!gLinkUp)
		return;
	oscpkt::PacketWriter pw;
	pw.addMessage(msg);
	/*
	 * Fire and forget. A dropped state echo is self-healing: the next lane
	 * change re-sends, and NEPTR asks for a full resync on every presence
	 * transition anyway. Retrying here would only add latency to the common
	 * case to paper over a case that already recovers.
	 */
	sendto(gSock, pw.packetData(), pw.packetSize(), 0,
			(struct sockaddr *)&gPiAddr, sizeof(gPiAddr));
}

void hgLaneEmit(int lane)
{
	if(lane < 0 || lane >= HG_NLANES)
		return;
	oscpkt::Message msg("/hg/state");
	msg.pushInt32(lane).pushInt32(gHgLaneOn[lane]).pushInt32(gHgLaneIdx[lane]);
	hgSend(msg);
}

static void hgHello(void)
{
	oscpkt::Message msg("/hg/hello");
	msg.pushInt32(1);
	hgSend(msg);
}

/*
 * Drained from an auxiliary task, never from render(). Handles every pending
 * datagram in one pass so a burst of pedal values cannot build a backlog that
 * makes the pedal lag the foot.
 */
void hgOscTick(void *)
{
	if(!gLinkUp)
		return;

	char buf[1024];
	oscpkt::PacketReader pr;
	/* 2026-11-01 tier-A: at most 32 datagrams per tick so a UDP flood cannot
	 * pin this task; leftovers are drained on the next tick. */
	for(int dg = 0; dg < 32; dg++) {
		ssize_t n = recv(gSock, buf, sizeof(buf), 0);
		if(n <= 0)
			break;                       /* EAGAIN - socket is dry */

		pr.init(buf, (size_t)n);
		oscpkt::Message *m;
		while(pr.isOk() && (m = pr.popMessage()) != 0) {
			int lane, on, idx;
			float pedal;

			if(m->match("/hg/lane").popInt32(lane).popInt32(on)
					.popInt32(idx).isOkNoMoreArgs()) {
				hgLaneApply(lane, on, idx, 1);

			} else if(m->match("/hg/pedal").popFloat(pedal).isOkNoMoreArgs()) {
				if(pedal < 0.0f) pedal = 0.0f;
				if(pedal > 1.0f) pedal = 1.0f;
				gHgPedal = pedal;

			} else if(m->match("/hg/sync").isOkNoMoreArgs()) {
				hgEmitAll();
			}
		}
	}

	/*
	 * Heartbeat. Driven off the task's own call count rather than a wall clock
	 * because the task's rate is fixed and known - and because gettimeofday on
	 * every tick to send one packet a second is work for nothing.
	 */
	static int helloDiv = 0;
	if(++helloDiv >= HG_HELLO_TICKS) {
		helloDiv = 0;
		hgHello();
	}
}
