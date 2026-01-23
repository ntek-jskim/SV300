#include "os_port.h"
#include "wizchip_conf.h"
#include "wiznet_socket.h"
#include "ssplib.h"
#include "proxy.h"

//static uint8_t wiz_tx[2][260], wiz_rx[2][260];
static uint8_t wiz_tx[2][1096], wiz_rx[2][1096];

extern int build_unsol_frame(uint8_t *tb);
extern void decodeUnsolResp(uint8_t interface, uint8_t *tb, uint8_t *rb, int n);
//extern int decode_frame(int hid, uint8_t *rb, int n, uint8_t *tb) ;
extern void print_buf( char *title, uint8_t *data, int len);

extern int checkSoeQ(uint8_t interface);
extern int readEvent(uint8_t interface, uint8_t *tb);
extern void wakeup_unsol(int interface);
extern void setLedTimer(int port);
extern void enableWdt(int taskId);
extern void incWdtCounter(int taskId);
extern void build_mac(int interface, uint8_t *mac);
extern int smbTcpProc(uint8_t *prb, int nr, uint8_t *ptb, int *nw, int longFrame);
extern uint64_t getSysTick64(void);
extern void resetStart(void);
extern OsTaskId getProxyTaskId();

OsMutex     wiz_mutex;

void wiznet_lock(void) {
	osAcquireMutex(&wiz_mutex);
}

void wiznet_unlock(void) {
	osReleaseMutex(&wiz_mutex);
}

int W5500_Init(int mode)
{
	int retry=0;
	uint8_t tmp;
	uint8_t memsize[2][8] = { { 2, 2, 2, 2, 2, 2, 2, 2 }, { 2, 2, 2, 2, 2, 2, 2, 2 } };

	if (mode == 0)
		osCreateMutex(&wiz_mutex);

	reg_wizchip_cris_cbfunc(wiznet_lock, wiznet_unlock);
	reg_wizchip_cs_cbfunc(wizchip_select, wizchip_deselect);
	reg_wizchip_spi_cbfunc(wizchip_read, wizchip_write);
	reg_wizchip_spiburst_cbfunc(wizchip_burstread, wizchip_burstwrite);

	/* wizchip initialize*/
	if (ctlwizchip(CW_INIT_WIZCHIP, (void*) memsize) == -1) {
		printf("WIZCHIP Initialized fail.\r\n");
		return -1;
	}

	do {
		if (ctlwizchip(CW_GET_PHYLINK, (void*) &tmp) == -1)
			printf("Unknown PHY Link stauts.\r\n");
		else
			break;
		// PHY 상태 검사 지연  추가
		osDelayTask(1000);
	} while (tmp == PHY_LINK_OFF && retry < 3);
	
	printf("W5500 PHY Link stauts = %x\n", tmp);
	return 0;
}


void W5500_IntEnable() {
//	int mask = IK_SOCK_1;
//	GPIO_IntEnable();
//	ctlwizchip(CW_SET_INTRMASK, &mask);
}

#if 1
wiz_NetInfo gWIZNETINFO = {{0x00, 0x08, 0xdc, 0xab, 0xcd, 0xef},
                           {192, 168, 7, 222},
                           {255, 255, 255, 0},
                           {192, 168, 1, 1},
                           {0, 0, 0, 0},
                           NETINFO_STATIC };
#else						   
wiz_NetInfo gWIZNETINFO = {{0x00, 0x09, 0xdc, 0xab, 0xcd, 0xef},
                           {192, 168, 7, 223},
                           {255, 255, 255, 0},
                           {192, 168, 1, 1},
                           {0, 0, 0, 0},
                           NETINFO_STATIC };
#endif

