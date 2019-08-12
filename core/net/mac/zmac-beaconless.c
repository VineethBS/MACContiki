/**
 * \file
 *         Beaconless ZMAC implementation
 *         Modification of TDMA code in Contiki 2.2 by Adam Dunkels
 * \author
 *         Vineeth B. S. <vineethbs@gmail.com>
 */

#include "net/rime/rime.h"
#include "net/mac/nullmac.h"
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

/* TDMA configuration */
#define NR_SLOTS 6
#define SLOT_LENGTH (CLOCK_SECOND/NR_SLOTS)
#define BACKOFF_MAX (SLOT_LENGTH/5)
#define GUARD_PERIOD (SLOT_LENGTH/10)
#define PERIOD_LENGTH CLOCK_SECOND

#define MY_SLOT (linkaddr_node_addr.u8[0] % NR_SLOTS)

static struct ctimer slot_timer;

/*---------------------------------------------------------------------------*/
static struct send_packet_data {
	struct ctimer slot_timer;
	clock_time_t slot_start;
	mac_callback_t sent;
	void *ptr;
} p;

static void
_send_packet(void *ptr)
{
	PRINTF("Beaconless ZMAC : transmitting at %u\n", (unsigned) clock_time());
	struct send_packet_data *d = ptr;
	NETSTACK_RDC.send(d->sent, d->ptr);
}

static void
_zmac_csma(void *ptr)
{
	struct send_packet_data *d = ptr;

	if (NETSTACK_RADIO.channel_clear()) {
		PRINTF("Channel is clear : transmitting at %u\n", (unsigned) clock_time());
		NETSTACK_RDC.send(d->sent, d->ptr);
	} else {
		PRINTF("Channel is not clear\n");
		PRINTF("TIMER Rescheduling until %lu\n", d->slot_start);
		ctimer_set(&d->slot_timer, d->slot_start, _send_packet, &d);
	}
}

static void
send_packet(mac_callback_t sent, void *ptr)
{
	clock_time_t now, rest, period_start, slot_start, backoff_delay;

	p.sent = sent;
	p.ptr = ptr;

	/* Calculate slot start time */
	now = clock_time();
	rest = now % PERIOD_LENGTH;
	period_start = now - rest;
	slot_start = period_start + MY_SLOT*SLOT_LENGTH;

	/* Check if we are inside our slot */
	if(now < slot_start || now > slot_start + SLOT_LENGTH - GUARD_PERIOD) {
		PRINTF("TIMER We are outside our slot: %lu != [%lu,%lu]\n", now, slot_start, slot_start + SLOT_LENGTH);
		while(now > slot_start + SLOT_LENGTH - GUARD_PERIOD) {
			slot_start += PERIOD_LENGTH;
		}
		// Backoff & Channel check - if channel busy then reschedule
		PRINTF("ZMAC Backing off since not owner of slot\n");
		backoff_delay = random_rand() % BACKOFF_MAX;
		ctimer_set(&slot_timer, backoff_delay, _zmac_csma, &p);
	} else {
		if(now > slot_start + SLOT_LENGTH - GUARD_PERIOD) {
			PRINTF("TIMER No more time to transmit\n");
		} else {
			PRINTF("TIMER Inside slot: %lu == [%lu,%lu]\n", now, slot_start, slot_start + SLOT_LENGTH);
			NETSTACK_RDC.send(sent, ptr);
		}
	}
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
const struct mac_driver beaconlessZMAC_driver = {
		"Beaconless ZMAC",
		init,
		send_packet,
		packet_input,
		on,
		off,
		channel_check_interval,
};
/*---------------------------------------------------------------------------*/
