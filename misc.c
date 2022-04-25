/*

kRtspProxyd

General useful functions

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
#include <linux/ctype.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/net.h>
#include <linux/sched.h>
#include <linux/skbuff.h>
#include <linux/unistd.h>
#include <linux/file.h>
//Ryan added this code.
#include <linux/hardirq.h>
//#include <linux/smp_lock.h>

#include <net/ip.h>
#include <net/sock.h>

#include <asm/atomic.h>
#include <asm/errno.h>
//after Linux kernel 5.0, the semaphore.h was moved to the linux folder.
//#include <asm/semaphore.h>
#include <linux/semaphore.h>
#include <asm/processor.h>
#include <asm/uaccess.h>

#include "structure.h"
#include "prototypes.h"

#ifndef ECONNRESET
#define ECONNRESET 102
#endif

#define xtod(c)         ((c) <= '9' ? '0' - (c) : 'a' - (c) - 10)

/*

Readrest reads and discards all pending input on a socket. This is required
before closing the socket.

*/
static void ReadRest(struct socket *sock)
{
	struct msghdr		msg;
	struct iovec		iov;
	int			len;

	mm_segment_t		oldfs;
	
	
	EnterFunction("ReadRest");
	
	
	if (sock->sk==NULL)
		return;

	len = 1;
		
	while (len>0)
	{
		static char		Buffer[1024];   /* Never read, so doesn't need to
							   be SMP safe */

		msg.msg_name     = 0;
		msg.msg_namelen  = 0;
		//after the linux kernel 5.0, the msghdr struct change msg_iov to msg_iter.
		//msg.msg_iov	 = &iov;
		//msg.msg_iovlen   = 1;
		iov.iov_base = &Buffer[0];
		iov.iov_len  = (__kernel_size_t)1024;
		msg.msg_iter.iov     = &iov;
		msg.msg_control  = NULL;
		msg.msg_controllen = 0;
		msg.msg_flags    = MSG_DONTWAIT;
		//after the linux kernel 5.0, the msghdr struct change msg_iov to msg_iter.
		//msg.msg_iov->iov_base = &Buffer[0];
		//msg.msg_iov->iov_len  = (__kernel_size_t)1024;
		//msg.msg_iter.iov_base = &Buffer[0];
		//msg.msg_iter.iov_len  = (__kernel_size_t)1024;


		len = 0;
		oldfs = get_fs(); set_fs(KERNEL_DS);
		
		//after Linux kernel 5.0, the sock_recvmsg changed to three elements.
		//len = sock_recvmsg(sock,&msg,1024,MSG_DONTWAIT);
		len = sock_recvmsg(sock,&msg,MSG_DONTWAIT);
		set_fs(oldfs);
	}
	LeaveFunction("ReadRest");
}

void CleanUpSession(struct rtsp_session *session, const int CPUNR)
{
       int i;
      	EnterFunction("CleanUpSession");

   	/* Close the client socket ....*/
	if ((session->client_skt!=NULL)&&(session->client_skt->sk!=NULL))
	{
		ReadRest(session->client_skt);
		//after the linux kernel 5.0, remove sk->sleep and add sk_sleep function.
		//remove_wait_queue(session->client_skt->sk->sleep,&(session->sleep));
	    	remove_wait_queue(sk_sleep(session->client_skt->sk),&(session->sleep));
	    	sock_release(session->client_skt);
	    	session->client_skt = NULL;
	}
	/* Close the server socket ...*/
	if ((session->server_skt!=NULL)&&(session->server_skt->sk!=NULL))
	{
	       ReadRest(session->server_skt);
	    	sock_release(session->server_skt);
  		session->server_skt = NULL;
	}

       if(session->server_address)
       	kfree(session->server_address);
       if(session->sessionID)
       	kfree(session->sessionID);
      	for (i=0; i<session->numTracks; i++) {
		if (session->trk[i].RTP_S2P)
			remove_shok_ref(session->trk[i].RTP_S2P, gProxyIP, session->server_ip, 1, CPUNR);
		if (session->trk[i].RTP_P2C)
			remove_shok_ref(session->trk[i].RTP_P2C, gProxyIP, session->client_ip, 1, CPUNR);
      		}
	/* ... and release the memory for the structure. */
	free_page((unsigned long)session->cinbuf);
	free_page((unsigned long)session->coutbuf);
       free_page((unsigned long)session->sinbuf);
       free_page((unsigned long)session->soutbuf);
       
	kfree(session);
	
	atomic_dec(&ConnectCount);
	LeaveFunction("CleanUpSession");
}

long atoi(char *p)
{
        long n;
        int c, neg = 0;

        if (p == NULL)
                return 0;

        if (!isdigit(c = *p)) {
                while (isspace(c))
                        c = *++p;
                switch (c) {
                case '-':
                        neg++;
                case '+': /* fall-through */
                        c = *++p;
                }
                if (!isdigit(c))
                        return (0);
        }
        if (c == '0' && *(p + 1) == 'x') {
                p += 2;
                c = *p;
                n = xtod(c);
                while ((c = *++p) && isxdigit(c)) {
                        n *= 16; /* two steps to avoid unnecessary overflow */
                        n += xtod(c); /* accum neg to avoid surprises at MAX */
                }
        } else {
                n = '0' - c;
                while ((c = *++p) && isdigit(c)) {
                        n *= 10; /* two steps to avoid unnecessary overflow */
                        n += '0' - c; /* accum neg to avoid surprises at MAX */
                }
        }
        return (neg ? n : -n);

return (int)(*p);
}


