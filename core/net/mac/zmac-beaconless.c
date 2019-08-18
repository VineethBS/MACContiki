/**
 * \file
 *         Beaconless ZMAC implementation
 *         Modification of TDMA code in Contiki 2.2 by Adam Dunkels
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
#include "sys/rtimer.h"
#include "sys/clock.h"
#include "lib/random.h"


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

/* ZMAC backoff configuration */
#define BACKOFF_TIME RTIMER_SECOND/CLOCK_SECOND * 2

/* TDMA configuration */
#define NR_SLOTS 6UL
// #define SLOT_LENGTH (CLOCK_SECOND/NR_SLOTS)
#define SLOT_LENGTH 10UL
#define GUARD_PERIOD 1UL
#define PRE_GUARD_PERIOD 1UL
#define PERIOD_LENGTH 60UL

#define MY_SLOT ((linkaddr_node_addr.u8[0] - 1) % NR_SLOTS)

static struct ctimer slot_timer;
uint8_t timer_on = 0;

/*---------------------------------------------------------------------------*/
static void
transmit_packet(void *ptr)
{
	clock_time_t now, rest, period_start, slot_start;

	rtimer_clock_t backoff, backoff_start;
	now = clock_time();
	rest = now % PERIOD_LENGTH;
	period_start = now - rest;
	slot_start = period_start + MY_SLOT*SLOT_LENGTH;
	PRINTF("%d,%lu,%lu,%lu,%lu\n",packet_queued_flag, now,rest,period_start,slot_start);

	/* Check if we are inside our slot */
	if(now < slot_start || now > slot_start + SLOT_LENGTH - GUARD_PERIOD) {
		PRINTF("TIMER We are outside our slot: %lu != [%lu,%lu]\n", now, slot_start, slot_start + SLOT_LENGTH);
		// CSMA here
		if (packet_queued_flag) {
			backoff = random_rand() % (BACKOFF_TIME);
			backoff_start = RTIMER_NOW();
			PRINTF("Backing off %u\n", backoff);
			while (RTIMER_NOW() < backoff_start + backoff) {} // wait for the backoff here
			PRINTF("receiving packet %u\n", NETSTACK_RADIO.channel_clear());
			if (NETSTACK_RADIO.channel_clear()) {
				queuebuf_to_packetbuf(queued_packet);
				PRINTF("TIMER in non-owner slot and transmitting\n");
				NETSTACK_RDC.send(NULL, NULL);
				packet_queued_flag = 0;
			}
			PRINTF("TIMER Rescheduling until next slot at %lu\n", SLOT_LENGTH);
			ctimer_set(&slot_timer, SLOT_LENGTH, transmit_packet, NULL);
			return;
		}
		// If packet is not queued or could not get the channel
		if ((now >= slot_start - PRE_GUARD_PERIOD) && (now < slot_start + SLOT_LENGTH - GUARD_PERIOD)) {
			while (clock_time() < slot_start) {} // just wait for the slot
		} else {
			PRINTF("TIMER Rescheduling until next slot at %lu\n", SLOT_LENGTH);
			ctimer_set(&slot_timer, SLOT_LENGTH, transmit_packet, NULL);
			return;
		}
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
	ctimer_set(&slot_timer, SLOT_LENGTH, transmit_packet, NULL);
}

static struct send_packet_data {
	mac_callback_t sent;
	void *ptr;
} p;

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
		PRINTF("TIMER Starting TDMA timer at %lu\n", SLOT_LENGTH);
		ctimer_set(&slot_timer, SLOT_LENGTH, transmit_packet, NULL);
		timer_on = 1;
	}
	// sent(ptr, MAC_TX_DEFERRED, 1);
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
