/*

kRtspProxyd

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

//#include <linux/config.h>
//atfer linux kernel 2.6.19, it doesn't have the config.h, so it can include the autoconf.h file. 
#include <linux/kernel.h>
#include <linux/skbuff.h>
//Ryan added this code.
#include <linux/hardirq.h>
//#include <linux/smp_lock.h>
#include <linux/file.h>
#include <asm/uaccess.h>
#include <linux/errno.h>

#include <linux/inet.h> //for in_aton()
#include "structure.h"
#include "prototypes.h"
#include "proxy.h"

extern int errno;
static char *Buffer[CONFIG_KRTSPROXYD_NUMCPU];
#define MAX_LINE_BUFF 256 
#define MAX_CMD_BUFF 128 


void make_socket_nonblocking(struct socket *skt)
{

   //skt->sk->proc |= O_NONBLOCK; //fix me???
	skt->file->f_flags = O_NONBLOCK;
}

static char *find_transport_header(char *inp)
{
	inp = strchr(inp, ';');
	if (inp != NULL)
		inp++;
	return inp;
}


/*
	grab a full line from input
	put into strBuff as 0 terminated
	must maintain the exact eol that the 
	input string has.
	return next position.
*/

char* get_line_str( char* strBuff, const char *input, int buffSize)
{
	const char 	*p = input;
	int		sawEOLChar = 0;
        
        if( strBuff == NULL || input == NULL || buffSize <= 0){
           KRTSPROXYD_OUT(KERN_INFO "error occured in get_line_str!!\n");
           return NULL;
        }
	
       memset(strBuff, 0, buffSize);
	while( *p )
	{
        	if(buffSize <= 0){
                   KRTSPROXYD_OUT(KERN_INFO "error occured in get_line_str!!\n");
                   return NULL;
        	}
		
		if ( *p == '\r' ||  *p == '\n' )
		{	// grab all eol chars
			sawEOLChar = 1;
			*strBuff = *p;
			strBuff++;
			buffSize--;
		
		}
		else if ( sawEOLChar )
		{
			// we saw eol char(s) and now this is not one
			// that means we're done
			break;
		}
		else
		{	
			// grab all line chars
			*strBuff = *p;
			strBuff++;
			buffSize--;
			
		}
	
		p++;
	}
	
	*strBuff = 0;
	
	// we're not going to change the contents
	// of input, but the caller may have the right too.
	return (char*)p;

}

