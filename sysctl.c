/*

kRtspProxyd

Sysctl interface

*/
/****************************************************************
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2, or (at your option)
 *	any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 ****************************************************************/


#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/net.h>
#include <linux/sched.h>
#include <linux/skbuff.h>
//Ryan added this code.
#include <linux/hardirq.h>
//#include <linux/smp_lock.h>
#include <linux/sysctl.h>
#include <linux/un.h>
#include <linux/unistd.h>

#include <net/ip.h>
#include <net/sock.h>
#include <net/tcp.h>

#include <asm/atomic.h>
//after Linux kernel 5.0, the semaphore.h was moved to the linux folder.
//#include <asm/semaphore.h>
#include <linux/semaphore.h>
#include <asm/processor.h>
#include <asm/uaccess.h>

#include <linux/file.h>
#include "prototypes.h"



char sysctl_krtsproxyd_cacheroot[200] = "/var/cache-root";
int 	sysctl_krtsproxyd_stop 	= 0;
int 	sysctl_krtsproxyd_start 	= 0;
int 	sysctl_krtsproxyd_unload 	= 0;
int 	sysctl_krtsproxyd_clientport = 554;
int 	sysctl_krtsproxyd_permreq    = S_IROTH; /* "other" read-access is required by default*/
int 	sysctl_krtsproxyd_permforbid = S_IFDIR | S_ISVTX | S_IXOTH | S_IXGRP | S_IXUSR;
 				/* forbidden is execute, directory and sticky*/
int 	sysctl_krtsproxyd_logging 	= 0;
char sysctl_krtsproxyd_serverip[20] = "#"; //the IP of the proxy server
int 	sysctl_krtsproxyd_serverport= 554; //rtsp default

char	sysctl_krtsproxyd_dynamicstring[200];
int 	sysctl_krtsproxyd_sloppymime= 0;
int	sysctl_krtsproxyd_threads	= 1; //change this value according to the CPU number of your machine
int	sysctl_krtsproxyd_maxconnect = 50;


static struct ctl_table_header *krtsproxyd_table_header;

//after Linux kernel 5.0, the ctl_table changed to struct.
//static int sysctl_SecureString(ctl_table *table, int *name, int nlen,
//		  void *oldval, size_t *oldlenp,
//		  void *newval, size_t newlen, void **context);
static int sysctl_SecureString(struct ctl_table *table, int *name, int nlen,
		  void *oldval, size_t *oldlenp,
		  void *newval, size_t newlen, void **context);


//after Linux kernel 5.0, the ctl_table changed to struct.
//static int proc_dosecurestring(ctl_table *table, int write, struct file *filp,
//		  void *buffer, size_t *lenp);
static int proc_dosecurestring(struct ctl_table *table, int write, struct file *filp,
		  void *buffer, size_t *lenp);

//after Linux kernel 5.0, the ctl_table changed to struct.
//static ctl_table krtsproxyd_table[] = {
/*static struct ctl_table krtsproxyd_table[] = {
	{	NET_KRTSPROXYD_CACHEROOT,
		"cacheroot",
		&sysctl_krtsproxyd_cacheroot,
		sizeof(sysctl_krtsproxyd_cacheroot),
		0644,
		NULL,
		proc_dostring,
		&sysctl_string,
		NULL,
		NULL,
		NULL
	},
	{	NET_KRTSPROXYD_STOP,
		"stop",
		&sysctl_krtsproxyd_stop,
		sizeof(int),
		0644,
		NULL,
		proc_dointvec,
		&sysctl_intvec,
		NULL,
		NULL,
		NULL
	},
	{	NET_KRTSPROXYD_START,
		"start",
		&sysctl_krtsproxyd_start,
		sizeof(int),
		0644,
		NULL,
		proc_dointvec,
		&sysctl_intvec,
		NULL,
		NULL,
		NULL
	},
	{	NET_KRTSPROXYD_UNLOAD,
		"unload",
		&sysctl_krtsproxyd_unload,
		sizeof(int),
		0644,
		NULL,
		proc_dointvec,
		&sysctl_intvec,
		NULL,
		NULL,
		NULL
	},
	{	NET_KRTSPROXYD_THREADS,
		"threads",
		&sysctl_krtsproxyd_threads,
		sizeof(int),
		0644,
		NULL,
		proc_dointvec,
		&sysctl_intvec,
		NULL,
		NULL,
		NULL
	},
	{	NET_KRTSPROXYD_MAXCONNECT,
		"maxconnect",
		&sysctl_krtsproxyd_maxconnect,
		sizeof(int),
		0644,
		NULL,
		proc_dointvec,
		&sysctl_intvec,
		NULL,
		NULL,
		NULL
	},
	{	NET_KRTSPROXYD_SLOPPYMIME,
		"sloppymime",
		&sysctl_krtsproxyd_sloppymime,
		sizeof(int),
		0644,
		NULL,
		proc_dointvec,
		&sysctl_intvec,
		NULL,
		NULL,
		NULL
	},
	{	NET_KRTSPROXYD_CLIENTPORT,
		"clientport",
		&sysctl_krtsproxyd_clientport,
		sizeof(int),
		0644,
		NULL,
		proc_dointvec,
		&sysctl_intvec,
		NULL,
		NULL,
		NULL
	},
	{	NET_KRTSPROXYD_PERMREQ,
		"perm_required",
		&sysctl_krtsproxyd_permreq,
		sizeof(int),
		0644,
		NULL,
		proc_dointvec,
		&sysctl_intvec,
		NULL,
		NULL,
		NULL
	},
	{	NET_KRTSPROXYD_PERMFORBID,
		"perm_forbid",
		&sysctl_krtsproxyd_permforbid,
		sizeof(int),
		0644,
		NULL,
		proc_dointvec,
		&sysctl_intvec,
		NULL,
		NULL,
		NULL
	},
	{	NET_KRTSPROXYD_LOGGING,
		"logging",
		&sysctl_krtsproxyd_logging,
		sizeof(int),
		0644,
		NULL,
		proc_dointvec,
		&sysctl_intvec,
		NULL,
		NULL,
		NULL
	},
	{	NET_KRTSPROXYD_SERVERIP,
		"serverip",
		&sysctl_krtsproxyd_serverip,
		sizeof(sysctl_krtsproxyd_serverip),
		0644,
		NULL,
		proc_dostring,
		&sysctl_string,
		NULL,
		NULL,
		NULL
	},
	{	NET_KRTSPROXYD_SERVERPORT,
		"serverport",
		&sysctl_krtsproxyd_serverport,
		sizeof(int),
		0644,
		NULL,
		proc_dointvec,
		&sysctl_intvec,
		NULL,
		NULL,
		NULL
	},
	{	NET_KRTSPROXYD_DYNAMICSTRING,
		"dynamic",
		&sysctl_krtsproxyd_dynamicstring,
		sizeof(sysctl_krtsproxyd_dynamicstring),
		0644,
		NULL,
		proc_dosecurestring,
		&sysctl_SecureString,
		NULL,
		NULL,
		NULL
	},

	{0,0,0,0,0,0,0,0,0,0,0}	};

example : 
/*struct ctl_table
{
    const char *procname; /* Text ID for /proc/sys, or zero */
    /*void *data;
    int maxlen;
    mode_t mode;
    struct ctl_table *child;
    struct ctl_table *parent; /* Automatically set */
    /*proc_handler *proc_handler; /* Callback for text         formatting */
    /*void *extra1;
    void *extra2;
};


	*/
