#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "contiki.h"
#include "contiki-net.h"
#include "rest.h"
#include "buffer.h"

////////////////////////////
#include "dev/leds.h"
////////////////////////////

#define DEBUG 1
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#define PRINT6ADDR(addr) PRINTF("%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x", ((uint8_t *)addr)[0], ((uint8_t *)addr)[1], ((uint8_t *)addr)[2], ((uint8_t *)addr)[3], ((uint8_t *)addr)[4], ((uint8_t *)addr)[5], ((uint8_t *)addr)[6], ((uint8_t *)addr)[7], ((uint8_t *)addr)[8], ((uint8_t *)addr)[9], ((uint8_t *)addr)[10], ((uint8_t *)addr)[11], ((uint8_t *)addr)[12], ((uint8_t *)addr)[13], ((uint8_t *)addr)[14], ((uint8_t *)addr)[15])
#define PRINTLLADDR(lladdr) PRINTF(" %02x:%02x:%02x:%02x:%02x:%02x ",(lladdr)->addr[0], (lladdr)->addr[1], (lladdr)->addr[2], (lladdr)->addr[3],(lladdr)->addr[4], (lladdr)->addr[5])
#else
#define PRINTF(...)
#define PRINT6ADDR(addr)
#define PRINTLLADDR(addr)
#endif

#define SERVER_NODE(ipaddr)   uip_ip6addr(ipaddr, 0xfe80, 0, 0, 0, 0x0212, 0x7401, 0x0001, 0x0101)
#define LOCAL_PORT 61617
#define REMOTE_PORT 61616

char temp[100];
int xact_id;
static uip_ipaddr_t server_ipaddr;
static struct uip_udp_conn *client_conn;
static struct etimer et;
#define MAX_PAYLOAD_LEN   100

#define NUMBER_OF_URLS 3
char* service_urls[NUMBER_OF_URLS] = {"togglered", "toggleblue", "togglegreen"};

static void
response_handler(coap_packet_t* response)
{
  uint16_t payload_len = 0;
  uint8_t* payload = NULL;
  payload_len = coap_get_payload(response, &payload);

  PRINTF("Response transaction id: %u", response->tid);
  if (payload) {
    memcpy(temp, payload, payload_len);
    temp[payload_len] = 0;
    PRINTF(" payload: %s\n", temp);
////////////////////////////////////////////////////////////////////////////////////////////
  if (strncmp(temp, "Light Red\n", temp)==0) {
          leds_on(LEDS_RED);
          } else if(strncmp(temp,"Light Green\n", temp)==0) {
     //       led = LEDS_GREEN;
            leds_on(LEDS_GREEN);
    //        current ++;
            //send_data();
          } else if (strncmp(temp,"Light Blue\n", temp)==0) {
    //        led = LEDS_BLUE;
            leds_on(LEDS_BLUE);
          }
////////////////////////////////////////////////////////////////////////////////////////////
  }
}

static void
send_data(void)
{
  char buf[MAX_PAYLOAD_LEN];

  if (init_buffer(COAP_DATA_BUFF_SIZE)) {
    int data_size = 0;
    int service_id = random_rand() % NUMBER_OF_URLS;
    coap_packet_t* request = (coap_packet_t*)allocate_buffer(sizeof(coap_packet_t));
    init_packet(request);

    coap_set_method(request, COAP_GET);
    request->tid = xact_id++;
    request->type = MESSAGE_TYPE_CON;
    coap_set_header_uri(request, service_urls[service_id]);

    data_size = serialize_packet(request, buf);

    PRINTF("Client sending request to:[");
    PRINT6ADDR(&client_conn->ripaddr);
    PRINTF("]:%u/%s\n", (uint16_t)REMOTE_PORT, service_urls[service_id]);
    uip_udp_packet_send(client_conn, buf, data_size);

    delete_buffer();
  }
}

static void
handle_incoming_data()
{
  PRINTF("Incoming packet size: %u \n", (uint16_t)uip_datalen());
  if (init_buffer(COAP_DATA_BUFF_SIZE)) {
    if (uip_newdata()) {
      coap_packet_t* response = (coap_packet_t*)allocate_buffer(sizeof(coap_packet_t));
      if (response) {
        parse_message(response, uip_appdata, uip_datalen());
        response_handler(response);
      }
    }
    delete_buffer();
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
RESOURCE(led, METHOD_POST | METHOD_PUT , "led");

void
led_handler(REQUEST* request, RESPONSE* response)
{
  char color[10];
  char mode[10];
  uint8_t led = 0;
  int success = 1;

  if (rest_get_query_variable(request, "color", color, 10)) {
    PRINTF("color %s\n", color);

    if (!strcmp(color,"red")) {
      led = LEDS_RED;
    } else if(!strcmp(color,"green")) {
      led = LEDS_GREEN;
    } else if ( !strcmp(color,"blue") ) {
      led = LEDS_BLUE;
    } else {
      success = 0;
    }
  } else {
    success = 0;
  }

  if (success && rest_get_post_variable(request, "mode", mode, 10)) {
    PRINTF("mode %s\n", mode);

    if (!strcmp(mode, "on")) {
      leds_on(led);
    } else if (!strcmp(mode, "off")) {
      leds_off(led);
    } else {
      success = 0;
    }
  } else {
    success = 0;
  }

  if (!success) {
    rest_set_response_status(response, BAD_REQUEST_400);
  }
}


/*A simple actuator example. Toggles the red led*/
RESOURCE(toggle, METHOD_GET | METHOD_PUT | METHOD_POST, "toggle");
void
toggle_handler(REQUEST* request, RESPONSE* response)
{
  leds_toggle(LEDS_RED);
}
///////////////////////////////////////////////////////////////////

PROCESS(coap_client_example, "COAP Client Example");
AUTOSTART_PROCESSES(&coap_client_example);

PROCESS_THREAD(coap_client_example, ev, data)
{
  PROCESS_BEGIN();
////////////////////////////////////////////////////
  rest_activate_resource(&resource_led);
  rest_activate_resource(&resource_toggle);
////////////////////////////////////////////////////
  SERVER_NODE(&server_ipaddr);

  /* new connection with server */
  client_conn = udp_new(&server_ipaddr, UIP_HTONS(REMOTE_PORT), NULL);
  udp_bind(client_conn, UIP_HTONS(LOCAL_PORT));

  PRINTF("Created a connection with the server ");
  PRINT6ADDR(&client_conn->ripaddr);
  PRINTF(" local/remote port %u/%u\n",
  UIP_HTONS(client_conn->lport), UIP_HTONS(client_conn->rport));

  etimer_set(&et, 5 * CLOCK_SECOND);
  while(1) {
    PROCESS_YIELD();
    if (etimer_expired(&et)) {
      send_data();
      etimer_reset(&et);
    } else if (ev == tcpip_event) {
      handle_incoming_data();
    }
  }

  PROCESS_END();
}
