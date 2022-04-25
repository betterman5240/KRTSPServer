/***************************************************************************************

kRtspProxyd

Main program

Authors: Kaiqin Fan

Bugs Report: Email to  kqfan@163.com

kRtspProxyd consists of 1 thread, this main-thread handles ALL connections
simultanious. It does this by keeping queues with the requests in 2 diferrent
stages.

The stages are

1 Wait4sessionprocess	-	Connection is accepted, rtsp session is created and is waiting for service
2 DataSwitching		-     Reply and relay Rtp/Rtcp packets between servers and clients

Acknowledgments:

Thanks to the authors of KHTTPD, for most of the system architecture design idea are "stolen" from this famous open-source project.
And thanks to the genius programmers of apple Corporation, because most of the RTSP parsing codes are derived from their codes. 


****************************************************************************************/

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

//Ryan Fixes compiler error.
//static int errno;
#define __KERNEL_SYSCALLS__

//#include <linux/config.h>
//atfer linux kernel 2.6.19, it doesn't have the config.h, so it can include the autoconf.h file. 
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/init.h>
#include <linux/wait.h>
//Ryan added this code.
#include <linux/hardirq.h>
//#include <linux/smp_lock.h>
#include <linux/inet.h>
#include <asm/unistd.h>

//Ryan added this code
#include <linux/errno.h>

#include "structure.h"
#include "prototypes.h"
#include "sysctl.h"

struct krtsproxyd_threadinfo threadinfo[CONFIG_KRTSPROXYD_NUMCPU];  /* The actual work-queues */

//Ryan added these code
extern int errno;

atomic_t	ConnectCount;
atomic_t	DaemonCount;

static int	ActualThreads; /* The number of actual, active threads */


static int ConnectionsPending(const int CPUNR)
{
       if (threadinfo[CPUNR].RtspSessionQueue!=NULL) return O_NONBLOCK;
	if (threadinfo[CPUNR].gShokQueue!=NULL) return O_NONBLOCK;
  return 0;
}



static wait_queue_head_t DummyWQ[CONFIG_KRTSPROXYD_NUMCPU];
static atomic_t Running[CONFIG_KRTSPROXYD_NUMCPU]; 

static int MainDaemon(void *cpu_pointer)
{
	int CPUNR;
	sigset_t tmpsig;
	DECLARE_WAITQUEUE(main_wait,current);
	
	//MOD_INC_USE_COUNT;

	
	CPUNR=0;
	if (cpu_pointer!=NULL)
	CPUNR=(int)*(int*)cpu_pointer;

	sprintf(current->comm,"kRtspProxyd - C%i",CPUNR);
	//daemonize();
	
	init_waitqueue_head(&(DummyWQ[CPUNR]));
	

	/* Block all signals except SIGKILL, SIGSTOP and SIGHUP */
        spin_lock_irq(&current->sighand->siglock);
        tmpsig = current->blocked;
        siginitsetinv(&current->blocked, sigmask(SIGKILL) | sigmask(SIGSTOP)| sigmask(SIGHUP));
        recalc_sigpending();
        spin_unlock_irq(&current->sighand->siglock);
                                                                                                               
	
	
	if (MainSocket->sk==NULL)
	 	return 0;
	//Kernel 5.0 , the sk->sleep change to sk_sleep()
	//add_wait_queue_exclusive(MainSocket->sk->sleep,&(main_wait));
	add_wait_queue_exclusive(sk_sleep(MainSocket->sk),&(main_wait));
	atomic_inc(&DaemonCount);
	atomic_set(&Running[CPUNR],1);
	
	while (sysctl_krtsproxyd_stop==0)
	{
		int changes = 0;
		               		
		changes +=AcceptConnections(CPUNR,MainSocket);
		if (ConnectionsPending(CPUNR))
		{

		       changes += Wait4SessionProcess(CPUNR);
		       changes += DataSwitching(CPUNR);
			/* Test for incoming connections _again_, because it is possible
			   one came in during the other steps, and the wakeup doesn't happen
			   then.
			*/
			changes +=AcceptConnections(CPUNR,MainSocket);
		}
		
		if (changes==0) 
		{
                     KRTSPROXYD_OUT(KERN_INFO "changes = 0!\n");
			//linux kenel remove interruptible_sleep_on_time in the kernel 5.0 
			//(void)interruptible_sleep_on_timeout(&(DummyWQ[CPUNR]), 1);
			wait_event_interruptible_timeout(DummyWQ[CPUNR],(changes == 0),1);	

		}
			
		if (signal_pending(current)!=0)
		{
			KRTSPROXYD_OUT(KERN_NOTICE "kRtspProxyd: Ring Ring - signal received\n");
			break;		  
		}
	
	}
	//Kernel 5.0 , the sk->sleep change to sk_sleep()
	//remove_wait_queue(MainSocket->sk->sleep,&(main_wait));
	remove_wait_queue(sk_sleep(MainSocket->sk),&(main_wait));


	StopSessionProcess(CPUNR);
			
	atomic_set(&Running[CPUNR],0);
	atomic_dec(&DaemonCount);
	KRTSPROXYD_OUT(KERN_NOTICE "kRtspProxyd: Main Daemon %i has ended\n",CPUNR);
	//MOD_DEC_USE_COUNT;
	return 0;
}

