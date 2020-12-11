/* 
  * 
  *  kRtspProxyd
  * 
  *  Many basic functions for rtsp parsing
  *
  */

#include <linux/kernel.h>
#include <linux/net.h>
#include <net/sock.h>
#include <linux/inet.h>
#include "proxy.h"
#include "prototypes.h"

#define MAX_SOCK_ADDR 128
int	gUserLimit = 20;
atomic_t	gNumUsers = ATOMIC_INIT(0);
shok	*gShokList = NULL;

static int gUDPPortMin = 2000;
static int gUDPPortMax = 65536;
atomic_t gNextPort = ATOMIC_INIT(-1);
atomic_t gMaxPorts = ATOMIC_INIT(0);
unsigned int gProxyIP; //sefIP


t_cmd_map cmd_map[] = {
	{"DESCRIBE",	ttDescribe},
	{"SETUP",	ttSetup},	
	{"PLAY",	ttPlay},
	{"PAUSE",	ttPause},
	{"STOP",	ttStop},
	{"TEARDOWN",	ttTeardown},
	{"OPTIONS",     ttOptions},
	{"ANNOUNCE",    ttAnnounce},
	{"REDIRECT", 	ttRedirect},
	{"GET_PARAMETER", ttGetParameter},
	{"SET_PARAMETER", ttSetParameter},
	{NULL,		ttNone}
};

char *str_sep(char **stringp, char *delim)
{
	int j, dl, i, sl;
	char *newstring, *ret;

	if (*stringp == NULL)
		return NULL;

	dl = strlen(delim);
	sl = strlen(*stringp);
	newstring = NULL;
	ret = *stringp;

	for (i=0; i<sl; i++) {
		for (j=0; j<dl; j++) {
			if ((*stringp)[i] == delim[j]) {
				(*stringp)[i] = '\0';
				newstring = &((*stringp)[i+1]);
				i = sl; j = dl;
			}
		}
	}

	*stringp = newstring;
	return ret;
}


static inline char to_lower(char c)
{
	if (c >= 'A' && c <= 'Z')
		return ((c - 'A') + 'a');
	return c;
}


int strn_casecmp(char *str1, char *str2, int l)
{
	int	ret;
	
	ret = to_lower(*str1) - to_lower(*str2);
	while (l-- && to_lower(*str1) && to_lower(*str2) && ((ret = to_lower(*str1++) - to_lower(*str2++)) == 0))
		;
	return ret;
}

int is_command(char *inp, char *cmd, char *server)
{
	int 	l;
	char	*p;

	l = strlen(inp);

	if (l < 17)		/* "RTSP/1.0" (8) + " rtsp:// " (9) */
		return 0;
	if (strn_casecmp(&inp[l-8], "RTSP/1.0", 8) != 0)
		return 0;

	p = inp;
	while (*p && (*p != ' '))
		*cmd++ = *p++;
	*cmd = '\0';

	if (strn_casecmp(p, " rtsp://", 8) != 0)
		return 0;

	p += 8;
	while (*p && (*p != '/'))
		*server++ = *p++;
	*server = '\0';

	return 1;
}

/**********************************************/
int has_two_crlfs(char *s)
{
	int		l, n;
	char	*p;
	l = strlen(s);
	if (l < 4)
		return 0;
	n = 3;
	p = s + n;
	while (n < l) {
		if (s[n] != '\n')
			n += 1;
		else if (s[n-1] != '\r' || s[n-2] != '\n' || s[n-3] != '\r')
			n += 2;
		else
			return n+1;
	}
	return 0;
}


void send_rtsp_error(struct socket *sock, int refusal)
{ 

       char *refusal_string;

       EnterFunction("send_rtsp_error");
       
	switch (refusal) {
		case kServerNotFound:
 			refusal_string = "RTSP/1.0 462 Destination unreachable\r\n";
			break;
		case kUnknownError:
 			refusal_string = "RTSP/1.0 500 Unknown proxy error\r\n";
			break;
		case kPermissionDenied:
 			refusal_string = "RTSP/1.0 403 Proxy denied\r\n";
			break;
		case kTooManyUsers:
 			refusal_string = "RTSP/1.0 503 Too many proxy users\r\n";
			break;
		default:
 			refusal_string = "RTSP/1.0 500 Unknown proxy error\r\n";
			break;
	}

	(void)SendBuffer(sock, refusal_string, strlen(refusal_string));
	LeaveFunction("send_rtsp_error");
}

