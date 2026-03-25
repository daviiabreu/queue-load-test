#ifndef _LWIPOPTS_H
#define _LWIPOPTS_H

#define NO_SYS                      1
#define LWIP_SOCKET                 0
#define LWIP_NETCONN                0
#define LWIP_DHCP                   1
#define LWIP_DNS                    1
#define LWIP_RAW                    1
#define LWIP_TCP                    1
#define LWIP_UDP                    1
#define LWIP_NETIF_HOSTNAME         1
#define LWIP_NETIF_STATUS_CALLBACK  1

#define MEM_SIZE                    4000
#define MEMP_NUM_TCP_PCB            4
#define MEMP_NUM_UDP_PCB            4
#define PBUF_POOL_SIZE              8

#endif
