/*
 * Copyright (c) 2014, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
/**
 * \file
 *         Example file using RPL for a data collection.
 *         Can be deployed in the Indriya or Twist testbeds.
 *
 * \author Simon Duquennoy <simonduq@sics.se>
 */

#include "contiki-conf.h"
#include "net/netstack.h"
#include "net/mac/tsch/tsch-schedule.h"
#include "net/rpl/rpl-private.h"
//#include "net/mac/tsch/tsch-schedule.h"
#include "net/ip/uip-debug.h"
#include "lib/random.h"
#include "net/mac/tsch/tsch-rpl.h"
#include "deployment.h"
#include "simple-udp.h"
#include "tools/orchestra.h"
#include <stdio.h>

#define SEND_INTERVAL   (10*CLOCK_SECOND)
#define UDP_PORT 1234

static struct simple_udp_connection unicast_connection;

/*---------------------------------------------------------------------------*/
PROCESS(unicast_sender_process, "Collect-only Application");
AUTOSTART_PROCESSES(&unicast_sender_process);
/*---------------------------------------------------------------------------*/
static void
receiver(struct simple_udp_connection *c,
         const uip_ipaddr_t *sender_addr,
         uint16_t sender_port,
         const uip_ipaddr_t *receiver_addr,
         uint16_t receiver_port,
         const uint8_t *data,
         uint16_t datalen)
{
  LOGA((void*)data, "App: received");
}
/*---------------------------------------------------------------------------*/
int
can_send_to(uip_ipaddr_t *ipaddr) {
  return uip_ds6_is_addr_onlink(ipaddr)
      || uip_ds6_route_lookup(ipaddr)
      || uip_ds6_defrt_choose(NULL);
}
/*---------------------------------------------------------------------------*/
int
app_send_to(uint16_t id, uint32_t seqno, unsigned int to_send_cnt)
{

  struct app_data data;
  uip_ipaddr_t dest_ipaddr;

  data.magic = UIP_HTONL(LOG_MAGIC);
  data.seqno = UIP_HTONL(seqno);
  data.src = UIP_HTONS(node_id);
  data.dest = UIP_HTONS(id);
  data.hop = 0;

  set_ipaddr_from_id(&dest_ipaddr, id);

  if(can_send_to(&dest_ipaddr)) {
    LOGA(&data, "App: sending");
    simple_udp_sendto(&unicast_connection, &data, sizeof(data), &dest_ipaddr);
    return 1;
  } else {
    data.seqno = UIP_HTONL(seqno + to_send_cnt - 1);
    LOGA(&data, "App: could not send");
    return 0;
  }
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(unicast_sender_process, ev, data)
{
  static struct etimer periodic_timer;
  static struct etimer send_timer;
  uip_ipaddr_t global_ipaddr;
  static unsigned int cnt;
  static unsigned int to_send_cnt;
  static uint32_t seqno;
  // node_id_burn(1) ;  
  PROCESS_BEGIN();

  if(deployment_init(&global_ipaddr, NULL, ROOT_ID)) {
    
  // while(1)
     LOG("App: %u start\n", node_id);
  } else {
    PROCESS_EXIT();
  }
  simple_udp_register(&unicast_connection, UDP_PORT,
                      NULL, UDP_PORT, receiver);
   // LOG("App: 11111\n");

#if WITH_TSCH
#if WITH_ORCHESTRA
  orchestra_init();
   // LOG("App: orchestra\n");
#else
  tsch_schedule_create_minimal();
  //  LOG("App: Scheduleminimal\n");
#endif
#endif
       //  LOG("App: 11111\n");
 //if(node_id != ROOT_ID) {
if(node_id == 2 /*|| node_id == 3 || node_id == 4 ||node_id == 5 ||node_id == 6 ||node_id == 38 ||node_id == 33 ||node_id == 41 ||node_id == 36 ||node_id == 39*/) {
      //   LOG("App: asdfsafs\n");
    etimer_set(&periodic_timer, SEND_INTERVAL);
       //   LOG("App: 123213123132\n");
    while(1) {
     //     LOG("App: sdfasdfasf\n");
      etimer_set(&send_timer, random_rand() % (SEND_INTERVAL));
      PROCESS_WAIT_UNTIL(etimer_expired(&send_timer));
     // LOG("App: 1111123123\n");
      if(default_instance != NULL) {
        to_send_cnt++;
        while(to_send_cnt > 0) {
          seqno = ((uint32_t)node_id << 16) + cnt;
       //    LOG("App: 222222\n");
          if(app_send_to(ROOT_ID, seqno, to_send_cnt)) {
      //   LOG("App: 3333333\n");
            cnt++;
            to_send_cnt--;
            if(to_send_cnt > 0) {
              etimer_set(&send_timer, CLOCK_SECOND);
              PROCESS_WAIT_UNTIL(etimer_expired(&send_timer));
            }
          } else {
            break;
          }
        }
      } else {
        LOG("App: no DODAG    %d\n", node_id);

      }
      PROCESS_WAIT_UNTIL(etimer_expired(&periodic_timer));
      etimer_reset(&periodic_timer);
    }
  }
//LOG("App: processs end\n");
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
