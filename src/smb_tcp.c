#include "os_port.h"
#include "board.h"
#include "meter.h"
#include "stdio.h"
#include "string.h"
#include "modbus.h"
#include "smb_tcp.h"


//	extern uint8_t own_hw_adr[];
//	extern uint8_t lhost_name[];
//	extern LOCALM localm[];
extern OsMutex    mut_wfc;
extern OsTaskId   t_smb, t_tcp;
extern uint8_t stcpdebug;
uint8_t sock_mcast, sock_udp, stcpdebug;

#ifdef __FREERTOS
#else
	//uint8_t gwbuf[2100];
	SMB_INFO	session[N_CONNECTION] __attribute__ ((section ("EXT_RAM"), zero_init));
#endif

//extern int modbusSlvProcFrame(uint8_t *prx, uint16_t rxsize, uint8_t *ptx, int proto);
//extern int modbusSlvProcFrame(uint8_t *prb, int nr, uint8_t *ptb, int longFrame);

int modbusSlvChkTcpHeader(uint8_t *pBuf, uint16_t nr)
{
	uint16_t	flen = (uint16_t)pBuf[4]<<8 | pBuf[5];

	/* Checked Minimum Frame Length	*/
	if( nr < 6 ) {
		printf("[ MODBUS TCP SLAVE ] : reveive frame size error ... minimum size!\n");
		return	-1;
	}

	/* Checked Modbus Frame Length	*/
	if( flen != nr-6 ) {
		printf("[ MODBUS TCP SLAVE ] : reveive frame size error ...  frame length\n");
		return	-1;
	}
//	ddprintf("[ MODBUS TCP SLAVE ] : reveive frame ok\n");
	/* Checked Unit Address			*/
	/*
	if( pBuf[6] != addr ) {
		DebugMessage(319, "[ MODBUS TCP SLAVE ] : unit Num. error ... [%d][%d]\n", pBuf[6], addr);
		return	_ERROR;
	}
	*/
	return	0;
}




int modbusMakeTcpFrame(uint8_t *prx, uint8_t *ptx, uint16_t length)
{
	ptx[0] = prx[0];
	ptx[1] = prx[1];
	ptx[2] = prx[2];
	ptx[3] = prx[3];
	ptx[4] = length >> 8;
	ptx[5] = length;

	return (length + 6);
}


int smbTcpProc(uint8_t *prb, int nr, uint8_t *ptb, int *nw, int longFrame)
{
	int ret=-1, i, ix;
	uint16_t seq, len=0;
	
	if (modbusSlvChkTcpHeader(prb, nr) == 0) {
		dprtbuffer(1, "[RX]", prb, nr); 
		len = (uint16_t)prb[0] << 8 | prb[1];
		len = modbusSlvProcFrame(&prb[6], len, &ptb[6], longFrame);
		len = modbusMakeTcpFrame(prb, ptb, len);  
		//nr = readSlaveData(seq, len, largeF, &psrc[6], pdst);
		if (len > 0) {
			Board_LED_Toggle(2);
			dprtbuffer(1, "[TX]", ptb, len); 
			ret = 0;
		}
		else {
			printf("Erron on modbusSlvProcFrame ...\n");
		}	
		*nw = len;
	}
	
	return ret;
}

#ifdef ETH_ENABLE
static int map_session (uint8_t socket) 
{
  int i;

  for (i = 0; i < N_CONNECTION; i++) {
    if (session[i].sock == socket) {
      return (i);
    }
  }
  return -1;
}


static void kill_session (SMB_INFO *pss) 
{

  pss->status =  STATE_IDLE;
	pss->nr = 0;
	pss->nw = 0;
}

void sessionTimer()
{
	int i;
	
	for (i=0; i<N_CONNECTION; i++) {
		if (session[i].pollTimer > 0) {
			session[i].pollTimer--;
		}
	}
}	

int sendData(SMB_INFO *pss)
{
	int maxlen, ret=-1;
	uint8_t *pbuf;
	
	if (tcp_check_send(pss->sock)) {
		// get max buffer size;
		maxlen = tcp_max_dsize(pss->sock);						
		if (maxlen > pss->nw) {
			pbuf = tcp_get_buf(pss->nw);
			memcpy(pbuf, pss->txb, pss->nw);
			tcp_send(pss->sock, pbuf, pss->nw);
			ret = 0;
		}
		else {
			printf("net enough memory ...\n");
		}							
	}
	else {
		printf("not ready to send ...\n");
	}
	return ret;
}	

// This function shall be frequently called from the main loop. 

