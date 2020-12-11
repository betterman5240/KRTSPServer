#ifndef _KRTSPROXYD_SECURITY_H
#define _KRTSPROXYD_SECURITY_H

struct DynamicString;

struct DynamicString
{
	struct DynamicString* Next;
	char value[32-sizeof(void*)];  /* fill 1 cache-line */
};

#endif