int str_casecmp(char *str1, char *str2)
{
	int	ret;
	
	ret = *str1 - *str2;
	while (*str1 && *str2 && ((ret = *str1++ - *str2++) == 0))
		;
	return ret;
}

int cmd_to_transaction_type(char *cmd)
{
	t_cmd_map	*map;
	map = cmd_map;
	while (map->cmd != NULL) {
		if (str_casecmp(map->cmd, cmd) == 0)
			return map->type;
		map++;
	}
	return ttNone;
}

/**********************************************/
int has_trackID(char *inp, int *trackID)
{
	int 	l;
	char	*p;
	l = strlen(inp);

	if (l < 18)		/* "RTSP/1.0" (8) + "trackID=n " (10) */
		return 0;
	if (strn_casecmp(inp + l - 8, "RTSP/1.0", 8) != 0)
		return 0;

	p = inp;
	while (p) {
		p = strchr(p, '=');
		if (p - 7 < inp) {
			p++;
			continue;
		}
		if (strn_casecmp(p - 7, "trackid=", 8) != 0) {
			p++;
			continue;
		}
		*trackID = atoi(p + 1);
		return 1;
	}
	return 0;
}


int track_id_to_idx(rtsp_session *s, int id)
{
	int i;
	for (i=0; i<s->numTracks; i++) {
		if (s->trk[i].ID == id)
			return i;
	}
	return -1;
}

/**********************************************/
int has_client_port(char *inp, unsigned short *port)
{
	int		l;
	char	*p;
	l = strlen(inp);

	if (l < 23)		/* "Transport:<>client_port=n" (23) */
		return 0;
	if (strn_casecmp(inp, "transport", 9) != 0)
		return 0;
	
	p = inp;
	while (p) {
		p = strchr(p, '=');
		if (p - 11 < inp) {
			p++;
			continue;
		}
		if (strn_casecmp(p - 11, "client_port=", 12) != 0) {
			p++;
			continue;
		}
		*port = atoi(p + 1);
		*++p = '\0';
		return 1;
	}
	return 0;
}


ipList *find_ip_in_list(ipList *list, unsigned int ip)
{
	ipList *cur = list;

      EnterFunction("find_ip_in_list");

	while (cur) {
	
		if (cur->ip == ip) {
			return cur;
		}
		cur = cur->next;
	}
      LeaveFunction("find_ip_in_list");
      return NULL;
}

shok *find_available_shok(unsigned int fromIP, unsigned int toIP, int withSib, const int CPUNR)
{	
	shok	*cur = threadinfo[CPUNR].gShokQueue;

	while (cur) {
		KRTSPROXYD_OUT(KERN_INFO " looking for IP %xin shok\n", toIP);
		if (find_ip_in_list(cur->ips, toIP) == NULL) {
			if (withSib) {
				KRTSPROXYD_OUT(KERN_INFO "looking for IP %x in SIB shok\n", toIP);
				if (find_ip_in_list(cur->sib->ips, toIP) == NULL)
					return cur;
			}
			else
				return cur;
		}
		cur = cur->next;
	}

	return NULL;
}

int add_ip_to_list(ipList **list, unsigned int ip)
{
	ipList	*newEl;

	newEl = (ipList*)kmalloc(sizeof(ipList), GFP_KERNEL);
	if (!newEl)
		return 0;
	newEl->ip = ip;
	newEl->what_to_do = NULL;
	newEl->what_to_do_it_with = NULL;
	newEl->next = *list;
	*list = newEl;

	return 1;
}

 struct socket *new_socket_tcp(void)
{
	struct socket *sock;
	int err;
	
