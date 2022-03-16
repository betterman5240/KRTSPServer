/*

kRtspProxyd

Permissions/Security functions

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
#include <linux/un.h>
#include <linux/unistd.h>

#include <net/ip.h>
#include <net/sock.h>
#include <net/tcp.h>

#include <asm/atomic.h>
#include <asm/semaphore.h>
#include <asm/processor.h>
#include <asm/uaccess.h>

#include <linux/file.h>

#include "sysctl.h"
#include "security.h"
#include "prototypes.h"
#include "proxy.h"

/* Prototypes */

static struct DynamicString *DynamicList=NULL;

void AddDynamicString(const char *String)
{
	struct DynamicString *Temp;
	
	EnterFunction("AddDynamicString");
	
	Temp = (struct DynamicString*)kmalloc(sizeof(struct DynamicString),(int)GFP_KERNEL);
	
	if (Temp==NULL) 
		return;
		
	memset(Temp->value,0,sizeof(Temp->value));
	strncpy(Temp->value,String,sizeof(Temp->value)-1);
	
	Temp->Next = DynamicList;
	DynamicList = Temp;
	
	LeaveFunction("AddDynamicString");
}

void GetSecureString(char *String)
{
	struct DynamicString *Temp;
	int max;
	
	EnterFunction("GetSecureString");
	
	*String = 0;
	
	memset(String,0,255);
	
	strncpy(String,"Dynamic strings are : -",255);
	Temp = DynamicList;
	while (Temp!=NULL)
	{
		max=253 - strlen(String) - strlen(Temp->value);
		strncat(String,Temp->value,max);
		max=253 - strlen(String) - 3;
		strncat(String,"- -",max);
		Temp = Temp->Next;
	}	
	
	LeaveFunction("GetSecureString");
}
