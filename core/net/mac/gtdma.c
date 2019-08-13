/**
 * \file
 *         TDMA with a synchronization beacon implementation (header file)
 * \author
 *         Govind
 */

/*
1 . Beacon edits are commented in #
2 . guarding is implemented
3 . Probably final implementation
*/

/*

----------------- Issues ------------
Logging into a file is probably not supported as Cfs-Coffee may not be supported in the mote.
Cfs code is error free but is unable to generate a file
*/

#define DEBUG 1     // Use PRINTF to log statements
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

//###############

//using rime fncs
#include "contiki.h"
#include "net/rime/rime.h"

// ##############

//Other header files

#include "net/mac/gtdma.h" //Mac driver header file
#include "net/netstack.h"  //Contiki netstack dependencies
#include "sys/ctimer.h"    //Ctimer contiki
#include "sys/rtimer.h"    //Rtimer contiki
#include "sys/clock.h"     //Clock contiki
#include "sys/node-id.h"   //Use node-id of a node
#include "lib/random.h"
#include <string.h>
#include <stdio.h>

#define SLOT_LENGTH (RTIMER_SECOND/3)         //slot length
#define GUARD_PERIOD (RTIMER_SECOND/12)       //guard period to handle clock drift
#define PERIOD_LENGTH RTIMER_SECOND           //period length which is slotlength * number of slots

uint16_t slotcount=1;         //use a new variable to count the slots (initial value is zero)
uint8_t btimes=0;            //No: of times beacon was received
const uint8_t slotnum=3;     //No: of slots or in other words no: of nodes excluding the beacon
uint8_t num_beacons=0;       //use a variable to count the number of beacons sent by the special node

//Assume we are having three slots as slotnum is defined as three 0,1,2 => no: of slots is slotnum

rtimer_clock_t base_time;                     //store time of reception of beacon as the origin time
rtimer_clock_t wait1;                         //temp delay variable
rtimer_clock_t wait2;                         //temp delay variable
rtimer_clock_t wait3;                         //temp delay variable

//define some variables to keep track of the packets sent and received
uint8_t num_trans = 0;    //Packets transmitted from a node
uint8_t num_recv = 0;     //Packets that a node receives
uint16_t num_step;        //variable used for time - sync

//format of send struct containing a mac callback and a void ptr
//same defn used in anrg tutorials

struct send_struct
{
    mac_callback_t sent;
    void *ptr;
};

static void base_reset(uint16_t x)
{
  base_time=base_time+x*PERIOD_LENGTH;
}

static void trans(void *call_ptr)
{
  struct send_struct *var=call_ptr;
  printf("Transmitted by GTDMA");
  NETSTACK_RDC.send(var->sent,var->ptr);
  num_trans++;
}

//----------------send fnc----------------------//

static void send_packet(mac_callback_t sent,void *ptr)
{
    //Create the necessary callback params
    struct send_struct A;
    A.ptr=ptr;
    A.sent=sent;
    void *callback_pointer=&A;
    //static struct rtimer rt;         //Initiate an rtimer
    unsigned short slotid=node_id;     //Retrieve the rime addr ( node id )

    if(slotid == slotnum+1)
    {
        printf("I transmit the beacon");
        NETSTACK_RDC.send(sent,ptr);     //occasionally sends beacon
    }

    else
    {
        /* 1 . If we have received the beacon proceed after setting origin,
           2 . else wait for beacon */
       if(btimes>0)
       {
          //We have the beacon and can transmit
          rtimer_clock_t present_time=RTIMER_NOW();

           /*
           ---------------CASES-----------------
           1 . slot has arrived
           2 . slot in this period has passed
           3 . slot is yet to come in this period
           */

          //Base translation
          if ( present_time - base_time > PERIOD_LENGTH )
          {
              num_step= ( present_time - base_time )/PERIOD_LENGTH;
              base_reset(num_step);
          }
          else
          {
              /* code */
          }


           slotcount=(present_time-base_time)/SLOT_LENGTH;
           if ( slotcount == (slotid-1))
           {
             if( present_time < ( base_time+( slotid )*SLOT_LENGTH-GUARD_PERIOD) )
               {
                   printf("SLOT ARRIVED ...");
                   trans(callback_pointer);
                   printf("ID: %d Transmit Time @ %u :",slotid,present_time);
               }
             else
               {
                   wait3 = (base_time+PERIOD_LENGTH-present_time) + (slotid-1)*SLOT_LENGTH;
                   printf("ID: %d Transmit Time @ %u :",slotid,present_time+wait3);
                   clock_wait(wait3);
                   trans(callback_pointer);
                   //rtimer_set(&rt, (RTIMER_NOW()+wait3) , 1 , (void (*)(void *))trans , callback_pointer);
               }

           }
           else if ( slotcount < (slotid-1) )
           {
               wait1=base_time+(slotid-1)*SLOT_LENGTH-present_time;
               printf("ID: %d Transmit Time @ %u :",slotid,present_time+wait1);
               clock_wait(wait1);
               trans(callback_pointer);
               //rtimer_set(&rt, (RTIMER_NOW()+wait1) , 1 , (void (*)(void *))trans , callback_pointer);
           }
           else
           {
               wait2=(base_time+PERIOD_LENGTH-present_time) + (slotid-1)*SLOT_LENGTH;
               printf("ID: %d Transmit Time @ %u :",slotid,present_time+wait2);
               clock_wait(wait2);
               trans(callback_pointer);
               //rtimer_set(&rt, (RTIMER_NOW()+wait2) , 1 , (void (*)(void *))trans , callback_pointer);
           }


       }

       else
       {
           printf("Havent received the beacon");
       }

    }
}

//-------------------end------------------------//


//----------------recv fnc----------------------//
static void
packet_input(void)
{
    num_recv++;
    NETSTACK_NETWORK.input();

    char *str=(char *)packetbuf_dataptr();
    uint8_t cmpval=strcmp("Beacon",str);    //Retrieve the string and check if it is a beacon

    if(cmpval==0 && btimes==0)
    {
        base_time=RTIMER_NOW();
        btimes=btimes+1;
        printf("Trigger Beacon @ %u ,sec : %u /n",base_time,RTIMER_SECOND);
    }

    else
    {
        /* Statements if needed */
    }

}

//-------------------end------------------------//

static int
on(void)
{
  return NETSTACK_RDC.on();
}

static int
off(int keep_radio_on)
{
  return NETSTACK_RDC.off(keep_radio_on);
}

static unsigned short
channel_check_interval(void)
{
  return 0;
}

static void
init(void)
{
    // Initialize the mac - driver
}

const struct mac_driver gtdma_driver = {
  "GTDMA",
  init,
  send_packet,
  packet_input,
  on,
  off,
  channel_check_interval,
};