       err = sock_create(PF_INET, SOCK_STREAM, IPPROTO_TCP, &sock);
      	
	if (err<0) 
       {
	     KRTSPROXYD_OUT(KERN_ERR "Error during creation of tcp socket; terminating\n");
	     sock_release(sock);
	     return NULL;
	}    
	atomic_inc(&gMaxPorts);
	return sock;
}

void set_socket_reuse_address(struct socket *skt)
{
	int i = 1;
	skt->sk->prot->setsockopt(skt->sk, SOL_SOCKET, SO_REUSEADDR, (char*)&i, sizeof(i));
}

/**********************************************/
static struct socket *new_socket_udp(void)
{
	struct socket *sock = NULL;
	int err;
	
       err = sock_create(PF_INET, SOCK_DGRAM, IPPROTO_UDP, &sock);
	if (err<0) 
       {
	     KRTSPROXYD_OUT(KERN_ERR "Error during creation of udp socket; terminating\n");
	     sock_release(sock);
	     return NULL;
	}    
	atomic_inc(&gMaxPorts);
	return sock;
}

int bind_socket_to_address(struct socket *skt, unsigned int address, unsigned short port, int is_listener)
{
	struct sockaddr_in sin;
	int err;

	if (address == (unsigned int)0xFFFFFFF)
		address = INADDR_ANY;
		
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	sin.sin_addr.s_addr = address; //already is network byte order!

	err = skt->ops->bind(skt,(struct sockaddr*)&sin,sizeof(sin));
	if (err < 0)
	{
		KRTSPROXYD_OUT(KERN_ERR "kRtspProxyd: Error binding socket. This means that some other \n");
		KRTSPROXYD_OUT(KERN_ERR "        daemon is (or was a short time ago) using port %i.\n",port);
		return 0;	
	}

	/* Grrr... setsockopt() does this. */
//	skt->sk->reuse   = 1; //success!!
       return 1;
}

//fromIp and toIp are all NBO
shok *make_new_shok(unsigned int fromIP, unsigned int toIP, int withSib, const int CPUNR)
{
	shok *theShok1 = NULL, *theShok2 = NULL;
	struct socket *skt1 = NULL, *skt2 = NULL;
	unsigned short port1 = (unsigned short)0xFFFF, port2 = (unsigned short)0xFFFF;

	theShok1 = (shok*)kmalloc(sizeof(shok), GFP_KERNEL);
	if (!theShok1)
		goto bail_error;
	if (withSib) {
		theShok2 = (shok*)kmalloc(sizeof(shok), GFP_KERNEL);
		if (!theShok2)
			goto bail_error;
	}

	if (atomic_read(&gNextPort) == -1)
		atomic_set(&gNextPort, gUDPPortMin);

retry:

	if ((skt1 = new_socket_udp()) == NULL)
		goto bail_error;

       	if ((atomic_read(&gNextPort) & 0x1) && withSib) //odd number
			atomic_inc(&gNextPort);
		if (atomic_read(&gNextPort) > gUDPPortMax)
          		atomic_set(&gNextPort, gUDPPortMin);
 while (bind_socket_to_address(skt1, fromIP, port1 = atomic_read(&gNextPort), 0) != 1)
 	{
 	    atomic_inc(&gNextPort);
 	    if ((atomic_read(&gNextPort) & 0x1) && withSib) //odd number
			atomic_inc(&gNextPort);
		if (atomic_read(&gNextPort) > gUDPPortMax)
          		atomic_set(&gNextPort, gUDPPortMin);
 	}
 atomic_inc(&gNextPort);

 if (withSib) {
       	if ((skt2 = new_socket_udp()) == NULL)
			goto bail_error;

		if (bind_socket_to_address(skt2, fromIP, port2 = atomic_read(&gNextPort), 0) != 1) {
			sock_release(skt1);
			sock_release(skt2);
			skt1 = NULL;
			skt2 = NULL;
			atomic_inc(&gNextPort);
			goto retry;
		}
		else atomic_inc(&gNextPort);
	}

       make_socket_nonblocking(skt1);
	theShok1->socket = skt1;
	theShok1->port = port1;
	theShok1->ips = NULL;

	if (withSib) {
              make_socket_nonblocking(skt2);

		theShok2->socket = skt2;
		theShok2->port = port2;
		theShok2->ips = NULL;
		theShok2->sib = theShok1;

		theShok1->sib = theShok2;
		theShok1->next = theShok2;
		theShok2->next = threadinfo[CPUNR].gShokQueue;
	}
	else {
		theShok1->sib = NULL;
		theShok1->next = threadinfo[CPUNR].gShokQueue;
	}

	add_ips_to_shok(theShok1, fromIP, toIP, withSib);
	threadinfo[CPUNR].gShokQueue = theShok1;

	return theShok1;	

bail_error:
	sock_release(skt1);
	sock_release(skt2);
	if (theShok1 != NULL)
		kfree(theShok1);
	if (theShok2 != NULL)
		kfree(theShok2);
	return NULL;
}