static int CountBuf[CONFIG_KRTSPROXYD_NUMCPU];

/*

The ManagementDaemon has a very simple task: Start the real daemons when the user wants us
to, and cleanup when the users wants to unload the module.

Initially, kRtspProxyd didn't have this thread, but it is the only way to have "delayed activation",
a feature required to prevent accidental activations resulting in unexpected backdoors.

*/
static int ManagementDaemon(void *unused)
{
	sigset_t tmpsig;
	int waitpid_result;
	
	DECLARE_WAIT_QUEUE_HEAD(WQ);
	
	
	sprintf(current->comm,"KRtspProxyd manager");
	//daemonize();
	

	/* Block all signals except SIGKILL and SIGSTOP */

        spin_lock_irq(&current->sighand->siglock);
        tmpsig = current->blocked;
        siginitsetinv(&current->blocked, sigmask(SIGKILL) | sigmask(SIGSTOP)| sigmask(SIGHUP));
        recalc_sigpending();
        spin_unlock_irq(&current->sighand->siglock);

	/* main loop */
	while (sysctl_krtsproxyd_unload==0)
	{
		int I;
		
		
		/* First : wait for activation */
		
                /*initialize global variables*/
                if(strcmp(sysctl_krtsproxyd_serverip, "#") == 0)
                  KRTSPROXYD_OUT(KERN_ERR "Please set IP address of the proxy server \n through proc filesystem!\n"); 
                while(strcmp(sysctl_krtsproxyd_serverip,"#") == 0 && (!signal_pending(current)))
                {
                  current->state = TASK_INTERRUPTIBLE;
                  //linux kenel remove interruptible_sleep_on_time in the kernel 5.0 
									//interruptible_sleep_on_timeout(&WQ, HZ);
									wait_event_interruptible_timeout(WQ,strcmp(sysctl_krtsproxyd_serverip,"#") == 0 && (!signal_pending(current)) ,HZ);
	        }
                
                /*set IP address*/
                gProxyIP = in_aton(sysctl_krtsproxyd_serverip);
		
		while ( (sysctl_krtsproxyd_start==0) && (!signal_pending(current)) && (sysctl_krtsproxyd_unload==0) )
		{
			current->state = TASK_INTERRUPTIBLE;
			//linux kenel remove interruptible_sleep_on_time in the kernel 5.0 
			//interruptible_sleep_on_timeout(&WQ, HZ);
			wait_event_interruptible_timeout(WQ,(sysctl_krtsproxyd_start==0) && (!signal_pending(current)) && (sysctl_krtsproxyd_unload==0) ,HZ);
		}
		
		if ( (signal_pending(current)) || (sysctl_krtsproxyd_unload!=0) )
		 	break;
		 	
		/* Then start listening and spawn the daemons */
		 	
		if (StartListening(sysctl_krtsproxyd_serverport)==0)
		{
			continue;
		}
		
		ActualThreads = sysctl_krtsproxyd_threads;
		if (ActualThreads<1) 
			ActualThreads = 1;
			
		if (ActualThreads>CONFIG_KRTSPROXYD_NUMCPU) 
			ActualThreads = CONFIG_KRTSPROXYD_NUMCPU;
		
		/* Write back the actual value */
		
		sysctl_krtsproxyd_threads = ActualThreads;
		/* Clean all queues */
		memset(threadinfo, 0, sizeof(struct krtsproxyd_threadinfo));
		 	
		I=0;
		while (I<ActualThreads)
		{
			atomic_set(&Running[I],1);
			(void)kernel_thread(MainDaemon,&(CountBuf[I]), CLONE_FS | CLONE_FILES | CLONE_SIGHAND);
			I++;
		}
		
		/* Then wait for deactivation */
		sysctl_krtsproxyd_stop = 0;

		while ( (sysctl_krtsproxyd_stop==0) && (!signal_pending(current)) && (sysctl_krtsproxyd_unload==0) )
		{
			if (atomic_read(&DaemonCount)<ActualThreads)
			{
				I=0;
				while (I<ActualThreads)
				{
					if (atomic_read(&Running[I])==0)
					{
						atomic_set(&Running[I],1);
						(void)kernel_thread(MainDaemon,&(CountBuf[I]), CLONE_FS | CLONE_FILES | CLONE_SIGHAND);
						KRTSPROXYD_OUT(KERN_CRIT "kRtspProxyd: Restarting daemon %i \n",I);
					}
					I++;
				}
			}
			//linux kenel remove interruptible_sleep_on_time in the kernel 5.0 
			//interruptible_sleep_on_timeout(&WQ, HZ);
			wait_event_interruptible_timeout(WQ,( (sysctl_krtsproxyd_stop==0) && (!signal_pending(current)) && (sysctl_krtsproxyd_unload==0) ),HZ);
			
			/* reap the daemons */
			//linux kernel 5.0 removed daemonize function. so I try to mask these codes.
			//waitpid_result = waitpid(-1,NULL,__WCLONE|WNOHANG);
			
		}
		
		
		/* The user wants us to stop. So stop listening on the socket. */
		if (sysctl_krtsproxyd_stop!=0)	
		{
			/* Wait for the daemons to stop, one second per iteration */
			while (atomic_read(&DaemonCount)>0){
		 	//linux kenel remove interruptible_sleep_on_time in the kernel 5.0 
			//interruptible_sleep_on_timeout(&WQ, HZ);
			wait_event_interruptible_timeout(WQ,(atomic_read(&DaemonCount)>0),HZ);
			}
			StopListening();
		}
	
	}
	
	sysctl_krtsproxyd_stop = 1;

	/* Wait for the daemons to stop, one second per iteration */
	while (atomic_read(&DaemonCount)>0){
 		//linux kenel remove interruptible_sleep_on_time in the kernel 5.0 
		//interruptible_sleep_on_timeout(&WQ, HZ);
		wait_event_interruptible_timeout(WQ,(atomic_read(&DaemonCount)>0),HZ);
	}
		
	//linux kernel 5.0 removed daemonize function. so I try to mask these codes.
	//waitpid_result = 1;
	/* reap the zombie-daemons */
	//while (waitpid_result>0)
	//	waitpid_result = waitpid(-1,NULL,__WCLONE|WNOHANG);
		
	StopListening();
	
	
	KRTSPROXYD_OUT(KERN_NOTICE "kRtspProxyd: Management daemon stopped. \n And you can unload the module now.\n");

	//MOD_DEC_USE_COUNT;

	return 0;
}

int __init kRtspProxyd_init(void)
{
	int I;

	//MOD_INC_USE_COUNT;
	
	I=0;
	while (I<CONFIG_KRTSPROXYD_NUMCPU)
	{
		CountBuf[I]=I;
		
		I++;
	}
	
	atomic_set(&ConnectCount,0);
	atomic_set(&DaemonCount,0);
	
	StartSysctl();
	
	(void)kernel_thread(ManagementDaemon,NULL, CLONE_FS | CLONE_FILES | CLONE_SIGHAND);
	
	return 0;
}

void kRtspProxyd_cleanup(void)
{
	EndSysctl();
}

	module_init(kRtspProxyd_init)
	module_exit(kRtspProxyd_cleanup)
    MODULE_LICENSE("GPL");
	MODULE_AUTHOR("Kaiqin Fan, ICCC, Huazhong University of Science and Technology");
