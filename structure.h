#ifndef _KRTSPROXYD_STRUCTURE_H_
#define _KRTSPROXYD_STRUCTURE_H_

#include <linux/time.h>
#include <linux/wait.h>

/*
struct krtsproxyd_threadinfo represents the 2 queues that 1 thread has to deal with.
It is padded to occupy 1 (Intel) cache-line, to avoid "cacheline-pingpong".
32bytes is a cache-line's size
*/
struct krtsproxyd_threadinfo
{
       struct rtsp_session *RtspSessionQueue; 
	struct shok *gShokQueue;
	
	char  dummy[32-(((sizeof(void *))) << 1) ];  /* Padding for cache-lines */
};



#endif
