#ifndef _KRTSPROXYD_PROTOTYPES_H
#define _KRTSPROXYD_PROTOTYPES_H


#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/net.h>
#include <linux/time.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <net/sock.h>
#include <asm/uaccess.h>

#include "structure.h"
#include "proxy.h"
#include "debug.h"
/* General defines and stuff */


#define CONFIG_KRTSPROXYD_NUMCPU 16    /* Maximum number of threads */

/* sockets.c */
int  StartListening(const int Port);
void StopListening(void);

extern struct socket *MainSocket;


/* sysctl.c */
void StartSysctl(void);
void EndSysctl(void);

extern int sysctl_krtsproxyd_stop;


/* main.c */
extern struct krtsproxyd_threadinfo threadinfo[CONFIG_KRTSPROXYD_NUMCPU];
extern atomic_t ConnectCount;
extern struct wait_queue main_wait[CONFIG_KRTSPROXYD_NUMCPU];

/* misc.c */

int SendBuffer(struct socket *sock, const char *Buffer,const size_t Length);
int SendBuffer_async(struct socket *sock, const char *Buffer,const size_t Length);
int remove_shok(struct shok *theShok, int withSib, const int CPUNR);
void remove_shok_ref(struct shok *theShok, unsigned int fromIP, unsigned int toIP, int withSib, const int CPUNR);

/* accept.c */

int AcceptConnections(const int CPUNR,struct socket *Socket);

/* wait4sessionprocess.c */

int Wait4SessionProcess(const int CPUNR);
void StopSessionProcess(const int CPUNR);

/* dataswitching.c */
int DataSwitching(const int CPUNR);

/* security.c */

void AddDynamicString(const char *String);
//void GetSecureString(char *String);


/* logging.c */

//int Logging(const int CPUNR);
//void StopLogging(const int CPUNR);


/* Other prototypes */



#endif
