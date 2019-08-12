/**
 * \file
 *         Beaconless TDMA implementation
 * \author
 *         Vineeth B. S. <vineethbs@gmail.com>
 */

#include "net/mac/nullmac.h"
#include "net/netstack.h"
#include "net/ip/uip.h"
#include "net/ip/tcpip.h"
#include "net/packetbuf.h"
#include "net/netstack.h"
#include "sys/rtimer.h"

/* TDMA configuration */
#define NR_SLOTS 3
#define SLOT_LENGTH (RTIMER_SECOND/NR_SLOTS)
#define GUARD_PERIOD (RTIMER_SECOND/NR_SLOTS/4)

#define MY_SLOT (node_id % NR_SLOTS)
#define PERIOD_LENGTH RTIMER_SECOND

static struct rtimer rtimer;

/*---------------------------------------------------------------------------*/
static struct send_packet_data {
	mac_callback_t sent;
	void *ptr;
};

static struct send_packet_data p;

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
	rtimer_clock_t now, rest, period_start, slot_start;

	p.sent = sent;
	p.ptr = ptr;

	/* Calculate slot start time */
	now = RTIMER_NOW();
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
		r = rtimer_set(&rtimer, slot_start, 1, _send_packet, p);
	}

	if(RTIMER_NOW() > slot_start + SLOT_LENGTH - GUARD_PERIOD) {
			PRINTF("TIMER No more time to transmit\n");
			break;
		}
	NETSTACK_RDC.send(sent, ptr);
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