/*
 *	Display an IP address in readable format. 
 */
char *ntoa(__u32 in)
{
	static char buff[18];
	char *p;

	p = (char *) &in;
	sprintf(buff, "%d.%d.%d.%d",
		(p[0] & 255), (p[1] & 255), (p[2] & 255), (p[3] & 255));
	return(buff);
}

/*
SendBuffer and Sendbuffer_async send "Length" bytes from "Buffer" to the "sock"et.
The _async-version is non-blocking.

A positive return-value indicates the number of bytes sent, a negative value indicates
an error-condition.
*/
int SendBuffer(struct socket *sock, const char *Buffer,const size_t Length)
{
	struct msghdr	msg;
	mm_segment_t	oldfs;
	struct iovec	iov;
	int 		len;
	
	EnterFunction("SendBuffer");
	
	msg.msg_name     = 0;
	msg.msg_namelen  = 0;
	//after the linux kernel 5.0, the msghdr struct change msg_iov to msg_iter.
	iov.iov_base = (void*) Buffer;
	iov.iov_len  = (__kernel_size_t)Length;
	msg.msg_iter.iov     = &iov;
	// msg.msg_iov	 = &iov;
	// msg.msg_iovlen   = 1;
	msg.msg_control  = NULL;
	msg.msg_controllen = 0;
	msg.msg_flags    = MSG_NOSIGNAL; 
	//after the linux kernel 5.0, the msghdr struct change msg_iov to msg_iter.   
	// msg.msg_iov->iov_len = (__kernel_size_t)Length;
	// msg.msg_iov->iov_base = (void*) Buffer;
		
	len = 0;
	oldfs = get_fs(); set_fs(KERNEL_DS);
	//after the linux kernel 5.0, the sock_sendmsg function change to two elements.
	//len = sock_sendmsg(sock,&msg,(size_t)(Length-len));
	len = sock_sendmsg(sock, &msg);
	set_fs(oldfs);
	LeaveFunction("SendBuffer");
	return len;	
}

//asynchronous version of SendBuffer()
int SendBuffer_async(struct socket *sock, const char *Buffer,const size_t Length)
{
	struct msghdr	msg;
	mm_segment_t	oldfs;
	struct iovec	iov;
	int 		len;
	
	EnterFunction("SendBuffer_async");
	msg.msg_name     = 0;
	msg.msg_namelen  = 0;
	//after the linux kernel 5.0, the msghdr struct change msg_iov to msg_iter.
	iov.iov_base = (void*) Buffer;
	iov.iov_len  = (__kernel_size_t)Length;
	msg.msg_iter.iov     = &iov;
	//msg.msg_iov	 = &iov;
	//msg.msg_iovlen   = 1;
	msg.msg_control  = NULL;
	msg.msg_controllen = 0;
	msg.msg_flags    = MSG_DONTWAIT|MSG_NOSIGNAL;
	//after the linux kernel 5.0, the msghdr struct change msg_iov to msg_iter.    
	// msg.msg_iov->iov_base = (void*) Buffer;
	// msg.msg_iov->iov_len  = (__kernel_size_t)Length;
	

	if (sock->sk)
	{
              KRTSPROXYD_OUT(KERN_ERR "the sock->sk != NULL\n");	
		oldfs = get_fs(); set_fs(KERNEL_DS);
		//after the linux kernel 5.0, the sock_sendmsg function change to two elements.
		//len = sock_sendmsg(sock,&msg,(size_t)(Length));
		len = sock_sendmsg(sock, &msg);
		set_fs(oldfs);
	} else
	{
		return -ECONNRESET;
	}
	
	LeaveFunction("SendBuffer_async");
        KRTSPROXYD_OUT(KERN_INFO "the length of sock_sendmsg is %d\n", len);
	return len;	
}

//receive tcp data handler
int ReceiveBuffer(struct socket *sock,void *my_msg,int size){
	mm_segment_t oldfs;
	struct msghdr msg;
	struct iovec	iov;
	int len;

       EnterFunction("ReceiveBuffer");
       
	if(sock==NULL){
               KRTSPROXYD_OUT(KERN_INFO "sock = NULL\n");
		return -1;
               } 
	msg.msg_name     = 0;
	msg.msg_namelen  = 0;
	//after the linux kernel 5.0, the msghdr struct change msg_iov to msg_iter.
	iov.iov_base = my_msg;
	iov.iov_len  = (size_t)size;
	msg.msg_iter.iov     = &iov;
	//msg.msg_iov	 = &iov;
	//msg.msg_iovlen   = 1;
	msg.msg_control  = NULL;
	msg.msg_controllen = 0;
	msg.msg_flags    = 0;
	// msg.msg_iov->iov_base = my_msg;
	// msg.msg_iov->iov_len  = (size_t)size;
	len = 0;

	oldfs = get_fs(); set_fs(KERNEL_DS);
      
	KRTSPROXYD_OUT(KERN_INFO "before sock_recvmsg\n");
	//after the linux kernel 5.0, the sock_recvmsg function change to three elements.
	//len = sock_recvmsg(sock,&msg,size, MSG_NOSIGNAL|MSG_DONTWAIT);
	len = sock_recvmsg(sock,&msg, MSG_NOSIGNAL|MSG_DONTWAIT);

	set_fs(oldfs);
	KRTSPROXYD_OUT(KERN_INFO "recvmsg returned len=%d\n",len);
	LeaveFunction("ReceiveBuffer");
	return len;

}