void W5500_NetConfig_Display()
{
	uint8_t tmpstr[6] = {0,};
	wiz_PhyConf *phycfg = (wiz_PhyConf *)tmpstr;
	wiz_NetInfo _netinfo;

	ctlnetwork(CN_GET_NETINFO, (void*) &_netinfo);

	// Display Network Information
	ctlwizchip(CW_GET_ID,(void*)tmpstr);
	// PHY status
	ctlwizchip(CW_GET_PHYCONF, (void*)tmpstr);
	printf("PHY CFG [OPMD=%x, Duplex=%x, Mode=%x, Spd=%x]\n", phycfg->by, phycfg->duplex, phycfg->mode, phycfg->speed);

	if(gWIZNETINFO.dhcp == NETINFO_DHCP) 
		printf("\r\n===== %s NET CONF : DHCP =====\r\n",(char*)tmpstr);
	else 
		printf("\r\n===== %s NET CONF : Static =====\r\n",(char*)tmpstr);
	
	printf(" MAC : %02X:%02X:%02X:%02X:%02X:%02X\r\n", _netinfo.mac[0], _netinfo.mac[1], _netinfo.mac[2], _netinfo.mac[3], _netinfo.mac[4], _netinfo.mac[5]);
	printf(" IP : %d.%d.%d.%d\r\n", _netinfo.ip[0], _netinfo.ip[1], _netinfo.ip[2], _netinfo.ip[3]);
	printf(" GW : %d.%d.%d.%d\r\n", _netinfo.gw[0], _netinfo.gw[1], _netinfo.gw[2], _netinfo.gw[3]);
	printf(" SN : %d.%d.%d.%d\r\n", _netinfo.sn[0], _netinfo.sn[1], _netinfo.sn[2], _netinfo.sn[3]);
	printf("=======================================\r\n");
	
//	while (1) {
//			ctlwizchip(CW_GET_PHYCONF, (void*)tmpstr);
//			printf("PHY CFG [OPMD=%x, Duplex=%x, Mode=%x, Spd=%x]\n", phycfg->by, phycfg->duplex, phycfg->mode, phycfg->speed);
//		
//			ctlwizchip(CW_GET_PHYLINK, (void*)tmpstr);
//			printf("PHY LINK STATUS=%x\n", tmpstr[0]);
//			osDelay(1000);
//	}
}

void W5500_NetConfig(int ifno)
{
	uint8_t mac[6];

#if 1
	uint8_t ip[] = {192, 168, 11, 222};
	uint8_t sn[] = {255, 255, 255, 0};
	uint8_t gw[] = {192, 168, 11, 1};

	/* wizchip netconf */
	build_mac(ifno, mac);
	memcpy(gWIZNETINFO.mac, mac, sizeof(mac));
	memcpy(gWIZNETINFO.ip, ip, sizeof(gWIZNETINFO.ip));
	memcpy(gWIZNETINFO.sn, sn, sizeof(gWIZNETINFO.sn));
	memcpy(gWIZNETINFO.gw, gw, sizeof(gWIZNETINFO.gw));
#else
	uint8_t ip[] = {192, 168, 7, 222};
	uint8_t sn[] = {255, 255, 255, 0};
	uint8_t gw[] = {192, 168, 7, 1};

	build_mac(ifno, mac);
	memcpy(gWIZNETINFO.mac, gWIZNETINFO.mac, sizeof(mac));
	memcpy(gWIZNETINFO.ip, ip, sizeof(gWIZNETINFO.ip));
	memcpy(gWIZNETINFO.sn, sn, sizeof(gWIZNETINFO.sn));
	memcpy(gWIZNETINFO.gw, gw, sizeof(gWIZNETINFO.gw));
#endif

	ctlnetwork(CN_SET_NETINFO, (void *)&gWIZNETINFO);
	W5500_NetConfig_Display();
}

#define	DATA_BUF_SIZE 300

static uint8_t txb[DATA_BUF_SIZE];
static uint8_t rxb[DATA_BUF_SIZE];