int process_session(rtsp_session *s, const int CPUNR)
{
	int			i, canRead, numDelims = 0;
	char		*pBuf, *p;
	char		temp[RTSP_SESSION_BUF_SIZE];

	char		lineBuff[MAX_LINE_BUFF];
	track_info	*t;
	char        cmd[MAX_CMD_BUFF], *w;
	int           responseHeaderLen = 0;
       char        *startBuff;
       int len = 0;
       int count = 0;
	
       EnterFunction("process_session");
	/* see if we have any commands coming in */
	pBuf = s->cinbuf + s->amtInClientInBuffer;
       canRead = RTSP_SESSION_BUF_SIZE - s->amtInClientInBuffer - 1;

    if((s->state == stRecvClientCommand) && (canRead>0) && !skb_queue_empty(&(s->client_skt->sk->sk_receive_queue))){      
	/* First, read the data */
        len = ReceiveBuffer(s->client_skt, pBuf, canRead);
	if(len < 0) { //error occurred!!
              KRTSPROXYD_OUT(KERN_ERR "failed in recievebuffer()\n");
		switch(errno){
                     case EAGAIN:
                       KRTSPROXYD_OUT(KERN_INFO "EAGAIN\n");
               	       break;
               	case EPIPE: //connection broke
               	case ENOTCONN: //shutdown
               	case ECONNRESET:
                        KRTSPROXYD_OUT(KERN_INFO "EPIPE,ENOTCONN,ECONNRESET\n");
               		s->state = stClientShutdown;
               		break;
               	default:
               		KRTSPROXYD_OUT(KERN_ERR "problem reading from session socket\n");			
               		break;
		}
	}
	else if (len == 0){
			// if readable and # of bytes is 0, then the client has shut down
               	s->state = stClientShutdown;
        }
	else {
			pBuf[len] = '\0';  //terminate it!!! 
                     KRTSPROXYD_OUT(KERN_INFO "the packet is right!the command is %s--ENDING\n", pBuf);
			s->amtInClientInBuffer += len;
	       }
	}
	/* see what we have to do as a result of this or previous commands */
	switch (s->state) {
		case stIdle:
			break;

		case stRecvClientCommand:
			// have we read a full command yet?
			if (s->amtInClientInBuffer == 0 || ! has_two_crlfs(s->cinbuf))
			{
				break;
                        }
				 
                      
			memset(temp, 0, RTSP_SESSION_BUF_SIZE);
                     count++;
			strcpy(temp, s->cinbuf);
			KRTSPROXYD_OUT(KERN_INFO "the CLIENT COMMAND is %s..ENDING\n", temp);
			pBuf = temp;
			
			if ( (p = str_sep(&pBuf, "\r\n")) != NULL ) {
				if (is_command(p, cmd, temp)) 
				{
					if (s->server_address != NULL)
						kfree(s->server_address);
					s->server_address = kmalloc(strlen(temp) + 1, GFP_KERNEL);
					if(!s->server_address) 
						{
						    s->state = stError;
						    return count;
						}
					strcpy(s->server_address, temp);
					
                                   KRTSPROXYD_OUT(KERN_INFO "s->server_address = %s\n", s->server_address);
                                   
					// take port off address (if any)
					if ((w = strchr(s->server_address, ':')) != NULL)
						*w++ = '\0';
					// make an async request for the IP number
					s->tempIP = in_aton(s->server_address); //network byte order
					if (s->tempIP == INADDR_NONE){ //invalid address!!
						send_rtsp_error(s->client_skt, kServerNotFound);
					       s->state = stBadServerName;
					}
					else 
					  s->state = stParseClientCommand;

				}
			}
			else {
				KRTSPROXYD_OUT(KERN_ERR "Couldn't make sense of client command\n");
				s->state = stError;
			}
			break;

		case stParseClientCommand:

			// analysis the client's command
                        KRTSPROXYD_OUT(KERN_INFO "in stParseClientCommand\n");
			count++;
			startBuff= s->soutbuf;
			pBuf = s->cinbuf;
			s->amtInServerOutBuffer = 0; //initialize
						

			KRTSPROXYD_OUT(KERN_INFO "the CLIENT COMMAND being PARSED is %s\n", pBuf);
			while ((p =  str_sep(&pBuf, "\r\n")) != NULL) 
			{
				// Count the empty fields; three in a row is the end of the header
				if (*p == '\0') {
					if (++numDelims == 3)
						break;
					continue;
				}
				else
					numDelims = 0;

				// see if we can snarf our data out of the headers
				if (is_command(p, cmd, temp)) {
					unsigned int	ip;

					// get server address
					if (s->server_address != NULL)
						kfree(s->server_address);
					s->server_address = kmalloc(strlen(temp) + 1, GFP_KERNEL);
					if(!s->server_address) 
						{
						    s->state = stError;
						    return count;
						}

					strcpy(s->server_address, temp);

					// get server port (if any)
					if ((w = strchr(s->server_address, ':')) != NULL) {
						*w++ = '\0';
						s->server_port = atoi(w);
						KRTSPROXYD_OUT(KERN_INFO "kernel server_port is %d\n", s->server_port);
					}
					// check to see if command is pointing to the same server that
					// we're already connected to.
					ip = in_aton(s->server_address); //network byte order
					if ((ip != s->server_ip) && (s->server_skt != NULL)) {
						sock_release(s->server_skt);
						atomic_dec(&gMaxPorts);
						s->server_skt = NULL;
						s->server_ip = ip; //nbo
					}
					if (ip == INADDR_NONE) {
						s->state = stBadServerName;
						return count;
					}
					s->server_ip = ip;
					
						KRTSPROXYD_OUT(KERN_INFO "%s command for server %s:%d (ip %s)\n",
								cmd, s->server_address, s->server_port,
								ntoa(s->server_ip));
					
					s->transaction_type = cmd_to_transaction_type(cmd);

					if (s->transaction_type == ttSetup && has_trackID(p, &i)) {
                                         KRTSPROXYD_OUT(KERN_INFO "s->transaction_type = ttSETUP\n");				
		                            if ((len = track_id_to_idx(s, i)) == -1) {
							len = s->numTracks;
							s->trk[s->numTracks++].ID = i;
						}
						s->cur_trk = len;
					}
				}
				else if (s->transaction_type == ttSetup
					     && has_client_port(p, &(s->trk[s->cur_trk].ClientRTPPort))) 
				    {
					t = s->trk + s->cur_trk;
					KRTSPROXYD_OUT(KERN_INFO "Client ports for track %d are %d-%d\n",
								s->cur_trk, t->ClientRTPPort, t->ClientRTPPort + 1);
				

					// make rtp/rtcp port pair for proxy=>server
					if (make_udp_port_pair(gProxyIP, s->server_ip, &t->RTP_S2P, &t->RTCP_S2P, CPUNR) == -1) {						
						s->server_skt = NULL;
						KRTSPROXYD_OUT(KERN_ERR "Couldn't create udp port pair for proxy=>server\n");
						s->state = stError;
						break;
					}
                                   
					KRTSPROXYD_OUT(KERN_INFO "Created ports for server to proxy on track %d are %d-%d\n",
								s->cur_trk, t->RTP_S2P->port, t->RTCP_S2P->port);
					
					// reconstruct the client port string
					sprintf(temp, "%s%d-%d", p, t->RTP_S2P->port, t->RTCP_S2P->port);
					p = temp;
				}

                    if (0 == strn_casecmp(p, "x-dynamic-rate", 14))// don't send to server not supported.
                         p = "x-dynamic-rate: 0";

				// put the line in the outgoing buffer
				len = strlen(p);
				memcpy(s->soutbuf + s->amtInServerOutBuffer, p, len);
				s->amtInServerOutBuffer += len;
				s->soutbuf[s->amtInServerOutBuffer++] = '\r';
				s->soutbuf[s->amtInServerOutBuffer++] = '\n';
			}

                      if (*(s->cinbuf + s->amtInServerOutBuffer) == 0)
			{
				s->soutbuf[s->amtInServerOutBuffer++] = '\r';
				s->soutbuf[s->amtInServerOutBuffer++] = '\n';
			}
			s->amtInClientInBuffer -= s->amtInServerOutBuffer;
			if (s->amtInClientInBuffer > 0)
			{
				memcpy(s->cinbuf, s->cinbuf + s->amtInServerOutBuffer, s->amtInClientInBuffer);
                            s->cinbuf[s->amtInClientInBuffer] = 0;//mark
                            KRTSPROXYD_OUT(KERN_INFO "in stParseClientCommand memcpy to s->cinbuf=%s\n", s->cinbuf);
			}
			else if (s->amtInClientInBuffer < 0)
				s->amtInClientInBuffer = 0;
        		s->state = stServerTransactionSend;
KRTSPROXYD_OUT(KERN_INFO "set s->state to stServerTransactionSend!\n");
                     if (s->soutbuf[s->amtInServerOutBuffer - 4] != '\r')
			{
				s->soutbuf[s->amtInServerOutBuffer++] = '\r';
				s->soutbuf[s->amtInServerOutBuffer++] = '\n';
			}
			KRTSPROXYD_OUT(KERN_INFO "in stParseClientCommand SEND TO SERVER=%s\n", s->soutbuf);
                    KRTSPROXYD_OUT(KERN_INFO "s->amtInServerOutBuffer = %d\n", s->amtInServerOutBuffer);
			break;

		case stServerTransactionSend:
			// check to see if we've got a connection to the server open
                     KRTSPROXYD_OUT(KERN_INFO "in stServerTransactionSend\n");

			count++;

			if (s->server_skt == NULL) {
				if ((s->server_skt = new_socket_tcp()) == NULL) {
					KRTSPROXYD_OUT(KERN_ERR "Couldn't open a socket to connect to server\n");
					s->state = stError;
					return count;
				}
                             KRTSPROXYD_OUT(KERN_INFO "new_socket_tcp success!\n");
 		    	        set_socket_reuse_address(s->server_skt);
                             //make_socket_nonblocking(s->server_skt);
                             KRTSPROXYD_OUT(KERN_INFO "make_socket_nonblocking() succsess!!!\n");
				 s->server_skt_pending_connection = 1;
                      
				if ((i = connect_to_address(s->server_skt, s->server_ip, s->server_port)) != 0) {
				
						switch (errno) {
						case EISCONN:		/* already connected */
							break;
						case EINPROGRESS:	/* connection can't be completed immediately */
						case EAGAIN:		/* connection can't be completed immediately */
						case EALREADY:		/* previous connection attempt hasn't been completed */
							break;
						default:
							sock_release(s->server_skt);
							atomic_dec(&gMaxPorts);
							s->server_skt = NULL;
							KRTSPROXYD_OUT(KERN_ERR "couldn't connect to server %s",ntoa(s->server_ip));
							s->state = stCantConnectToServer;
							return count;
					}
				}
				
				else
					s->server_skt_pending_connection = 0;
			}

			// check to see if we're connected (writable) and send the command
			if (s->amtInServerOutBuffer  && (s->server_skt_pending_connection == 0 ))
			{
				s->server_skt_pending_connection = 0;
				if ((len = SendBuffer(s->server_skt, s->soutbuf, s->amtInServerOutBuffer)) < 0) {
					switch (errno) {
						case EPIPE:			// connection broke
						case ENOTCONN:		// shut down
						case ECONNRESET:
							s->state = stServerShutdown;
							break;
						case EAGAIN:	// was busy - try again
						case EINTR:		// got interrupted - try again
							break;
						default:
							KRTSPROXYD_OUT(KERN_ERR "writing to server error");
							s->state = stError;
							return count;
						}
				}
				else if (len == 0)
					s->state = stServerShutdown;
				else {
					s->amtInServerOutBuffer -= len;
					if (s->amtInServerOutBuffer == 0)
                                        {
						s->state = stServerTransactionRecv;
                                        }
				}
			}

			break;

		case stServerTransactionRecv:
			// check to see if we've got a response from the server
	  	       KRTSPROXYD_OUT(KERN_INFO "in stServerTransactionRecv\n");
			count++;

                 	if (s->server_skt == NULL) {
				s->state = stServerShutdown;
				break;
			}
			pBuf = s->sinbuf + s->amtInServerInBuffer;

       	       canRead = RTSP_SESSION_BUF_SIZE - s->amtInServerInBuffer - 1;
                     if (canRead > 0 && !skb_queue_empty(&(s->server_skt->sk->sk_receive_queue))) {
				if ((len = ReceiveBuffer(s->server_skt, pBuf, canRead)) < 0) {
				 switch (errno) {
						case EAGAIN:
							
							break;
						case EPIPE:			// connection broke
						case ENOTCONN:		// shut down
						case ECONNRESET:
							s->state = stServerShutdown;
							break;
						default:
							KRTSPROXYD_OUT(KERN_ERR "problems reading from server\n");
							break;
					}
				}
				else {
					pBuf[len] = '\0';
					KRTSPROXYD_OUT(KERN_INFO "read %d bytes from server:%s\n", len, pBuf);
					s->amtInServerInBuffer += len;
				}
			}
			else{ 
                         KRTSPROXYD_OUT(KERN_INFO "Cannot read now\n");
                         break;
                        }
			// DMS - if there is a content-length, make sure we've gotten all that data too.
			if ((s->totalContentLength > 0) && (s->amtInServerInBuffer < s->totalContentLength))
			{	
				break;
			}
                     if ( s->totalContentLength == 0 )
			{	//-rt if totalContentLength != 0,then we must have seen "has_two_crlfs"
				// and already parsed out the content-length header.
				// now we won't be able to find it again becuase str_sep
				// in the parsing code below has already replaced the CRLFs with \0's.
			
				// did we get complete response headers yet?
				responseHeaderLen = has_two_crlfs(s->sinbuf);
				
				if (responseHeaderLen == 0)
				{	
				       KRTSPROXYD_OUT(KERN_INFO "we have not get complete response headers!!\n");
					break;
				}
			}
			
		if ( !s->haveParsedServerReplyHeaders )
		{				
				// we can only do this one time!
				
            			// munge the data for the client, while snarfing what we need
			
			for (pBuf = s->sinbuf; (p = str_sep(&pBuf, "\r\n")) != NULL; ) {
				// Count the empty fields; three in a row is end of the header
				if (*p == '\0') {
					if (++numDelims == 3)
						break;
					continue;
				}
				else
					numDelims = 0;

                            if (0 == strn_casecmp(p, "x-Accept-Dynamic-Rate", 21))
		                 p = "x-Accept-Dynamic-Rate: 0";
					 
				// see if we can snarf any data out of the headers
				if (has_content_length(p, &s->contentLength)) {
					s->totalContentLength = s->contentLength + responseHeaderLen;
				}
				else if (has_sessionID(p, temp)) {
					if (!s->sessionID)
					{
						s->sessionID = kmalloc(strlen(temp) + 1, GFP_KERNEL);
						if(!s->sessionID)
                                             {
                                               KRTSPROXYD_OUT(KERN_ERR "cannot alloate session ID's Mem!\n");
               				     s->state = stError;
               				     return count;
                                             }
						strcpy(s->sessionID, temp);
					}
					else if (str_casecmp(s->sessionID, temp) != 0)
							KRTSPROXYD_OUT(KERN_ERR "Bad session ID in response from server");
				}
				else if (s->transaction_type == ttSetup && has_ports(p, &i, &len)) {
					t = s->trk + s->cur_trk;
					t->ServerRTPPort = len;
					
					KRTSPROXYD_OUT(KERN_INFO "Server ports for track %d are %d-%d \n",
								s->cur_trk, t->ServerRTPPort, t->ServerRTPPort + 1);

					// make rtp/rtcp port pair here proxy=>client
					if (make_udp_port_pair(gProxyIP, s->client_ip, &t->RTP_P2C, 
						&t->RTCP_P2C, CPUNR) == -1) 
					{		
						s->server_skt = NULL;
						KRTSPROXYD_OUT(KERN_INFO "Couldn't create udp port pair for proxy=>client \n");
						s->state = stError;
						break;
					}

					// set up transfer param blocks
					t->RTP_S2C_tpb.status = &s->die;
					t->RTP_S2C_tpb.send_from = t->RTP_P2C;
					t->RTP_S2C_tpb.send_to_ip = s->client_ip;
					t->RTP_S2C_tpb.send_to_port = t->ClientRTPPort;
					strcpy(t->RTP_S2C_tpb.socketName, "RTP Server to Client");
					upon_receipt_from(t->RTP_S2P, s->server_ip, 
						(do_routine)transfer_data, (void *)&(t->RTP_S2C_tpb));

					t->RTCP_S2C_tpb.status = &s->die;
					t->RTCP_S2C_tpb.send_from = t->RTCP_P2C;
					t->RTCP_S2C_tpb.send_to_ip = s->client_ip;
					t->RTCP_S2C_tpb.send_to_port = t->ClientRTPPort + 1;
					strcpy(t->RTCP_S2C_tpb.socketName, "RTCP Server to Client");

					upon_receipt_from(t->RTCP_S2P, s->server_ip, 
						(do_routine)transfer_data, (void *)&(t->RTCP_S2C_tpb));

					t->RTCP_C2S_tpb.status = &s->die;
					t->RTCP_C2S_tpb.send_from = t->RTCP_S2P;
					t->RTCP_C2S_tpb.send_to_ip = s->server_ip;
					t->RTCP_C2S_tpb.send_to_port = t->ServerRTPPort + 1;
					strcpy(t->RTCP_C2S_tpb.socketName,"RTCP Client to Server");

					upon_receipt_from(t->RTCP_P2C, s->client_ip, 
						(do_routine)transfer_data, (void *)&(t->RTCP_C2S_tpb));

					KRTSPROXYD_OUT(KERN_INFO "Created ports for proxy to client on track %d are %d-%d\n",
								s->cur_trk,
								t->RTP_P2C->port, t->RTCP_P2C->port);

					// reconstruct the client;server string
						w = find_transport_header(p);
						if (w != NULL)
							*w = '\0';
						sprintf(temp, "%sclient_port=%d-%d;server_port=%d-%d;source=%s", p,
							t->ClientRTPPort, t->ClientRTPPort+1,
							t->RTP_P2C->port, t->RTCP_P2C->port,
							ntoa(gProxyIP));
						p = temp;
				}

				// put the line in the outgoing buffer
				len = strlen(p);
				if(len + s->amtInClientOutBuffer + 2 > RTSP_SESSION_BUF_SIZE)
				{
				    s->state = stError;
				    return count;
				}
				memcpy(s->coutbuf + s->amtInClientOutBuffer, p, len);
				s->amtInClientOutBuffer += len;
				s->coutbuf[s->amtInClientOutBuffer++] = '\r';
				s->coutbuf[s->amtInClientOutBuffer++] = '\n';
				
				s->responseBodyP = pBuf + 3;
			}
                     if(s->amtInClientOutBuffer +2 > RTSP_SESSION_BUF_SIZE)
                     {
                            KRTSPROXYD_OUT(KERN_ERR "s->amtInClientOutBuffer + 2>RTSP_SESSION_BUF_SIZE\n");
                     	s->state = stError;
                            return count;
                     }
			s->coutbuf[s->amtInClientOutBuffer++] = '\r';
			s->coutbuf[s->amtInClientOutBuffer++] = '\n';
		}

              // the headers are done now.
			s->haveParsedServerReplyHeaders = 1;
			
			// DMS - if there is a content-length, make sure we've gotten all that data too.
			if ((s->totalContentLength > 0) && (s->amtInServerInBuffer < s->totalContentLength))
			{	
				break;
			}
			
	
			pBuf = s->responseBodyP;
			
			// munge and add the content if there is any
			if (s->contentLength) 
			{
				if (s->transaction_type == ttDescribe) 
				{
				       char *nextBuffPos = pBuf;
				       nextBuffPos = get_line_str( lineBuff, nextBuffPos, MAX_LINE_BUFF );
                                //for dbg
                                if(nextBuffPos == NULL){
                                  KRTSPROXYD_OUT(KERN_ERR "nextBuffPos == NULL\n");
                                  s->state = stError;
                                  return count;
                                 }
				while ( *lineBuff )
				{
						//  c=IN IP0 ?
						if (has_IN_IP(lineBuff, temp)) 
						{
						}
				
						// put the line in the outgoing buffer
						len = strlen(lineBuff);
						
					if( len + s->amtInClientOutBuffer > RTSP_SESSION_BUF_SIZE )
					{
					        KRTSPROXYD_OUT(KERN_ERR "len + s->amtInClientOutBuffer > RTSP_SESSION_BUF_SIZE \n");
					        s->state = stError;
					        return count;
					}
	
						memcpy(s->coutbuf + s->amtInClientOutBuffer, lineBuff, len);
						s->amtInClientOutBuffer += len;
						
						nextBuffPos = get_line_str( lineBuff, nextBuffPos, MAX_LINE_BUFF );
                                         if(nextBuffPos == NULL)
                                         	  {//for dbg
                                                s->state = stError;
                                                 return count;
                                             }
				  }
				}
				else {
					if( s->contentLength + s->amtInClientOutBuffer > RTSP_SESSION_BUF_SIZE )
					{
					  KRTSPROXYD_OUT(KERN_ERR "s->contentLength + s->amtInClientOutBuffer > RTSP_SESSION_BUF_SIZE\n"); 
					  s->state=stError;
					  return count;
					}
					memcpy(s->coutbuf + s->amtInClientOutBuffer, pBuf, s->contentLength);
					s->amtInClientOutBuffer += s->contentLength;
				}
			}

			s->state = stSendClientResponse;
			s->amtInServerInBuffer = 0;
			s->totalContentLength = 0;
			s->haveParsedServerReplyHeaders = 0;
			s->contentLength = 0;
			break;

		case stSendClientResponse:
			// check to see that we're still connected (writable) and send the response
                     KRTSPROXYD_OUT(KERN_INFO "in stSendClientResponse\n");
			count++;

			if (s->amtInClientOutBuffer){// && isWritable(s->client_skt)) {
	                        
                        KRTSPROXYD_OUT(KERN_INFO "the Length of the message is %d\n",s->amtInClientOutBuffer);
                        KRTSPROXYD_OUT(KERN_INFO "and the message is %s\n", s->coutbuf); 	
                           if ((len = SendBuffer(s->client_skt, s->coutbuf, s->amtInClientOutBuffer)) < 0) {
          				switch (errno) {
						case EPIPE:			// connection broke
						case ENOTCONN:		// shut down
						case ECONNRESET:
							s->state = stClientShutdown;
							break;
						case EAGAIN:	// was busy - try again
						case EINTR:		// got interrupted - try again
							break;
						default:
							KRTSPROXYD_OUT(KERN_ERR "writing to client error\n");
							s->state = stError;
							return count;
					}
				}
				else {
					s->amtInClientOutBuffer -= len;
					if (s->amtInClientOutBuffer == 0){
						if (s->transaction_type == ttTeardown)
							s->state = stClientShutdown;
						else
							s->state = stRecvClientCommand;
                                                
                                        }
				}
			}

			break;
			

		case stClientShutdown:
			s->die = 1;
                     KRTSPROXYD_OUT(KERN_INFO "stClientShutdown\n");
                     count = 0;
			break;

		case stBadServerName:
                     KRTSPROXYD_OUT(KERN_INFO "stBadServerName\n");
			send_rtsp_error(s->client_skt, kServerNotFound);
			s->state = stServerShutdown;
		       count = 0;
			break;

		case stCantConnectToServer:
                     KRTSPROXYD_OUT(KERN_INFO "stCantConnectToServer\n");
			send_rtsp_error(s->client_skt, kServerNotFound);
			s->state = stServerShutdown;
			count = 0;
			break;
			
		case stServerShutdown:
			KRTSPROXYD_OUT(KERN_ERR "Server shutdown (ip %s)", ntoa(s->server_ip));
			s->die = 1;
			count = 0;
			break;

		case stError:
			send_rtsp_error(s->client_skt, kUnknownError);
			KRTSPROXYD_OUT(KERN_ERR "error condition\n");
			s->die = 1;
			count = 0;
			break;
          }

    LeaveFunction("process_session");

    return count; 
}

