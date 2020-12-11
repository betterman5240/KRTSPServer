#ifndef _PROXY_H_
#define _PROXY_H_


#include <asm/unistd.h>
#include "prototypes.h"
#include "structure.h"
#include "sysctl.h"


enum {
	stIdle,
	stError,

	stRecvClientCommand,
	stWaitingForIPAddress,
	stParseClientCommand,
	stSendClientResponse,
	stServerTransactionSend,
	stServerTransactionRecv,
	stClientShutdown,
	stServerShutdown,
	stBadServerName,
	stCantConnectToServer,
	stDone
};		// rtsp session states

enum {
	ttNone,
	ttDescribe,
	ttSetup,
	ttPlay,
	ttPause,
	ttStop,
	ttTeardown,
	ttOptions,
	ttAnnounce,
	ttRedirect,
	ttGetParameter,
	ttSetParameter
};		// rtsp command types

enum {
	kPermissionDenied,
	kTooManyUsers,
	kServerNotFound,
	kUnknownError
};		// refusal type

extern unsigned int gProxyIP;
extern int gUserLimit;
extern atomic_t gNumUsers;
extern atomic_t gMaxPorts;
#define RTSP_SESSION_BUF_SIZE 4096	
#define MAX_SOCKET_NAME 64
#define MX_TRACKS 8 
typedef int (*do_routine)(void *refCon, char *buf, int bufSize);

typedef struct ipList {
	struct ipList	*next;
	unsigned int		ip;
	do_routine		what_to_do;
	void 			*what_to_do_it_with;
} ipList;

typedef struct {
	char	*cmd;
	int		type;
} t_cmd_map;

typedef struct shok {
	struct shok	*next;
       struct socket *socket; //receive from whom?
       unsigned short port;  //HBO
	ipList	*ips; //NBO
	struct shok	*sib;		// sibling - rtcp or rtp
} shok;

typedef struct trans_pb {
	int		*status;		// set to 1 when needs to die
	shok 	*send_from;
       unsigned int send_to_ip;   
       unsigned short send_to_port;
       long long int 	packetSendCount;
	long long int 	nextDropPacket;
	long long int 	droppedPacketCount;
	long long int 	packetCount;
	char		socketName[MAX_SOCKET_NAME];

} trans_pb;

typedef struct {
	int		ID;
	shok	*RTP_S2P;
	shok	*RTCP_S2P;
	shok	*RTP_P2C;
	shok	*RTCP_P2C;
	unsigned short ClientRTPPort;
	unsigned	short ServerRTPPort;
	trans_pb	RTP_S2C_tpb;
	trans_pb	RTCP_S2C_tpb;
	trans_pb	RTCP_C2S_tpb;
} track_info;

/*rtsp session stuff*/
typedef struct rtsp_session {
	struct rtsp_session *next;
	int		                 die;
       int                            new_session;
       struct socket	           *client_skt; //add
       unsigned int              client_ip; //add
	char	                         *server_address;
       struct socket            *server_skt; //add
       unsigned int server_ip; //add
	unsigned 	short server_port; //add
	int		server_skt_pending_connection;
	int		state;
	int		transaction_type;
	char	*sessionID;

       wait_queue_t sleep;	/* For putting in the socket's waitqueue */

	int		cur_trk;
	int		numTracks;
	track_info	trk[MX_TRACKS];

	char	*cinbuf; //one page
	int		amtInClientInBuffer;
	char	*coutbuf;
	int		amtInClientOutBuffer;
	char	*sinbuf;
	int		amtInServerInBuffer;
	char	*soutbuf;
	int		amtInServerOutBuffer;

	int		totalContentLength;
	int		haveParsedServerReplyHeaders;
	int 		contentLength;
	char           *responseBodyP;
	
	unsigned int	tempIP; //nbo
} rtsp_session;

extern int has_two_crlfs(char *s);
extern char *str_sep(char **stringp, char *delim);
extern int is_command(char *inp, char *cmd, char *server);
extern int cmd_to_transaction_type(char *cmd);
extern int has_trackID(char *inp, int *trackID);
extern int track_id_to_idx(rtsp_session *s, int id);
extern int has_client_port(char *inp, unsigned short *port);
extern int make_udp_port_pair(unsigned int fromIP, unsigned int toIP, shok **rtpSocket, shok **rtcpSocket, const int CPUNR);
extern shok *make_new_shok(unsigned int fromIP, unsigned int toIP, int withSib, const int CPUNR);
extern void set_socket_reuse_address(struct socket *skt);
extern int connect_to_address(struct socket *skt, unsigned int address, unsigned port);
extern int has_content_length(char *inp, int *len);
extern int has_sessionID(char *inp, char *sessionID);
extern int has_ports(char *inp, unsigned int *client_port, unsigned int *server_port);
extern int upon_receipt_from(shok *theShok, int fromIP, do_routine doThis, void *withThis);
extern char *source_eq_string(char *inp);
extern int has_IN_IP(char *inp, char *str);
extern struct socket *new_socket_tcp(void);
extern int UdpReceiveBuffer(struct socket *sock, void * ubuf, size_t size, unsigned flags, struct sockaddr *addr, int *addr_len);
extern int UdpSendBuffer(struct socket *sock, void *buff, size_t len, unsigned flags, struct sockaddr *addr, int addr_len);
extern ipList *find_ip_in_list(ipList *list, unsigned int ip);
extern rtsp_session *new_session(void);
extern int transfer_data(void *refCon, char *buf, int bufSize);
extern int ReceiveBuffer(struct socket *sock, void *my_msg, int size);
extern void send_rtsp_error(struct socket *sock, int refusal);
extern char *ntoa(__u32 in);
extern long atoi(char *p);
extern int str_casecmp(char *str1, char *str2);
extern void CleanUpSession(struct rtsp_session *session, const int CPUNR);
extern int add_ips_to_shok(shok *theShok, unsigned int fromIP, unsigned int toIP, int withSib);
extern void make_socket_nonblocking(struct socket *skt);
extern int strn_casecmp(char *str1, char *str2, int l);
extern void GetSecureString(char *String);
#endif
