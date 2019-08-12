/**
 * \file
 *         Beaconless TDMA implementation
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
#define GUARD_PERIOD (CLOCK_SECOND/NR_SLOTS/4)
#define PERIOD_LENGTH CLOCK_SECOND

#define MY_SLOT (linkaddr_node_addr.u8[0] % NR_SLOTS)

static struct ctimer slot_timer;

/*---------------------------------------------------------------------------*/
static struct send_packet_data {
	mac_callback_t sent;
	void *ptr;
} p;

static void
_send_packet(void *ptr)
{
	PRINTF("Beaconless TDMA : transmitting at %u\n", (unsigned) clock_time());
	struct send_packet_data *d = ptr;
	NETSTACK_RDC.send(d->sent, d->ptr);
}

static void
send_packet(mac_callback_t sent, void *ptr)
{
	PRINTF("My slot is %u, Slot length is %lu, Period is %lu\n", MY_SLOT, SLOT_LENGTH, PERIOD_LENGTH);
	clock_time_t now, rest, period_start, slot_start;

	p.sent = sent;
	p.ptr = ptr;

	/* Calculate slot start time */
	now = CTIMER_NOW();
	rest = now % PERIOD_LENGTH;
	period_start = now - rest;
	slot_start = period_start + MY_SLOT*SLOT_LENGTH;

	/* Check if we are inside our slot */
	if(now < slot_start || now > slot_start + SLOT_LENGTH - GUARD_PERIOD) {
		PRINTF("TIMER We are outside our slot: %u != [%u,%u]\n", now, slot_start, slot_start + SLOT_LENGTH);
		while(now > slot_start + SLOT_LENGTH - GUARD_PERIOD) {
			slot_start += PERIOD_LENGTH;
		}
		PRINTF("TIMER Rescheduling until %u\n", slot_start);
		ctimer_set(&slot_timer, slot_start, _send_packet, &p);
	}

	if(clock_time() > slot_start + SLOT_LENGTH - GUARD_PERIOD) {
			PRINTF("TIMER No more time to transmit\n");
	} else {
		NETSTACK_RDC.send(sent, ptr);
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
const struct mac_driver beaconlessTDMA_driver = {
		"Beaconless TDMA",
		init,
		send_packet,
		packet_input,
		on,
		off,
		channel_check_interval,
};
/*---------------------------------------------------------------------------*/