int32_t wiznet_modbus_server(uint8_t sn, uint16_t port, uint32_t *rTimer)
{
	int32_t ret, nr, nw, ix = 0;
	uint16_t size = 0, sentsize = 0;
	uint8_t intmask;
	uint8_t sr = getSn_SR(sn);

	switch (sr)
	{
	case SOCK_ESTABLISHED:
		if (getSn_IR(sn) & Sn_IR_CON)
		{
			uint8_t cli_ip[4];
			
			getSn_DIPR(sn, cli_ip);
			printf("==> %d:Connected, %d.%d.%d.%d\n", sn, cli_ip[0], cli_ip[1], cli_ip[2], cli_ip[3]);
			setSn_IR(sn, Sn_IR_CON);
			// reset idle timer
			*rTimer = 0;
		}
		if ((size = getSn_RX_RSR(sn)) > 0)
		{
			size = (size > DATA_BUF_SIZE) ? DATA_BUF_SIZE : size;
			nr = wiznet_recv(sn, rxb, size);

			if (nr <= 0)
				return nr;

			*rTimer = 0;			
			//if (sn == 3) printf("%d:%d bytes received\n", sn, nr);
			if (smbTcpProc(rxb, nr, txb, &nw, 0) == 0)
			{
				ret = wiznet_send(sn, txb, nw);
				if (ret < 0)
				{
					wiznet_disconnect(sn);
					return ret;
				}
			}
		}
		// 2025-1-24, 10s(1000) 간 데이터 수신 없으면 socket 닫는다
		else {
			*rTimer += 1;
			if (*rTimer > 1000) {
				*rTimer = 0;
				printf("wiznet_modbus_server, idle timeout ...\n");
				wiznet_disconnect(sn);
			}
		}
		break;

	case SOCK_CLOSE_WAIT:
		printf("%d:CloseWait\n", sn);
		if ((ret = wiznet_disconnect(sn)) != SOCK_OK)
			return ret;
		//printf("%d:Closed\n", sn);
		break;

	case SOCK_INIT:
		printf("%d:Listen, port [%u]\n", sn, port);
		if ((ret = wiznet_listen(sn)) != SOCK_OK)
			return ret;
		break;

	case SOCK_CLOSED:
		printf("%d:Closed\n", sn);
		if ((ret = wiznet_socket(sn, Sn_MR_TCP, port, 0x00)) == sn)
		{
			printf("> Modbus Socket[%d] : OPEN\n", sn);
		}
		// socket open 실패시 재시작한다 
		else {
			printf("> Modbus Socket[%d] : Failed\n", sn);
			return 0x1234;
		}
		break;

	default:
		break;
	}

	return 1;
}

static uint8_t w5500_buf[N_PROXY_BUF];
static uint8_t dest_ip[] = {192,168,11,200};
static int dest_port = 22;

// buffer가 부족한 경우 nsent가 nw 보다 적다
int wiznet_nsend(int sn, uint8_t *buf, int nw) {
	int nsent, os=0;
	
	while (nw > 0) {
		nsent = wiznet_send(sn, &buf[os], nw);
		if (nsent != nw) {
			//printf("--> l=%d, nsent=%d\n", nw, nsent);
			osDelayTask(1);
		}
		nw -= nsent;
		os += nsent;
	}
	
	return nw == 0 ? 0 : -1; 
}




// 2025-3-25
void W5500_ProxyClient(int sn) 
{
	int32_t ret;
	int cmd;
	size_t length, nw;
   uint16_t received_len;
	int error;
	
	switch (getSn_SR(sn))
	{
	case SOCK_CLOSED:
		wiznet_close(sn);
		error = popSvrData(&cmd, w5500_buf, &length);
		if (error == 0) {			
			if (cmd == PROXY_ACCEPT) {				
				wiznet_socket(sn, Sn_MR_TCP, 22, SF_IO_NONBLOCK); // Non-blocking mode
				wiznet_connect(sn, dest_ip, dest_port);
				printf("-> wiznet_connect ...\n");
			}
		}
		break;

	case SOCK_INIT:
		printf("SOCK_INIT,  wiznet_connect ...\n");
		wiznet_connect(sn, dest_ip, dest_port);
		break;

	case SOCK_ESTABLISHED:
		if (getSn_IR(sn) & Sn_IR_CON)
		{
			setSn_IR(sn, Sn_IR_CON); // 클리어
			printf("-> established ...\n");
		}
		
		error = popSvrData(&cmd, w5500_buf, &length);
		if (error == 0) {
			if (cmd == PROXY_S_TO_C) {
				wiznet_nsend(sn, w5500_buf, length);
//				if (nw != length) {
//					printf("--> wiznet_send, l=%d, nsent=%d\n", length, nw);
//				}
			}
			else if (cmd == PROXY_CLOSE) {
				wiznet_close(sn);
			}
		}

		// 수신 확인 (non-blocking)
		if (isCliQFull() == 0 && (received_len = getSn_RX_RSR(sn)) > 0)
		{			
			//printf("w5500 proxy, recv cli to svr, len=%d\n", received_len);
			if (received_len > N_PROXY_BUF)
				received_len = N_PROXY_BUF;

			ret = wiznet_recv(sn, w5500_buf, received_len);
			if (ret > 0)
			{
				pushCliData(PROXY_C_TO_S, w5500_buf, received_len);
			}
		}
		break;

	case SOCK_CLOSE_WAIT:
		wiznet_disconnect(sn);
		break;

	case SOCK_FIN_WAIT:
	case SOCK_CLOSING:
	case SOCK_TIME_WAIT:
	case SOCK_LAST_ACK:
		// 수동으로 close 시도
		wiznet_close(sn);
		break;
	}
}

