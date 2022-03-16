/*

kRtspProxyd

Accept connections

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
#include "structure.h"
#include "prototypes.h"
#include "sysctl.h"
#include "proxy.h"
//Ryan added this code.
#include <linux/hardirq.h>
//#include <linux/smp_lock.h>
#include <linux/inet.h>

int AcceptConnections(const int CPUNR, struct socket *Socket)
{
	struct rtsp_session *NewSession; //add
	struct socket *NewSock;
	struct sockaddr_in skaddr;
	int err, len;
	int count = 0;
	int error;
	


	EnterFunction("AcceptConnections");
	if (atomic_read(&ConnectCount)>sysctl_krtsproxyd_maxconnect)
	{
		KRTSPROXYD_OUT(KERN_ERR "AcceptConnections - too many active connections\n");
		return 0;
	}
	
	if (Socket==NULL) return 0;
	
	/* 
	   Quick test to see if there are connections on the queue.
	   This is cheaper than accept() itself because this saves us
	   the allocation of a new socket. (Which doesn't seem to be 
	   used anyway)
	*/
	//remove these code, Ryan remove
   	/*if (Socket->sk->tp_pinfo.af_tcp.accept_queue==NULL)
	{
		return 0;
	}*/
	
    KRTSPROXYD_OUT(KERN_INFO "accept_queue != NULL\n");
	error = 0;	
	while (error>=0)
	{
		NewSock = sock_alloc();
		if (NewSock==NULL){
                        KRTSPROXYD_OUT(KERN_INFO "sock_alloc() failed!\n");
			break;
                }
			
		NewSock->type = Socket->type;
		NewSock->ops = Socket->ops;
		
			// after linux kernel 5.0 accept (sock,newsock,type, bool kern)
	       //error = Socket->ops->accept(Socket, NewSock, O_NONBLOCK);
			error = Socket->ops->accept(Socket, NewSock, O_NONBLOCK, true);
		

		if (error<0)
		{
                     KRTSPROXYD_OUT(KERN_ERR "failed in accept!\n");
			sock_release(NewSock);
			break;
		}
		//after linux kernel 5.0, struct sock remove the "state" member
		//if (NewSock->sk->state==TCP_CLOSE)
		if(NewSock->sk->sk_state==TCP_CLOSE)
		{
			sock_release(NewSock);
			continue;
		}
		//allocate a rtsp_session for the connection
		NewSession = new_session();
		if(!NewSession){
			KRTSPROXYD_OUT(KERN_ERR "sorry, cannot create a new session\n");
                     sock_release(NewSock);		
                     break;
              }

		NewSession->client_skt = NewSock;
		//after linux kernel, the getname function removed the length member.
		//err = NewSock->ops->getname(NewSock, (struct sockaddr *)&skaddr, &len, 1);
		err=NewSock->ops->getname(NewSock, (struct sockaddr *)&skaddr, 1);
		if(err!=0){
		  KRTSPROXYD_OUT(KERN_ERR "sorry, getname calling failed!\n");
		  sock_release(NewSock);
	         break;
		}
		NewSession->client_ip = skaddr.sin_addr.s_addr;
		
	       //add to session queue
              KRTSPROXYD_OUT(KERN_INFO "the client ip is %s\n", ntoa(NewSession->client_ip));
		NewSession->next = threadinfo[CPUNR].RtspSessionQueue;
	       KRTSPROXYD_OUT(KERN_INFO "add new session to session queue!\n"); 	
	       
		init_waitqueue_entry(&NewSession->sleep,current);
		//after linux kernel, the sock struct removed the sleep member.
		//add_wait_queue(NewSock->sk->sleep,&(NewSession->sleep)); Ryan remove these code.
		add_wait_queue(sk_sleep(NewSock->sk),&(NewSession->sleep));
		threadinfo[CPUNR].RtspSessionQueue = NewSession;
		atomic_inc(&ConnectCount);
              atomic_inc(&gNumUsers);
		count++;
	}		
	
	LeaveFunction("AcceptConnections");
	return count;
}