int Wait4SessionProcess(const int CPUNR)
{
	struct rtsp_session *CurrentSession,**Prev;
	int count = 0;
	EnterFunction("Wait4SessionProcess");
      
	CurrentSession = threadinfo[CPUNR].RtspSessionQueue;
	
	Prev = &(threadinfo[CPUNR].RtspSessionQueue);
	
	while (CurrentSession!=NULL)
	{
              if(CurrentSession->new_session == 1)
              {
                CurrentSession->new_session = 0;
		if (gUserLimit && (atomic_read(&gNumUsers) > gUserLimit)){
                CurrentSession->die = 1;
                send_rtsp_error(CurrentSession->client_skt, kTooManyUsers);
               }
              }
		/* If the connection is lost, remove from queue */
		
		if ((CurrentSession->client_skt->sk->sk_state != TCP_ESTABLISHED
		    && CurrentSession->client_skt->sk->sk_state != TCP_CLOSE_WAIT)
		    ||CurrentSession->die)
		{
			struct rtsp_session*Next;
			
			Next = CurrentSession->next;
			
			*Prev = CurrentSession->next;
			CurrentSession->next = NULL;
			
                        KRTSPROXYD_OUT(KERN_INFO "cleanupsession!\n");		
			CleanUpSession(CurrentSession, CPUNR);
			CurrentSession = Next;
			continue;
		}
	//service session	
                count += process_session(CurrentSession, CPUNR);

		Prev = &(CurrentSession->next);
		CurrentSession = CurrentSession->next;
	}

	LeaveFunction("Wait4SessionProcess");
	return count;
}

void StopSessionProcess(const int CPUNR)
{
	struct rtsp_session *CurrentSession,*Next;
	
	EnterFunction("StopSessionProcess");
	CurrentSession = threadinfo[CPUNR].RtspSessionQueue;

	while (CurrentSession!=NULL)
	{
		Next = CurrentSession->next;
		CleanUpSession(CurrentSession, CPUNR);
		CurrentSession = Next;		
	}
	
	threadinfo[CPUNR].RtspSessionQueue = NULL;
	
	free_page((unsigned long)Buffer[CPUNR]);
	Buffer[CPUNR]=NULL;
	
	EnterFunction("StopWaitingForHeaders");
}

