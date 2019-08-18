/**
 * \file
 *         TDMA with a synchronization beacon implementation (header file)
 *         Modification of TDMA code in Contiki 2.2 by Adam Dunkels
 *         TDMA slotting is done using RTimer while Beaconing is done via CTimer
 * \author
 *         Vineeth B. S. <vineethbs@gmail.com>
 */

#include "net/rime/rime.h"
#include "net/queuebuf.h"
#include "net/mac/nullmac.h"
#include "net/netstack.h"
#include "net/ip/uip.h"
#include "net/ip/tcpip.h"
#include "net/packetbuf.h"
#include "net/netstack.h"
#include "sys/ctimer.h"
#include "sys/clock.h"
#include "sys/rtimer.h"


#include <string.h>

#define DEBUG 1
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else /* DEBUG */
#define PRINTF(...)
#endif /* DEBUG */

/* Buffers for holding the packets ------------------------------------------*/
#define NUM_PACKETS 1 // since we are considering LIFO
static int packet_queued_flag;
static struct queuebuf* queued_packet;

/*---------------------------------------------------------------------------*/
/* TDMA beacon generation for the COORDINATOR NODE */
#define COORDINATOR_NODE 1
#define BEACON_PERIOD 10 * CLOCK_SECOND
#define BEACON_INITIAL_PERIOD CLOCK_SECOND
#define BEACON_NODE (linkaddr_node_addr.u8[0] == COORDINATOR_NODE)

static void beacon_broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from) {};
static const struct broadcast_callbacks beacon_broadcast_call = {beacon_broadcast_recv};
static struct broadcast_conn beacon_broadcast;
static struct ctimer beacon_timer;
// Beacon sending is assumed to have the highest priority

static void _send_beacon()
{
	packetbuf_copyfrom("TDMABeacon", 10);
	PRINTF("Beacon sent to lower layer at %lu\n", clock_time());
	broadcast_send(&beacon_broadcast);
	clock_time_t next_beacon_time = BEACON_PERIOD;
	ctimer_set(&beacon_timer, next_beacon_time, _send_beacon, NULL);
}
/*---------------------------------------------------------------------------*/

/* TDMA beacon reception and synchronization for the non-COORDINATOR nodes */
#define BEACON_NOT_RECEIVED 0
#define BEACON_RECEIVED 1

static unsigned beacon_received_flag = 0;
rtimer_clock_t last_beacon_receive_time;

linkaddr_t beacon_node;

/* TDMA configuration */
#define NR_SLOTS 6
#define SLOT_LENGTH (RTIMER_SECOND/NR_SLOTS/100)
#define GUARD_PERIOD (RTIMER_SECOND/NR_SLOTS/100/10)
#define PERIOD_LENGTH RTIMER_SECOND/100

#define MY_SLOT ((linkaddr_node_addr.u8[0] - 1) % NR_SLOTS)

static struct rtimer slot_timer;
uint8_t timer_on = 0;

/*---------------------------------------------------------------------------*/
static struct send_packet_data {
	mac_callback_t sent;
	void *ptr;
} p;

static void
transmit_packet(void *ptr)
{
	rtimer_clock_t now, rest, period_start, slot_start;

	now = RTIMER_NOW();
	if (beacon_received_flag == BEACON_NOT_RECEIVED) {
		rest = now % PERIOD_LENGTH;
		if (BEACON_NODE) { // the first time the BEACON node just sends the packet hoping that there are no collisions
			beacon_received_flag = BEACON_RECEIVED;
			last_beacon_receive_time = now;
			rest = 0;
		}
	} else {
		if (BEACON_NODE) {
			last_beacon_receive_time = now;
		}
		rest = (now - last_beacon_receive_time) % PERIOD_LENGTH;
	}

	period_start = now - rest;
	slot_start = period_start + MY_SLOT * SLOT_LENGTH;
	PRINTF("%u,%u,%u,%u\n",now,rest,period_start,slot_start);

	/* Check if we are inside our slot */
	if(now < slot_start || now > slot_start + SLOT_LENGTH - GUARD_PERIOD) {
		PRINTF("TIMER We are outside our slot: %u != [%u,%u]\n", now, slot_start, slot_start + SLOT_LENGTH);
		while(now > slot_start + SLOT_LENGTH - GUARD_PERIOD) {
			slot_start += PERIOD_LENGTH;
		}
		PRINTF("TIMER Rescheduling until %u\n", slot_start);
		rtimer_set(&slot_timer, slot_start - clock_time(), 0, transmit_packet, NULL);
		return;
	}

	if(clock_time() > slot_start + SLOT_LENGTH - GUARD_PERIOD) {
		PRINTF("TIMER No more time to transmit\n");
	} else {
		if (packet_queued_flag) {
			queuebuf_to_packetbuf(queued_packet);
			PRINTF("TIMER In slot and transmitting\n");
			NETSTACK_RDC.send(NULL, NULL);
			packet_queued_flag = 0;
		}
	}
	slot_start += PERIOD_LENGTH;
	rtimer_set(&slot_timer, slot_start - clock_time(), 0, transmit_packet, NULL);
}

/*---------------------------------------------------------------------------*/
static void
send_packet(mac_callback_t sent, void *ptr)
{
	p.sent = sent;
	p.ptr = ptr;

	// Step 1: Cleanup the queuebuf
	if (packet_queued_flag) {
		queuebuf_free(queued_packet);
		packet_queued_flag = 0;
	}
	// Step 2: Copy the packetbuf to the queued packet
	queued_packet = queuebuf_new_from_packetbuf();
	if (queued_packet == NULL) {
		packet_queued_flag = 0;
		sent(ptr, MAC_TX_ERR, 1);
	}
	packet_queued_flag = 1;
	// Step 3: Start transmission
	if (!timer_on)
	  {
	    PRINTF("TIMER Starting TDMA timer\n");
	    ctimer_set(&slot_timer, SLOT_LENGTH, transmit_packet, NULL);
	    timer_on = 1;
	  }
	// sent(ptr, MAC_TX_DEFERRED, 1);
}

/*---------------------------------------------------------------------------*/
static void
packet_input(void)
{
	if ((packetbuf_holds_broadcast() && strcmp((char *) packetbuf_dataptr(), "TDMABeacon")) &&
			linkaddr_cmp(&beacon_node, packetbuf_addr(PACKETBUF_ADDR_SENDER)))

	{
		beacon_received_flag = BEACON_RECEIVED;
		last_beacon_receive_time = RTIMER_NOW();
		PRINTF("TDMA Beacon: Received TDMA Beacon, setting receive time to %u\n", last_beacon_receive_time);
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
	packet_queued_flag = 0; // no packets queued when we start

	beacon_node.u8[0] = 1;
	beacon_node.u8[1] = 0;

	PRINTF("%u Slot length %u, Period length %u\n", RTIMER_SECOND, SLOT_LENGTH, PERIOD_LENGTH);
	beacon_received_flag = BEACON_NOT_RECEIVED;

	if (BEACON_NODE) {
		PRINTF("Initializing beacon sending at co-ordinator node\n");
		broadcast_open(&beacon_broadcast, 129, &beacon_broadcast_call);
		clock_time_t next_beacon_time = BEACON_INITIAL_PERIOD;
		ctimer_set(&beacon_timer, next_beacon_time, _send_beacon, NULL);
	}
}

/*---------------------------------------------------------------------------*/
const struct mac_driver beaconTDMA_withrtimer_driver = {
		"TDMA with Beacon",
		init,
		send_packet,
		packet_input,
		on,
		off,
		channel_check_interval,
};
/*---------------------------------------------------------------------------*/
