/*

kRtspProxyd

Basic socket functions

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

#include "prototypes.h"
#include <linux/kernel.h>
#include <linux/net.h>
#include <linux/version.h>
//Ryan added this code.
#include <linux/hardirq.h>
//#include <linux/smp_lock.h>
#include <net/sock.h>


/*

MainSocket is shared by all threads, therefore it has to be
a global variable.

*/
struct socket *MainSocket=NULL;


int StartListening(const int Port)
{
	struct socket *sock;
	struct sockaddr_in sin;
	int error;
	
	EnterFunction("Start Listening for RTSP messages...");
	
	/* First create a socket */
	
	error = sock_create(PF_INET,SOCK_STREAM,IPPROTO_TCP,&sock);
	if (error<0) 
       {
	     KRTSPROXYD_OUT(KERN_ERR "Error during creation of socket; terminating\n");
	     sock_release(sock);
	     return 0;
	}    
        KRTSPROXYD_OUT(KERN_INFO "sock_create success!\n");

	/* Now bind the socket */
	
	sin.sin_family	     = AF_INET;
	sin.sin_addr.s_addr  = INADDR_ANY;
	sin.sin_port         = htons((unsigned short)Port);
	
	error = sock->ops->bind(sock,(struct sockaddr*)&sin,sizeof(sin));
	if (error<0)
	{
		KRTSPROXYD_OUT(KERN_ERR "kRtspProxyd: Error binding socket. This means that some other \n");
		KRTSPROXYD_OUT(KERN_ERR "        daemon is (or was a short time ago) using port %i.\n",Port);
		return 0;	
	}
        
        KRTSPROXYD_OUT(KERN_INFO "bind success!\n");

	/* Grrr... setsockopt() does this. */
	//after the linux kernel 5.0, the sock sturct change reuse to sk_reuse.
	//sock->sk->reuse   = 1;
      sock->sk->sk_reuse = 1;

	/* Now, start listening on the socket */
	
	/* I have no idea what a sane backlog-value is. 48 works so far. */
	
	error=sock->ops->listen(sock,48);	
	if (error!=0)
	{
		KRTSPROXYD_OUT(KERN_ERR "kRtspProxyd: Error listening on socket \n");
	       sock_release(sock);
		return 0;
	}
        KRTSPROXYD_OUT(KERN_INFO "listen success!\n");
	MainSocket = sock;
	
	EnterFunction("StartListening end");
	return 1; 
}	

void StopListening(void)
{
	struct socket *sock;
	
	EnterFunction("StopListening");
	if (MainSocket==NULL) return;
	
	sock=MainSocket;
	MainSocket = NULL;
	sock_release(sock);

	LeaveFunction("StopListening");
}