void smb_tcp(void) 
{
	int 		i, ret;
	SMB_INFO *pss;
	
	for (i=0; i<N_CONNECTION; i++) {
		pss = &session[i];
		
		switch (pss->status) {
		case STATE_IDLE:
			ret = tcp_get_state(pss->sock);
			if (ret == TCP_STATE_FREE || ret == TCP_STATE_CLOSED) {	
				// 접속이 끊어지면 다시 listen 상태로 들어간다
				tcp_listen(pss->sock, pss->port);
			}
			break;
			
		case STATE_ACTIVE:
			if (pss->nr) {
				if (smbTcpProc(pss->rxb, pss->nr, pss->txb, &pss->nw, pss->longFrame) == 0) {
					sendData(pss);
				}
				pss->nr = 0; 
			}
			else {
//				if ((pdb->mbus_protocol == 1) && (pevent->fr != pevent->re) && (pss->pollTimer == 0)) {
//					dprintf("send event data, id=%d, nw=%d\n", i, pss->nw);
//					pss->nw = onionEventFrame(pss->txb);
//					sendData(pss);
//					pevent->re = (pevent->re+1) % MAX_EVENT;
//					pss->pollTimer = 10;
//				}
			}
			break;
		}
	}
}

U16 tcp_callback (U8 soc, U8 event, U8 *ptr, U16 par) 
{
  /* This function is called on TCP event */
	int id = map_session(soc);
	SMB_INFO *pss;
	if (id == -1) 
		return 0;
		
	pss = &session[id];
  switch (event) {
    case TCP_EVT_CONREQ:
      /* Remote host is trying to connect to our TCP socket. */
      /* 'ptr' points to Remote IP, 'par' holds the remote port. */
      /* Return 1 to accept connection, or 0 to reject connection */
			printf("id=%d, ip=%d.%d.%d.%d, port=%d\n", id, ptr[0], ptr[1], ptr[2], ptr[3], ntohs(par));
      return (1);
			
    case TCP_EVT_ABORT:
      /* Connection was aborted */
			printf("TCP_EVT_ABORT, id=%d\n", id);
      break;
			
    case TCP_EVT_CONNECT:
      /* Socket is connected to remote peer. */
			pss->status = STATE_ACTIVE;
			printf("TCP_EVT_CONNECT, id=%d\n", id);
      break;
			
    case TCP_EVT_CLOSE:		
      /* Connection has been closed */
			printf("TCP_EVT_CLOSE, id=%d\n", id);
			kill_session(pss);
      break;
			
    case TCP_EVT_ACK:
      /* Our sent data has been acknowledged by remote peer */
			//dprintf("TCP_EVT_ACK, id=%d\n", id);
      break;
			
    case TCP_EVT_DATA:
      /* TCP data frame has been received, 'ptr' points to data */
      /* Data length is 'par' bytes */	
			if (par >= sizeof(pss->rxb)) {
				printf("TCP_EVT_DATA, Frame Length Too Big, nr=%d, id=%d\n", par, id);
			}
			else {
				//printf("id=%d, nr=%d\n", id, par);
				memcpy(pss->rxb, ptr, par);
				pss->nr = par;
			}
			//os_evt_set(0x1, t_smb);
      break;
  }
  return (0);
}


// ftp server 추가하면 2개의 tcp socket을 점유하며
// socket이 부족할 경우 "sys_error"가 호출되면 blocking 된다 
void init_smb_session()
{	
	int i;
	
	memset(session, 0, sizeof(session));
	
	printf("[[MODBUS TCP PORT = %d]]\n", pComm->tcpPort);
	
	// 0, 1 : standard tcp port
	// 2, 3 : extended tcp port
	for (i=0; i<N_CONNECTION; i++) {
		session[i].port = pComm->tcpPort;	// 502
		session[i].longFrame = 0;
	}
	
	for (i=0; i<N_CONNECTION; i++) {
		session[i].sock = tcp_get_socket (TCP_TYPE_SERVER, 0, 60, tcp_callback);
		if (session[i].sock != 0) {
			if (tcp_listen (session[i].sock, session[i].port) == __TRUE) {
				session[i].status = STATE_IDLE;
			}
		}
  	}
}

#if 0
void getMeterInfoV0(uint8_t *sb) {
	int i;
	memcpy(sb, localm[NETIF_ETH].IpAdr, 4);
	memcpy(sb+4, own_hw_adr, 6);
	strcpy((char *)(sb+10), pdb->comm.host);
}

