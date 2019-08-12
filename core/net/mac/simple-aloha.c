/**
 * \file
 *         Simple-ALOHA implementation
 * \author
 *         Vineeth B. S. <vineethbs@gmail.com>
 */

#include "net/mac/simple-aloha.h"
#include "net/netstack.h"
#include "net/ip/uip.h"
#include "net/ip/tcpip.h"
#include "net/packetbuf.h"
#include "net/netstack.h"

#include "sys/ctimer.h"
#include "sys/clock.h"

#include "lib/random.h"

#define DEBUG 1
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else /* DEBUG */
#define PRINTF(...)
#endif /* DEBUG */

/*---------------------------------------------------------------------------*/
static struct ctimer transmit_timer;

static struct send_packet_data {
	mac_callback_t sent;
	void *ptr;
};

static struct send_packet_data p;

static void
_send_packet(void *ptr)
{
	PRINTF("Simple-ALOHA : transmitting at %u\n", (unsigned) clock_time());
	struct send_packet_data *d = ptr;
	NETSTACK_RDC.send(d->sent, d->ptr);
}

static void
send_packet(mac_callback_t sent, void *ptr)
{
	p.sent = sent;
	p.ptr = ptr;
	clock_time_t delay = random_rand() % CLOCK_SECOND;
	PRINTF("Simple-ALOHA : at %u scheduling transmission in %u ticks\n", (unsigned) clock_time(),(unsigned) delay);
	ctimer_set(&transmit_timer, delay, _send_packet, &p);
	sent(ptr, MAC_TX_DEFERRED, 1);
}

/*---------------------------------------------------------------------------*/
static void
packet_input(void)
{
	NETSTACK_LLSEC.input();
}
/*---------------------------------------------------------------------------*/
static int
on(void)
{
	return NETSTACK_RDC.on();
}
/*---------------------------------------------------------------------------*/
static int
off(int keep_radio_on)
{
	return NETSTACK_RDC.off(keep_radio_on);
}
/*---------------------------------------------------------------------------*/
static unsigned short
channel_check_interval(void)
{
	return 0;
}
/*---------------------------------------------------------------------------*/
static void
init(void)
{
}
/*---------------------------------------------------------------------------*/
const struct mac_driver simplealoha_driver = {
		"Simple ALOHA",
		init,
		send_packet,
		packet_input,
		on,
		off,
		channel_check_interval,
};
/*---------------------------------------------------------------------------*/
