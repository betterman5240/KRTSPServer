#ifndef __KRTSPROXYD_DEBUG_H__
#define __KRTSPROXYD_DEBUG_H__
/*output the debug information*/
//#define KRTSPROXYD_DEBUG
#ifdef  KRTSPROXYD_DEBUG
#define KRTSPROXYD_OUT(msg...)					\
	do{									\
		printk(msg);					\
	}while(0)

#define EnterFunction(x)   printk(KERN_INFO "Enter: %s, %s line %i\n",x,__FILE__,__LINE__)
#define LeaveFunction(x)   printk(KERN_INFO "Leave: %s, %s line %i\n",x,__FILE__,__LINE__)

#else

#define KRTSPROXYD_OUT(msg...)  do{ }while(0)
#define EnterFunction(x)  do {} while (0)
#define LeaveFunction(x)  do {} while (0)

#endif

#endif
