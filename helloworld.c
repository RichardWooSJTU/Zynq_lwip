/******************************************************************************
*
* Copyright (C) 2009 - 2014 Xilinx, Inc.  All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* Use of the Software is limited solely to applications:
* (a) running on a Xilinx device, or
* (b) that interact with a Xilinx device through a bus or interconnect.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* XILINX  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
* OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*
* Except as contained in this notice, the name of the Xilinx shall not be used
* in advertising or otherwise to promote the sale, use or other dealings in
* this Software without prior written authorization from Xilinx.
*
******************************************************************************/

/*
 * helloworld.c: simple test application
 *
 * This application configures UART 16550 to baud rate 9600.
 * PS7 UART (Zynq) is not initialized by this application, since
 * bootrom/bsp configures it to baud rate 115200
 *
 * ------------------------------------------------
 * | UART TYPE   BAUD RATE                        |
 * ------------------------------------------------
 *   uartns550   9600
 *   uartlite    Configurable only in HW design
 *   ps7_uart    115200 (configured by bootrom/bsp)
 */

#include <stdio.h>
#include "xparameters.h"
#include "xil_printf.h"
#include "netif/xadapter.h"
#include "platform.h"
#include "xil_cache.h"
#include "lwip/dhcp.h"
#include "lwip/err.h"
#include "lwip/udp.h"
#include "zynq_interrupt.h"

void lwip_init();
int start_udp8080();
void lwip_loop();

XScuGic XScuGicInstance;
int data[20000];
unsigned data_cnt = 0;

int main()
{
	//make data packet
	int i = 0;
	for (; i < 20000; ++i) data[i] = i;
	InterruptInit(XPAR_XSCUTIMER_0_DEVICE_ID,&XScuGicInstance);
	lwip_loop();
    cleanup_platform();
    return 0;
}

void
print_ip(char *msg, struct ip4_addr *ip)
{
	xil_printf(msg);
	xil_printf("%d.%d.%d.%d\n\r", ip4_addr1(ip), ip4_addr2(ip), ip4_addr3(ip), ip4_addr4(ip));
}

void
print_ip_settings(struct ip4_addr *ip, struct ip4_addr *mask, struct ip4_addr *gw)
{

	print_ip("Board IP: ", ip);
	print_ip("Netmask : ", mask);
	print_ip("Gateway : ", gw);
}

static struct netif server_netif;
struct netif *echo_netif;
#define PLATFORM_EMAC_BASEADDR XPAR_XEMACPS_0_BASEADDR

void lwip_loop()
{
//	struct ip_addr ipaddr, netmask, gw;
	struct ip4_addr ipaddr, netmask, gw;
	unsigned char mac_ethernet_address[] = { 0x00, 0x0a, 0x35, 0x00, 0x01, 0x02 };// 初始化mac地址
	echo_netif = &server_netif;//
	init_platform();

	//非DHCP模式下，设地址、网关和掩码都为0
	IP4_ADDR(&ipaddr,  192, 168,   0, 107);
	IP4_ADDR(&netmask, 255, 255, 255,  0);
	IP4_ADDR(&gw,      192, 168,   0,  1);


	lwip_init();//初始化

	//设置为echo_netif
	if (!xemac_add(echo_netif, &ipaddr, &netmask,&gw, mac_ethernet_address,PLATFORM_EMAC_BASEADDR)) {
		printf("Error adding N/W interface\n\r");
	}
	//设为默认网卡
	netif_set_default(echo_netif);

	platform_enable_interrupts();
	//打开网口
	netif_set_up(echo_netif);
	print_ip_settings(&ipaddr, &netmask, &gw);
	start_udp8080();
	while(1)
	{
		xemacif_input(echo_netif);
	}


}

static struct udp_pcb *udp8080_pcb=NULL;
struct ip4_addr target_addr;

/*
 * Callback Fun
 */
void udp_recive(void *arg,
		struct udp_pcb *pcb,
		struct pbuf *p_rx,
		struct ip4_addr *addr,
		u16_t port)
{
	char *pData;
	xil_printf("copy data from udp socket %d bytes",p_rx->tot_len);
	if(p_rx != NULL)
	{
		pData = (char *)p_rx->payload;
		u16_t i = 0;
		for (; i < p_rx->len; ++i)
		{
			xil_printf("\n %x",pData[i]);
		}
		pbuf_free(p_rx);
	}
	udp_senddata();
}

/*
 * Test udp send data
 */
#define UDP_SEND_SIZE 4000
void udp_senddata()
{
	struct pbuf *p_tx;

	//初始化pbuf
	p_tx = pbuf_alloc(PBUF_TRANSPORT, UDP_SEND_SIZE, PBUF_POOL);
	memcpy(p_tx->payload, data+data_cnt, UDP_SEND_SIZE);
	data_cnt += 4000;
	//向ip层发送
	struct ip4_addr dst_addr;
	IP4_ADDR(&dst_addr,  192, 168,   0, 100);
	udp_sendto(udp8080_pcb, p_tx, &dst_addr, 8080);
	pbuf_free(p_tx);
}


/*
 * Create new pcb, bind pcb and port, set call back function
 */
int start_udp8080()
{
	err_t err;
	unsigned port = 8080;
	/* Create new pcb, allocate memory space to pcb */
	udp8080_pcb = udp_new();
	if (!udp8080_pcb) {
		xil_printf("Error creating PCB. Out of Memory\n\r");
		return -1;
	}
	/* bind to specified @port */
	err = udp_bind(udp8080_pcb, IP_ADDR_ANY, port);
	xil_printf("IPADDR binded to: %x \n" ,IP_ADDR_ANY->addr);
	if (err != ERR_OK) {
		xil_printf("Unable to bind to port %d: err = %d\n\r", port, err);
		return -2;
	}
	/* Set call back function for udp receive */
	udp_recv(udp8080_pcb, udp_recive, 0);
	IP4_ADDR(&target_addr, 192,168,0,100);
	xil_printf("build!\n");
	return 0;
}
