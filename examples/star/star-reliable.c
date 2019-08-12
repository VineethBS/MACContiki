/**
 * \file
 *         Reliable single-hop unicast example
 * \author
 *         Vineeth B. S.
 */

#include "contiki.h"
#include "net/rime/rime.h"
#include <stdio.h>

/*---------------------------------------------------------------------------*/
PROCESS(mac_star, "MAC with star topology");
AUTOSTART_PROCESSES(&mac_star);
/*---------------------------------------------------------------------------*/
static void
recv_uc(struct runicast_conn *c, const linkaddr_t *from, uint8_t seqno)
{
  printf("reliable unicast message received from %d.%d after %d\n",
	 from->u8[0], from->u8[1], seqno);
}
/*---------------------------------------------------------------------------*/
static void
sent_uc(struct runicast_conn *c, int status, int num_tx)
{
  const linkaddr_t *dest = packetbuf_addr(PACKETBUF_ADDR_RECEIVER);
  if(linkaddr_cmp(dest, &linkaddr_null)) {
    return;
  }
  printf("unicast message sent to %d.%d: status %d num_tx %d\n",
    dest->u8[0], dest->u8[1], status, num_tx);
}
/*---------------------------------------------------------------------------*/
static const struct runicast_callbacks runicast_callbacks = {recv_uc, sent_uc};
static struct runicast_conn ruc;
static uint8_t max_retrx = 5;
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(mac_star, ev, data)
{
  PROCESS_EXITHANDLER(unicast_close(&ruc);)
    
  PROCESS_BEGIN();

  printf("My rime address is %d.%d\n", linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1]);
  runicast_open(&ruc, 146, &runicast_callbacks);

  while(1) {
    static struct etimer et;
    linkaddr_t addr;
    
    etimer_set(&et, CLOCK_SECOND);
    
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

    packetbuf_copyfrom("Hello", 5);
    addr.u8[0] = 1;
    addr.u8[1] = 0;
    if(!linkaddr_cmp(&addr, &linkaddr_node_addr)) {
      runicast_send(&ruc, &addr, max_retrx);
    }

  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