/**********************************************/
int add_ips_to_shok(shok *theShok, unsigned int fromIP, unsigned int toIP, int withSib)
{
	add_ip_to_list(&(theShok->ips), toIP);
	if (withSib)
		add_ip_to_list(&(theShok->sib->ips), toIP);
	return 1;
}

/**********************************************/
int make_udp_port_pair(unsigned int fromIP, unsigned int toIP, shok **rtpSocket, shok **rtcpSocket, const int CPUNR)
{
	shok	*theShok;

	KRTSPROXYD_OUT(KERN_INFO "MAKE_UDP_PORT_PAIR from %x to %x\n", fromIP, toIP);

	if ((theShok = find_available_shok(fromIP, toIP, 1, CPUNR)) != NULL) {
		
		add_ips_to_shok(theShok, fromIP, toIP, 1);
	}
	else {
		theShok = make_new_shok(fromIP, toIP, 1, CPUNR);

	}

	if (theShok && theShok->sib) {
		*rtpSocket = theShok;
		*rtcpSocket = theShok->sib;
		return 1;
	}
	else
		return -1;
}

int has_content_length(char *inp, int *len)
{
	int		l;
	char	*p;
	l = strlen(inp);

	if (l < 16)		/* "Content-Length:n" (16) */
		return 0;
	if (strn_casecmp(inp, "content-length", 14) != 0)
		return 0;
	p = strchr(inp, ':');
	p++;
	while (*p && (*p == ' '))
		p++;
	if (p) {
		*len = atoi(p);
		return 1;
	}
	else
		return 0;
}

int has_sessionID(char *inp, char *sessionID)
{
	int		l;
	char	*p;
	l = strlen(inp);

	if (l < 9)		/* "Session:x" (9) */
		return 0;
	if (strn_casecmp(inp, "session", 7) != 0)
		return 0;
	p = strchr(inp, ':');
	p++;
	while (*p && (*p == ' '))
		p++;
	if (p) {
		strcpy(sessionID, p);
		return 1;
	}
	else
		return 0;
}


int upon_receipt_from(shok *theShok, int fromIP, do_routine doThis, void *withThis)
{
	ipList	*listEl;
	KRTSPROXYD_OUT(KERN_INFO "UPON_RECEIPT_FROM %x\n", fromIP);
	listEl = find_ip_in_list(theShok->ips, fromIP);
	if (!listEl)
		return -1;
	listEl->what_to_do = doThis;
	listEl->what_to_do_it_with = withThis;
	return 0;
}

/**********************************************/
int has_IN_IP(char *inp, char *str)
{
	int		l;
	char	*p;
	l = strlen(inp);

	if (l < 10)		/* "c=IN IP4 n" (10) */
		return 0;
	if (strn_casecmp(inp, "c=IN IP4 ", 9) != 0)
		return 0;
	p = inp + 9;
	while (*p && (*p == ' '))
		p++;

	while (*p && ((*p >= '0' && *p <= '9') || *p == '.'))
		*str++ = *p++;
	*str = '\0';
	return 1;
}

