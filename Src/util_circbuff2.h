//========================================================================
//utilities for defining circular buffers
//impl 2; the use of 'stuct' keeps data members together, and generates
//slightly smaller code.

//This is implemented as macros, so as to be useful in C (where we don't
//have templates as in C++).

//This implementation takes a somewhat more object-oriented approach, so
//that other implementations may be more generalized and factored (by being
//albe to have a 'this' pointer to queue).

//These implementations perform no locking, so you will need to do that
//in your own code at the appropriate time.

#ifndef __UTIL_CIRCBUFF2_H
#define __UTIL_CIRCBUFF2_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>


//==============================================================
//simple circular buffer

//This queue is realized with a fixed-size buffer known at compile-time.


//This defines the instance data for the circular buffer.
//'prefix' is a name prefix, so you can have several if you like.
//'type' is the data type; e.g. uint8_t
//'size' is the count of types in the queue.  NOTE:  MUST be power of 2!

/*
For example, the following:
	CIRCBUF(MyQueue,uint8_t,128)
	
	XXX
*/


/*
the implementation; a macro to make it template-esque for C.
*/

//the base type consists of indices, size, and optional debug members
typedef struct circbuff_t circbuff_t;
struct circbuff_t
{
	volatile unsigned int _nIdxRead;
	volatile unsigned int _nLength;
	const unsigned int _nSize;
	const unsigned int _nTypeSize;
#ifdef DEBUG
	volatile unsigned int _nMaxLength;
#endif
};


//the derived type consists of the base type, with the buffer following
#define CIRCBUFTYPE(instance,type,size)	\
typedef struct instance##_circbuff_t instance##_circbuff_t;	\
struct instance##_circbuff_t	\
{	\
volatile circbuff_t _base;	\
volatile uint8_t _abyBuffer[size*sizeof(type)];	\
};


//the instance data is initialized with some critical size params
#define CIRCBUFINST(instance,type,size)	\
instance##_circbuff_t instance =	\
{	\
	{ 0, 0, size, sizeof(type) }	\
};


//you can declare the type and instance in one go, which is probably the usual case
#define CIRCBUF(instance,type,size)	\
CIRCBUFTYPE(instance,type,size)	\
CIRCBUFINST(instance,type,size)



void circbuff_init ( void* pvCirc );
unsigned int circbuff_capacity ( void* pvCirc );
unsigned int circbuff_count ( void* pvCirc );
int circbuff_empty ( void* pvCirc );
int circbuff_full ( void* pvCirc );
int circbuff_enqueue ( void* pvCirc, const void* val );
void circbuff_dequeue ( void* pvCirc, void* val );

#ifdef DEBUG
unsigned int circbuff_max ( void* pvCirc );
#endif



#ifdef __cplusplus
}
#endif

#endif
