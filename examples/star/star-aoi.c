/**
 * \file
 *         Modification of Best-effort single-hop unicast example for AoI calculation
 * \author
 *         Vineeth B. S.
 */

#include "contiki.h"
#include "net/rime/rime.h"
#include <stdio.h>
#include <string.h>

# define RATE1 CLOCK_SECOND * 4 + random_rand() % (CLOCK_SECOND)
# define RATE2 CLOCK_SECOND * 3 + random_rand() % (CLOCK_SECOND)
# define RATE3 CLOCK_SECOND * 2 + random_rand() % (CLOCK_SECOND)
# define RATE4 CLOCK_SECOND + random_rand() % (CLOCK_SECOND)
# define RATE5 random_rand() % (CLOCK_SECOND)
# define RATE6 random_rand() % (CLOCK_SECOND/2)
# define RATE7 random_rand() % (CLOCK_SECOND/4)
# define RATE8 random_rand() % (CLOCK_SECOND/8)
# define RATE9 random_rand() % (CLOCK_SECOND/16)
# define RATE10 random_rand() % (CLOCK_SECOND/32)
# define RATE11 random_rand() % (CLOCK_SECOND/64)
# define RATE12 random_rand() % (CLOCK_SECOND/128)

static struct payload {
	unsigned int packet_counter;
};

/*---------------------------------------------------------------------------*/
PROCESS(mac_star, "MAC with star topology");
AUTOSTART_PROCESSES(&mac_star);
/*---------------------------------------------------------------------------*/
static void
recv_uc(struct unicast_conn *c, const linkaddr_t *from)
{
	printf("Receive callback\n");
	static struct payload rcv_data;
	packetbuf_copyto(&rcv_data);
	printf("unicast message %u received from %d.%d\n", rcv_data.packet_counter, from->u8[0], from->u8[1]);
}
/*---------------------------------------------------------------------------*/
static void
sent_uc(struct unicast_conn *c, int status, int num_tx)
{
	const linkaddr_t *dest = packetbuf_addr(PACKETBUF_ADDR_RECEIVER);
	if(linkaddr_cmp(dest, &linkaddr_null)) {
		return;
	}
	// printf("unicast message sent to %d.%d: status %d num_tx %d\n", dest->u8[0], dest->u8[1], status, num_tx);
}
/*---------------------------------------------------------------------------*/
static const struct unicast_callbacks unicast_callbacks = {recv_uc, sent_uc};
static struct unicast_conn uc;


/*---------------------------------------------------------------------------*/
PROCESS_THREAD(mac_star, ev, data)
{
	PROCESS_EXITHANDLER(unicast_close(&uc);)
	PROCESS_BEGIN();

	static struct payload pkt_ctr;
	pkt_ctr.packet_counter = 0;

	printf("My rime address is %d.%d\n", linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1]);
	unicast_open(&uc, 146, &unicast_callbacks);

	while(1) {
		static struct etimer et;
		linkaddr_t addr;
		etimer_set(&et, RATE1);

		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
		printf("Packet counter %u\n", pkt_ctr.packet_counter);
		packetbuf_copyfrom(&pkt_ctr, sizeof(pkt_ctr));
		// packetbuf_copyfrom("hello", 5);

		addr.u8[0] = 1;
		addr.u8[1] = 0;
		if(!linkaddr_cmp(&addr, &linkaddr_node_addr)) {
			unicast_send(&uc, &addr);
			pkt_ctr.packet_counter ++;
		}
	}
	PROCESS_END();
}
/*---------------------------------------------------------------------------*/
