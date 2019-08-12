/**
 * \file
 *         TDMA with a synchronization beacon implementation (header file)
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

#include <string.h>

#define DEBUG 1
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else /* DEBUG */
#define PRINTF(...)
#endif /* DEBUG */

/*---------------------------------------------------------------------------*/
/* TDMA beacon generation for the COORDINATOR NODE */
#define COORDINATOR_NODE 1
#define BEACON_PERIOD 10 * CLOCK_SECOND
#define BEACON_NODE (linkaddr_node_addr.u8[0] == COORDINATOR_NODE)

static void beacon_broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from) {};
static const struct broadcast_callbacks beacon_broadcast_call = {beacon_broadcast_recv};
static struct broadcast_conn beacon_broadcast;
static struct ctimer beacon_timer;
// Beacon sending is assumed to have the highest priority

static void _send_beacon()
{
	packetbuf_copyfrom("TDMABeacon", 10);
	PRINTF("Beacon sent at %lu\n", clock_time());
	broadcast_send(&beacon_broadcast);
	clock_time_t next_beacon_time = clock_time() + BEACON_PERIOD;
	ctimer_set(&beacon_timer, next_beacon_time, _send_beacon, NULL);
}
/*---------------------------------------------------------------------------*/

/* TDMA beacon reception and synchronization for the non-COORDINATOR nodes */
#define BEACON_NOT_RECEIVED 0
#define BEACON_RECEIVED 1

static unsigned beacon_received_flag;
clock_time_t last_beacon_receive_time;

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
	PRINTF("Beaconed TDMA : transmitting at %u\n", (unsigned) clock_time());
	struct send_packet_data *d = ptr;
	NETSTACK_RDC.send(d->sent, d->ptr);
}

static void
send_packet(mac_callback_t sent, void *ptr)
{
	clock_time_t now, rest, period_start, slot_start;
	int num_periods;

	p.sent = sent;
	p.ptr = ptr;

	/* Calculate slot start time */
	now = clock_time();
	if (beacon_received_flag == BEACON_NOT_RECEIVED) {
		rest = now % PERIOD_LENGTH;
	} else {
		rest = (now - last_beacon_receive_time) % PERIOD_LENGTH;
	}
	period_start = now - rest;
	slot_start = period_start + MY_SLOT*SLOT_LENGTH;
	PRINTF("%lu,%lu,%lu,%lu\n",now,rest,period_start,slot_start);

	/* Check if we are inside our slot */
	if(now < slot_start || now > slot_start + SLOT_LENGTH - GUARD_PERIOD) {
		PRINTF("TIMER We are outside our slot: %lu != [%lu,%lu]\n", now, slot_start, slot_start + SLOT_LENGTH);
		while(now > slot_start + SLOT_LENGTH - GUARD_PERIOD) {
			slot_start += PERIOD_LENGTH;
		}
		PRINTF("TIMER Rescheduling until %lu\n", slot_start);
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
	if (packetbuf_holds_broadcast() &&
			strcmp(packetbuf_dataptr, "TDMABeacon")) {
		beacon_received_flag = BEACON_RECEIVED;
		last_beacon_receive_time = clock_time();
		PRINTF("TDMA Beacon: Received TDMA Beacon, setting receive time to %lu\n", last_beacon_receive_time);
	} else {
		NETSTACK_LLSEC.input();
	}
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
	beacon_received_flag = BEACON_NOT_RECEIVED;

	if (BEACON_NODE) {
		PRINTF("Initializing beacon sending at co-ordinator node");
		broadcast_open(&beacon_broadcast, 129, &beacon_broadcast_call);
		clock_time_t next_beacon_time = clock_time() + BEACON_PERIOD;
		ctimer_set(&beacon_timer, next_beacon_time, _send_beacon, NULL);
		_send_beacon();
	}
}

/*---------------------------------------------------------------------------*/
const struct mac_driver beaconTDMA_driver = {
		"TDMA with Beacon",
		init,
		send_packet,
		packet_input,
		on,
		off,
		channel_check_interval,
};
/*---------------------------------------------------------------------------*/
