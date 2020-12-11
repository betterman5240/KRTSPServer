#ifndef _KRTSPROXYD_SYSCTL_H
#define _KRTSPROXYD_SYSCTL_H

extern char 	sysctl_krtsproxyd_cacheroot[200];
extern int 	sysctl_krtsproxyd_stop;
extern int 	sysctl_krtsproxyd_start;
extern int 	sysctl_krtsproxyd_unload;
extern int 	sysctl_krtsproxyd_clientport;
extern int 	sysctl_krtsproxyd_permreq;
extern int 	sysctl_krtsproxyd_permforbid;
extern int 	sysctl_krtsproxyd_logging;
extern char     sysctl_krtsproxyd_serverip[20];
extern int 	sysctl_krtsproxyd_serverport;
extern int 	sysctl_krtsproxyd_sloppymime;
extern int 	sysctl_krtsproxyd_threads;
extern int	       sysctl_krtsproxyd_maxconnect;
/* /proc/sys/net/krtsproxyd/ */
enum {
	NET_KRTSPROXYD_CACHEROOT	= 1,
	NET_KRTSPROXYD_START	= 2,
	NET_KRTSPROXYD_STOP		= 3,
	NET_KRTSPROXYD_UNLOAD	= 4,
	NET_KRTSPROXYD_CLIENTPORT	= 5,
	NET_KRTSPROXYD_PERMREQ	= 6,
	NET_KRTSPROXYD_PERMFORBID	= 7,
	NET_KRTSPROXYD_LOGGING	= 8,
	NET_KRTSPROXYD_SERVERIP = 9,
	NET_KRTSPROXYD_SERVERPORT	= 10,
	NET_KRTSPROXYD_DYNAMICSTRING= 11,
	NET_KRTSPROXYD_SLOPPYMIME   = 12,
	NET_KRTSPROXYD_THREADS	= 13,
	NET_KRTSPROXYD_MAXCONNECT	= 14
};
enum {
       NET_KRTSPROXYD = 19  //so ugly 
};

#endif