char *source_eq_string(char *inp)
{
	int		l;
	char	*p;
	l = strlen(inp);

	if (l < 7)			/* source= */
		return NULL;
	p = inp;
	while (p) {
		p = strchr(p, '=');
		if (p - 6 < inp) {
			p++;
			continue;
		}
		if (strn_casecmp(&p[-6], "source=", 7) != 0) {
			p++;
			continue;
		}
		return &p[-6];
	}
	return NULL;
}

int transfer_data(void *refCon, char *buf, int bufSize)
{
	trans_pb	*tpb = (trans_pb*)refCon;
	int			ret = 0;
	int                  isRTCP = 0;
       struct sockaddr_in sin;
	if (!tpb)
		return -1;
	
	sin.sin_family = AF_INET;
	sin.sin_port = htons(tpb->send_to_port);
	sin.sin_addr.s_addr = tpb->send_to_ip;

	if (strstr(tpb->socketName,"RTCP"))
	{	
		isRTCP = 1;
	}
        

	ret = UdpSendBuffer(tpb->send_from->socket, buf, bufSize, O_NONBLOCK, (struct sockaddr *)&sin, sizeof(sin));
       KRTSPROXYD_OUT(KERN_INFO "Sent %d bytes to %s on port %d\n", ret, ntoa(tpb->send_to_ip), tpb->send_to_port);
	return ret;
}

int has_ports(char *inp, unsigned int *client_port, unsigned int *server_port)
{
	int		l, got_server = 0, got_client = 0;
	char	*p;
	l = strlen(inp);

	if (l < 40)		/* "Transport:<>client_port=n-nserver_port=n-n" (40) */
		return 0;
	if (strn_casecmp(inp, "transport", 9) != 0)
		return 0;

	p = inp;
	while (p && !(got_client && got_server)) {
		p = strchr(p, '=');
		if (p - 11 < inp) {
		}
		else if (strn_casecmp(&p[-11], "client_port=", 12) == 0) {
			got_client = 1;
			*client_port = atoi(&p[1]);
		}
		else if (strn_casecmp(&p[-11], "server_port=", 12) == 0) {
			got_server = 1;
			*server_port = atoi(&p[1]);
		}
		p++;
	}
	if (got_client && got_server)
		return 1;
	else
		return 0;
}

int connect_to_address(struct socket *skt, unsigned int address, unsigned port)
{
	struct sockaddr_in sin;
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port); //fixme?
	sin.sin_addr.s_addr = address; //already network byte order!!
        KRTSPROXYD_OUT(KERN_INFO "connect to server-addr:port %s, %d\n",ntoa(address),port);
	return skt->ops->connect(skt, (struct sockaddr*)&sin, sizeof(sin), 0);
}

/*
 *	Send a datagram to a given address. We move the address into kernel
 *	space and check the user space data area is readable before invoking
 *	the protocol.
 */

int UdpSendBuffer(struct socket *sock, void * buff, size_t len, unsigned flags,
			   struct sockaddr *addr, int addr_len)
{
	int err = -1;
	struct msghdr msg;
	struct iovec iov;
	mm_segment_t oldfs;
	
	if (!sock)
		goto out;
	iov.iov_base=buff;
	iov.iov_len=len;
	msg.msg_name=0;
	msg.msg_iov=&iov;
	msg.msg_iovlen=1;
	msg.msg_control=NULL;
	msg.msg_controllen=0;
	msg.msg_namelen=0;
	msg.msg_name=(void *)addr;
	msg.msg_namelen=(__kernel_size_t)addr_len;
        msg.msg_flags = MSG_DONTWAIT;

	oldfs = get_fs(); set_fs(KERNEL_DS);
       err = sock_sendmsg(sock, &msg, len);
       set_fs(oldfs);

out:
	return err;
}