static struct ctl_table krtsproxyd_table[] = {
	{}
};
	
	
static struct ctl_table krtsproxyd_dir_table[] = {
	// {NET_KRTSPROXYD, "krtsproxyd", NULL, 0, 0555, krtsproxyd_table,0,0,0,0,0},
	// {0,0,0,0,0,0,0,0,0,0,0}
	{
		.procname	= "krtspproxyd",
		.mode		= 0555,
		.child		= krtsproxyd_table,
	},
	{}
};

static struct ctl_table krtsproxyd_root_table[] = {
	// {CTL_NET, "net", NULL, 0, 0555, krtsproxyd_dir_table,0,0,0,0,0},
	// {0,0,0,0,0,0,0,0,0,0,0}
	{
		.procname	= "net",
		.mode		= 0555,
		.child		= krtsproxyd_dir_table,
	},
	{}
};
	

void StartSysctl(void)
{
	//krtsproxyd_table_header = register_sysctl_table(krtsproxyd_root_table,1);
	krtsproxyd_table_header = register_sysctl_table(krtsproxyd_root_table);
}


void EndSysctl(void)
{
	unregister_sysctl_table(krtsproxyd_table_header);
}

static int proc_dosecurestring(struct ctl_table *table, int write, struct file *filp,
		  void *buffer, size_t *lenp)
{
	size_t len;
	char *p, c=0;
	char String[256];
	
	if ((table->data==0) || (table->maxlen==0) || (*lenp==0) ||
	    ((filp->f_pos!=0) && (write==0))) {
		*lenp = 0;
		return 0;
	}
	
	if (write!=0) {
		len = 0;
		p = buffer;
		while (len < *lenp) {
			if(get_user(c, p++))
				return -EFAULT;
			if (c == 0 || c == '\n')
				break;
			len++;
		}
		if (len >= table->maxlen)
			len = table->maxlen-1;
		if(copy_from_user(String, buffer,(unsigned long)len))
			return -EFAULT;
		((char *) String)[len] = 0;
		filp->f_pos += *lenp;
		AddDynamicString(String);
	} else {
		GetSecureString(String);
		len = strlen(String);
		if (len > table->maxlen)
			len = table->maxlen;
		if (len > *lenp)
			len = *lenp;
		if (len!=0)
			if(copy_to_user(buffer, String,(unsigned long)len))
				return -EFAULT;
		if (len < *lenp) {
			if(put_user('\n', ((char *) buffer) + len))
				return -EFAULT;
			len++;
		}
		*lenp = len;
		filp->f_pos += len;
	}
	return 0;
}

static int sysctl_SecureString (/*@unused@*/ struct ctl_table *table, 
				/*@unused@*/int *name, 
				/*@unused@*/int nlen,
		  		/*@unused@*/void *oldval, 
		  		/*@unused@*/size_t *oldlenp,
		  		/*@unused@*/void *newval, 
		  		/*@unused@*/size_t newlen, 
		  		/*@unused@*/void **context)
{
	return -ENOSYS;
}