void getMeterInfoV1(RESP_NETINF *sb) {
	int i;
	strcpy(sb->keystr, "*netinf*");
	memcpy(sb->ip, localm[NETIF_ETH].IpAdr, 4);
	memcpy(sb->mac, own_hw_adr, 6);
	strcpy(sb->host, pdb->comm.host);	
	for (i=0; i<6; i++) {
		sb->sn[i] = pInfo->sn[i];
	}
	sb->hwModel = pInfo->hwModel;
	sb->hwVer = pInfo->hwVer;
	sb->fwVer = pInfo->fwVer;
	sb->fwDate[0] = pInfo->fwBuildYear;
	sb->fwDate[1] = pInfo->fwBuildMon;
	sb->fwDate[2] = pInfo->fwBuildDay;	
}
#endif
// 버전별로 동작한다 
U16 mcast_cb(U8 soc, U8 *rmtip, U16 rmtport, U8 *buf, U16 len) 
{
	uint8_t mcastip[] = {238,0,100,1};	
	int i;
	
//  rip  = rip;
//  rport= rport;
//  len  = len;

  if (soc != sock_mcast) {
    /* Check if this is the socket we are connected to */
    return (0);
  } 

	printf("UDP, ip=%d.%d.%d.%d, rport=%d, len=%d\n", rmtip[0], rmtip[1], rmtip[2], rmtip[3], rmtport, len);
	
	// multicast
	if (strncmp((char *)buf, "*get_network_info*", 18) == 0) {		
		uint8_t *sendbuf=(uint8_t *)udp_get_buf(64); 
		getMeterInfoV0(sendbuf);		
		udp_send(soc, mcastip, rmtport, (U8 *)sendbuf, 64);
	}
	// unicast
	else if (strncmp((char *)buf, "@get_network_info@", 18) == 0) {		
		RESP_NETINF *sendbuf=(RESP_NETINF *)udp_get_buf(64); 
		getMeterInfoV1(sendbuf);		
		udp_send(soc, rmtip, rmtport, (U8 *)sendbuf, sizeof(RESP_NETINF));
	}
	
  return (0);
}


//U16 udp_cb(U8 soc, U8 *rip, U16 rport, U8 *buf, U16 len) 
//{
//	RESP_NETINF *sendbuf;
//	int i;

//  if (soc != sock_udp) {
//    /* Check if this is the socket we are connected to */
//    return (0);
//  }
//  
//	printf("UDP, ip=%d.%d.%d.%d, rport=%d, len=%d\n", rip[0], rip[1], rip[2], rip[3], rport, len);
//		
//	if (strncmp((char *)buf, "*get_network_info*", 18) == 0) {		
//		printf("send ip & mac info ...\n");
//		sendbuf = (RESP_NETINF *)udp_get_buf(64);
//		getMeterInfo(sendbuf);		
//		udp_send(soc, rip, rport, (U8 *)sendbuf, sizeof(RESP_NETINF));
//	}
//  return (0);
//}


// multicast group
void init_mcast_session()
{
	U8 mcgroup[] = {238,0,100,1};
  /* Initialize UDP Socket and start listening */
	
	if (igmp_join(mcgroup) == __TRUE) {
		printf("group : %d.%d.%d.%d\n", mcgroup[0], mcgroup[1], mcgroup[2], mcgroup[3]);
	}
//	else {
//		printf("Failed to join a host group ...\n");
//	}
	
  sock_mcast = udp_get_socket (0, UDP_OPT_SEND_CS | UDP_OPT_CHK_CS, mcast_cb);
  if (sock_mcast != 0) {
    udp_open (sock_mcast, 5050);		
  }
}
#endif

//void init_udp_session() {
//	sock_udp = udp_get_socket (0, UDP_OPT_SEND_CS | UDP_OPT_CHK_CS, udp_cb);
//	if (sock_udp != 0) {
//		udp_open(sock_udp, 5050);
//  }
//	else {
//		printf("Can't open udp_sock() \n");
//	}
//}


//void tcpnet_smbtcp(void)
//{
//	int delay = 5;

//#ifdef	ETH_EVENT
//	while (1) {
//		os_evt_wait_and(0x1, 0xffff);
//		while (main_TcpNet() == __TRUE) {
//		}
//		smb_tcp();
//		incTaskCount(TASK_TCPNET);
//	}	
//#else	
//	while (1) {
//		main_TcpNet();
//		smb_tcp();
//		os_dly_wait(delay);	// ??? loop내에서 호출되지 않는다 
//		//os_tsk_pass();		
//		//incTaskCount(TASK_TCPNET);
//	}	
//#endif	
//}