/**********************************************/
int remove_shok(shok *theShok, int withSib, const int CPUNR)
{
	shok	*cur = NULL, *last;
	//shok	*curSib = NULL;

	cur = threadinfo[CPUNR].gShokQueue;
	if (cur == theShok) {
		threadinfo[CPUNR].gShokQueue = cur->next;
		goto doSib;
	}

	last = cur;
	cur = cur->next;
	while (cur) {
		if (cur == theShok) {
			last->next = cur->next;
			goto doSib;
		}
		last = cur;
		cur = cur->next;
	}
	return 0;

doSib:
	if (cur->sib)
		cur->sib->sib = NULL;
	if (withSib && cur->sib)
		remove_shok(cur->sib, 0, CPUNR);

	{
		ipList *ipn, *ipl = cur->ips;
		while (ipl) {
			ipn = ipl->next;
			kfree(ipn);
			ipl = ipn;
		}
	}
	sock_release(cur->socket);
	kfree(cur);
	return 1;
}


/**********************************************/
int remove_ip_from_list(ipList **list, int ip)
{
	ipList	*last, *theEl = *list;

	if (theEl->ip == ip) { //fix me!! nbo or hbo?
		*list = theEl->next;
		kfree(theEl);
		return 1;
	}

	last = theEl;
	theEl = theEl->next;
	while (theEl) {
		if (theEl->ip == ip) {
			last->next = theEl->next;
			kfree(theEl);
			return 1;
		}
		last = theEl;
		theEl = theEl->next;
	}
	return 0;
}

/**********************************************/
void remove_shok_ref(shok *theShok, unsigned int fromIP, unsigned int toIP, int withSib, const int CPUNR)
{
	remove_ip_from_list(&(theShok->ips), toIP);
	if (withSib)
		remove_ip_from_list(&(theShok->sib->ips), toIP);
	if (theShok->sib->ips == NULL)
		remove_shok(theShok->sib, 0, CPUNR);
	if (theShok->ips == NULL)
		remove_shok(theShok, 0, CPUNR);
}

/*
 *	Receive a frame from the socket and optionally record the address of the 
 *	sender. We verify the buffers are writable and if needed move the
 *	sender address from kernel to user space.
 */

int UdpReceiveBuffer(struct socket *sock, void * ubuf, size_t size, unsigned flags, struct sockaddr *addr, int *addr_len)
{
	struct iovec iov;
	struct msghdr msg;
	char address[MAX_SOCK_ADDR];
	int err = -1;
	mm_segment_t	oldfs;

	if (!sock)
		goto out;

	msg.msg_control=NULL;
	msg.msg_controllen=0;
	msg.msg_iovlen=1;
	msg.msg_iov=&iov;
	iov.iov_len=(size_t)(size-1);
	iov.iov_base=(void *)ubuf;
	msg.msg_name= address;
	msg.msg_namelen= MAX_SOCK_ADDR;
	msg.msg_flags = 0;    
 
        oldfs = get_fs(); set_fs(KERNEL_DS);
	err = sock_recvmsg(sock, &msg, size-1, MSG_DONTWAIT);
	set_fs(oldfs);
	if(err >= 0)
         {
          memcpy((void *)addr, (void *)address, *addr_len);
          KRTSPROXYD_OUT(KERN_INFO "the FromIP is %s\n", ntoa(((struct sockaddr_in *)addr)->sin_addr.s_addr)); 
          }  
out:
	return err;
}