static uint32_t _idleTimer[] = {0, 0, 0, 0};
// modbus tcp server 
void W5500_TcpServer(void *arg)
{
	int i, result, reset=0, retry=0;
	uint8_t sn[] = {0, 1, 2, 3};	
	int argv = 0;
	
	setProxyClientTid();
	
w5500_restart:
	W5500_SPI_Init();
	W5500_Init(argv);
	W5500_NetConfig(1);
	
	while (1)
	{
		// 0..2 : modbus server
		for (i=0, reset=0; i<3; i++) {
			result = wiznet_modbus_server(sn[i], 502, &_idleTimer[i]);
			if(result == 0x1234) {
				reset++;
			}
		}
		
		// proxy client
		W5500_ProxyClient(sn[3]);
		
		// 2025-3-18, ESD 인가시 모든 포트 상태가 closed로 읽히며 W5500을 재 초기화 한다 
		if(reset == 4) {
			W5500_SPI_Init();
			if (++retry >= 3) {
				printf("W5000 restart\n");
				argv = 1;	// mutex를 제외한 재 초기화 
				goto w5500_restart;
			}
		}
		else {
			retry = 0;
		}

     	//osDelayTask(10);
		osThreadFlagsWait(0x1, 10);
	}
}


//extern osThreadId_t  tid_cmd_server[];

//void GPIO_IRQ_CB(int arg) {
//   //printf("wiznet IRQ\n");
//   Board_LED_Toggle(1);
//   if (tid_cmd_server[0] != 0) {
//      osThreadFlagsSet(tid_cmd_server[0], 0x1);
//   }
//}

#ifdef UDP_SERVER

void wiznet_server_thread(void *arg) {
	int result, nr, nw, ir, hid = *(int *)arg;	
	uint8_t svr_addr[4];
	uint16_t svr_port;
	//
	uint16_t lport;
	int buflen, sock;
	uint8_t *tb, *rb;
	
#ifdef GEMS9000
#else	
	lport = pnet->lport;
#endif	
	tb = wiz_rx[hid];
	rb = wiz_tx[hid];
	buflen = sizeof(wiz_rx[0]);
	
	sock = hid + 2;
	
	printf("wiznet_server_thread, hid = %d, sock=%d, binding port = %d\n", hid, sock, lport);
	// server socket, BLOCK mode 사용시 api 내부에서 데이터 수신시까지 무한 루프 걸리므로 사용하지 않는다  
	result = wiznet_socket(sock, Sn_MR_UDP, lport, SF_IO_NONBLOCK); //Sn_MR_MULTI | Sn_MR_ND); 
	if (result < 0) {
		printf("--> wiznet_server_thread, Error on wiznet_socket=%d\n", sock);
#ifdef GEMS9000
#else
		LED_On(LED_FAIL);
#endif		
		return;
	}
	
#ifdef GEMS9000
#else	
	enableWdt(TASK_WIZ_SVR);
#endif	
	while(1) {
#ifdef GEMS9000
#else		
		incWdtCounter(TASK_WIZ_SVR);
#endif
		// Interrupt를 clear 한다 
		ir = getSn_IR(sock); 
		if (ir) {
			setSn_IR(sock, ir);
			//printf("--> IR = %x\n", ir);
		}
		nr = wiznet_recvfrom(sock, rb, buflen, svr_addr, &svr_port);
		if (nr <= 0) {
			osDelayTask(1);
		}
		else {
#ifdef GEMS9000
#else			
			opr->lid[hid].receptionTs[1] = tickCount64;
			//printf("wiznet_server_thread, recv frame \n");
//			debugBuffer("[WIZRX]", 4, rb, nr);
			setLedTimer(LED_COMM_1);
#endif
			
#ifdef DEBUG_BUF			
			print_buf("<RX>", rb, nr);
#endif			
//			nw = decode_frame(hid, rb, nr, tb);		
			if (nw > 0) {
				wiznet_sendto(sock, tb, nw, svr_addr, svr_port);			
//				debugBuffer("[WIZTX]", 4, tb, nw);				
#ifdef DEBUG_BUF							
				print_buf("<TX>", tb, nw);
#endif				
			}
		}
	}
}

