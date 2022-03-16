/*
  * 
  * KrtspProxyd
  * 
  * for data switching between the content server and the client
  *
  */

//#include <linux/config.h>
//atfer linux kernel 2.6.19, it doesn't have the config.h, so it can include the autoconf.h file. 
#include <linux/kernel.h>
//Ryan remove these code.
//#include <linux/locks.h>
#include <linux/skbuff.h>

#include <linux/errno.h>

#include <net/tcp.h>

#include <linux/inet.h>

#include <asm/uaccess.h>
//Ryan added this code.
#include <linux/hardirq.h>
//#include <linux/smp_lock.h>

#include "structure.h"
#include "prototypes.h"

extern int errno;

int DataSwitching(const int CPUNR)
{
       struct shok	*cur;
       char	 *buf;
       struct sockaddr_in sin;
       int addr_len = sizeof(sin);
       int count = 0;

       EnterFunction("DataSwitching");

       cur = threadinfo[CPUNR].gShokQueue;

       if((buf = kmalloc((size_t)2048, GFP_KERNEL))==NULL)
       {
            KRTSPROXYD_OUT(KERN_ERR "not enough MEMORY!\n");
            return count;
       }
	while (cur) {
		do_routine	 doit;
		int			 ret = 0;
		unsigned int     fromIP = (unsigned int)0xFFFFFFFF;
		unsigned short fromPort = (unsigned short)0xFFFF;

      again:
              
	       doit = NULL;
             //after linux kernel 5.0, the receive_queue changed to sk_receive_queue.
             //if(!skb_queue_empty(&(cur->socket->sk->receive_queue)))//have we gotten data till now?
	       		if(!skb_queue_empty(&(cur->socket->sk->sk_receive_queue)))//have we gotten data till now?
            	{
	       ret = UdpReceiveBuffer(cur->socket, buf, 2048, 0, (struct sockaddr *)&sin, &addr_len);
		if (ret > 0) {            
			ipList *ipl = NULL;
			
	        KRTSPROXYD_OUT(KERN_INFO "success in RECV data from UDP socket!!and the data length is %d\n", ret);
                     count++;
                     //net byte order!!
	               fromIP = sin.sin_addr.s_addr;
                      fromPort = ntohs(sin.sin_port);
       		ipl = find_ip_in_list(cur->ips, fromIP);
			if (ipl)
                           	doit = ipl->what_to_do;
			if (doit && ipl) {
				ret = (*doit)(ipl->what_to_do_it_with, buf, ret); //send out data
				if ( ret == -1) {
					// what to do about termination, etc.
					KRTSPROXYD_OUT(KERN_ERR "put returns error %d\n", errno);
                                   count--;
				}
				else if (ret == 0) {
					// client or server whatever died
					if (((trans_pb*)(ipl->what_to_do_it_with))->status)
						*(((trans_pb*)(ipl->what_to_do_it_with))->status) = 1;
			              KRTSPROXYD_OUT(KERN_INFO "client/server died\n");
       				count--;
                                 }
				// what to do about incomplete packet transmission
			}
			goto again;
		}
		else if (ret< 0) {
			if (errno != 11) //not eagain
				KRTSPROXYD_OUT(KERN_ERR "recv_udp returns errno %d\n", errno);
		}
		cur = cur->next;
          }
          else 
          	cur = cur->next;
	}
       kfree(buf);
       LeaveFunction("DataSwitching"); 
	return count;
}