/**********************************************/
rtsp_session *new_session(void)
{
	rtsp_session	*s;
	int			  i;

	s = (rtsp_session*)kmalloc(sizeof(rtsp_session), GFP_KERNEL);
	if (s) {
		s->next = NULL;
		s->die = 0; //false
         	s->client_skt = NULL;
                s->new_session = 1;
		s->client_ip = (unsigned int)0xFFFFFFFF; //255.255.255.255 invalid address
		s->server_address = NULL;
		s->server_skt = NULL;
		s->server_ip = (unsigned int)0xFFFFFFFF;
		s->server_port = 554;
		s->server_skt_pending_connection = 0;//false
		s->state = stRecvClientCommand; //initialize the status
		s->transaction_type = ttNone;
		s->sessionID = NULL;

		s->cur_trk = 0;
		for (i=0; i<MX_TRACKS; i++) {
			s->trk[i].ID = 0;
			s->trk[i].ClientRTPPort = (unsigned short)0xFFFF;
			s->trk[i].ServerRTPPort = (unsigned short)0xFFFF;
			s->trk[i].RTP_S2P = NULL;
			s->trk[i].RTCP_S2P = NULL;
			s->trk[i].RTP_P2C = NULL;
			s->trk[i].RTCP_P2C = NULL;
			s->trk[i].RTP_S2C_tpb.status = NULL;
			s->trk[i].RTP_S2C_tpb.send_from = NULL;
			s->trk[i].RTP_S2C_tpb.send_to_ip = (unsigned int)0xFFFFFFFF;
			s->trk[i].RTP_S2C_tpb.send_to_port = (unsigned short)0xFFFF;
			s->trk[i].RTP_S2C_tpb.packetSendCount = 0;
			s->trk[i].RTP_S2C_tpb.nextDropPacket = 0;
			s->trk[i].RTP_S2C_tpb.droppedPacketCount = 0;

			s->trk[i].RTCP_S2C_tpb.status = NULL;
			s->trk[i].RTCP_S2C_tpb.send_from = NULL;
			s->trk[i].RTCP_S2C_tpb.send_to_ip = (unsigned int)0xFFFFFFFF;
			s->trk[i].RTCP_S2C_tpb.send_to_port = (unsigned short)0xFFFF;
			s->trk[i].RTCP_S2C_tpb.packetSendCount = 0;
			s->trk[i].RTCP_S2C_tpb.nextDropPacket = 0;
			s->trk[i].RTCP_S2C_tpb.droppedPacketCount = 0;

			s->trk[i].RTCP_C2S_tpb.status = NULL;
			s->trk[i].RTCP_C2S_tpb.send_from = NULL;
			s->trk[i].RTCP_C2S_tpb.send_to_ip = (unsigned int)0xFFFFFFFF;
			s->trk[i].RTCP_C2S_tpb.send_to_port = (unsigned short)0xFFFF;
			s->trk[i].RTCP_C2S_tpb.packetSendCount = 0;
			s->trk[i].RTCP_C2S_tpb.nextDropPacket = 0;
			s->trk[i].RTCP_C2S_tpb.droppedPacketCount = 0;

		}
		s->numTracks = 0;

              //init buffers
              s->cinbuf = (char*)get_free_page((int)GFP_KERNEL);
		if (s->cinbuf == NULL) 
		{
			KRTSPROXYD_OUT(KERN_CRIT "kRtspProxyd: Not enough memory for basic needs\n");
			return NULL;
		}
              s->coutbuf = (char*)get_free_page((int)GFP_KERNEL);
		if (s->coutbuf == NULL) 
		{
			KRTSPROXYD_OUT(KERN_CRIT "kRtspProxyd: Not enough memory for basic needs\n");
			free_page((unsigned long)s->cinbuf);
			return NULL;
		} 
		s->sinbuf = (char*)get_free_page((int)GFP_KERNEL);
		if (s->sinbuf == NULL) 
		{
			KRTSPROXYD_OUT(KERN_CRIT "kRtspProxyd: Not enough memory for basic needs\n");
			free_page((unsigned long)s->cinbuf);
			free_page((unsigned long)s->coutbuf);
			return NULL;
		} 
		s->soutbuf = (char*)get_free_page((int)GFP_KERNEL);
		if (s->soutbuf == NULL) 
		{
			KRTSPROXYD_OUT(KERN_CRIT "kRtspProxyd: Not enough memory for basic needs\n");
			free_page((unsigned long)s->cinbuf);
			free_page((unsigned long)s->coutbuf);
                     free_page((unsigned long)s->sinbuf);
			return NULL;
		}		
		
		s->amtInClientInBuffer = 0;
		s->amtInClientOutBuffer = 0;
		s->amtInServerInBuffer = 0;
		s->amtInServerOutBuffer = 0;
		
		s->totalContentLength = 0;	// headers + body
		s->contentLength = 0;		// just body
		s->haveParsedServerReplyHeaders = 0;

	}

	return s;
}

