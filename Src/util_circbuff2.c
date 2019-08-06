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

#include "util_circbuff2.h"
#include <string.h>


//==============================================================
//simple circular buffer

//This queue is realized with a fixed-size buffer known at compile-time.
//There are separate code instantiations with that constants and object
//names embedded in the implementation.


//This defines the instance data for the circular buffer.
//'prefix' is a name prefix, so you can have several if you like.
//'type' is the data type; e.g. uint8_t
//'size' is the count of types in the queue.  NOTE:  MUST be power of 2!

/*
For example, the following:
	CIRCBUF_SIMPLE(MyQueue,uint8_t,128)
will cause this API to be realized:

initialize, or reset, the circular buffer
inline unsigned int MyQueue_init ( void );

what is the capacity of the buffer
inline unsigned int MyQueue_capacity ( void );

how many items are in the buffer
inline unsigned int MyQueue_count ( void );

is the buffer empty?
inline int MyQueue_empty ( void );

is the buffer full?
inline int MyQueue_full ( void );

put something in the buffer; true on success, false of fail (full)
int MyQueue_enqueue ( uint8_t val );

take something out of the buffer, if you can, 0 otherwise
uint8_t MyQueue_dequeue();
*/



//This definition is simply to facilitate a macro to downcast to the base type
//and access the derived buffer pointer.  In a perfect world, this will wind up
//creating in no code whatsoever -- just an elaborate cast.
CIRCBUFTYPE(pseudotype,uint8_t,1)
#define DOWNCAST(ptrder) (&((pseudotype_circbuff_t*)(ptrder))->_base)




void circbuff_init ( void* pvCirc )
{
	DOWNCAST(pvCirc)->_nIdxRead = 0;
	DOWNCAST(pvCirc)->_nLength = 0;
#ifdef DEBUG
	DOWNCAST(pvCirc)->_nMaxLength = 0;
#endif
}


unsigned int circbuff_capacity ( void* pvCirc )
{
	return DOWNCAST(pvCirc)->_nSize;
}


unsigned int circbuff_count ( void* pvCirc )
{
	return DOWNCAST(pvCirc)->_nLength;
}


int circbuff_empty ( void* pvCirc )
{
	return 0 == circbuff_count(pvCirc);
}


int circbuff_full ( void* pvCirc )
{
	return circbuff_capacity(pvCirc) == circbuff_count(pvCirc);
}


inline static unsigned int circbuff_idxMod ( void* pvCirc, unsigned int idx )
{
	return idx & (circbuff_capacity(pvCirc)-1);
}


inline static unsigned int circbuff_idxInc ( void* pvCirc, unsigned int index )
{
	return circbuff_idxMod ( pvCirc, index + 1 );
}


int circbuff_enqueue ( void* pvCirc, const void* val )
{
	if ( circbuff_full(pvCirc) )
		return 0;
	memcpy ( (void*)&((pseudotype_circbuff_t*)pvCirc)->_abyBuffer[
				circbuff_idxMod(pvCirc, DOWNCAST(pvCirc)->_nIdxRead + DOWNCAST(pvCirc)->_nLength) *
				DOWNCAST(pvCirc)->_nTypeSize],
			val,
			DOWNCAST(pvCirc)->_nTypeSize );
	++DOWNCAST(pvCirc)->_nLength;
#ifdef DEBUG
	if ( DOWNCAST(pvCirc)->_nLength > DOWNCAST(pvCirc)->_nMaxLength )
		DOWNCAST(pvCirc)->_nMaxLength = DOWNCAST(pvCirc)->_nLength;
#endif
	return 1;
}


void circbuff_dequeue ( void* pvCirc, void* val )
{
	if ( circbuff_empty(pvCirc) )
		return;
	--DOWNCAST(pvCirc)->_nLength;
	memcpy ( val,
			(void*)&((pseudotype_circbuff_t*)pvCirc)->_abyBuffer[
				DOWNCAST(pvCirc)->_nIdxRead *
				DOWNCAST(pvCirc)->_nTypeSize],
			DOWNCAST(pvCirc)->_nTypeSize );
	DOWNCAST(pvCirc)->_nIdxRead = circbuff_idxInc ( pvCirc, DOWNCAST(pvCirc)->_nIdxRead );
}



#ifdef DEBUG
unsigned int circbuff_max ( void* pvCirc )
{
	return DOWNCAST(pvCirc)->_nMaxLength;
}
#endif