int wiznet_recvfrom_to(uint8_t sn, uint8_t* buf, uint16_t len, uint8_t* addr, uint16_t* port, int to) {
	int nr;

	while (to > 0) {
		nr = wiznet_recvfrom(sn, buf, len, addr, port);
		if (nr <= 0) {
			osDelayTask(1);
			to--;
		}
		else {
			break;
		}
	} 
	return nr;
}


int wiznet_unsol_socket(int sock, uint8_t *dst_addr, int dst_port) {
	int result;
	
	// 2022-10-6, bug fix
#ifdef MULTICAST		
	// IPV4 Multicast address
	uint8_t dhar[6] = {0x1, 0x00, 0x5e};	
	dhar[3] = dst_addr[1];
	dhar[4] = dst_addr[2];
	dhar[5] = dst_addr[3];

	// unsol client socket
	// dest mac, ip, port
	setSn_DHAR(sock, dhar);
#endif	
	setSn_DIPR(sock, dst_addr);
	setSn_DPORT(sock, dst_port)

	result = wiznet_socket(sock, Sn_MR_UDP, 0, SF_IO_NONBLOCK); //Sn_MR_MULTI | Sn_MR_ND); 
	if (result < 0) {
		printf("--> wiznet_unsol_socket, error on wiznet_socket, sock=%d, error=%d\n", sock, result);
		return result;
	}
	
	// 2022-10-6, bug fix
#ifdef MULTICAST	
	// SEND_MAC 은 Host 가 설정한 Sn_DHAR 을 Destination hardware address 로 하여 Data 를 전송한다.
	setSn_CR(sock,Sn_CR_SEND_MAC);	
#endif	
	
	return sock;
}

int wiznet_sendRecv(int wsock, uint8_t *txb, int nw, uint8_t *rxb, int nr, int to, uint8_t *dst_addr, uint16_t dst_port) {
	uint8_t svr_addr[4];
	uint16_t svr_port;
	int n;
	
	// flush rx buffer
	wiznet_recvfrom(wsock, rxb, nr, svr_addr, &svr_port);
	/*
	if (flInf->modType != MOD_TYPE_DO) {	
		setLedTimer(LED_COMM_1);
	}
	else {
		setLedTimer(LED_COMM_2);
	}
	*/
	if ((n = wiznet_sendto(wsock, txb, nw, dst_addr, dst_port)) <= 0) {
		//printf("wiz_unsol_client, error on sendto, err=%d\n", n);
		return n;
	}
	//printf( "wiznet_sendto, dst=%d.%d.%d.%d, %d\n", dst_addr[0],dst_addr[1], dst_addr[2], dst_addr[3], dst_port);
//	debugBuffer("[WIZTX]", 5, txb, n);
	
	// 다른 io module의 multicast frame도 수신한다 
	if ((n = wiznet_recvfrom_to(wsock, rxb, nr, svr_addr, &svr_port, to)) <= 0) {
		//printf("wiz_unsol_client, error on recvfrom, err=%d\n", nr);
		return 0;
	}	
	else {
//		debugBuffer("[WIZRX]", 5, rxb, n);
#ifdef GEMS9000
#else		
		setLedTimer(LED_COMM_1);
#endif		
//		if (flInf->modType != MOD_TYPE_DO) {	
//			setLedTimer(LED_COMM_1);
//		}
//		else {
//			setLedTimer(LED_COMM_2);
//		}
		return n;
	}
}

#endif
