/*
   Copyright 2013, John Howard

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#include <assert.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <stddef.h>

/****************************************************/

#include "dp_vnum.h"

#define VERSIONED_NAME "ohdp " DP_VNUM

/****************************************************/

#ifdef _WIN32
	#include <sys/types.h>
	#include <sys/stat.h>
	#include <fcntl.h>
	#include <io.h>
	#include <float.h>

	#define ALIGN1 __declspec(align(1))
	#define ALIGN16 __declspec(align(16))
	#define ALIGN32 __declspec(align(32))

	#define dup2 _dup2
	#define dup _dup
	#define fileno _fileno
	#define strcasecmp _stricmp
	#define isnanf _isnan
	#define isinff(z) (!_finite(z))
	#define copysign _copysign

	#pragma warning(disable:4305 4244 4996)
	#ifndef _DEBUG
		#define DP_INLINE  __inline
	#else
		#include <CRTDBG.h>
		#define DP_INLINE
	#endif

	#define NAN (log(-1.0))

	#ifdef _M_X64
		#define BREAK do{ _CrtDbgBreak(); } while(0)
	#else
		#define BREAK do{ __asm int 3; } while(0)
	#endif

	#ifdef _WINDDK
		#define CDECLCALL __cdecl
	#else
		#define CDECLCALL
	#endif
	
	#ifdef _WIN32DLL
		#ifdef _WINDDK
			#define EXTERNC
		#else
			#define EXTERNC extern "C"
		#endif
		
		#if 0
			/* double FLOAT is better compatability with external tools */
			#define FLOAT double
			#define modff modf
		#else
			/* single FLOAT saves space */
			#define FLOAT float
		#endif

		#ifndef _DEBUG
			#define PRINT_STDERR( _sz ) { ; }
			#define PRINT_STDOUT( _sz ) { ; }
		#else
			extern "C" void __stdcall OutputDebugStringA( char* );
			#define PRINT_STDERR( _sz ) do { OutputDebugStringA( _sz ); } while(0);
			#define PRINT_STDOUT( _sz ) do { OutputDebugStringA( _sz ); } while(0);
		#endif
	#else
		#define EXTERNC
		#define FLOAT float

		#define PRINT_STDERR( _sz ) do { fprintf( stderr, _sz ); } while(0);
		#define PRINT_STDOUT( _sz ) do { fprintf( stdout, _sz ); } while(0);
	#endif
#else
	#include <unistd.h>

	#define ALIGN1 __attribute__((aligned(1)))
	#define ALIGN16 __attribute__((aligned(16)))
	#define ALIGN32 __attribute__((aligned(32)))

	#ifndef _DEBUG
		#define DP_INLINE  inline static
	#else
		#define DP_INLINE
	#endif
	#define EXTERNC
	#define FLOAT float

	#define PRINT_STDERR( _sz ) do { fprintf( stderr, _sz ); } while(0);
	#define PRINT_STDOUT( _sz ) do { fprintf( stdout, _sz ); } while(0);
#endif

//  -march=pentium4 -msse3
// #include <xmmintrin.h>
// #include <pmmintrin.h>

#define CHAR char
#define UINT8 unsigned char
#define UINT16 unsigned short int
#define UINT32 unsigned long int
#define INT32 long int
#define PVOID void*

/**********************************************/

static UINT32 s_uException = 0;
EXTERNC void ex_clear() { s_uException = 0; }
EXTERNC void ex_set() { s_uException = 1; }
EXTERNC UINT32 ex_didFail() { return s_uException; }

#ifdef _DEBUG
#define ASSERT( x ) do { if( !(x) ) { BREAK; } } while(0)
	#define VERIFY( x ) do { if( !ex_didFail() ) { if( !(x) ) { assert( x ); ex_set(); } } } while(0)
#else
	#define ASSERT( x ) do {} while(0)
	#define VERIFY( x ) do { if( !ex_didFail() ) { if( !(x) ) { ex_set(); } } } while(0)
#endif

/**********************************************/

#define MAX_UINT8  ((UINT8)(0-1))
#define MAX_UINT16 ((UINT16)(0-1))
#define MAX_UINT32 ((UINT32)(0-1))
#define MIN(a,b)   ( ( (a) < (b) ) ? (a) : (b) )
#define MAX(a,b)   ( ( (a) > (b) ) ? (a) : (b) )

//#define ENABLE_OUTOFLINE_NUMERICAL_CLEANUP
#define ENABLE_INLINE_NUMERICAL_CLEANUP
//#define ENABLE_SPECIFIED_MINMAX_STATE // buggy, don't enable yet
#define ENABLE_PRIORITIZEDSTAGES
#define ENABLE_FORWARDPASS_PRIORITY_ORDER_SCAN // provides improvement?
#define ENABLE_NONMAPPABLEELEMENTS
#define ENABLE_CAPACITY_SCALING
#define ENABLE_NEWHEADSCALING
#define ENABLE_SYMMETRIC_FINDLARGER
#define ENABLE_MALLOC_CACHE
#define ENABLE_NEW_CALCS
#define ENABLE_NEW_DYNLOSS
#define ENABLE_ADAPTIVEFAILURE
#define ENABLE_HK_SCALING // changes solution, but don't disable yet

//#define DEBUG_BACKWARDPASS
//#define DEBUG_FORWARDPASS
//#define DEBUG_INITPASS
//#define DEBUG_MEMORY
//#define DEBUG_RELATION_FINDLARGER_SIDEDNESS

/**********************************************/
/* constants */

/* I'll tell you why this is not safe to change: the dp
 * assumes memory is clear and zero when computing the
 * solution. The solution computing process only writes
 * non-zero solutions to memory, leaving untouched memory
 * left as 'no solution' ie 0.
 */
#define DP_MALLOC_BYTE 0x00

#define DP_OPTIMIZE_FOR_POWER (1)
#define DP_OPTIMIZE_FOR_FLOW  (2)

#define DP_WEIGHT_RELATIVE		(1<<30)
#define DP_WEIGHT_CODEMASK		((1<<16) - 1)
#define DP_WEIGHT_DEFAULT		(0)
#define DP_WEIGHT_EQUAL			(1)
#define DP_WEIGHT_MAXPOWER		(2) /* preference given to larger units */
#define DP_WEIGHT_MAXFLOW		(3)
#define DP_WEIGHT_MINPOWER		(4) /* preference given to smaller units */
#define DP_WEIGHT_MINFLOW		(5)

/**********************************************/
/* conversion coefs from discharge to power (KW) */

#define DP_CONV_UNDEFINED (0)
#define DP_CONV_IMPERIAL  (62.4 /* POUNDSPERCUBICFT */ * 0.746 /* KWPERHP */ / 550 /* FTPOUNDSPERHP */ ) /* from cfs */
#define DP_CONV_METRIC    (1000.0 /* WATERDENSITYINKGPERM3 */ * 9.81 /* ACCELDUETOGRAVITY */ / 1000 /* WATTSPERKW */ ) /* from cms */

#define DP_UNITS_UNDEFINED (0)
#define DP_UNITS_IMPERIAL  (1)
#define DP_UNITS_METRIC    (2)

#define DP_UNIT_LENGTH	(0)
#define DP_UNIT_FLOW	(1)
#define DP_UNIT_POWER	(2)

/**********************************************/

#define DP_CONFIG_SIGFIGS 2

/**********************************************/

EXTERNC CHAR* dp_gsVERSION = DP_VNUM;

CHAR* gsUniversalUnits[] = { "kw", "mw", 0 };
CHAR* gsImperialUnits[] = { "ft", "cfs", "kcfs", 0 };
CHAR* gsMetricUnits[] = { "m", "cms", 0 };
CHAR* gszHuh = " ? ";

UINT32 guMWMode = 0; /* 0 or 1 switches between kw/cfs and mw/kcfs */
UINT32 guUnitMode = DP_UNITS_UNDEFINED;

EXTERNC FLOAT gfConvFactor = DP_CONV_UNDEFINED;
EXTERNC FLOAT gfPlantLossCoef = 0.0; /* % power production loss from hydraulic, generator and transmission systems to the calib*/
EXTERNC FLOAT gfCoordinationFactorA = 0.95; /* sets % of capacity @ optimum to be leftover for later stages in the backpass */
EXTERNC FLOAT gfCoordinationFactorB = 0.6; /* sets minimum operation as a % of max capacity during backpass */

/**********************************************/

UINT32 guDebugMode = 0;
UINT32 guMALLOC = 0;
UINT32 guMemCacheOn = 0;

/**********************************************/

#define DP_MAX_PRINTBUF_CHARS 512
CHAR gcPrintBuff[ DP_MAX_PRINTBUF_CHARS + 1 ];

/**********************************************/

#ifdef _WINDDK

DP_INLINE float modff_ddkhack( float a, float* pb )
{
	float cF, dF, eF;
	double cD, dD, eD;
	//
	cF = a;
	eF = modff( cF, &dF );
	//
	cD = (double)a;
//	eD = modff( cD, &dD ); // x86?
	eD = modf( cD, &dD ); // x64?
	*pb = (float)dD;
	return (float)eD;
}
#undef modff
#define modff modff_ddkhack

#endif //_WINDDK

/**********************************************/

void* dp_mem_alloc( int _n, char* _f, int _l)
{
	void* _p = ( _n <= 0 ) ? 0 : (void*)malloc( _n );
	assert( _p );
	if( !_p )
	{
		sprintf( gcPrintBuff, "%s:%d (" VERSIONED_NAME  ") malloc failure.\n", _f, _l );
		PRINT_STDERR( gcPrintBuff );
		exit(-1);
	}
	memset( _p, DP_MALLOC_BYTE, _n );
	guMALLOC += _n;
	return _p;
}

void dp_mem_free( void* _p )
{
#ifdef DEBUG_MEMORY
	sprintf( gcPrintBuff, "memory: deallocation of from %X\n", _p );
	PRINT_STDERR( gcPrintBuff );
#endif //DEBUG_MEMORY
	free( _p );
}

#define DP_FREE( _p ) do{ if( _p ) { dp_mem_free( _p ); _p = 0; } }while(0)
#define DP_MALLOC( _p, _t, _n, _f, _l ) do{ assert( !_p ); if( _p ) { free( _p ); } _p = (_t*)dp_mem_alloc( sizeof(_t) * _n, _f, _l ); }while(0)

/**********************************************/

#define DP_CACHE_ADDR_INFO( _pData ) ((int*)(   ((int*)_pData)-1   ))
#define DP_CACHE_ADDR_DATA( _pInfo ) ((void*)(   ((int*)_pInfo)+1   ))

void* dp_cache_alloc( void* _pData, int _n, char* _f, int _l)
{
	int* _pInfo = _pData ? DP_CACHE_ADDR_INFO( _pData ) : 0;
	assert( _n > 0 );
	if( _pData )
	{
		if( guMemCacheOn && ( *_pInfo >= _n ) ) /* == or >= ? */
		{
			goto clearandreturn;
		}
		_pInfo = DP_CACHE_ADDR_INFO( _pData );
		free( _pInfo );
		guMALLOC -= _n + sizeof(int);
	}
	_pInfo = (int*)malloc( _n + sizeof(int) ); assert( _pInfo );
	_pData = DP_CACHE_ADDR_DATA( _pInfo );
	guMALLOC += _n + sizeof(int);
	*_pInfo = _n;
#ifdef DEBUG_MEMORY
	sprintf( gcPrintBuff, "memory: allocation of %d at %X\n", _n, _pData );
	PRINT_STDERR( gcPrintBuff );
#endif //DEBUG_MEMORY
clearandreturn:
	memset( _pData, DP_MALLOC_BYTE, _n );
	return _pData;
}

void* dp_cache_free( void* _pData )
{
	if( !_pData ) { return 0; }

	{
		int* _pInfo = DP_CACHE_ADDR_INFO( _pData );
		int _n = *_pInfo;

		memset( _pData, DP_MALLOC_BYTE, _n );
		if( guMemCacheOn ) { return _pData; }

#ifdef DEBUG_MEMORY
		sprintf( gcPrintBuff, "memory: deallocation at %X\n", _pData );
		PRINT_STDERR( gcPrintBuff );
#endif //DEBUG_MEMORY

		free( _pInfo );
		guMALLOC -= _n + sizeof(int);
		return 0;
	}
}

#ifdef ENABLE_MALLOC_CACHE
	#define DP_FREE_CACHE( _p, _t ) do{ _p = (_t*)dp_cache_free( _p ); }while(0)
	#define DP_MALLOC_CACHE( _p, _t, _n, _f, _l ) do{ _p = (_t*)dp_cache_alloc( _p, sizeof(_t) * _n, _f, _l ); }while(0)
#else
	#define DP_FREE_CACHE DP_FREE
	#define DP_MALLOC_CACHE DP_MALLOC
#endif

/*********************************************/

/*********************************************/

/* forward references */
EXTERNC void dp_cleanup();

void parse_freeallblocks();
void dp_cleanup_fatal();
char* char_units( UINT32 iUnitType );
void ui_parse_units( char* szType );

/*********************************************/

/* strtok eraser - for backtracking! */
/* use like:
	CHAR* tokRestore = tok;
	tok = strtok( 0, " " );
	if( strcasecmp( tok, "OptionalKeyword" ) == 0 ) {
		// do something
	} else {
		// whoops, OptionalKeyword wasn't there!
		tok = strtok( strtokRestore( tok, tokRestore, ' ' ), " " );
	}
 */
CHAR* strtokRestore( CHAR* pCurrent, CHAR* pRestore, CHAR cEraser )
{
	CHAR* p = pRestore;
	while( 1 )
	{
		if( !(*p) )
		{
			*p = cEraser;
			if( p > pCurrent ) { break; }
		}
		p++;
	}
	return pRestore;
}

char* char_units( UINT32 iUnitType )
{
	if( guUnitMode == DP_UNITS_UNDEFINED )
	{
		return gszHuh;
	} else {
		if( iUnitType == DP_UNIT_POWER )
		{
			return gsUniversalUnits[ guMWMode ];
		} else {
			if( guUnitMode == DP_UNITS_METRIC )
				return gsMetricUnits[ iUnitType ];
			else
				return gsImperialUnits[ iUnitType + guMWMode ];
		}
	}
}

/*********************************************/

CHAR* float_format( FLOAT aFloat )
{
	FLOAT fInt;
	FLOAT fAbsFloat = fabs(aFloat);
	FLOAT fFrac = modff( aFloat, &fInt );
	FLOAT fAbsFrac = fabs(fFrac);
    if( isnanf(aFloat) ) {				return "    NAN"; /* hey, that's not a number! */
    } else if( isinff(aFloat) ) {		return "    INF"; /* hey, that's really big! */
    } else if( fAbsFloat > 10000000 ) {	return "%1.4e"; /* hey, that's big! */
	} else if( fAbsFrac < 0.00001 ){	return "%7.0f"; /* effectively a whole number */
    } else if( fAbsFloat >= 1000000 ) {	return "%.0f";
    } else if( fAbsFloat >= 100000 ) {	return "%7.0f";
    } else if( fAbsFloat >= 10000 ) {	return "%7.1f";
	} else if( fAbsFloat >= 1000 ) {	return "%7.2f";
	} else if( fAbsFloat >= 100 ) {		return "%7.3f";
	} else if( fAbsFloat >= 10 ) {		return "%7.4f";
	} else if( fAbsFloat >= 0.001 ) {	return "%7.5f";
	} else if( fAbsFloat < 1.0e-10 ) {	return "%5.1f";
	} else {							return "%7.8f"; }
}

FLOAT float_round( FLOAT fVal, UINT32 uSigfigs )
{
	FLOAT fWholeDigits = floor( log10( fabs( fVal ) ) ) + 1;
	FLOAT fSigNif = fabs( fVal ) / powf( 10, fWholeDigits - uSigfigs );
	FLOAT fIntPart = 0;
	modff( fabs( fSigNif ) + 0.5, &fIntPart ); /* round to number with larger magnitude, -ve or +ve */
	return copysign( fIntPart * powf( 10, fWholeDigits - uSigfigs ), fVal );
}

void float_print( FILE* pFile, FLOAT aFloat, CHAR cDelim )
{
	fprintf( pFile, float_format( aFloat ), aFloat ); putc( cDelim, pFile );
}

// version 5
DP_INLINE FLOAT float_interpolate5( FLOAT v, FLOAT f0, FLOAT t0, FLOAT f1, FLOAT t1 )
{
	return t0 + ((v - f0) * (t1 - t0)) / (f1 - f0);
}

// Jim Gordon Equation
DP_INLINE FLOAT float_EffAdjFactor( float hNet, float hRated, float fExp )
{
	FLOAT fEffAdj = 0.5 * powf( fabs( hNet - hRated ) / hRated, fExp );
	fEffAdj = MIN( 0.20, fEffAdj ); /* sanity check: max eff loss from head is 20% */
	return ( hNet < hRated ) ? 1.0 - fEffAdj : 1.0 + fEffAdj;
}

void arr_float_print( FILE* pFile, FLOAT* pFloat, UINT32 num, CHAR cDelim )
{
	UINT16 i;
	for( i=0; i<num; i++ )
	{
		sprintf( gcPrintBuff, float_format( pFloat[i] ), pFloat[i] );
		PRINT_STDOUT( gcPrintBuff );
		sprintf( gcPrintBuff, "%c", (i==(num-1)) ? '\n' : cDelim );
		PRINT_STDOUT( gcPrintBuff );
	}
}

void arr_float_print_t( FILE* pFile, FLOAT* pFloat, UINT32 num, UINT32 stride, CHAR cDelim )
{
	UINT16 i;
	for( i=0; i<num; i++ ) { fprintf( pFile, float_format( pFloat[i*stride] ), pFloat[i*stride] ); putc( cDelim, pFile ); }
}

void arr_float_print_t2( FILE* pFile, FLOAT* pFloat, UINT32 num, UINT32 stride, CHAR cDelim, CHAR cEOL )
{
	UINT16 i;
	for( i=0; i<num; i++ )
	{
		fprintf( pFile, float_format( pFloat[i*stride] ), pFloat[i*stride] );
		if( (i < num) || (cEOL == 0x00) ) { putc( cDelim, pFile ); } else { putc( cEOL, pFile ); }
	}
}

void arr_float_copy( FLOAT* pDest, FLOAT* pSrc, UINT32 num )
{
	UINT16 i;
	for( i=0; i<num; i++ ) { pDest[i] = pSrc[i]; }
}

void arr_float_sum( FLOAT *pfSum, FLOAT *pSrc, UINT32 num, UINT32 stride )
{
	UINT16 i;
	FLOAT fSum = 0;
	for( i=0; i<num; i++ ) { fSum += pSrc[ i * stride ]; }
	*pfSum = fSum;
}

void arr_float_mark_nan_as_zero( FLOAT* pSrc, UINT32 num )
{
	UINT16 i;
	for( i=0; i<num; i++ ) { if( isnanf( pSrc[ i ] ) ) pSrc[ i ] = 0; }
}

void arr_float_mark_inf_as_zero( FLOAT* pSrc, UINT32 num )
{
	UINT16 i;
	for( i=0; i<num; i++ ) { if( isinff( pSrc[ i ] ) ) pSrc[ i ] = 0; }
}

void arr_float_div2( FLOAT* pDest, FLOAT* pSrcN, FLOAT* pSrcD, UINT32 num )
{
	UINT16 i;
	for( i=0; i<num; i++ )
	{
#ifdef ENABLE_INLINE_NUMERICAL_CLEANUP
		FLOAT f = pSrcN[i] / pSrcD[i];
		pDest[i] = ( isnanf( f ) || isinff( f ) ? 0 : f );
#else
		pDest[i] = pSrcN[i] / pSrcD[i];
#endif
	}
}

void arr_float_step( FLOAT* pArr, FLOAT fOffset, FLOAT fIncr, UINT32 num )
{
	UINT16 i;
	for( i=0; i<num; i++ ) { pArr[i] = i * fIncr + fOffset; }
}

void arr_float_divide( FLOAT* pDest, FLOAT fValue, UINT32 num, UINT32 stride )
{
	UINT16 i;
	for( i=0; i<num; i++ ) { pDest[ i * stride ] /= fValue; }
}

void arr_float_count_nonzeropos( FLOAT* pCount, FLOAT* pSrc, UINT32 num, UINT32 stride )
{
	FLOAT fTol = 1e-10;
	UINT16 i;
	for( i=0; i<num; i++ ) { *pCount += ( pSrc[ i * stride ] < fTol ) ? 1 : 0; }
}

void arr_float_multiply( FLOAT* pDest, FLOAT fValue, UINT32 num, UINT32 stride )
{
	UINT16 i;
	for( i=0; i<num; i++ ) { pDest[ i * stride ] *= fValue; }
}

void arr_float_sum_stages_pos( FLOAT* pDest, FLOAT* pSrc, UINT32 uStates, UINT32 uStages )
{
	FLOAT fTol = 1E-3;
	UINT16 i, j;
	for( i=0; i<uStates; i++ )
	{
		pDest[i] = 0;
		for( j=0; j<uStages; j++ )
		{
			FLOAT v = pSrc[ j * uStates + i ];
#ifdef ENABLE_INLINE_NUMERICAL_CLEANUP
			if( !isnanf( v ) && !isinff( v ) )
#endif
			{
				if( v > fTol ) { pDest[i] += v; }
			}
		}
	}
}

void arr_float_average_stages_pos( FLOAT* pDest, FLOAT* pSrc, UINT32 uStates, UINT32 uStages )
{
	FLOAT fTol = 1E-3;
	UINT16 i, j, k;
	for( i=0; i<uStates; i++ )
	{
		k = 0;
		pDest[i] = 0;
		for( j=0; j<uStages; j++ )
		{
			FLOAT v = pSrc[ j * uStates + i ];
#ifdef ENABLE_INLINE_NUMERICAL_CLEANUP
			if( !isnanf( v ) && !isinff( v ) )
#endif
			{
				if( v > fTol ) { pDest[i] += v; k++; }
			}
		}
		if( k > 0 ) { pDest[i] /= k; }
	}
}

void arr_float_transpose( FLOAT* pDest, FLOAT* pSrc, UINT32 uVectSize, UINT32 uNVects )
{
	UINT16 v, nv;
	for( nv=0; nv<uNVects; nv++ )
		for( v=0; v<uVectSize; v++ )
			{ pDest[ uNVects * v + nv ] = pSrc[ uVectSize * nv + v ]; }
}

void arr_float_set( FLOAT* pDest, FLOAT fValue, UINT32 num )
{
	UINT16 i;
	for( i=0; i<num; i++ ) { pDest[i] = fValue; }
}

void arr_float_clear_conditional( FLOAT* pDest, UINT16* pTest, UINT32 num )
{
	UINT16 i;
	for( i=0; i<num; i++ ) { if( pTest[i] == 0 ) { pDest[i] = 0; } }
}

void arr_float_scale( FLOAT* pArr, FLOAT fValue, UINT32 num )
{
	UINT16 i;
	for( i=0; i<num; i++ ) { pArr[i] = pArr[i] * fValue; }
}

/* applies pMap to pSrc to accomplish the copy to pDest */
DP_INLINE void arr_float_copy_indirect( FLOAT* pDest, FLOAT* pSrc, UINT16* pMap, UINT32 num )
{
	UINT16 i;
	for( i=0; i<num; i++ ) {
#ifdef ENABLE_NONMAPPABLEELEMENTS
		if( pMap[ i ] != MAX_UINT16 )
		{
#else
		{
#endif
			pDest[i] = pSrc[ pMap[ i ] ];
		}
	}
}

DP_INLINE void arr_float_max_nostride( UINT16* puIndex, FLOAT* pFloat, UINT32 count )
{
	FLOAT fMaxValue = pFloat[0];
	UINT16 uIndex = 0;
	UINT16 i;
	for( i=1; i<count; i++ )
	{
		FLOAT v = pFloat[ i ];
		if( v > fMaxValue ) { uIndex = i; fMaxValue = v; }
	}
	*puIndex = uIndex;
}

DP_INLINE void arr_float_max( UINT16* puIndex, FLOAT* pFloat, UINT32 count, UINT32 uStride )
{
	FLOAT fMaxValue = pFloat[0];
	UINT16 uIndex = 0;
	UINT16 i;
	for( i=1; i<count; i++ )
	{
		FLOAT v = pFloat[ i * uStride ];
		if( v > fMaxValue ) { uIndex = i; fMaxValue = v; }
	}
	*puIndex = uIndex;
}

DP_INLINE void arr_float_max_nostride_count_pos( UINT16* puIndex, FLOAT* pFloat, UINT8* pCount, UINT32 n )
{
#if 0
	FLOAT fTol = 1e-8;
#endif
	FLOAT fMaxValue = 0;
	UINT16 uIndex = MAX_UINT16;
	UINT16 i;
	for( i=0; i<n; i++ )
	{
#if 1
		if( pCount[ i ] > 0 )
#else
		if( pCount[ i ] > fTol )
#endif
		{
			FLOAT v = pFloat[ i ] / pCount[ i ];
			if( v > fMaxValue ) { uIndex = i; fMaxValue = v; }
		}
	}
	if( uIndex != MAX_UINT16 ) { *puIndex = uIndex; }
}

DP_INLINE void arr_float_max_count_pos( UINT16* puIndex, FLOAT* pFloat, UINT8* pCount, UINT32 n, UINT32 uStride )
{
#if 0
	FLOAT fTol = 1e-8;
#endif
	FLOAT fMaxValue = 0;
	UINT16 uIndex = MAX_UINT16;
	UINT16 i;
	for( i=0; i<n; i++ )
	{
#if 1
		if( pCount[ i * uStride ] > 0 )
#else
		if( pCount[ i * uStride ] > fTol )
#endif
		{
			FLOAT v = pFloat[ i * uStride ] / pCount[ i * uStride ];
			if( v > fMaxValue ) { uIndex = i; fMaxValue = v; }
		}
	}
	if( uIndex != MAX_UINT16 ) { *puIndex = uIndex; }
}

DP_INLINE void arr_float_tol( UINT16* puIndex, FLOAT* pFloat, UINT16 uOptimum, FLOAT fTol, UINT32 count, UINT32 uStride )
{
	FLOAT fOptimum = pFloat[ uOptimum ];
	FLOAT fTolValue = fOptimum;
	UINT16 uIndex = uOptimum;
	UINT16 i;
	for( i = uOptimum-1; i != MAX_UINT16; i-- )
	{
		FLOAT v = pFloat[ i * uStride ];
		if( v >= fOptimum * fTol ) { uIndex = i; fTolValue = v; } else { break; }
	}
	*puIndex = uIndex;
}

DP_INLINE void arr_float_tol_nostride( UINT16* puIndex, FLOAT* pFloat, UINT16 uOptimum, FLOAT fTol, UINT32 count )
{
	FLOAT fOptimum = pFloat[ uOptimum ];
	FLOAT fTolValue = fOptimum;
	UINT16 uIndex = uOptimum;
	UINT16 i;
	for( i = uOptimum-1; i != MAX_UINT16; i-- )
	{
		FLOAT v = pFloat[ i ];
		if( v >= fOptimum * fTol ) { uIndex = i; fTolValue = v; } else { break; }
	}
	*puIndex = uIndex;
}

DP_INLINE void arr_float_last_nonzero( UINT16* puIndex, FLOAT* pFloat, UINT32 num, UINT32 uStride )
{
	FLOAT fTol = 1E-6;
	UINT16 uIndex = MAX_UINT16;
	UINT16 i;
	for( i=0; i<num; i++ )
	{
		FLOAT v = pFloat[ i * uStride ];
		if( v > fTol ) { uIndex = i; }
	}
	*puIndex = uIndex;
}

DP_INLINE void arr_float_last_nonzero_nostride( UINT16* puIndex, FLOAT* pFloat, UINT32 num )
{
	FLOAT fTol = 1E-6;
	UINT16 uIndex = MAX_UINT16;
	UINT16 i;
	for( i=0; i<num; i++ )
	{
		FLOAT v = pFloat[ i ];
		if( v > fTol ) { uIndex = i; }
	}
	*puIndex = uIndex;
}

DP_INLINE void arr_float_max_unmarked_prioritized_count(
	UINT16* puStage, FLOAT* pValues, UINT8* pCounts, UINT8* pMarklist, UINT16* pPriorityOrder, UINT32 uStages, UINT32 uStride
)
{
	FLOAT fNextBestValue = 0;
	UINT16 uNextBestStage = MAX_UINT16; // init to 'error state'
	UINT32 s;
#ifdef ENABLE_FORWARDPASS_PRIORITY_ORDER_SCAN
	for( s = uStages - 1; s != MAX_UINT32; s--)
#else
	for( s = 0; s < uStages; s++)
#endif
	{
		UINT16 u16PrioritizedStage = pPriorityOrder[ s ];
		UINT8 u8 = pCounts[ u16PrioritizedStage * uStride ]; // TODO: cache ick
		if( u8 > 0 && (pMarklist[ u16PrioritizedStage ] != MAX_UINT8) )
		{
			FLOAT v = pValues[ u16PrioritizedStage * uStride ] / u8; // TODO: cache ick
			if( v > fNextBestValue )
			{
				fNextBestValue = v;
				uNextBestStage = u16PrioritizedStage;
			}
		}
	}
	if( uNextBestStage != MAX_UINT16 ) { *puStage = uNextBestStage; }
}

/* pFloat array must be sorted in ascending order */
DP_INLINE void arr_float_findlarger( FLOAT* pFloat, UINT32 num, FLOAT fTestValue, FLOAT* pfValue, UINT32* puIndex )
{
	UINT16 u;
	UINT32 uValue = num - 1;
    FLOAT fValue = pFloat[ uValue ];
	FLOAT fTol = 1E-6;

#ifdef ENABLE_SYMMETRIC_FINDLARGER

	for( u = num - 2; u != MAX_UINT16; u-- ) /* search backwards to make mem access coherent */
	{
		if( pFloat[ u ] < pFloat[ u + 1 ] ) {
			if( pFloat[ u ] <= fTestValue ) break; /* found on front side */
		} else {
			if( pFloat[ u ] >= fTestValue ) break; /* found on backside */
		}
	}
	if( u == MAX_UINT16 ) { uValue = 0; fValue = 0; }
	else { fValue = pFloat[ u + 1 ]; uValue = u + 1; }

#else //ENABLE_SYMMETRIC_FINDLARGER

	VERIFY( pFloat[ num - 2 ] < pFloat[ num - 1 ] );

	for( u = num - 2; u != MAX_UINT; u-- ) /* search backwards to make mem access coherent */
	{
		if( pFloat[ u ] <= fTestValue ) { fValue = pFloat[ u + 1 ]; uValue = u + 1; break; }
	}

	/* not in table */
	if( u == MAX_UINT ) { uValue = 0; fValue = 0; }

#endif //ENABLE_SYMMETRIC_FINDLARGER

	*pfValue = fValue;
	*puIndex = uValue;
#undef RELATION_FINDLARGER_SIDEDNESS
}

DP_INLINE void arr_float_metric( FLOAT* pfMetricValue, FLOAT* pFloat, UINT32 count )
{
	UINT16 i;
	FLOAT fMetricValue = 0;
	for( i=0; i<count; i++ ) { fMetricValue += pFloat[ i ]; }
	*pfMetricValue = fMetricValue;
}

/*********************************************/

void arr_uint8_print( FILE* pFile, UINT8* pUInt, UINT32 num, CHAR cDelim )
{
	UINT8 i;
	for( i=0; i<num; i++ )
	{
		sprintf( gcPrintBuff, "%7hu%c", (UINT16)pUInt[i], (i==(num-1)) ? '\n' : cDelim );
		PRINT_STDOUT( gcPrintBuff );
	}
}

void arr_uint8_print_t( FILE* pFile, UINT8* pUInt, UINT32 num, UINT32 stride, CHAR cDelim )
{
	UINT8 i;
	for( i=0; i<num; i++ ) { fprintf( pFile, "%7hu", (UINT16)pUInt[i*stride] ); putc( cDelim, pFile ); }
}
	
void arr_uint8_clear_conditional( UINT8* pDest, UINT16* pTest, UINT32 num )
{
	UINT16 i;
	for( i=0; i<num; i++ ) { if( pTest[i] == 0 ) { pDest[i] = 0; } }
}

void arr_uint8_copy( UINT8* pDest, UINT8* pSrc, UINT32 num )
{
	UINT16 i;
	for( i=0; i<num; i++ ) { pDest[i] = pSrc[i]; }
}

/*********************************************/

void arr_uint16_print( FILE* pFile, UINT16* pUInt, UINT32 num, CHAR cDelim )
{
	UINT8 i;
	for( i=0; i<num; i++ )
	{
		sprintf( gcPrintBuff, "%7hu%c", pUInt[i], (i==(num-1)) ? '\n' : cDelim );
		PRINT_STDOUT( gcPrintBuff );
	}
}

void arr_uint16_print_t( FILE* pFile, UINT16* pUInt, UINT32 num, UINT32 stride, CHAR cDelim )
{
	UINT8 i;
	for( i=0; i<num; i++ ) { fprintf( pFile, "%7hu", pUInt[i*stride] ); putc( cDelim, pFile ); }
}
	
void arr_uint16_clear_conditional( UINT16* pDest, UINT32* pTest, UINT32 num )
{
	UINT16 i;
	for( i=0; i<num; i++ ) { if( pTest[i] == 0 ) { pDest[i] = 0; } }
}

void arr_uint16_copy( UINT16* pDest, UINT16* pSrc, UINT32 num )
{
	UINT16 i;
	for( i=0; i<num; i++ ) { pDest[i] = pSrc[i]; }
}

void arr_uint16_set( UINT16* pDest, UINT16 uValue, UINT32 num )
{
	UINT16 i;
	for( i=0; i<num; i++ ) { pDest[i] = uValue; }
}

void arr_uint16_sum( UINT32 *pSum, UINT16 *pSrc, UINT32 num, UINT32 stride )
{
	UINT16 i;
	UINT32 uSum = 0;
	for( i=0; i<num; i++ ) { uSum += pSrc[ i * stride ]; }
	*pSum = uSum;
}

void arr_uint16_sum_nostride( UINT32 *pSum, UINT16 *pSrc, UINT32 num )
{
	UINT16 i;
	UINT32 uSum = 0;
	for( i=0; i<num; i++ ) { uSum += pSrc[ i ]; }
	*pSum = uSum;
}

/*********************************************/

void arr_uint32_print( FILE* pFile, UINT32* pUInt, UINT32 num, CHAR cDelim )
{
	UINT32 i;
	for( i=0; i<num; i++ )
	{
		sprintf( gcPrintBuff, "%7lu%c", pUInt[i], (i==(num-1)) ? '\n' : cDelim );
		PRINT_STDOUT( gcPrintBuff );
	}
}

void arr_uint32_print_t( FILE* pFile, UINT32* pUInt, UINT32 num, UINT32 stride, CHAR cDelim )
{
	UINT32 i;
	for( i=0; i<num; i++ ) { fprintf( pFile, "%7lu", pUInt[i*stride] ); putc( cDelim, pFile ); }
}

void arr_uint32_copy( UINT32* pDest, UINT32* pSrc, UINT32 num )
{
	UINT32 i;
	for( i=0; i<num; i++ ) { pDest[i] = pSrc[i]; }
}

void arr_uint32_set( UINT32* pDest, UINT32 uValue, UINT32 num )
{
	UINT32 i;
	for( i=0; i<num; i++ ) { pDest[i] = uValue; }
}

void arr_uint32_step( UINT32* pDest, UINT32 uOffset, UINT32 uIncr, UINT32 num )
{
	UINT32 i;
	for( i=0; i<num; i++ ) { pDest[i] = uOffset + i * uIncr; }
}

void arr_uint32_sum( UINT32 *pSum, UINT32 *pSrc, UINT32 num, UINT32 stride )
{
	UINT32 i;
	UINT32 uSum = 0;
	for( i=0; i<num; i++ ) { uSum += pSrc[ i * stride ]; }
	*pSum = uSum;
}

void arr_uint32_testEQ( UINT32* puCount, UINT32* puFirst, UINT32* pSrc, UINT32 uValue, UINT32 num )
{
	UINT32 i;
	UINT32 uc = 0;
	UINT32 uf = MAX_UINT32;
	for( i=0; i<num; i++ ) { uc += pSrc[i] == uValue ? 1 : 0; if( uc == 1 ) { uf = i; } }
	*puCount = uc;
	*puFirst = uf;
}

void arr_uint32_transpose( UINT32* pDest, UINT32* pSrc, UINT32 uVectSize, UINT32 uNVects )
{
	UINT32 v, nv;
	for( nv=0; nv<uNVects; nv++ )
		for( v=0; v<uVectSize; v++ )
			{ pDest[ uNVects * v + nv ] = pSrc[ uVectSize * nv + v ]; }
}

/*********************************************/

typedef struct tagCurveDef
{
	CHAR*  szName;
	UINT32 uPoints;
	FLOAT* pfFloFact; /* capacity factor: % cfs */
	FLOAT* pfEffFact; /* efficiency factor: % energy conversion */
	FLOAT* pfPowFact; /* power factor: kw / ft-cfs */
} CurveDef;

UINT32 guBuiltinCurves = 0;
UINT32    guCurves = 0;
CurveDef* gpCurves = 0;

EXTERNC void curve_cleanup()
{
	if( gpCurves ) { free( gpCurves ); gpCurves = 0; }
	guCurves = 0;
}

UINT32 curve_find( CHAR* szCurveName )
{
	UINT32 uCurve;
	for( uCurve = 0; uCurve < guCurves; uCurve++ ) {
		if( strcasecmp( gpCurves[ uCurve ].szName, szCurveName ) == 0 ) { return uCurve; }
	}
	{
		sprintf( gcPrintBuff, "%s:%d (" VERSIONED_NAME  ") Curve '%s' not found\n", __FILE__, __LINE__, szCurveName );
		PRINT_STDERR( gcPrintBuff );
		dp_cleanup_fatal();
	}
	return MAX_UINT32;
}

EXTERNC UINT32 curve_register_n( UINT32 np, FLOAT* pCap, FLOAT* pPow, FLOAT* pEff )
{
	void* pVoid = realloc( gpCurves, ( guCurves + 1 ) * sizeof(CurveDef) );
	if( pVoid == 0 )
	{
		sprintf( gcPrintBuff, "%s:%d (" VERSIONED_NAME  ") realloc failed\n", __FILE__, __LINE__ ); dp_cleanup_fatal();
		PRINT_STDERR( gcPrintBuff );
		return 0;
	}
	gpCurves = (CurveDef*)pVoid;

	gpCurves[ guCurves ].szName = "N/A";
	gpCurves[ guCurves ].uPoints = np;
	gpCurves[ guCurves ].pfFloFact = pCap;
	gpCurves[ guCurves ].pfEffFact = pEff;
	gpCurves[ guCurves ].pfPowFact = pPow;
	guCurves++;
	return guCurves-1;
}

// caller owned memory
void curve_register( CHAR* szCurveName, UINT32 np, FLOAT* pCap, FLOAT* pPow, FLOAT* pEff )
{
	UINT32 uCurve;
	for( uCurve = 0; uCurve < guCurves; uCurve++ ) {
		if( strcasecmp( gpCurves[ uCurve ].szName, szCurveName ) == 0 )
		{
			sprintf( gcPrintBuff, "%s:%d (" VERSIONED_NAME  ") Curve '%s' already named\n", __FILE__, __LINE__, szCurveName );
			PRINT_STDERR( gcPrintBuff );
			dp_cleanup_fatal();
			return;
		}
	}
	uCurve = curve_register_n( np, pCap, pPow, pEff );
	gpCurves[ uCurve ].szName = szCurveName;
}

void curve_list( UINT32 uUserCurves )
{
	UINT32 uCurve, uPoints;
	for( uCurve = ( uUserCurves ? guBuiltinCurves : 0 ); uCurve < guCurves; uCurve++ )
	{
		fprintf( stdout, "curve %s flo ", gpCurves[ uCurve ].szName );
		for( uPoints = 0; uPoints < gpCurves[ uCurve ].uPoints; uPoints++ )
		{ fprintf( stdout, float_format( gpCurves[ uCurve ].pfFloFact[ uPoints ] ), gpCurves[ uCurve ].pfFloFact[ uPoints ] ); fprintf( stdout, " " ); } fprintf( stdout, "\n" );
		fprintf( stdout, "curve %s pow ", gpCurves[ uCurve ].szName );
		for( uPoints = 0; uPoints < gpCurves[ uCurve ].uPoints; uPoints++ )
		{ fprintf( stdout, float_format( gpCurves[ uCurve ].pfPowFact[ uPoints ] ), gpCurves[ uCurve ].pfPowFact[ uPoints ] ); fprintf( stdout, " " ); } fprintf( stdout, "\n" );
		fprintf( stdout, "curve %s eff ", gpCurves[ uCurve ].szName );
		for( uPoints = 0; uPoints < gpCurves[ uCurve ].uPoints; uPoints++ )
		{ fprintf( stdout, float_format( gpCurves[ uCurve ].pfEffFact[ uPoints ] ), gpCurves[ uCurve ].pfEffFact[ uPoints ] ); fprintf( stdout, " " ); } fprintf( stdout, "\n" );
	}
}

// takes [0,1] returns [0,1]
DP_INLINE FLOAT curve_eff_from_qfact( UINT32 uCurve, FLOAT fQFact )
{
	FLOAT f1;
	UINT32 i1;
	arr_float_findlarger( gpCurves[ uCurve ].pfFloFact, gpCurves[ uCurve ].uPoints, fQFact, &f1, &i1 );
	if( i1 == 0 )
	{
		return 0; /* assume discontinuity is intentional */
	} else {
		UINT32 i0 = i1 - 1;
		FLOAT f0 = gpCurves[ uCurve ].pfFloFact[ i0 ];
		FLOAT t0 = gpCurves[ uCurve ].pfEffFact[ i0 ];
		FLOAT t1 = gpCurves[ uCurve ].pfEffFact[ i1 ];
		return float_interpolate5( fQFact, f0, t0, f1, t1 );
	}
}

// takes [0,1] returns [0,1]
DP_INLINE FLOAT curve_eff_from_pfact( UINT32 uCurve, FLOAT fPFact )
{
	FLOAT f1;
	UINT32 i1;
	arr_float_findlarger( gpCurves[ uCurve ].pfPowFact, gpCurves[ uCurve ].uPoints, fPFact, &f1, &i1 );
	if( i1 == 0 )
	{
		return 0; /* assume discontinuity is intentional */
	} else {
		UINT32 i0 = i1 - 1;
		FLOAT f0 = gpCurves[ uCurve ].pfPowFact[ i0 ];
		FLOAT t0 = gpCurves[ uCurve ].pfEffFact[ i0 ];
		FLOAT t1 = gpCurves[ uCurve ].pfEffFact[ i1 ];
		return float_interpolate5( fPFact, f0, t0, f1, t1 );
	}
}

EXTERNC void curve_register_builtins()
{
	{
		static CHAR* szCurve = "CrossFlow";
		static FLOAT flo[] = {0,.10,.11,.13,.16,.182,.21,.232,.275,.31,.35,.40,.48,.60,.80,.91,1.00,1.10};
		static FLOAT pow[] = {0,.10,.11,.13,.16,.182,.21,.232,.275,.31,.35,.40,.48,.60,.80,.91,1.00,1.10};
		static FLOAT eff[] = {0,.655,.68,.71,.735,.75,.765,.775,.79,.80,.81,.8175,.825,.83,.83,.825,.82,.815};
		curve_register( szCurve, sizeof(flo) / sizeof(FLOAT), flo, pow, eff );
	}{
		static CHAR* szCurve = "FixedPropeller";
		static FLOAT flo[] = {0,.175,.22,.25,.285,.325,.375,.435,.485,.525,.60,.68,.75,.82,.91,1.00,1.08};
		static FLOAT pow[] = {0,.175,.22,.25,.285,.325,.375,.435,.485,.525,.60,.68,.75,.82,.91,1.00,1.08};
		static FLOAT eff[] = {0,.65,.70,.725,.75,.775,.80,.825,.84,.85,.86,.865,.8675,.8675,.865,.858,.85};
		curve_register( szCurve, sizeof(flo) / sizeof(FLOAT), flo, pow, eff );
	}{
		static CHAR* szCurve = "Francis";
		static FLOAT flo[] = {0,.25,.30,.34,.388,.44,.477,.49,.54,.66,.71,.765,.82,.89,.91,.94,1.00};
		static FLOAT pow[] = {0,.25,.30,.34,.388,.44,.477,.49,.54,.66,.71,.765,.82,.89,.91,.94,1.00};
		static FLOAT eff[] = {0,.435,.50,.55,.60,.65,.68,.70,.73,.80,.825,.85,.87,.89,.89,.885,.85};
		curve_register( szCurve, sizeof(flo) / sizeof(FLOAT), flo, pow, eff );
	}{
		static CHAR* szCurve = "Kaplan";
		static FLOAT flo[] = {0,.161,.20,.24,.288,.338,.406,.482,.565,.663,.80,.88,.91,1.00};
		static FLOAT pow[] = {0,.161,.20,.24,.288,.338,.406,.482,.565,.663,.80,.88,.91,1.00};
		static FLOAT eff[] = {0,.45,.50,.55,.60,.65,.70,.75,.80,.85,.90,.92,.92,.88};
		curve_register( szCurve, sizeof(flo) / sizeof(FLOAT), flo, pow, eff );
	}{
		static CHAR* szCurve = "Pelton";
		static FLOAT flo[] = {0,.035,.05,.066,.079,.089,.102,.12,.145,.172,.21,.264,.345,.42,.495,.56,.68,.85,1.00,1.10};
		static FLOAT pow[] = {0,.035,.05,.066,.079,.089,.102,.12,.145,.172,.21,.264,.345,.42,.495,.56,.68,.85,1.00,1.10};
		static FLOAT eff[] = {0,.40,.50,.60,.65,.675,.70,.725,.75,.775,.80,.825,.85,.865,.875,.88,.885,.885,.88,.875};
		curve_register( szCurve, sizeof(flo) / sizeof(FLOAT), flo, pow, eff );
	}{
		static CHAR* szCurve = "Turgo";
		static FLOAT flo[] = {0,.12,.15,.18,.21,.25,.28,.325,.37,.43,.49,.60,.68,.80,.90,1.00,1.10};
		static FLOAT pow[] = {0,.12,.15,.18,.21,.25,.28,.325,.37,.43,.49,.60,.68,.80,.90,1.00,1.10};
		static FLOAT eff[] = {0,.61,.68,.72,.75,.78,.80,.82,.835,.85,.86,.87,.873,.872,.869,.864,.856};
		curve_register( szCurve, sizeof(flo) / sizeof(FLOAT), flo, pow, eff );
	}
	guBuiltinCurves = guCurves;
}

void curve_test()
{
	/* ensure ascending values */
/*
	CHAR* szCurve = "CrossFlow";
	UINT32 uCurve = curve_find( szCurve );
	float_print( stdout, gpCurves[ uCurve ].uPoints, gpCurves[ uCurve ].pfFloFact );
	float_print( stdout, gpCurves[ uCurve ].uPoints, gpCurves[ uCurve ].pfEffFact );
	float_print( stdout, gpCurves[ uCurve ].uPoints, gpCurves[ uCurve ].pfPowFact );
*/
	if( guDebugMode ) {
		sprintf( gcPrintBuff, "%s:%d (" VERSIONED_NAME  ") Curve_test complete\n", __FILE__, __LINE__ );
		PRINT_STDERR( gcPrintBuff );
	}
}

/*********************************************/
/* called TurbineDef but actually more of a UnitDef as we specify some generator factors */

typedef struct tagTurbineDef
{
	CHAR* szName;
	UINT32 uCurveNum;
	FLOAT fRatedHead;
	FLOAT fMaxFlow;
	FLOAT fMaxPower;
	FLOAT fWeight;
	FLOAT fHeadloss; /* hydraulic loss coef k used in eqn: ( k qMax ) / qMax */
	FLOAT fGenCapacity; /* % extension of unit maxpower or maxflow  */
	FLOAT fGenEfficiency; /* energy conversion efficiency [0-1] */
	UINT32 uGenCurveNum; /* gen efficiency curve, in liu of a fixed effficiency constant. when MAX_UINT, curve is not used. */
} TurbineDef;

UINT32 guTurbs = 0;
TurbineDef* gpTurbs = 0;

EXTERNC void turbine_cleanup()
{
	if( gpTurbs ) { free( gpTurbs ); gpTurbs = 0; }
	guTurbs = 0;
}

UINT32 turbine_find( CHAR* szTurbineName )
{
	UINT32 uCurve;
	for( uCurve = 0; uCurve < guTurbs; uCurve++ ) {
		if( strcasecmp( gpTurbs[ uCurve ].szName, szTurbineName ) == 0 ) { return uCurve; }
	}
	{
		sprintf( gcPrintBuff, "%s:%d (" VERSIONED_NAME  ") Turbine '%s' not found\n", __FILE__, __LINE__, szTurbineName );
		PRINT_STDERR( gcPrintBuff );
		dp_cleanup_fatal();
	}
	return MAX_UINT32;
}

UINT32 turbine_find_err( CHAR* szTurbineName )
{
	UINT32 uCurve;
	for( uCurve = 0; uCurve < guTurbs; uCurve++ ) {
		if( strcasecmp( gpTurbs[ uCurve ].szName, szTurbineName ) == 0 ) { return uCurve; }
	}
	return MAX_UINT32;
}

EXTERNC UINT32 turbine_register_n( UINT32 uType, FLOAT h, FLOAT q, FLOAT p )
{
	void* pVoid = realloc( (void*)gpTurbs, ( guTurbs + 1 ) * sizeof(TurbineDef) );
	if( pVoid == 0 )
	{
		sprintf( gcPrintBuff, "%s:%d (" VERSIONED_NAME  ") realloc failed\n", __FILE__, __LINE__ ); dp_cleanup_fatal();
		PRINT_STDERR( gcPrintBuff );
		return 0;
	}
	gpTurbs = (TurbineDef*)pVoid;

	gpTurbs[ guTurbs ].szName = "N/A"; /* default name in case of using the DLL api */
	gpTurbs[ guTurbs ].uCurveNum = uType;
	gpTurbs[ guTurbs ].fRatedHead = h;
	gpTurbs[ guTurbs ].fMaxFlow = q;
	gpTurbs[ guTurbs ].fMaxPower = p;
	gpTurbs[ guTurbs ].fWeight = 1.0; /* default to equal weighting */
	gpTurbs[ guTurbs ].fHeadloss = 0; /* default to no hydraulic losses k*Qmax/Qmax*/
	gpTurbs[ guTurbs ].fGenCapacity = 1.0; /* default to turbine limited */
	gpTurbs[ guTurbs ].fGenEfficiency = .95; /* default to 95% efficiency */
	gpTurbs[ guTurbs ].uGenCurveNum = MAX_UINT32;
	guTurbs++;
	return guTurbs-1;
}

// caller owned memory
void turbine_register( CHAR* szTurbineName, CHAR* szCurveName, FLOAT h, FLOAT q, FLOAT p )
{
	UINT32 uTurb;
	for( uTurb = 0; uTurb < guTurbs; uTurb++ )
	{
		if( strcasecmp( gpTurbs[ uTurb ].szName, szTurbineName ) == 0 )
		{
			sprintf( gcPrintBuff, "%s:%d (" VERSIONED_NAME  ") Turbine '%s' already named\n", __FILE__, __LINE__, szTurbineName );
			PRINT_STDERR( gcPrintBuff );
			dp_cleanup_fatal();
			return;
		}
	}
	{
		UINT32 uType = curve_find( szCurveName );
		if( uType == MAX_UINT32 )
		{
			sprintf( gcPrintBuff, "%s:%d (" VERSIONED_NAME  ") Curve '%s' not found\n", __FILE__, __LINE__, szCurveName );
			PRINT_STDERR( gcPrintBuff );
			dp_cleanup_fatal();
			return;
		}
		{
			uTurb = turbine_register_n( uType, h, q, p );
			gpTurbs[ uTurb ].szName = szTurbineName;
		}
	}
}

EXTERNC void turbine_geneff( UINT32 uTurbine, FLOAT fGenEfficiency )
{
	/* only apply one not both  */
	gpTurbs[ uTurbine ].fGenEfficiency = fGenEfficiency;
	gpTurbs[ uTurbine ].uGenCurveNum = MAX_UINT32;
}

EXTERNC void turbine_headloss( UINT32 uTurbine, FLOAT fHeadloss )
{
	gpTurbs[ uTurbine ].fHeadloss = fHeadloss;
}

EXTERNC void turbine_weight( UINT32 uTurbine, FLOAT fWeight )
{
	gpTurbs[ uTurbine ].fWeight = fWeight;
}

EXTERNC void turbine_gencap( UINT32 uTurbine, FLOAT fGenCapacity )
{
	gpTurbs[ uTurbine ].fGenCapacity = fGenCapacity;
}

EXTERNC void turbine_gencurve( UINT32 uTurbine, UINT32 uGenCurveNum )
{
	/* only apply one not both  */
	gpTurbs[ uTurbine ].fGenEfficiency = 1.0;
	gpTurbs[ uTurbine ].uGenCurveNum = uGenCurveNum;
}

void turbine_gencurvename( UINT32 uTurbine, CHAR* szCurveName )
{
	UINT32 uType = curve_find( szCurveName );
	if( uType == MAX_UINT32 )
	{
		sprintf( gcPrintBuff, "%s:%d (" VERSIONED_NAME  ") Curve '%s' not found\n", __FILE__, __LINE__, szCurveName );
		PRINT_STDERR( gcPrintBuff );
		dp_cleanup_fatal();
		return;
	}
	/* only apply one not both  */
	gpTurbs[ uTurbine ].fGenEfficiency = 1.0;
	gpTurbs[ uTurbine ].uGenCurveNum = uType;
}

void turbine_list()
{
	UINT32 uTurb;
	for( uTurb = 0; uTurb < guTurbs; uTurb++ )
	{
		fprintf( stdout, "unit %s %s ", gpTurbs[ uTurb ].szName, gpCurves[ gpTurbs[ uTurb ].uCurveNum ].szName );
		fprintf( stdout, float_format( gpTurbs[ uTurb ].fRatedHead ), gpTurbs[ uTurb ].fRatedHead );
		fprintf( stdout, " %s ", char_units( DP_UNIT_LENGTH ) );
		fprintf( stdout, float_format( gpTurbs[ uTurb ].fMaxFlow ), gpTurbs[ uTurb ].fMaxFlow );
		fprintf( stdout, " %s ", char_units( DP_UNIT_FLOW ) );
		fprintf( stdout, float_format( gpTurbs[ uTurb ].fMaxPower ), gpTurbs[ uTurb ].fMaxPower );
		fprintf( stdout, " %s", char_units( DP_UNIT_POWER ) );
		fprintf( stdout, " capacity " );
		fprintf( stdout, float_format( gpTurbs[ uTurb ].fGenCapacity ), gpTurbs[ uTurb ].fGenCapacity );
		fprintf( stdout, " weight " );
		fprintf( stdout, float_format( gpTurbs[ uTurb ].fWeight ), gpTurbs[ uTurb ].fWeight );
		fprintf( stdout, " headloss " );
		fprintf( stdout, float_format( gpTurbs[ uTurb ].fHeadloss ), gpTurbs[ uTurb ].fHeadloss );
		fprintf( stdout, " geneff " );
		fprintf( stdout, float_format( gpTurbs[ uTurb ].fGenEfficiency ), gpTurbs[ uTurb ].fGenEfficiency );
		fprintf( stdout, " gencurve %s ", (gpTurbs[ uTurb ].uGenCurveNum == MAX_UINT32) ? "nil" : gpCurves[ gpTurbs[ uTurb ].uGenCurveNum ].szName );
		fprintf( stdout, "\n" );
	}
}

/*****************/

// takes [0,1] returns [0,1]
DP_INLINE FLOAT turbine_eff_from_qfact( UINT32 uTurb, FLOAT fQFact )
{
	TurbineDef* pTurb = &gpTurbs[ uTurb ];
	CurveDef* pCurve = &gpCurves[ pTurb->uCurveNum ];

	FLOAT f1;
	UINT32 i1;
	arr_float_findlarger( pCurve->pfFloFact, pCurve->uPoints, fQFact, &f1, &i1 );
	if( i1 == 0 )
	{
		return 0; /* assume discontinuity is intentional */
	} else {
		UINT32 i0 = i1 - 1;
		FLOAT f0 = pCurve->pfFloFact[ i0 ];
		FLOAT t0 = pCurve->pfEffFact[ i0 ];
		FLOAT t1 = pCurve->pfEffFact[ i1 ];
		return float_interpolate5( fQFact, f0, t0, f1, t1 );
	}
}

// takes [0,1] returns [0,1]
DP_INLINE FLOAT turbine_eff_from_pfact( UINT32 uTurb, FLOAT fPFact )
{
	TurbineDef* pTurb = &gpTurbs[ uTurb ];
	CurveDef* pCurve = &gpCurves[ pTurb->uCurveNum ];

	FLOAT f1;
	UINT32 i1;
	arr_float_findlarger( pCurve->pfPowFact, pCurve->uPoints, fPFact, &f1, &i1 );
	if( i1 == 0 )
	{
		return 0; /* assume discontinuity is intentional */
	} else {
		UINT32 i0 = i1 - 1;
		FLOAT f0 = pCurve->pfPowFact[ i0 ];
		FLOAT t0 = pCurve->pfEffFact[ i0 ];
		FLOAT t1 = pCurve->pfEffFact[ i1 ];
		return float_interpolate5( fPFact, f0, t0, f1, t1 );
	}
}

DP_INLINE FLOAT turbine_maxpower_at_currenthead( UINT32 uTurb, FLOAT h )
{
	FLOAT pMax = gpTurbs[ uTurb ].fMaxPower;
	FLOAT hRated = gpTurbs[ uTurb ].fRatedHead;
#ifdef ENABLE_NEW_CALCS
	FLOAT pAtCurrentHead = pMax * float_EffAdjFactor( h, hRated, 2.0 );
#else /* ENABLE_NEW_CALCS */
	FLOAT pAtCurrentHead = pMax * powf( (h / hRated), 1.5 );
#endif /* ENABLE_NEW_CALCS */
	return pAtCurrentHead;
}

DP_INLINE FLOAT turbine_maxdischarge_at_currenthead( UINT32 uTurb, FLOAT h )
{
	FLOAT qMax = gpTurbs[ uTurb ].fMaxFlow;
	FLOAT hRated = gpTurbs[ uTurb ].fRatedHead;
#ifdef ENABLE_NEW_CALCS
	FLOAT qAtCurrentHead = qMax * float_EffAdjFactor( h, hRated, 2.0 );
#else /* ENABLE_NEW_CALCS */
	FLOAT qAtCurrentHead = qMax * powf( (h / hRated), 1.5 );
#endif /* ENABLE_NEW_CALCS */
	return qAtCurrentHead;
}

DP_INLINE FLOAT turbine_power( UINT32 uTurb, FLOAT hCurrent, FLOAT qCurrent )
{
#ifdef ENABLE_NEW_CALCS
	FLOAT qMax = gpTurbs[ uTurb ].fMaxFlow;
	FLOAT pMax = gpTurbs[ uTurb ].fMaxPower;
	FLOAT hRated = gpTurbs[ uTurb ].fRatedHead;
	FLOAT hLossCoef = gpTurbs[ uTurb ].fHeadloss;
	FLOAT fTol = 1E-3;
	/**/
	FLOAT fGenEfficiency = gpTurbs[ uTurb ].uGenCurveNum != MAX_UINT32 ?
								curve_eff_from_qfact( gpTurbs[ uTurb ].uGenCurveNum, qCurrent / qMax ) :
								gpTurbs[ uTurb ].fGenEfficiency;
	/**/
	int bDischExceedsMax = ( qCurrent > qMax );
#ifdef ENABLE_NEW_DYNLOSS
	FLOAT hHydraulicLoss = hLossCoef * powf( qCurrent / qMax, 2.0 );
#else
	FLOAT hHydraulicLoss = powf( hLossCoef * qMax / qCurrent, 2.0 );
#endif /* ENABLE_NEW_DYNLOSS */
	FLOAT hNet = hCurrent - hHydraulicLoss;
	FLOAT fEffAdjFact = float_EffAdjFactor( hNet, hRated, 2.0 );
	FLOAT fGeneratorAndYardLoss = ( 1.0 - gfPlantLossCoef ) * fGenEfficiency;
	/**/
	FLOAT fEff = turbine_eff_from_qfact( uTurb, qCurrent / qMax );
	int bEffSmallerThanTol = ( fEff < fTol );
	/**/
	FLOAT pCurrent = MAX( 0, fEffAdjFact * fEff * ( qCurrent * hCurrent * gfConvFactor ) * fGeneratorAndYardLoss ); /* sanity check */
	int bPowExceedsMax = ( pCurrent > pMax );
#else /* ENABLE_NEW_CALCS */
	FLOAT pMax = gpTurbs[ uTurb ].fMaxPower;
	FLOAT qMax = gpTurbs[ uTurb ].fMaxFlow;
	FLOAT hRated = gpTurbs[ uTurb ].fRatedHead;
	int bDischExceedsMax = ( qCurrent > qMax );
	FLOAT fEff = turbine_eff_from_qfact( uTurb, qCurrent / qMax );
	FLOAT fTol = 1E-3;
	int bEffSmallerThanTol = ( fEff < fTol );
#ifdef ENABLE_NEWHEADSCALING
	FLOAT hNet = hRated - hCurrent - gpTurbs[ uTurb ].fHeadloss;
	FLOAT hScaling = ( 1 + (hNet > hRated ? +0.5 : -0.5) * powf( hNet / hRated, 2.0 ) );
#else // ENABLE_NEWHEADSCALING
	FLOAT hScaling = powf( (hCurrent / hRated), 1.5 ) ;
#endif // ENABLE_NEWHEADSCALING
	FLOAT fGenEfficiency = gpTurbs[ uTurb ].fGenEfficiency;
	FLOAT pAtRatedHead = fEff * ( qCurrent * hCurrent * gfConvFactor );
	FLOAT pCurrent = pAtRatedHead * hScaling * ( 1.0 - gfPlantLossCoef ) * ( fGenEfficiency );
	int bPowExceedsMax = ( pCurrent > pMax );
#endif /* ENABLE_NEW_CALCS */
	/* check flags */
	if( guDebugMode )
	{
		if( bDischExceedsMax )
		{
			char* szFlow = char_units( DP_UNIT_FLOW );
			char* szLength = char_units( DP_UNIT_LENGTH );
			fprintf( stdout, "condition on unit %s: %f %s at %f %s exceeds maximum rating (%f %s at %f %s).\n",
					gpTurbs[ uTurb ].szName,
					qCurrent, szFlow,
					hCurrent, szLength,
					qMax, szFlow,
					hRated, szLength );
		}
		if( bEffSmallerThanTol )
		{
			char* szFlow = char_units( DP_UNIT_FLOW );
			char* szLength = char_units( DP_UNIT_LENGTH );
			fprintf( stdout, "condition on unit %s: efficiency of %f at %f %s too low for rating of %f %s at %f %s.\n",
					gpTurbs[ uTurb ].szName,
					fEff,
					qCurrent, szFlow,
					qMax, szFlow,
					hRated, szLength );
		}
		if( bPowExceedsMax )
		{
			char* szPower = char_units( DP_UNIT_POWER );
			char* szLength = char_units( DP_UNIT_LENGTH );
			fprintf( stdout, "condition on unit %s: %f %s at %f %s exceeds maximum rating (%f %s at %f %s).\n",
					gpTurbs[ uTurb ].szName,
					pCurrent, szPower,
					hCurrent, szLength,
					pMax, szPower,
					hRated, szLength );
		}
	}
#ifdef ENABLE_INLINE_NUMERICAL_CLEANUP
	return ( isnanf( pCurrent ) || isinff( pCurrent ) ? 0 : pCurrent );
#else
	return pCurrent;
#endif
}

DP_INLINE FLOAT turbine_discharge( UINT32 uTurb, FLOAT hCurrent, FLOAT pCurrent )
{
#ifdef ENABLE_NEW_CALCS
	FLOAT pMax = gpTurbs[ uTurb ].fMaxPower;
	FLOAT qMax = gpTurbs[ uTurb ].fMaxFlow;
	FLOAT hRated = gpTurbs[ uTurb ].fRatedHead;
	FLOAT hLossCoef = gpTurbs[ uTurb ].fHeadloss;
	FLOAT fTol = 1E-3;
	/**/
	FLOAT fGenEfficiency = gpTurbs[ uTurb ].uGenCurveNum != MAX_UINT32 ?
								curve_eff_from_qfact( gpTurbs[ uTurb ].uGenCurveNum, pCurrent / pMax ) : /* note: approx */
								gpTurbs[ uTurb ].fGenEfficiency;
	/**/
#ifdef ENABLE_NEW_DYNLOSS
	FLOAT hHydraulicLoss = hLossCoef * powf( pCurrent / pMax, 2.0 );
#else
	FLOAT hHydraulicLoss = powf( hLossCoef * pMax / pCurrent, 2.0 );
#endif /* ENABLE_NEW_DYNLOSS */
	FLOAT hNet = hCurrent - hHydraulicLoss;
	FLOAT fEffAdjFact = float_EffAdjFactor( hNet, hRated, 2.0 );
	FLOAT fGeneratorAndYardLoss = ( 1.0 - gfPlantLossCoef ) * fGenEfficiency;
	/**/
	FLOAT pTurbine = pCurrent / fGeneratorAndYardLoss;
	int bPowExceedsMax = ( pTurbine > pMax );
	FLOAT fEff = turbine_eff_from_pfact( uTurb, pTurbine / pMax );
	int bEffSmallerThanTol = ( fEff < fTol );
	/**/
	FLOAT qCurrent = MAX( 0, pTurbine / ( fEffAdjFact * fEff * ( hCurrent * gfConvFactor ) ) ); /* sanity check */
	int bDischExceedsMax = ( qCurrent > qMax );
#else /* ENABLE_NEW_CALCS */
	FLOAT pMax = gpTurbs[ uTurb ].fMaxPower;
	FLOAT qMax = gpTurbs[ uTurb ].fMaxFlow;
	FLOAT hRated = gpTurbs[ uTurb ].fRatedHead;
	FLOAT fGenEfficiency = gpTurbs[ uTurb ].fGenEfficiency;
	FLOAT pAtSpindle = pCurrent * ( 1.0 + gfPlantLossCoef ) * ( 2.0 - fGenEfficiency );
#ifdef ENABLE_NEWHEADSCALING
	FLOAT hNet = hRated - hCurrent - gpTurbs[ uTurb ].fHeadloss;
	FLOAT hScaling = ( 1 + (hNet > hRated ? +0.5 : -0.5) * powf( hNet / hRated, 2.0 ) );
#else // ENABLE_NEWHEADSCALING
	FLOAT hScaling = powf( (hCurrent / hRated), 1.5 );
#endif // ENABLE_NEWHEADSCALING
	FLOAT pAtRatedHead = pAtSpindle / hScaling;
	int bPowExceedsMax = ( pAtRatedHead > pMax );
	FLOAT fEff = turbine_eff_from_pfact( uTurb, pAtRatedHead / pMax );
	FLOAT qCurrent = 0;
	FLOAT fTol = 1E-3; /* 0.1% */
	int bEffSmallerThanTol = ( fEff < fTol );
	qCurrent = bEffSmallerThanTol ? 0 : ( pAtSpindle / ( fEff * ( hScaling * ( hCurrent * gfConvFactor ) ) ) );
	int bDischExceedsMax = ( qCurrent > qMax );
#endif /* ENABLE_NEW_CALCS */
	/* check flags */
	if( guDebugMode )
	{
		if( bPowExceedsMax )
		{
			char* szPower = char_units( DP_UNIT_POWER );
			char* szLength = char_units( DP_UNIT_LENGTH );
			fprintf( stdout, "condition on unit %s: %f %s at %f %s exceeds maximum rating (%f %s at %f %s).\n",
					gpTurbs[ uTurb ].szName,
					pTurbine, szPower,
					hCurrent, szLength,
					pMax, szPower,
					hRated, szLength );
		}
		if( bEffSmallerThanTol )
		{
			char* szPower = char_units( DP_UNIT_POWER );
			char* szLength = char_units( DP_UNIT_LENGTH );
			fprintf( stdout, "condition on unit %s: efficiency of %f at %f %s too low for rating of %f %s at %f %s.\n",
					gpTurbs[ uTurb ].szName,
					fEff,
					pCurrent, szPower,
					pTurbine, szPower,
					hRated, szLength );
		}
		if( bDischExceedsMax )
		{
			char* szFlow = char_units( DP_UNIT_FLOW );
			char* szLength = char_units( DP_UNIT_LENGTH );
			fprintf( stdout, "condition on unit %s: %f %s at %f %s exceeds maximum rating (%f %s at %f %s).\n",
					gpTurbs[ uTurb ].szName,
					qCurrent, szFlow,
					hCurrent, szLength,
					qMax, szFlow,
					hRated, szLength );
		}
	}
#ifdef ENABLE_INLINE_NUMERICAL_CLEANUP
	return ( isnanf( qCurrent ) || isinff( qCurrent ) ? 0 : qCurrent );
#else
	return qCurrent;
#endif
}

/*****************/

void turbine_test()
{
	UINT32 uTurb = 0;

	ui_parse_units( "cfs" );
	turbine_register( "TestUnit1", "Kaplan", 220 /*ft*/, 8673 /*cfs*/, 167000 /*kw*/ ); // high head project

	/* ensure increasing head increases power at the same discharge */
	{
		FLOAT p1 = turbine_power( uTurb, (1.0 * gpTurbs[ uTurb ].fRatedHead), (0.75 * gpTurbs[ uTurb ].fMaxFlow) );
		FLOAT p2 = turbine_power( uTurb, (1.1 * gpTurbs[ uTurb ].fRatedHead), (0.75 * gpTurbs[ uTurb ].fMaxFlow) );
		if( p2 < p1 || isnanf(p2) || isnanf(p1) )
		{
			sprintf( gcPrintBuff, "%s:%d (" VERSIONED_NAME  ") Turbine_test fails\n", __FILE__, __LINE__ );
			PRINT_STDERR( gcPrintBuff );
			dp_cleanup_fatal();
		}
	}

	/* ensure increasing head decreases discharge at the same power */
	{
		FLOAT q1 = turbine_discharge( uTurb, (1.0 * gpTurbs[ uTurb ].fRatedHead), (0.75 * gpTurbs[ uTurb ].fMaxPower) );
		FLOAT q2 = turbine_discharge( uTurb, (1.1 * gpTurbs[ uTurb ].fRatedHead), (0.75 * gpTurbs[ uTurb ].fMaxPower) );
		if( q2 > q1 || isnanf(q2) || isnanf(q1) )
		{
			sprintf( gcPrintBuff, "%s:%d (" VERSIONED_NAME  ") Turbine_test fails\n", __FILE__, __LINE__ );
			PRINT_STDERR( gcPrintBuff );
			dp_cleanup_fatal();
		}
	}

	/* TODO: construct test for far side of efficiency curve... */

	ui_parse_units( 0 );
	guTurbs = 0;

	if( guDebugMode )
	{
		sprintf( gcPrintBuff, "%s:%d (" VERSIONED_NAME  ") Turbine_test complete\n", __FILE__, __LINE__ );
		PRINT_STDERR( gcPrintBuff );
	}
}

/*********************************************/

DP_INLINE void arr_turbine_hk_and_q( UINT32 uTurb, FLOAT h, FLOAT* pHK, FLOAT* pFlo, FLOAT* pPowStates, UINT32 n )
{
	FLOAT fTol = 1E-6;
	UINT32 i = 0;
	FLOAT q, p;
	for( i=0; i<n; i++ )
	{
		p = pPowStates[ i ];
		q = (p < fTol) ? 0 : turbine_discharge( uTurb, h, p ); // check degenerate case
		pHK[ i ] = ( q < fTol ) ? 0 : p / q; // handle DivByZero
		pFlo[ i ] = q;
	}
}

DP_INLINE void arr_turbine_hk_and_p( UINT32 uTurb, FLOAT h, FLOAT* pHK, FLOAT* pPow, FLOAT* pFloStates, UINT32 n )
{
	FLOAT fTol = 1E-6;
	UINT32 i = 0;
	FLOAT q, p;
	for( i=0; i<n; i++ )
	{
		q = pFloStates[ i ];
		p = (q < fTol) ? 0 : turbine_power( uTurb, h, q ); // check degenerate case
		pHK[ i ] = ( q < fTol ) ? 0 : p / q; // handle DivByZero
		pPow[ i ] = p;
	}
}

DP_INLINE void arr_turbine_p( UINT32 uTurb, FLOAT h, FLOAT* pPow, FLOAT* pFloStates, UINT32 n )
{
	FLOAT fTol = 1E-6;
	UINT32 i = 0;
	FLOAT q;
	for( i=0; i<n; i++ )
	{
		q = pFloStates[ i ];
		pPow[ i ] = (q < fTol) ? 0 : turbine_power( uTurb, h, q ); // check degenerate case
	}
}

DP_INLINE void arr_turbine_q( UINT32 uTurb, FLOAT h, FLOAT* pFlo, FLOAT* pPowStates, UINT32 n )
{
	FLOAT fTol = 1E-6;
	UINT32 i = 0;
	FLOAT p;
	for( i=0; i<n; i++ )
	{
		p = pPowStates[ i ];
		pFlo[ i ] = (p < fTol) ? 0 : turbine_discharge( uTurb, h, p ); // check degenerate case
	}
}

/*********************************************/

INT32 giInteractiveMode = 0;
UINT32 guTransposeSolution = 0;
CHAR gcOutputDelimiter = ' ';
EXTERNC UINT32 guWeightCode = DP_WEIGHT_DEFAULT;

EXTERNC UINT32 guSolveMode = 0; /* which problem are we solving? */
EXTERNC FLOAT gfHeadCurr = 1.0; /**/

EXTERNC UINT32 guUserStepCount; /* number of objective function discretizations */
EXTERNC UINT32 guProblemStepCount; /* adjusted number of objective function discretizations */

FLOAT gfSolutionDelta; /* determined from guProblemStepCount and the sizes of the units */
FLOAT gfOtherSolutionDelta;

EXTERNC UINT32 guStages; /* simply, number of units */
EXTERNC UINT32 guStates; /* number of solution discretizations */

FLOAT gfStateMin, gfStateMax;

/* a cache of the objective function values for each stage and statestep */
FLOAT* gpfFlowAllocations = 0; /* [ states ] */
FLOAT* gpfPowerAllocations = 0; /* [ stages, steps ] */
FLOAT* gpfHKTableValues = 0; /* [ stages, steps ] */

/* a matrix that accumulates the allocation decisions. reevaluated for each scenario until the global decision matrix is filled */
FLOAT* gpfScenarioDecisionValues = 0; /* [ states, states ] but actually triangular */ /* THE BIG KAHUNA */
UINT16* gpu16ScenarioDecisionStates = 0;
UINT8* gpu8ScenarioDecisionCounts = 0;

/* these are the accumulated value of particular state decisions made in the current stage */
UINT16* gpu16StateOfMax = 0; /* [ states ] but also known as [ scenario ] */
FLOAT* gpfValueOfMax = 0; /* [ states ] but also known as [ scenario ] */
UINT8* gpu8CountOfMax = 0; /* [ states ] but also known as [ scenario ] */

EXTERNC FLOAT* gpfSolutionAllocations = 0; /* [ states ] */
EXTERNC FLOAT* gpfOtherSolutionAllocations = 0; /* [ states ] */
EXTERNC FLOAT* gpfHKSolutionAverages = 0; /* [ states ] */

/* the final decision matrices holding the allocations */
FLOAT* gpfGlobalDecisionValues = 0; /* [ stages, states ] */
UINT8* gpu8GlobalDecisionCounts = 0; /* [ stages, states ] */
UINT16* gpu16GlobalDecisionStateMap = 0; /* [ stages, states ] */
FLOAT* gpfGlobalDecisionAllocations = 0; /* [ stages, states ] */

UINT16* gpu16Solution = 0; /* [ stages, states ] */
EXTERNC FLOAT* gpfSolution = 0; /* [ stages, states ] */
EXTERNC FLOAT* gpfOtherSolution = 0; /* [ stages, states ] */
EXTERNC FLOAT* gpfHKSolution = 0; /* [ stages, states ] */

UINT16* gpu16LocalNearOptimumStates = 0; /* [ stages ] */
UINT16* gpu16LocalMaximumStates = 0; /* [ stages ] */
UINT16* gpu16LocalOptimumStates = 0; /* [ stages ] */
UINT8* gpu8StageChecklist = 0; /* [ stages ] */
UINT8* gpu8StageChecklistCopy = 0; /* [ stages ] */

UINT16* gpu16LocalDecisions = 0; /* [ states ] */
FLOAT* gpfStageMetric = 0; /* [ states ] */
UINT16* gpu16StageMap = 0; /* [ stages ] */

/************/

/* [0] == total of faceplate capacities, head-adjusted and adjusted for overcapacity */
/* [1] == total of faceplate capacities and adjusted for overcapacity */
/* [2] == total of faceplate capacities */
FLOAT gfTotalPower[ 3 ], gfTotalFlow[ 3 ]; /* max plant power output */

/* [0] == faceplate capacity, head-adjusted and adjusted for overcapacity */
/* [1] == faceplate capacities, adjusted for overcapacity */
/* [2] == faceplate capacities */
FLOAT gfMaxPower[ 3 ], gfMaxFlow[ 3 ]; /* max plant discharge */
FLOAT gfMinMaxPower[ 3 ], gfMinMaxFlow[ 3 ]; /* largest output of the smallest unit */

/**********************************************/

EXTERNC void dp_assign_weights()
{
	FLOAT fTol = 1E-6;
	UINT32 uTurb, u;

	/* determine largest unit and total sum */
	gfMaxPower[0] = gfMaxFlow[0] = gfMaxPower[1] = gfMaxFlow[1] = 0;
	gfMinMaxPower[0] = gfMinMaxFlow[0] = gfMinMaxPower[1] = gfMinMaxFlow[1] = 9e9; // big float
	gfTotalPower[0] = gfTotalFlow[0] = gfTotalPower[1] = gfTotalFlow[1] = 0;

	for( uTurb=0; uTurb<guTurbs; uTurb++ )
	{
		FLOAT p[3], q[3];

		p[0] = turbine_maxpower_at_currenthead( uTurb, gfHeadCurr );
		p[1] = p[2] = gpTurbs[ uTurb ].fMaxPower;
		q[0] = turbine_maxdischarge_at_currenthead( uTurb, gfHeadCurr );
		q[1] = q[2] = gpTurbs[ uTurb ].fMaxFlow;

		for( u = 0; u < 3; u++ )
		{
			FLOAT fPCapFact = 1.0;
			FLOAT fQCapFact = 1.0;

#ifdef ENABLE_CAPACITY_SCALING
			fPCapFact = u == 2 ? 1 : gpTurbs[ uTurb ].fGenCapacity;
			fQCapFact = u == 2 ? 1 : gpTurbs[ uTurb ].fGenCapacity;
#endif // ENABLE_CAPACITY_SCALING

			/* bounds check */
			p[u] = ( gpTurbs[ uTurb ].fMaxPower < fTol ) ? 0 : p[u];
			q[u] = ( gpTurbs[ uTurb ].fMaxFlow < fTol ) ? 0 : q[u];

			/* determine max and min over all units by taking into
			 * account any allowable over-capacity factor.
			 */
			gfMaxPower[u] = MAX( p[u] * fPCapFact, gfMaxPower[u] );
			if( fPCapFact > fTol ) { gfMinMaxPower[u] = MIN( p[u] * fPCapFact, gfMinMaxPower[u] ); }
			gfMaxFlow[u] = MAX( q[u] * fQCapFact, gfMaxFlow[u] );
			if( fQCapFact > fTol ) { gfMinMaxFlow[u] = MIN( q[u] * fQCapFact, gfMinMaxFlow[u] ); }

#ifdef ENABLE_NEW_CALCS
			gfTotalPower[u] += p[u] * fPCapFact;
			gfTotalFlow[u] += q[u] * fQCapFact;
#else /* ENABLE_NEW_CALCS */
			gfTotalPower[u] += p[u];
			gfTotalFlow[u] += q[u];
#endif /* ENABLE_NEW_CALCS */
		}
	}

	/* scale total, max & mimax values to preserve only specified number of sigfigs */
	for( u = 0; u < 2; u++ )
	{
		gfMaxPower[u] = float_round( gfMaxPower[u], DP_CONFIG_SIGFIGS );
		gfMaxFlow[u] = float_round( gfMaxFlow[u], DP_CONFIG_SIGFIGS );
		gfMinMaxPower[u] = float_round( gfMinMaxPower[u], DP_CONFIG_SIGFIGS );
		gfMinMaxFlow[u] = float_round( gfMinMaxFlow[u], DP_CONFIG_SIGFIGS );
		gfTotalPower[u] = float_round( gfTotalPower[u], DP_CONFIG_SIGFIGS );
		gfTotalFlow[u] = float_round( gfTotalFlow[u], DP_CONFIG_SIGFIGS );
	}

	/* assign weights: use faceplate capacities */
	for( uTurb=0; uTurb<guTurbs; uTurb++ )
	{
		FLOAT fW = gpTurbs[ uTurb ].fWeight;
		switch( guWeightCode & DP_WEIGHT_CODEMASK )
		{
			case DP_WEIGHT_DEFAULT: /* do nothing */ break;
			case DP_WEIGHT_EQUAL: fW = 1.0; break;
			case DP_WEIGHT_MAXPOWER: fW = gpTurbs[ uTurb ].fMaxPower; break;
			case DP_WEIGHT_MAXFLOW: fW = gpTurbs[ uTurb ].fMaxFlow; break;
			case DP_WEIGHT_MINPOWER: fW = gfTotalPower[1] - gpTurbs[ uTurb ].fMaxPower; break;
			case DP_WEIGHT_MINFLOW: fW = gfTotalFlow[1] - gpTurbs[ uTurb ].fMaxFlow; break;
			default: ;
		}
		if( guWeightCode & DP_WEIGHT_RELATIVE )
		{
			switch( guWeightCode )
			{
				case DP_WEIGHT_DEFAULT: break;
				case DP_WEIGHT_EQUAL: break;
				case DP_WEIGHT_MAXPOWER: /* fallthrough */
				case DP_WEIGHT_MINPOWER: fW /= gfTotalPower[1]; break;
				case DP_WEIGHT_MAXFLOW: /* fallthrough */
				case DP_WEIGHT_MINFLOW: fW /= gfTotalFlow[1]; break;
				default: ;
			}
		}

		fW = ( gpTurbs[ uTurb ].fMaxPower < fTol || gpTurbs[ uTurb ].fMaxFlow < fTol ) ? 0 : fW;
		gpTurbs[ uTurb ].fWeight = fW;
	}
}

void dp_print_weights()
{
	UINT32 uTurb;
	for( uTurb = 0; uTurb < guTurbs; uTurb++ )
	{
		fprintf( stdout, "weight %20s ", gpTurbs[ uTurb ].szName );
		fprintf( stdout, float_format( gpTurbs[ uTurb ].fWeight ), gpTurbs[ uTurb ].fWeight );
		fprintf( stdout, "\n" );
	}
}

void dp_print_weighting()
{
	fprintf( stdout, "weighting " );
	if(		 (guWeightCode & DP_WEIGHT_RELATIVE) )							fprintf( stdout, "relative " );
	/**/
	if( 	 (guWeightCode & DP_WEIGHT_CODEMASK) == DP_WEIGHT_DEFAULT )		fprintf( stdout, "default" );
	else if( (guWeightCode & DP_WEIGHT_CODEMASK) == DP_WEIGHT_EQUAL )		fprintf( stdout, "equal" );
	else if( (guWeightCode & DP_WEIGHT_CODEMASK) == DP_WEIGHT_MAXPOWER )	fprintf( stdout, "maxpower" );
	else if( (guWeightCode & DP_WEIGHT_CODEMASK) == DP_WEIGHT_MAXFLOW )		fprintf( stdout, "maxflow" );
	else if( (guWeightCode & DP_WEIGHT_CODEMASK) == DP_WEIGHT_MINPOWER )	fprintf( stdout, "minpower" );
	else if( (guWeightCode & DP_WEIGHT_CODEMASK) == DP_WEIGHT_MINFLOW )		fprintf( stdout, "minflow" );
	fprintf( stdout, "\n" );
}

void dp_print_config()
{
	fprintf( stdout, "losscoef " );
	fprintf( stdout, float_format( gfPlantLossCoef ), gfPlantLossCoef );
	fprintf( stdout, "coordinationfactora " );
	fprintf( stdout, float_format( gfCoordinationFactorA ), gfCoordinationFactorA );
	fprintf( stdout, "coordinationfactorb " );
	fprintf( stdout, float_format( gfCoordinationFactorB ), gfCoordinationFactorB );

	fprintf( stdout, " head " );
	fprintf( stdout, float_format( gfHeadCurr ), gfHeadCurr );
	fprintf( stdout, " %s ", char_units( DP_UNIT_LENGTH ) );
	fprintf( stdout, " min " );
	fprintf( stdout, float_format( gfStateMin ), gfStateMin );
	fprintf( stdout, " %s ", char_units( DP_UNIT_FLOW ) );
	fprintf( stdout, " max " );
	fprintf( stdout, float_format( gfStateMax ), gfStateMax );
	fprintf( stdout, " %s ", char_units( DP_UNIT_FLOW ) );
	fprintf( stdout, " unitsteps " );
	fprintf( stdout, "%lu\n", guUserStepCount );
}

/* determine problem size */
EXTERNC void dp_resize()
{
	ex_clear();

	if( !guSolveMode )
	{
		sprintf( gcPrintBuff, "%s:%d (" VERSIONED_NAME  ") Unspecified solvemode.\n", __FILE__, __LINE__ );
 		PRINT_STDERR( gcPrintBuff );
		dp_cleanup_fatal();
		return;
	}

	{
		CHAR* szSolveMode = (guSolveMode == DP_OPTIMIZE_FOR_POWER) ? "power" : "flow";

		sprintf( gcPrintBuff, "Solving for %s.\n", szSolveMode );
		PRINT_STDOUT( gcPrintBuff );
	}

	VERIFY( guUserStepCount >= 5 );
	guProblemStepCount = guUserStepCount;
	guStages = guTurbs;

#ifndef ENABLE_SPECIFIED_MINMAX_STATE
	gfStateMax = gfStateMin = 0;
#endif // ENABLE_SPECIFIED_MINMAX_STATE

	{
		FLOAT fMaxDecision = (guSolveMode == DP_OPTIMIZE_FOR_POWER) ? gfMaxPower[0] : gfMaxFlow[0];
		FLOAT fMinMaxDecision = (guSolveMode == DP_OPTIMIZE_FOR_POWER) ? gfMinMaxPower[0] : gfMinMaxFlow[0];
		FLOAT fTotDecision = (guSolveMode == DP_OPTIMIZE_FOR_POWER) ? gfTotalPower[0] : gfTotalFlow[0];

		FLOAT fMaxAltDecision = (guSolveMode == DP_OPTIMIZE_FOR_POWER) ? gfMaxFlow[0] : gfMaxPower[0];
		FLOAT fMinMaxAltDecision = (guSolveMode == DP_OPTIMIZE_FOR_POWER) ? gfMinMaxFlow[0] : gfMinMaxPower[0];
		FLOAT fTotAltDecision = (guSolveMode == DP_OPTIMIZE_FOR_POWER) ? gfTotalFlow[0] : gfTotalPower[0];

		UINT32 uNonZeroStageStepCount = guProblemStepCount - 1; // -1 to discount stage zero
		FLOAT fCommonStepSize = fMinMaxDecision / uNonZeroStageStepCount;

		UINT32 uIter = 0;
		FLOAT fBestSquare = 4e4; // a bigish number
		UINT32 uBestNonZeroStageStepCount = 0;

		if( guDebugMode )
		{
			sprintf( gcPrintBuff, "fMaxDecision: %f   fMinMaxDecision: %f   fTotDecision: %f\n", fMaxDecision, fMinMaxDecision, fTotDecision );
			PRINT_STDOUT( gcPrintBuff );
		}

		/* determine the value that factors at least uNonZeroStageStepCount times */
		VERIFY( fMinMaxDecision > uNonZeroStageStepCount );
		while( uNonZeroStageStepCount < 200 && fCommonStepSize > 10.0 ) /* tuning params */
		{
			fCommonStepSize = fMaxDecision / uNonZeroStageStepCount;
			{
				FLOAT fNumTotSteps = fTotDecision / fCommonStepSize;
				FLOAT fNumMaxSteps = fMaxDecision / fCommonStepSize;
				FLOAT fNumMinMaxSteps = fMinMaxDecision / fCommonStepSize;
				FLOAT fIntT, fFracT = modff( fNumTotSteps, &fIntT );
				FLOAT fIntM, fFracM = modff( fNumMaxSteps, &fIntM );
				FLOAT fIntMM, fFracMM = modff( fNumMinMaxSteps, &fIntMM );
				FLOAT fSquare = fFracT * fFracT + fFracM * fFracM + fFracMM * fFracMM;
				FLOAT fTol = 1E-1;
				/* heuristic approach to finding the LCM. */
				if( fSquare < fBestSquare )
				{
					uBestNonZeroStageStepCount = uNonZeroStageStepCount;
					fBestSquare = fSquare;
					if( fabs( fFracM - fFracT ) < fTol ) { break; }
				}
				uNonZeroStageStepCount++;
				uIter++;
			}
		}
		guProblemStepCount = ( uBestNonZeroStageStepCount == 0 ? guProblemStepCount : ( uBestNonZeroStageStepCount + 1 ) ); // +1 to include stage zero
		if( guProblemStepCount != guUserStepCount )
		{
			sprintf( gcPrintBuff, "UnitSteps adjusted by %d to improve rounding.\n", guProblemStepCount - guUserStepCount );
			PRINT_STDOUT( gcPrintBuff );
		}
		fCommonStepSize = fMaxDecision / ( uBestNonZeroStageStepCount == 0 ? guProblemStepCount - 1 : uBestNonZeroStageStepCount ); // +1 to include stage zero;

		gfSolutionDelta = fCommonStepSize;
		gfOtherSolutionDelta = fCommonStepSize * (fMaxAltDecision / fMaxDecision);
		{
			FLOAT fIntT;
			modff( fTotDecision / fCommonStepSize, &fIntT );
			guStates = fIntT + 1; // +1 to include stage zero
			gfStateMax = fIntT * fCommonStepSize;
		}
	}
}

/* perform allocations */
EXTERNC void dp_malloc()
{
	guMALLOC = 0;

	VERIFY( guStages * guStates > 0 );
	if( ex_didFail() )
	{
		sprintf( gcPrintBuff, "either zero Stages or States.\n" );
		PRINT_STDOUT( gcPrintBuff );
		return;
	}

	/* the big kahuna ... */
	DP_MALLOC_CACHE( gpfScenarioDecisionValues, FLOAT, guStates * guStates, __FILE__, __LINE__ ); /* [ states, states ] but actually triangular */
	DP_MALLOC_CACHE( gpu16ScenarioDecisionStates, UINT16, guStates * guStates, __FILE__, __LINE__ ); /* [ states, states ] but actually triangular */
	DP_MALLOC_CACHE( gpu8ScenarioDecisionCounts, UINT8, guStates * guStates, __FILE__, __LINE__ ); /* [ states, states ] but actually triangular */

	DP_MALLOC_CACHE( gpfHKTableValues, FLOAT, guStages * guProblemStepCount, __FILE__, __LINE__ ); /* [ stages, steps ] */
	DP_MALLOC_CACHE( gpfPowerAllocations, FLOAT, guStages * guProblemStepCount, __FILE__, __LINE__ ); /* [ stages, steps ] */
	DP_MALLOC_CACHE( gpfFlowAllocations, FLOAT, guStages * guProblemStepCount, __FILE__, __LINE__ ); /* [ stages, steps ] */

	/* other allocations... */
	DP_MALLOC_CACHE( gpfGlobalDecisionValues, FLOAT, guStages * guStates, __FILE__, __LINE__ ); /* [ stages, states ] */
	DP_MALLOC_CACHE( gpu8GlobalDecisionCounts, UINT8, guStages * guStates, __FILE__, __LINE__ ); /* [ stages, states ] */
	DP_MALLOC_CACHE( gpu16GlobalDecisionStateMap, UINT16, guStages * guStates, __FILE__, __LINE__ ); /* [ stages, states ] */
	DP_MALLOC_CACHE( gpfGlobalDecisionAllocations, FLOAT, guStages * guStates, __FILE__, __LINE__ ); /* [ stages, states ] */
	DP_MALLOC_CACHE( gpu16Solution, UINT16, guStages * guStates, __FILE__, __LINE__ ); /* [ stages, states ] */
	DP_MALLOC_CACHE( gpfSolution, FLOAT, guStages * guStates, __FILE__, __LINE__ ); /* [ stages, states ] */
	DP_MALLOC_CACHE( gpfOtherSolution, FLOAT, guStages * guStates, __FILE__, __LINE__ ); /* [ stages, states ] */
	DP_MALLOC_CACHE( gpfHKSolution, FLOAT, guStages * guStates, __FILE__, __LINE__ ); /* [ stages, states ] */

	/* smaller allocations */
	DP_MALLOC_CACHE( gpfSolutionAllocations, FLOAT, guStates, __FILE__, __LINE__ ); /* [ states ] */
	DP_MALLOC_CACHE( gpu16StateOfMax, UINT16, guStates, __FILE__, __LINE__ ); /* [ states ] */
	DP_MALLOC_CACHE( gpfValueOfMax, FLOAT, guStates, __FILE__, __LINE__ ); /* [ states ] */
	DP_MALLOC_CACHE( gpu8CountOfMax, UINT8, guStates, __FILE__, __LINE__ ); /* [ states ] */
	DP_MALLOC_CACHE( gpfOtherSolutionAllocations, FLOAT, guStates, __FILE__, __LINE__ ); /* [ states ] */
	DP_MALLOC_CACHE( gpfHKSolutionAverages, FLOAT, guStates, __FILE__, __LINE__ ); /* [ states ] */

	DP_MALLOC_CACHE( gpu16LocalNearOptimumStates, UINT16, guStages, __FILE__, __LINE__ ); /* [ stages ] */
	DP_MALLOC_CACHE( gpu16LocalMaximumStates, UINT16, guStages, __FILE__, __LINE__ ); /* [ stages ] */
	DP_MALLOC_CACHE( gpu16LocalOptimumStates, UINT16, guStages, __FILE__, __LINE__ ); /* [ stages ] */
	DP_MALLOC_CACHE( gpu8StageChecklist, UINT8, guStages, __FILE__, __LINE__ ); /* [ stages ] */
	DP_MALLOC_CACHE( gpu8StageChecklistCopy, UINT8, guStages, __FILE__, __LINE__ ); /* [ stages ] */

	DP_MALLOC_CACHE( gpu16LocalDecisions, UINT16, guStates, __FILE__, __LINE__ ); /* [ states ] */
	DP_MALLOC_CACHE( gpfStageMetric, FLOAT, guStates, __FILE__, __LINE__ ); /* [ states ] */
	DP_MALLOC_CACHE( gpu16StageMap, UINT16, guStages, __FILE__, __LINE__ ); /* [ stages ] */

	if( guDebugMode )
	{
		printf( "%lu total bytes allocated\n", guMALLOC );
	}
}

EXTERNC void dp_malloc_control( UINT32 uControl )
{
#ifdef ENABLE_MALLOC_CACHE
	if( uControl == 0 ) {
		/* disable */
		guMemCacheOn = 0;
	} else if( uControl == 1 ) {
		/* enable */
		guMemCacheOn = 1;
	} else if( uControl == 2 ) {
		/* flush */
		dp_cleanup();
	}
#else
	dp_cleanup();
#endif
}

EXTERNC void dp_cleanup()
{
	DP_FREE_CACHE( gpfScenarioDecisionValues, FLOAT );
	DP_FREE_CACHE( gpu16ScenarioDecisionStates, UINT16 );
	DP_FREE_CACHE( gpu8ScenarioDecisionCounts, UINT8 );

	DP_FREE_CACHE( gpfHKTableValues, FLOAT );
	DP_FREE_CACHE( gpfPowerAllocations, FLOAT );
	DP_FREE_CACHE( gpfGlobalDecisionValues, FLOAT );
	DP_FREE_CACHE( gpu8GlobalDecisionCounts, UINT8 );
	DP_FREE_CACHE( gpu16GlobalDecisionStateMap, UINT16 );
	DP_FREE_CACHE( gpfGlobalDecisionAllocations, FLOAT );
	DP_FREE_CACHE( gpu16Solution, UINT16 );
	DP_FREE_CACHE( gpfSolution, FLOAT );
	DP_FREE_CACHE( gpfOtherSolution, FLOAT );
	DP_FREE_CACHE( gpfHKSolution, FLOAT );
	DP_FREE_CACHE( gpfOtherSolutionAllocations, FLOAT );
	DP_FREE_CACHE( gpfHKSolutionAverages, FLOAT );

	DP_FREE_CACHE( gpu16StateOfMax, UINT16 );
	DP_FREE_CACHE( gpfValueOfMax, FLOAT );
	DP_FREE_CACHE( gpu8CountOfMax, UINT8 );
	DP_FREE_CACHE( gpfSolutionAllocations, FLOAT );
	DP_FREE_CACHE( gpfFlowAllocations, FLOAT );

	DP_FREE_CACHE( gpu16LocalNearOptimumStates, UINT16 );
	DP_FREE_CACHE( gpu16LocalMaximumStates, UINT16 );
	DP_FREE_CACHE( gpu16LocalOptimumStates, UINT16 );
	DP_FREE_CACHE( gpu8StageChecklist, UINT8 );
	DP_FREE_CACHE( gpu8StageChecklistCopy, UINT8 );

	DP_FREE_CACHE( gpu16LocalDecisions, UINT16 );
	DP_FREE_CACHE( gpfStageMetric, FLOAT );
	DP_FREE_CACHE( gpu16StageMap, UINT16 );
}

void dp_cleanup_fatal()
{
	if( !giInteractiveMode ) {
		dp_malloc_control( 2 );
		parse_freeallblocks();
		exit(-1);
	}
}

int CDECLCALL dp_fnStageMetricComparison(const void* pA, const void* pB)
{
#if 1 // removes typecast
	FLOAT a = gpfStageMetric[ *(UINT16*)pA ];
	FLOAT b = gpfStageMetric[ *(UINT16*)pB ];
#else
	UINT32 a = gpfStageMetric[ *(UINT16*)pA ];
	UINT32 b = gpfStageMetric[ *(UINT16*)pB ];
#endif

	return a < b ? -1 : ( a > b ? 1 : 0 );
}

EXTERNC void dp()
{
	UINT32 uStage, uState, uScenario;
	UINT16 u16PrioritizedStage;
	FLOAT fTol = 1E-6;

	ex_clear();

	// check for ill conditioning
	VERIFY( gfHeadCurr > fTol );
	if( ex_didFail() )
	{
		sprintf( gcPrintBuff, "infeasible problem: zero head.\n" );
		PRINT_STDOUT( gcPrintBuff );
		goto cleanfailure;
	}

	/* sync */

	arr_float_step( gpfSolutionAllocations, gfStateMin, gfSolutionDelta, guStates );
	arr_float_step( gpfOtherSolutionAllocations, gfStateMin, gfOtherSolutionDelta, guStates );

	/* sync */

	//////////////////////////////////////////
	// INIT HK TABLE

	// compute the flow allocations and resulting power

	{
		typedef void fnInitFunction( UINT32, FLOAT, FLOAT*, FLOAT*, UINT32 );

#ifdef ENABLE_NEW_CALCS
		FLOAT fHeadEffectOnUnit = 1.0;
#else
		FLOAT fHeadEffectOnUnit =		( guSolveMode == DP_OPTIMIZE_FOR_POWER ) ? ( gfTotalPower[0] / gfTotalPower[1] ) : ( gfTotalFlow[0] / gfTotalFlow[1] );
#endif
		FLOAT* pIndepVariable =				( guSolveMode == DP_OPTIMIZE_FOR_POWER ) ? gpfPowerAllocations : gpfFlowAllocations;
		FLOAT* pDepVariable =				( guSolveMode == DP_OPTIMIZE_FOR_POWER ) ? gpfFlowAllocations : gpfPowerAllocations;
		fnInitFunction* pVarInitFunction =	( guSolveMode == DP_OPTIMIZE_FOR_POWER ) ? arr_turbine_q : arr_turbine_p;
		FLOAT* pMaxTurbineParam =			( guSolveMode == DP_OPTIMIZE_FOR_POWER ) ? &(gpTurbs[ 0 ].fMaxPower) : &(gpTurbs[ 0 ].fMaxFlow);

		/* sync */

		for( uStage = 0; uStage < guStages; uStage++ ) /* par */
		{
			if( gpTurbs[ uStage ].fWeight < fTol ) {
				arr_float_set( &(pIndepVariable[ uStage * guProblemStepCount ]), 0, guProblemStepCount );
			} else {
				FLOAT fMaxTurbineParam = *(FLOAT*)( (TurbineDef*)pMaxTurbineParam + uStage ); /* note standard pointer semantics here */
				FLOAT fMax = fMaxTurbineParam * gpTurbs[ uStage ].fGenCapacity * fHeadEffectOnUnit;
				UINT32 uNum = (UINT32)( ( fMax - 0 ) / gfSolutionDelta ) + 1;

				// this fails, perhaps under very low-head conditions?
				// VERIFY( uNum <= guProblemStepCount );
				// if( ex_didFail() ) { return; }
				arr_float_step(
					&(pIndepVariable[ uStage * guProblemStepCount ]),
					0 /* TODO: min */,
					gfSolutionDelta,
					MIN( guProblemStepCount, uNum )
				);

			}
		}

		/* sync */

		for( uStage = 0; uStage < guStages; uStage++ ) /* par */
		{
			if( gpTurbs[ uStage ].fWeight < fTol ) {
				arr_float_set( &(pDepVariable[ uStage * guProblemStepCount ]), 0, guProblemStepCount );
			} else {
				(*pVarInitFunction)(
					uStage, gfHeadCurr,
					&(pDepVariable[ uStage * guProblemStepCount ]),
					&(pIndepVariable[ uStage * guProblemStepCount ]),
					guProblemStepCount );
			}
		}
	}

	/* sync */

#ifdef ENABLE_OUTOFLINE_NUMERICAL_CLEANUP
	for( uStage = 0; uStage < guStages; uStage++ ) /* par */
	{
		arr_float_mark_nan_as_zero( &(gpfPowerAllocations[ uStage * guProblemStepCount ]), guProblemStepCount );
		arr_float_mark_inf_as_zero( &(gpfPowerAllocations[ uStage * guProblemStepCount ]), guProblemStepCount );
		//
		arr_float_mark_nan_as_zero( &(gpfFlowAllocations[ uStage * guProblemStepCount ]), guProblemStepCount );
		arr_float_mark_inf_as_zero( &(gpfFlowAllocations[ uStage * guProblemStepCount ]), guProblemStepCount );
	}
#endif

	/* sync */

	// compute HK (the performance metric)
	for( uStage = 0; uStage < guStages; uStage++ ) /* par */
	{
		arr_float_div2(
			&(gpfHKTableValues[ uStage * guProblemStepCount ]),
			&(gpfPowerAllocations[ uStage * guProblemStepCount ]), /* [ stages, steps ] */
			&(gpfFlowAllocations[ uStage * guProblemStepCount ]), /* [ stages, steps ] */
			guProblemStepCount );
	}

	/* sync */

#ifdef ENABLE_OUTOFLINE_NUMERICAL_CLEANUP
	for( uStage = 0; uStage < guStages; uStage++ ) /* par */
	{
		arr_float_mark_nan_as_zero( &(gpfHKTableValues[ uStage * guProblemStepCount ]), guProblemStepCount );
		arr_float_mark_inf_as_zero( &(gpfHKTableValues[ uStage * guProblemStepCount ]), guProblemStepCount );
	}
#endif

	/* sync */

#ifdef ENABLE_HK_SCALING

	/* apply weighting / scaling */
	for( uStage = 0; uStage < guStages; uStage++ ) /* par */
	{
		arr_float_scale(
			&(gpfHKTableValues[ uStage * guProblemStepCount ]),
			gpTurbs[ uStage ].fWeight,
			guProblemStepCount
		);
	}

#endif

	/* sync */

#ifdef DEBUG_INITPASS
	fprintf( stdout, "\ngpfPowerAllocations\n" );
	for( uStage = 0; uStage < guStages; uStage++ )
	{
		sprintf( gcPrintBuff, "%s%c", gpTurbs[ uStage ].szName, gcOutputDelimiter ); PRINT_STDOUT( gcPrintBuff );
		arr_float_print( stdout, &(gpfPowerAllocations[ uStage * guProblemStepCount ]), guProblemStepCount, gcOutputDelimiter );
	}

	fprintf( stdout, "\ngpfFlowAllocations\n" );
	for( uStage = 0; uStage < guStages; uStage++ )
	{
		sprintf( gcPrintBuff, "%s%c", gpTurbs[ uStage ].szName, gcOutputDelimiter ); PRINT_STDOUT( gcPrintBuff );
		arr_float_print( stdout, &(gpfFlowAllocations[ uStage * guProblemStepCount ]), guProblemStepCount, gcOutputDelimiter );
	}

	fprintf( stdout, "\ngpfHKTableValues\n" );
	for( uStage = 0; uStage < guStages; uStage++ )
	{
		sprintf( gcPrintBuff, "%s%c", gpTurbs[ uStage ].szName, gcOutputDelimiter ); PRINT_STDOUT( gcPrintBuff );
		arr_float_print( stdout, &(gpfHKTableValues[ uStage * guProblemStepCount ]), guProblemStepCount, gcOutputDelimiter );
	}

#endif

	/* sync */

	// determine local optimum operating points
	for( uStage = 0; uStage < guStages; uStage++ ) /* par */
	{
		arr_float_max_nostride(
			&(gpu16LocalOptimumStates[ uStage ]),
			&(gpfHKTableValues[ uStage * guProblemStepCount ]),
			guProblemStepCount );
#if 1
		// unit cant run
		if( gpu16LocalOptimumStates[ uStage ] == MAX_UINT16 )
		{ gpu16LocalOptimumStates[ uStage ] = 0; }
#else
		VERIFY( gpu16LocalOptimumStates[ uStage ] != MAX_UINT16 );
		if( ex_didFail() )
		{
			sprintf( gcPrintBuff, "unrealistic problem: flat efficiency curve.\n" );
			PRINT_STDOUT( gcPrintBuff );
			goto cleanfailure;
		}
#endif
	}

	/* sync */

	// determine the largest operating point
	for( uStage = 0; uStage < guStages; uStage++ ) /* par */
	{
		arr_float_last_nonzero_nostride(
			&(gpu16LocalMaximumStates[ uStage ]),
			&(gpfHKTableValues[ uStage * guProblemStepCount ]),
			guProblemStepCount );
#if 1
		// unit cant run
		if( gpu16LocalMaximumStates[ uStage ] == MAX_UINT16 )
		{ gpu16LocalMaximumStates[ uStage ] = 0; }
#else
		VERIFY( gpu16LocalMaximumStates[ uStage ] != MAX_UINT16 );
		if( ex_didFail() )
		{
			sprintf( gcPrintBuff, "unrealistic problem: flat efficiency curve.\n" );
			PRINT_STDOUT( gcPrintBuff );
			goto cleanfailure;
		}
#endif
	}

	/* sync */

	// determine the lowest state for some percentage of optimum
	for( uStage = 0; uStage < guStages; uStage++ ) /* par */
	{
		arr_float_tol_nostride(
			&(gpu16LocalNearOptimumStates[ uStage ]),
			&(gpfHKTableValues[ uStage * guProblemStepCount ]),
			gpu16LocalOptimumStates[ uStage ],
			gfCoordinationFactorA,
			guProblemStepCount );
	}

	/* sync */

	// check for ill conditioning...
	{
		UINT32 u = 0;
		arr_uint16_sum_nostride( &u, gpu16LocalMaximumStates, guStages );
		VERIFY( u != 0 ); // bad
		VERIFY( u < MAX_UINT16 ); // also bad!
		if( ex_didFail() )
		{
			sprintf( gcPrintBuff, "infeasible problem: zero objective function.\n" );
			PRINT_STDOUT( gcPrintBuff );
			goto cleanfailure;
		}
	}

	/* sync */

	// look for HK problems...
	for( uStage = 0; uStage < guStages; uStage++ ) /* par */
	{
		UINT32 uCurve = gpTurbs[ uStage ].uCurveNum;
		UINT32 uPoints = gpCurves[ uCurve ].uPoints;
		if( uPoints > 1 ) // skip small curves
		{
			FLOAT* pfEff = gpCurves[ uCurve ].pfEffFact;
			FLOAT fEff8 = pfEff[ uPoints - 2 ];
			FLOAT fEff9 = pfEff[ uPoints - 1 ];
			//
			// if last point is less efficient then optimum point should come before last point
			int bOK1 = (fEff8 > fEff9) & (gpu16LocalOptimumStates[ uStage ] < gpu16LocalMaximumStates[ uStage ]);
			// if last point is same or best efficiency then optimum point should be last point
			int bOK2 = (fEff8 <= fEff9) & (gpu16LocalOptimumStates[ uStage ] == gpu16LocalMaximumStates[ uStage ]);
			if( !( bOK1 ^ bOK2 ) )
			{
				sprintf( gcPrintBuff, "condition on unit %s: maximum operating point at H/K of %f and optimum operating point at H/K of %f.\n",
					gpTurbs[ uStage ].szName,
					gpfHKTableValues[ uStage * guProblemStepCount + gpu16LocalMaximumStates[ uStage ] ],
					gpfHKTableValues[ uStage * guProblemStepCount + gpu16LocalOptimumStates[ uStage ] ] );
				PRINT_STDOUT( gcPrintBuff );
			}
		}
	}

	/* sync */

	// rank and sort stages in terms of lowest to highest overall performance
	for( uStage = 0; uStage < guStages; uStage++ )
	{
		gpu16StageMap[ uStage ] = (UINT16)uStage;

		// basically does an integration
		arr_float_metric(
			&(gpfStageMetric[ uStage ]),
			&(gpfHKTableValues[ uStage * guProblemStepCount ]),
			guProblemStepCount );

#ifndef ENABLE_HK_SCALING // note ifndef

		/* apply weighting / scaling */
		gpfStageMetric[ uStage ] *= gpTurbs[ uStage ].fWeight;

#endif

	}
#ifdef ENABLE_PRIORITIZEDSTAGES
 	qsort( gpu16StageMap, guStages, sizeof(UINT16), dp_fnStageMetricComparison );
#endif

	/* sync */

	//////////////////////////////////////////
	// SOLVE TRIVIAL PROBLEM (LAST STAGE)

	uStage = guStages - 1;
	u16PrioritizedStage = gpu16StageMap[ uStage ];
 {
	UINT16 u16LocalMaximumState = gpu16LocalMaximumStates[ u16PrioritizedStage ];
	UINT16 u16LocalOptimumState = gpu16LocalOptimumStates[ u16PrioritizedStage ];
	UINT16 u16LocalCoordinationState = u16LocalMaximumState;// * gfCoordinationFactorB;//jch

	UINT16 u16FRUpperBoundState = u16LocalMaximumState;
	UINT16 u16FRUpperBoundCount = u16FRUpperBoundState + 1;

	{
		VERIFY( u16LocalMaximumState < guProblemStepCount );
		if( ex_didFail() ) { goto cleanfailure; } else
		{
			UINT16* pu16GlobalDecisionStateMap = &(gpu16GlobalDecisionStateMap[ u16PrioritizedStage * guStates ]);
			FLOAT* pfGlobalDecisionValues = &(gpfGlobalDecisionValues[ u16PrioritizedStage * guStates ]);
			FLOAT* pfProblemHK = &(gpfHKTableValues[ u16PrioritizedStage * guProblemStepCount ]);
			UINT8* pu8GlobalDecisionCounts = &(gpu8GlobalDecisionCounts[ u16PrioritizedStage * guStates ]);
			FLOAT fTol = 1E-6;

			for( uState = 0; uState < u16FRUpperBoundCount; uState++ )
			{
				pu16GlobalDecisionStateMap[ uState ] = (UINT16)uState;
				pfGlobalDecisionValues[ uState ] = pfProblemHK[ uState ];
				pu8GlobalDecisionCounts[ uState ] = ( pfProblemHK[ uState ] < fTol ? 0 : 1 );
			}

#ifdef DEBUG_BACKWARDPASS
			fprintf( stdout, "\ngpfGlobalDecisionValues\n" );
			arr_float_print( stdout, gpfSolutionAllocations, u16FRUpperBoundCount, gcOutputDelimiter );
			arr_float_print( stdout, pfGlobalDecisionValues, u16FRUpperBoundCount, gcOutputDelimiter );
			arr_uint8_print( stdout, pu8GlobalDecisionCounts, u16FRUpperBoundCount, gcOutputDelimiter );
			arr_uint16_print( stdout, pu16GlobalDecisionStateMap, u16FRUpperBoundCount, gcOutputDelimiter );
#endif

		}
	}

	/* sync */

	/******************************************
	 * BACKPASS */

	for( uStage = guStages-2; uStage != MAX_UINT32; uStage-- ) /* seq */
	{
		UINT32 uHigherPriorityStage = gpu16StageMap[ uStage + 1 ];

		u16PrioritizedStage = gpu16StageMap[ uStage ];
		u16LocalMaximumState = gpu16LocalMaximumStates[ u16PrioritizedStage ];
		u16LocalOptimumState = gpu16LocalOptimumStates[ u16PrioritizedStage ];
		u16LocalCoordinationState = u16LocalMaximumState * gfCoordinationFactorB;

		u16FRUpperBoundState += u16LocalMaximumState;
#if 1
		u16FRUpperBoundCount = MIN( (UINT16)( u16FRUpperBoundState + 1 ), guStates );  // HACK shouldnt need min
#else
		u16FRUpperBoundCount = (UINT16)( u16FRUpperBoundState + 1 );

		VERIFY( u16FRUpperBoundState < guStates );
		if( ex_didFail() ) { goto cleanfailure; }
#endif
		/* each additional stage will provide its local solutionspace */
		for( uState = 0; uState < guStates; uState++)
		{
			if( uState <= u16LocalMaximumState ) {
				/* for scenarios that require Q less then the unit minium, all discharge 
				 * decisions are feasible solutions (off-cam operations are needed to
				 * so we keep a smooth plant curve, as impractal as this may seem) */
				gpu16LocalDecisions[ uState ] = (UINT16)uState;
			} else {
				/* for scenarios that require more Q than this unit's maximum, feasible 
				 * local discharge decisions come from only the on-cam range */
				UINT32 uOnCamStateCount = u16LocalMaximumState - u16LocalCoordinationState + 1;
				gpu16LocalDecisions[ uState ] = u16LocalCoordinationState + uState % uOnCamStateCount;
			}
		}

		/* compute the value of each possible operating decision in this stage
		 * considering the best decisions in stages _already_ computed
		 * (therefore, this computation goes into a matrix that is triangular) */
		/* limit by feasible region - this creates a diagonal in the solutionspace */
		for( uScenario = 0; uScenario < u16FRUpperBoundCount; uScenario++ ) /* par */
		{
			/* record effects of local decisions */
			for( uState = 0; uState <= uScenario; uState++ ) /* par */
			{
				FLOAT fTol = 1E-9;
				UINT16 u16LocalDecision = gpu16LocalDecisions[ uState ];

				FLOAT fLocalDecision = gpfHKTableValues[ u16PrioritizedStage * guProblemStepCount + u16LocalDecision ];
				if( fLocalDecision < fTol ) { fLocalDecision = 0; u16LocalDecision = 0; }

				{
					UINT16 u16GlobalDecision = uScenario - u16LocalDecision;
					FLOAT fGlobalDecision = gpfGlobalDecisionValues[ uHigherPriorityStage * guStates + u16GlobalDecision ];
					UINT8 u8GlobalDecisionCount = gpu8GlobalDecisionCounts[ uHigherPriorityStage * guStates + u16GlobalDecision ];

					// remove option as a solution if it is likely to be a local min
					if( u8GlobalDecisionCount < guStages - 1 - uStage && uScenario > u16LocalMaximumState )
					{ fLocalDecision = 0; u16LocalDecision = 0; }

					gpfScenarioDecisionValues[ uScenario * guStates + uState ] = fLocalDecision + fGlobalDecision;
					gpu16ScenarioDecisionStates[ uScenario * guStates + uState ] = u16LocalDecision;
					gpu8ScenarioDecisionCounts[ uScenario * guStates + uState ] = u8GlobalDecisionCount + ( u16LocalDecision == 0 ? 0 : 1 );
				}
			}

		}

		/* sync */

#ifdef DEBUG_BACKWARDPASS
		fprintf( stdout, "\ngpfScenarioDecisionValues\n" );
		arr_float_print( stdout, gpfSolutionAllocations, u16FRUpperBoundCount, gcOutputDelimiter );
		for( uScenario = 0; uScenario < u16FRUpperBoundCount; uScenario++ ) /* par */
		{
			fprintf( stdout, float_format( gpfSolutionAllocations[ uScenario ] ), gpfSolutionAllocations[ uScenario ] );
			fprintf( stdout, "\n" );
			UINT16 u16Count = (UINT16)( uScenario + 1 );
			arr_float_print( stdout, &(gpfScenarioDecisionValues[ uScenario * guStates ]), u16Count, gcOutputDelimiter );
			arr_uint16_print( stdout, &(gpu16ScenarioDecisionStates[ uScenario * guStates ]), u16Count, gcOutputDelimiter );
			arr_uint8_print( stdout, &(gpu8ScenarioDecisionCounts[ uScenario * guStates ]), u16Count, gcOutputDelimiter );
		}
#endif

		// extract this stage's best operations for each possible amount of global allocation
		for( uScenario = 0; uScenario < u16FRUpperBoundCount; uScenario++ ) /* par */
		{
			UINT16 uStateOfMax = 0; // note: zero default
			arr_float_max_nostride_count_pos(
				&(uStateOfMax),
				&(gpfScenarioDecisionValues[ uScenario * guStates ]),	/* [ states, states ] but actually triangular */
				&(gpu8ScenarioDecisionCounts[ uScenario * guStates ]),	/* [ states, states ] but actually triangular */
				uScenario + 1 );										/* NOTE + 1 to make a count */

			VERIFY( uStateOfMax != MAX_UINT16 );
			if( ex_didFail() ) { goto cleanfailure; }

			/* these are all [ states, states ] but triangular */
			gpfValueOfMax[ uScenario ] = gpfScenarioDecisionValues[ uScenario * guStates + uStateOfMax ];
			gpu16StateOfMax[ uScenario ] = gpu16ScenarioDecisionStates[ uScenario * guStates + uStateOfMax ];
			gpu8CountOfMax[ uScenario ] = gpu8ScenarioDecisionCounts[ uScenario * guStates + uStateOfMax ];
		}

#ifdef DEBUG_BACKWARDPASS
		fprintf( stdout, "\ngpu16StateOfMax\n" );
		arr_float_print( stdout, gpfSolutionAllocations, u16FRUpperBoundCount, gcOutputDelimiter );
		for( uScenario = 0; uScenario < u16FRUpperBoundCount; uScenario++ ) /* par */
		{
			fprintf( stdout, float_format( gpfSolutionAllocations[ uScenario ] ), gpfSolutionAllocations[ uScenario ] );
			fprintf( stdout, "\n" );
			UINT16 u16Count = (UINT16)( u16FRUpperBoundState - uScenario + 1 );
			arr_float_print( stdout, &(gpfValueOfMax[ uScenario ]), u16Count, gcOutputDelimiter );
			arr_uint16_print( stdout, &(gpu16StateOfMax[ uScenario ]), u16Count, gcOutputDelimiter );
			arr_uint8_print( stdout, &(gpu8CountOfMax[ uScenario ]), u16Count, gcOutputDelimiter );
		}
#endif
		/* sync */

		// store the optimal operating solution for the next stage to be computed
		arr_float_copy(
			&(gpfGlobalDecisionValues[ u16PrioritizedStage * guStates ]),			/* [ stages, states ] */
			gpfValueOfMax,
			guStates ); // u16FRUpperBoundCount ?
 		arr_uint16_copy(
			&(gpu16GlobalDecisionStateMap[ u16PrioritizedStage * guStates ]),	/* [ stages, states ] */
			gpu16StateOfMax,
			guStates ); // u16FRUpperBoundCount ?
 		arr_uint8_copy(
			&(gpu8GlobalDecisionCounts[ u16PrioritizedStage * guStates ]),	/* [ stages, states ] */
			gpu8CountOfMax,
			guStates ); // u16FRUpperBoundCount ?

		/* sync */

#ifdef DEBUG_BACKWARDPASS
		fprintf( stdout, "\ngpfGlobalDecisionValues\n" );
		arr_float_print( stdout, gpfSolutionAllocations, guStates, gcOutputDelimiter );
		arr_float_print( stdout, &(gpfGlobalDecisionValues[ u16PrioritizedStage * guStates ]), guStates, gcOutputDelimiter );
		arr_uint16_print( stdout, &(gpu16GlobalDecisionStateMap[ u16PrioritizedStage * guStates ]), guStates, gcOutputDelimiter );
		arr_uint8_print( stdout, &(gpu8GlobalDecisionCounts[ u16PrioritizedStage * guStates ]), guStates, gcOutputDelimiter );
#endif
	}
 }
	// unmap allocation states to actual allocations
	for( uStage = 0; uStage < guStages; uStage++ ) /* par */
	{
		arr_float_copy_indirect(
			&(gpfGlobalDecisionAllocations[ uStage * guStates ]), /* [ stages, states ] */
			gpfSolutionAllocations, /* [ states ] */
			&(gpu16GlobalDecisionStateMap[ uStage * guStates ]),	 /* [ stages, states ] */
			guStates );
	}

	/* sync */

#ifdef DEBUG_BACKWARDPASS
	fprintf( stdout, "\ngpfGlobalDecisionAllocations\n" );
	arr_float_print( stdout, gpfSolutionAllocations, guStates, gcOutputDelimiter );
	for( uStage = 0; uStage < guStages; uStage++ )
	{
		sprintf( gcPrintBuff, "%s%c", gpTurbs[ uStage ].szName, gcOutputDelimiter ); PRINT_STDOUT( gcPrintBuff );
		arr_float_print( stdout, &(gpfGlobalDecisionAllocations[ uStage * guStates ]), guStates, gcOutputDelimiter );
	}

	fprintf( stdout, "\ngpu16GlobalDecisionStateMap\n" );
	arr_float_print( stdout, gpfSolutionAllocations, guStates, gcOutputDelimiter );
	for( uStage = 0; uStage < guStages; uStage++ )
	{
		sprintf( gcPrintBuff, "%s%c", gpTurbs[ uStage ].szName, gcOutputDelimiter ); PRINT_STDOUT( gcPrintBuff );
		arr_uint16_print( stdout, &(gpu16GlobalDecisionStateMap[ uStage * guStates ]), guStates, gcOutputDelimiter );
	}

	fprintf( stdout, "\ngpfHKTableValues\n" );
	arr_float_print( stdout, gpfSolutionAllocations, guProblemStepCount, gcOutputDelimiter );
	for( uStage = 0; uStage < guStages; uStage++ )
	{
		sprintf( gcPrintBuff, "%s%c", gpTurbs[ uStage ].szName, gcOutputDelimiter ); PRINT_STDOUT( gcPrintBuff );
		arr_float_print( stdout, &(gpfHKTableValues[ uStage * guProblemStepCount ]), guProblemStepCount, gcOutputDelimiter );
	}
#endif

	/* tidy-up scenario's carry-forward memory - different than a complete clear!
	   otherwise the forwardpass gets all confused. */
	for( uStage = 0; uStage < guStages; uStage++ ) /* par */
	{
		arr_float_clear_conditional(
			&(gpfGlobalDecisionValues[ uStage * guStates ]),
			&(gpu16GlobalDecisionStateMap[ uStage * guStates ]),
			guStates
		);
		arr_uint8_clear_conditional(
			&(gpu8GlobalDecisionCounts[ uStage * guStates ]),
			&(gpu16GlobalDecisionStateMap[ uStage * guStates ]),
			guStates
		);
	}

	//////////////////////////////////////////
	// FORWARDPASS

#ifdef ENABLE_NONMAPPABLEELEMENTS
	// mark solution as 'none' before we trace forwards
	for( uStage = 0; uStage < guStages; uStage++ )
	{
		arr_uint16_set( &(gpu16Solution[ uStage * guStates ]), MAX_UINT16, guStates );
	}
#endif //ENABLE_NONMAPPABLEELEMENTS

	// establish the stage checklist
	for( uStage = 0; uStage < guStages; uStage++ ) /* par */
	{
		gpu8StageChecklist[ uStage ] = ( gpTurbs[ uStage ].fWeight < fTol ) ? MAX_UINT8 : uStage;
	}

	// for each state, select the best way to make the stage allocations
	for( uState = 0; uState < guStates; uState++ )
	{
		UINT32 uAllocationRemaining = MAX_UINT32;

		// reinit the checklist
		memcpy( gpu8StageChecklistCopy, gpu8StageChecklist, guStages * sizeof(UINT8) );

#ifdef DEBUG_FORWARDPASS
		sprintf( gcPrintBuff, "********* state %lu\n", uState);
		PRINT_STDOUT( gcPrintBuff );
#endif

		uAllocationRemaining = uState;
		while( uAllocationRemaining != 0 )
		{
			UINT16 u16PrioritizedStage = MAX_UINT16;
			arr_float_max_unmarked_prioritized_count(
					&u16PrioritizedStage,
					&(gpfGlobalDecisionValues[ 0 * guStates + uAllocationRemaining ]), /* [ stages, states ] */
					&(gpu8GlobalDecisionCounts[ 0 * guStates + uAllocationRemaining ]), /* [ stages, states ] */
					gpu8StageChecklistCopy, /* [ stages ] */
					gpu16StageMap, /* [ stages ] */
					guStages,
					guStates /* stride */ );
			if( u16PrioritizedStage == MAX_UINT16 )
			{

#ifdef DEBUG_FORWARDPASS
				sprintf( gcPrintBuff, "%lu left unallocated\n", uAllocationRemaining );
				PRINT_STDOUT( gcPrintBuff );
#endif

				break; // while
			}
			else
			{
				UINT16 uAllocatedStateForThisStage = MAX_UINT16;
				VERIFY( gpu8StageChecklistCopy[ u16PrioritizedStage ] != MAX_UINT8 ); // not in use
				if( ex_didFail() ) { goto cleanfailure; }

				gpu8StageChecklistCopy[ u16PrioritizedStage ] = MAX_UINT8; // don't use this again
				uAllocatedStateForThisStage = gpu16GlobalDecisionStateMap[ u16PrioritizedStage * guStates + uAllocationRemaining ];

				VERIFY( uAllocatedStateForThisStage < guProblemStepCount );
				if( ex_didFail() )
				{
#ifdef ENABLE_ADAPTIVEFAILURE
					uAllocatedStateForThisStage = guProblemStepCount - 1;
					ex_clear();
#else
					goto cleanfailure;
#endif
				}

#ifdef DEBUG_FORWARDPASS
				{
					FLOAT fMaxValue = gpfGlobalDecisionValues[ u16PrioritizedStage * guStates + uAllocationRemaining ];
					UINT8 u8Count = gpu8GlobalDecisionCounts[ u16PrioritizedStage * guStates + uAllocationRemaining ];
					sprintf( gcPrintBuff, "stage %lu: allocated %lu (%f)\n",
						u16PrioritizedStage,
						uAllocatedStateForThisStage,
						( u8Count == 0 ? 0.0 : fMaxValue / u8Count )
					);
					PRINT_STDOUT( gcPrintBuff );
				}
#endif

				gpu16Solution[ u16PrioritizedStage * guStates + uState ] = uAllocatedStateForThisStage;
				uAllocationRemaining -= uAllocatedStateForThisStage;

			}

#ifdef DEBUG_FORWARDPASS
			sprintf( gcPrintBuff, "leaving %lu\n", uAllocationRemaining );
			PRINT_STDOUT( gcPrintBuff );
#endif

		}
	}

	/* sync */

	// map the solution to the flow and power values
	{
		FLOAT* pPower = ( guSolveMode == DP_OPTIMIZE_FOR_POWER ) ? gpfSolution : gpfOtherSolution;
		FLOAT* pFlow = ( guSolveMode == DP_OPTIMIZE_FOR_POWER ) ? gpfOtherSolution : gpfSolution;
		assert( pFlow );
		assert( pPower );

		for( uStage = 0; uStage < guStages; uStage++ ) /* par */
		{
			arr_float_copy_indirect(
				&(pFlow[ uStage * guStates ]),							/* [ stages, states ] */
				&(gpfFlowAllocations[ uStage * guProblemStepCount ]),	/* [ stages, steps ] */
				&(gpu16Solution[ uStage * guStates ]),					/* [ stages, states ] */
				guStates );

			arr_float_copy_indirect(
				&(pPower[ uStage * guStates ]),							/* [ stages, states ] */
				&(gpfPowerAllocations[ uStage * guProblemStepCount ]),	/* [ stages, steps ] */
				&(gpu16Solution[ uStage * guStates ]),					/* [ stages, states ] */
				guStates );
		}
	}

	/* sync */

	// compute HK for the complete solution
	{
		FLOAT* pPower = ( guSolveMode == DP_OPTIMIZE_FOR_POWER ) ? gpfSolution : gpfOtherSolution;
		FLOAT* pFlow = ( guSolveMode == DP_OPTIMIZE_FOR_POWER ) ? gpfOtherSolution : gpfSolution;
		assert( pPower );
		assert( pFlow );

		for( uStage = 0; uStage < guStages; uStage++ ) /* par */
		{
			arr_float_div2(
				&(gpfHKSolution[ uStage * guStates ]),
				&(pPower[ uStage * guStates ]),
				&(pFlow[ uStage * guStates ]),
				guStates );
		}
	}

	/* sync */

	// summarise flow, power and average HK
	arr_float_sum_stages_pos( gpfSolutionAllocations, gpfSolution, guStates, guStages ); /* [ states ] */
	arr_float_sum_stages_pos( gpfOtherSolutionAllocations, gpfOtherSolution, guStates, guStages ); /* [ states ] */
	arr_float_average_stages_pos( gpfHKSolutionAverages, gpfHKSolution, guStates, guStages ); /* [ states ] */

	return;

cleanfailure:
	for( uStage = 0; uStage < guStages; uStage++ )
	{
		arr_uint16_set( &(gpu16Solution[ uStage * guStates ]), 0, guStates ); /* [ stages, states ] */
		arr_float_set( &(gpfHKSolution[ uStage * guStates ]), 0, guStates ); /* [ stages, states ] */
	}
	arr_float_set( gpfSolutionAllocations, 0, guStates ); /* [ states ] */
	arr_float_set( gpfOtherSolutionAllocations, 0, guStates ); /* [ states ] */
}

void dp_print_solution()
{
	FLOAT* pSolution = 		( guSolveMode == DP_OPTIMIZE_FOR_POWER ) ? gpfOtherSolution : gpfSolution;
	FLOAT* pOtherSolution =	( guSolveMode == DP_OPTIMIZE_FOR_POWER ) ? gpfSolution : gpfOtherSolution;
	CHAR* szDecisionVariable = ( guSolveMode == DP_OPTIMIZE_FOR_POWER ) ? "Power" : "Flow";
	CHAR* szOtherDecisionVariable = ( guSolveMode == DP_OPTIMIZE_FOR_POWER ) ? "Flow" : "Power";
	UINT32 uStage, uState;

	if( !guSolveMode )
	{
		sprintf( gcPrintBuff, "%s:%d (" VERSIONED_NAME  ") Unspecified solvemode.\n", __FILE__, __LINE__ );
		PRINT_STDERR( gcPrintBuff );
		dp_cleanup_fatal();
		return;
	}

	if( guTransposeSolution == 0 ) {
		sprintf( gcPrintBuff, "\n%s%c", "KW/CFS", gcOutputDelimiter ); PRINT_STDOUT( gcPrintBuff );
		arr_float_print( stdout, gpfHKSolutionAverages, guStates, gcOutputDelimiter );
		sprintf( gcPrintBuff, "%s%c", szOtherDecisionVariable, gcOutputDelimiter ); PRINT_STDOUT( gcPrintBuff );
		arr_float_print( stdout, gpfOtherSolutionAllocations, guStates, gcOutputDelimiter );
		sprintf( gcPrintBuff, "%s%c", szDecisionVariable, gcOutputDelimiter ); PRINT_STDOUT( gcPrintBuff );
		arr_float_print( stdout, gpfSolutionAllocations, guStates, gcOutputDelimiter );
		for( uStage = 0; uStage < guStages; uStage++ )
		{
			sprintf( gcPrintBuff, "%s%c", gpTurbs[ uStage ].szName, gcOutputDelimiter ); PRINT_STDOUT( gcPrintBuff );
			arr_float_print( stdout, &(gpfSolution[ uStage * guStates ]), guStates, gcOutputDelimiter );
		}

		if( guDebugMode )
		{
			sprintf( gcPrintBuff, "\n%s%c", szOtherDecisionVariable, gcOutputDelimiter ); PRINT_STDOUT( gcPrintBuff );
			arr_float_print( stdout, gpfOtherSolutionAllocations, guStates, gcOutputDelimiter );
			for( uStage = 0; uStage < guStages; uStage++ )
			{
				sprintf( gcPrintBuff, "%s%c", gpTurbs[ uStage ].szName, gcOutputDelimiter ); PRINT_STDOUT( gcPrintBuff );
				arr_float_print( stdout, &(gpfOtherSolution[ uStage * guStates ]), guStates, gcOutputDelimiter );
			}

			sprintf( gcPrintBuff, "\n%s%c", "KW/CFS", gcOutputDelimiter ); PRINT_STDOUT( gcPrintBuff );
			arr_float_print( stdout, gpfHKSolutionAverages, guStates, gcOutputDelimiter );
			for( uStage = 0; uStage < guStages; uStage++ )
			{
				sprintf( gcPrintBuff, "%s%c", gpTurbs[ uStage ].szName, gcOutputDelimiter ); PRINT_STDOUT( gcPrintBuff );
				arr_float_print( stdout, &(gpfHKSolution[ uStage * guStates ]), guStates, gcOutputDelimiter );
			}

			sprintf( gcPrintBuff, "\n%s%c", "DecisionValues", gcOutputDelimiter ); PRINT_STDOUT( gcPrintBuff );
			arr_float_print( stdout, gpfSolutionAllocations, guStates, gcOutputDelimiter );
			for( uStage = 0; uStage < guStages; uStage++ )
			{
				sprintf( gcPrintBuff, "%s%c", gpTurbs[ uStage ].szName, gcOutputDelimiter ); PRINT_STDOUT( gcPrintBuff );
				arr_float_print( stdout, &(gpfGlobalDecisionValues[ uStage * guStates ]), guStates, gcOutputDelimiter );
			}

			sprintf( gcPrintBuff, "\n%s%c", "DecisionCounts", gcOutputDelimiter ); PRINT_STDOUT( gcPrintBuff );
			arr_float_print( stdout, gpfSolutionAllocations, guStates, gcOutputDelimiter );
			for( uStage = 0; uStage < guStages; uStage++ )
			{
				sprintf( gcPrintBuff, "%s%c", gpTurbs[ uStage ].szName, gcOutputDelimiter ); PRINT_STDOUT( gcPrintBuff );
				arr_uint8_print( stdout, &(gpu8GlobalDecisionCounts[ uStage * guStates ]), guStates, gcOutputDelimiter );
			}

			sprintf( gcPrintBuff, "\n%s%c", "DecisionAllocations", gcOutputDelimiter ); PRINT_STDOUT( gcPrintBuff );
			arr_float_print( stdout, gpfSolutionAllocations, guStates, gcOutputDelimiter );
			for( uStage = 0; uStage < guStages; uStage++ )
			{
				sprintf( gcPrintBuff, "%s%c", gpTurbs[ uStage ].szName, gcOutputDelimiter ); PRINT_STDOUT( gcPrintBuff );
				arr_float_print( stdout, &(gpfGlobalDecisionAllocations[ uStage * guStates ]), guStates, gcOutputDelimiter );
			}

			sprintf( gcPrintBuff, "\n%s%c", "DecisionAllocationStates", gcOutputDelimiter ); PRINT_STDOUT( gcPrintBuff );
			arr_float_print( stdout, gpfSolutionAllocations, guStates, gcOutputDelimiter );
			for( uStage = 0; uStage < guStages; uStage++ )
			{
				sprintf( gcPrintBuff, "%s%c", gpTurbs[ uStage ].szName, gcOutputDelimiter ); PRINT_STDOUT( gcPrintBuff );
				arr_uint16_print( stdout, &(gpu16GlobalDecisionStateMap[ uStage * guStates ]), guStates, gcOutputDelimiter );
			}

			sprintf( gcPrintBuff, "\nHKTableBy%s(Weighted)%c", szDecisionVariable, gcOutputDelimiter ); PRINT_STDOUT( gcPrintBuff );
			arr_float_print( stdout, gpfSolutionAllocations, guProblemStepCount, gcOutputDelimiter );
			sprintf( gcPrintBuff, "\nHKTableBy%s(Weighted)%c", szOtherDecisionVariable, gcOutputDelimiter ); PRINT_STDOUT( gcPrintBuff );
			arr_float_print( stdout, gpfOtherSolutionAllocations, guStates, gcOutputDelimiter );
			for( uStage = 0; uStage < guStages; uStage++ )
			{
				sprintf( gcPrintBuff, "%s%c", gpTurbs[ uStage ].szName, gcOutputDelimiter ); PRINT_STDOUT( gcPrintBuff );
				arr_float_print( stdout, &(gpfHKTableValues[ uStage * guProblemStepCount ]), guProblemStepCount, gcOutputDelimiter );
			}
		}

	} else {
		fprintf( stdout, "%s%c", "KW/CFS", gcOutputDelimiter );
		fprintf( stdout, "%s%c", szOtherDecisionVariable, gcOutputDelimiter );
		fprintf( stdout, "%s%c", szDecisionVariable, gcOutputDelimiter );
		for( uStage = 0; uStage < guStages; uStage++ ) { fprintf( stdout, "%s%c", gpTurbs[ uStage ].szName, gcOutputDelimiter ); }

		if( guDebugMode )
		{
			fprintf( stdout, "%s%c", szOtherDecisionVariable, gcOutputDelimiter );
			for( uStage = 0; uStage < guStages; uStage++ ) { fprintf( stdout, "%s%c", gpTurbs[ uStage ].szName, gcOutputDelimiter ); }

			fprintf( stdout, "%s%c", "KW/CFS", gcOutputDelimiter );
			for( uStage = 0; uStage < guStages; uStage++ ) { fprintf( stdout, "%s%c", gpTurbs[ uStage ].szName, gcOutputDelimiter ); }

			fprintf( stdout, "%s%c", "DecisionValues", gcOutputDelimiter );
			for( uStage = 0; uStage < guStages; uStage++ ) { fprintf( stdout, "%s%c", gpTurbs[ uStage ].szName, gcOutputDelimiter ); }

			fprintf( stdout, "%s%c", "DecisionCounts", gcOutputDelimiter );
			for( uStage = 0; uStage < guStages; uStage++ ) { fprintf( stdout, "%s%c", gpTurbs[ uStage ].szName, gcOutputDelimiter ); }

			fprintf( stdout, "%s%c", "DecisionAllocations", gcOutputDelimiter );
			for( uStage = 0; uStage < guStages; uStage++ ) { fprintf( stdout, "%s%c", gpTurbs[ uStage ].szName, gcOutputDelimiter ); }

			fprintf( stdout, "%s%c", "DecisionAllocationStates", gcOutputDelimiter );
			for( uStage = 0; uStage < guStages; uStage++ ) { fprintf( stdout, "%s%c", gpTurbs[ uStage ].szName, gcOutputDelimiter ); }

			fprintf( stdout, "HKTableBy%s(Weighted)%c", szDecisionVariable, gcOutputDelimiter );
			fprintf( stdout, "HKTableBy%s(Weighted)%c", szOtherDecisionVariable, gcOutputDelimiter );
			for( uStage = 0; uStage < guStages; uStage++ ) { fprintf( stdout, "%s%c", gpTurbs[ uStage ].szName, gcOutputDelimiter ); }
		}

		putc( '\n', stdout );

		for( uState = 0; uState < guStates; uState++ )
		{
			float_print( stdout, gpfHKSolutionAverages[ uState ], gcOutputDelimiter );
			float_print( stdout, gpfOtherSolutionAllocations[ uState ], gcOutputDelimiter );
			float_print( stdout, gpfSolutionAllocations[ uState ], gcOutputDelimiter );
			arr_float_print_t( stdout, &(gpfSolution[ uState ]), guStages, guStates, gcOutputDelimiter );

			if( guDebugMode )
			{
				float_print( stdout, gpfOtherSolutionAllocations[ uState ], gcOutputDelimiter );
				arr_float_print_t( stdout, &(gpfOtherSolution[ uState ]), guStages, guStates, gcOutputDelimiter );

				float_print( stdout, gpfHKSolutionAverages[ uState ], gcOutputDelimiter );
				arr_float_print_t( stdout, &(gpfHKSolution[ uState ]), guStages, guStates, gcOutputDelimiter );

				float_print( stdout, gpfSolutionAllocations[ uState ], gcOutputDelimiter );
				arr_float_print_t( stdout, &(gpfGlobalDecisionValues[ uState ]), guStages, guStates, gcOutputDelimiter );

				float_print( stdout, gpfSolutionAllocations[ uState ], gcOutputDelimiter );
				arr_uint8_print_t( stdout, &(gpu8GlobalDecisionCounts[ uState ]), guStages, guStates, gcOutputDelimiter );

				float_print( stdout, gpfSolutionAllocations[ uState ], gcOutputDelimiter );
				arr_float_print_t( stdout, &(gpfGlobalDecisionAllocations[ uState ]), guStages, guStates, gcOutputDelimiter );

				float_print( stdout, gpfSolutionAllocations[ uState ], gcOutputDelimiter );
				arr_uint16_print_t( stdout, &(gpu16GlobalDecisionStateMap[ uState ]), guStages, guStates, gcOutputDelimiter );

				if( uState < guProblemStepCount ) {
					float_print( stdout, gpfSolutionAllocations[ uState ], gcOutputDelimiter );
					float_print( stdout, gpfOtherSolutionAllocations[ uState ], gcOutputDelimiter );
					arr_float_print_t( stdout, &(gpfHKTableValues[ uState ]), guStages, guProblemStepCount, gcOutputDelimiter );
				} else {
					UINT32 i = MAX_UINT32;
					putc( gcOutputDelimiter, stdout ); /* skip column */
					putc( gcOutputDelimiter, stdout ); /* skip column */
					for( i = 0; i < guStages; i++ ) { putc( gcOutputDelimiter, stdout ); }
				}
			}

			putc( '\n', stdout );
		}
	}
}

/**********************************************/

EXTERNC UINT32 guOPoints = 0;
UINT32 guOPRegressions = 0; /* mod 2 */

FLOAT gfOPDependent[2] = {0,0}; /* [ 2 ] */
FLOAT* gpfOPCapacity = 0; /* [ opoints ] */
FLOAT* gpfOPValue = 0; /* [ opoints, 2 ] */
EXTERNC FLOAT* gpfOPCoefM = 0; /* [ opoints ] */
EXTERNC FLOAT* gpfOPCoefB = 0; /* [ opoints ] */

/**********************************************/

void op_malloc()
{
	VERIFY( guOPoints > 0 );
	if( ex_didFail() )
	{
		sprintf( gcPrintBuff, "can't determine operating points - none defined.\n" );
		PRINT_STDOUT( gcPrintBuff );
		return;
	}

	DP_MALLOC_CACHE( gpfOPCapacity, FLOAT, guOPoints, __FILE__, __LINE__ ); /* [ opoints ] */
	DP_MALLOC_CACHE( gpfOPValue, FLOAT, guOPoints * 2, __FILE__, __LINE__ ); /* [ opoints, 2 ] */
	DP_MALLOC_CACHE( gpfOPCoefM, FLOAT, guOPoints, __FILE__, __LINE__ ); /* [ opoints ] */
	DP_MALLOC_CACHE( gpfOPCoefB, FLOAT, guOPoints, __FILE__, __LINE__ ); /* [ opoints ] */
}

EXTERNC void op_cleanup()
{
	DP_FREE_CACHE( gpfOPCapacity, FLOAT );
	DP_FREE_CACHE( gpfOPValue, FLOAT );
	DP_FREE_CACHE( gpfOPCoefM, FLOAT );
	DP_FREE_CACHE( gpfOPCoefB, FLOAT );
}

EXTERNC void op_set_operating_capacities( UINT32 u, FLOAT* pCap )
{
	guOPRegressions = 0;
	guOPoints = u;

	op_malloc();
	arr_float_copy( gpfOPCapacity, pCap, u );
}

EXTERNC void op_set_dependent( FLOAT fDep )
{
	UINT32 uCurrRegression = guOPRegressions & 0x01;
	gfOPDependent[ uCurrRegression ] = fDep;
}

void op_print()
{
	UINT32 uOPoint;
	CHAR sCharDelim[2] = {0,0};
	sCharDelim[0] = gcOutputDelimiter;

	sprintf( gcPrintBuff, "operating points\ncapacity%cslope%cintercept\n", gcOutputDelimiter, gcOutputDelimiter );
	PRINT_STDOUT( gcPrintBuff );
	for( uOPoint = 0; uOPoint < guOPoints; uOPoint++ )
	{
		sprintf( gcPrintBuff, float_format( gpfOPCapacity[ uOPoint ] ), gpfOPCapacity[ uOPoint ] );
		strcat( gcPrintBuff, sCharDelim );
		PRINT_STDOUT( gcPrintBuff );
		sprintf( gcPrintBuff, float_format( gpfOPCoefM[ uOPoint ] ), gpfOPCoefM[ uOPoint ] );
		strcat( gcPrintBuff, sCharDelim );
		PRINT_STDOUT( gcPrintBuff );
		sprintf( gcPrintBuff, float_format( gpfOPCoefB[ uOPoint ] ), gpfOPCoefB[ uOPoint ] );
		strcat( gcPrintBuff, sCharDelim );
		PRINT_STDOUT( gcPrintBuff );
		putc( '\n', stdout );
	}
}

/* call this to sample H/K for defined operating points and compute a
 * linear fit from consecutive optimization configurations. */
EXTERNC void op_regress()
{
	UINT32 uState, uOPoint;
	UINT32 uCurrRegression = guOPRegressions & 0x01;
	FLOAT fTotDecision;

	VERIFY( guOPoints > 0 );
	if( ex_didFail() )
	{
		sprintf( gcPrintBuff, "can't determine operating points - none defined.\n" );
		PRINT_STDOUT( gcPrintBuff );
		return;
	}

	fTotDecision = (guSolveMode == DP_OPTIMIZE_FOR_POWER) ? gfTotalPower[0] : gfTotalFlow[0];

	/* collect HK values */
	for( uOPoint = 0; uOPoint < guOPoints; uOPoint++ )
	{
		gpfOPValue[ uCurrRegression * guOPoints + uOPoint ] = 0;

		for( uState = 1; uState < guStates; uState++ )
		{
			if( gpfSolutionAllocations[ uState ] > gpfOPCapacity[ uOPoint ] * fTotDecision ) { break; }
			/* due to rounding of problem domain we might end up with extra output. so we will extrapolate. */
			if( gpfSolutionAllocations[ uState ] < gpfSolutionAllocations[ uState - 1 ] ) { uState--; break; }
		}

		/* catch run-off from table and change state so we can extrapolate */
		if( uState == guStates ) { uState--; }

		{
			/* interpolate HK for operating point of interest */
			/* also works to extrapolate */
			FLOAT fSOLBase = gpfSolutionAllocations[ uState - 1 ];
			FLOAT fCAPDelta = gpfOPCapacity[ uOPoint ] * fTotDecision - fSOLBase;
			FLOAT fSOLDelta = gpfSolutionAllocations[ uState ] - fSOLBase;
			FLOAT fHKBase = gpfHKSolutionAverages[ uState - 1 ];
			FLOAT fHKDelta = gpfHKSolutionAverages[ uState ] - fHKBase;
			/* only last 2 samples used */
			if( fSOLDelta < 1 ) {
				gpfOPValue[ uCurrRegression * guOPoints + uOPoint ] = fHKBase;
			} else {
				gpfOPValue[ uCurrRegression * guOPoints + uOPoint ] = fHKBase + fHKDelta * (fCAPDelta / fSOLDelta);
			}
		}

		{
			FLOAT fTol = 1e-6;
			FLOAT fB = 0;
			FLOAT fM = 0;
			if( uState >= guStates ) {
				UINT32 uSolutionUnits = ( guSolveMode == DP_OPTIMIZE_FOR_POWER ) ? DP_UNIT_POWER : DP_UNIT_FLOW;
				sprintf( gcPrintBuff, "regress: operating point %f %s can't be found in solution.\n", gpfOPCapacity[ uOPoint ], char_units( uSolutionUnits ) );
				PRINT_STDOUT( gcPrintBuff );
			} else {
				if( guOPRegressions == 0 ) {
					fB = gpfOPValue[ 0 * guOPoints + uOPoint ];
				} else {
					/* interpolate from earlier (A) to most recent point (B) */
					UINT32 uA = ( uCurrRegression == 1 ) ? 0 : 1;
					UINT32 uB = (uA == 1) ? 0 : 1;
					FLOAT fValA = gpfOPValue[ uA * guOPoints + uOPoint ];
					FLOAT fValB = gpfOPValue[ uB * guOPoints + uOPoint ];
					FLOAT fValDelta = fValB - fValA; /* from A to B */
					FLOAT fDepA = gfOPDependent[ uA ];
					FLOAT fDepB = gfOPDependent[ uB ];
					FLOAT fDepDelta = fDepB - fDepA; /* from A to B */
					/* fit line between values for each specified capacity */
					FLOAT fTol = 1E-6;
					fM = ( fabs( fValDelta ) <= fTol || fabs( fDepDelta ) <= fTol ) ? 0.0 : fValDelta / fDepDelta;
					fB = fValB - fM * fDepB; /* y = m x + b, solve for b */
				}
			}
			gpfOPCoefB[ uOPoint ] = fB;
			gpfOPCoefM[ uOPoint ] = fM;
		}
	}

	guOPRegressions++;
}

/**********************************************/

EXTERNC FLOAT gfDispatch = 0;
EXTERNC FLOAT* gpfUDSolution = 0; /* [ stages ] */
EXTERNC FLOAT* gpfUDOtherSolution = 0; /* [ stages ] */
EXTERNC FLOAT* gpfUDHKSolution = 0; /* [ stages ] */

/************/

EXTERNC void ud_malloc()
{
	VERIFY( guStages > 0 );
	if( ex_didFail() )
	{
		sprintf( gcPrintBuff, "unit dispatch: no stages defined.\n" );
		PRINT_STDOUT( gcPrintBuff );
		return;
	}

	DP_MALLOC_CACHE( gpfUDSolution, FLOAT, guStages, __FILE__, __LINE__ ); /* [ stages ] */
	DP_MALLOC_CACHE( gpfUDOtherSolution, FLOAT, guStages, __FILE__, __LINE__ ); /* [ stages ] */
	DP_MALLOC_CACHE( gpfUDHKSolution, FLOAT, guStages, __FILE__, __LINE__ ); /* [ stages ] */
}

EXTERNC void ud_cleanup()
{
	DP_FREE_CACHE( gpfUDSolution, FLOAT );
	DP_FREE_CACHE( gpfUDOtherSolution, FLOAT );
	DP_FREE_CACHE( gpfUDHKSolution, FLOAT );
}

/* ud_dispatch make the linearity assumption that if we don't find an exact solution we can
   interpolate between ajacent solution IFF ajacent solution have the same units running. */
EXTERNC void ud_dispatch()
{
	UINT32 uUsableStates = 0;
	UINT32 uLowerState = 0;
	UINT32 uHigherState = 0;
	FLOAT fInterpCoef = 1.0;
	FLOAT fTol = 1E-6;

	VERIFY( guStages > 0 );
	if( ex_didFail() )
	{
		sprintf( gcPrintBuff, "unit dispatch: no stages defined.\n" );
		PRINT_STDOUT( gcPrintBuff );
		return;
	}

	assert( gpfUDSolution );
	assert( gpfUDOtherSolution );
	assert( gpfUDHKSolution );

	/* clear */
	{
		UINT32 u;
		for( u = 0; u < guStages; u++ )
		{ gpfUDSolution[u] = gpfUDOtherSolution[u] = gpfUDHKSolution[u] = 0; }
	}

	if( gfDispatch < fTol )
	{
		return;
	}

	VERIFY( gfDispatch > 0 );
	if( ex_didFail() )
	{
		sprintf( gcPrintBuff, "unit dispatch: unable to dispatch negative quantity.\n" );
		PRINT_STDOUT( gcPrintBuff );
		return;
	}

	/* accomodate problematic table construction... */
	{
		/* determine table size as it may be smaller than expected... */
		uUsableStates = guStates;
		while( uUsableStates != MAX_UINT32 && gpfSolutionAllocations[ uUsableStates ] < 1.0 ) { uUsableStates--; }
		VERIFY( uUsableStates != MAX_UINT32 );
		if( ex_didFail() )
		{
			sprintf( gcPrintBuff, "unit dispatch: no solution available.\n" );
			PRINT_STDOUT( gcPrintBuff );
			return;
		}

		/* determine location of the dispatch value via search because the table may not be of uniform stepsize ... */
		{
			UINT32 uSearchA = 0;
			UINT32 uSearchB = uUsableStates - 1;
			UINT32 u;
			while( 1 )
			{
				u = ( uSearchA + uSearchB ) / 2;
				if( gfDispatch < gpfSolutionAllocations[ u ] ) { uSearchB = u - 1; }
				else if( gfDispatch > gpfSolutionAllocations[ u ] ) { uSearchA = u + 1; }
				else { break; } /* found it exactly */
				if( uSearchA >= uSearchB ) { u = uSearchB; break; }
			}
			uLowerState = u;
		}

		/* apply off-by-one correction to the search */
		{
			if( gfDispatch < gpfSolutionAllocations[ uLowerState ] && uLowerState > 0 ) uLowerState--;
			else if( gfDispatch > gpfSolutionAllocations[ uLowerState + 1 ] && uLowerState+1 < guStates-1 ) uLowerState++;

			uHigherState = uLowerState + 1;
		}
	}
	assert( uLowerState < guStates );

	/* are we not close enough? */
	if( fabs( gfDispatch - gpfSolutionAllocations[ uLowerState ] ) > fTol )
	{
		UINT32 bLinearityAssuptionValid = 0;

		VERIFY( uLowerState != MAX_UINT32 );
		if( ex_didFail() ) { return; }

		/* are we out past the end of the table? */
		if( uHigherState >= uUsableStates )
		{
			/* make linearity assumption so we can extrapolate */
			bLinearityAssuptionValid = 1;
		}
		else
		{
			/* if units neither start or stop across the discretization then we can make linearity assumption */
			UINT32 uNumSameStages = 0;
			UINT32 u;
			for( u = 0; u < guStages; u++ )
			{
				UINT32 uLower = u * guStates + uLowerState; // column major
				UINT32 uHigher = u * guStates + uHigherState; // column major
				int bBothNonZero = ( gpfSolution[ uLower ] > fTol && gpfSolution[ uHigher ] > fTol ) ? 1 : 0;
				int bBothZero = ( gpfSolution[ uLower ] < fTol && gpfSolution[ uHigher ] < fTol ) ? 1 : 0;
				uNumSameStages += ( bBothNonZero || bBothZero ) ? 1 : 0;
			}
			bLinearityAssuptionValid = ( uNumSameStages == guStages );
		}

		if( bLinearityAssuptionValid )
		{
			/* calc coef */
			fInterpCoef = gfDispatch / gpfSolutionAllocations[ uLowerState ];
		}
		else
		{
			FLOAT fLowerDiff = gfDispatch - gpfSolutionAllocations[ uLowerState ];
			FLOAT fHigherDiff = gpfSolutionAllocations[ uHigherState ] - gfDispatch;

			/* check neighbours */
			VERIFY( fLowerDiff > 0 && fHigherDiff > 0 );
			if( ex_didFail() ) { return; }

			/* set uLowerState to the closer soln */
			uLowerState = ( fLowerDiff < fHigherDiff ) ? uLowerState : uHigherState;
		}
	}
	assert( uLowerState < guStates );

	{
		UINT32 u;
		for( u = 0; u < guStages; u++ )
		{
			gpfUDSolution[ u ] = fInterpCoef * gpfSolution[ u * guStates + uLowerState ];
			gpfUDOtherSolution[ u ] = fInterpCoef * gpfOtherSolution[ u * guStates + uLowerState ];
		}
	}

	{
		FLOAT* pPower = ( guSolveMode == DP_OPTIMIZE_FOR_POWER ) ? gpfUDSolution : gpfUDOtherSolution;
		FLOAT* pFlow = ( guSolveMode == DP_OPTIMIZE_FOR_POWER ) ? gpfUDOtherSolution : gpfUDSolution;

		arr_float_div2( gpfUDHKSolution, pPower, pFlow, guStages );

#ifdef ENABLE_OUTOFLINE_NUMERICAL_CLEANUP
		arr_float_mark_nan_as_zero( gpfUDHKSolution, guStages );
		arr_float_mark_inf_as_zero( gpfUDHKSolution, guStages );
#endif
	}
}

void ud_print()
{
	CHAR* szDecisionVariable = ( guSolveMode == DP_OPTIMIZE_FOR_POWER ) ? "Power" : "Flow";
	CHAR* szOtherDecisionVariable = ( guSolveMode == DP_OPTIMIZE_FOR_POWER ) ? "Flow" : "Power";

	ex_clear();

	VERIFY( guStages > 0 );
	if( ex_didFail() )
	{
		sprintf( gcPrintBuff, "unit dispatch: no stages defined.\n" );
		PRINT_STDOUT( gcPrintBuff );
		return;
	}

	assert( gpfUDSolution );
	assert( gpfUDOtherSolution );
	assert( gpfUDHKSolution );

	if( guTransposeSolution == 0 ) {
		sprintf( gcPrintBuff, "%8s:%c", szDecisionVariable, gcOutputDelimiter ); PRINT_STDOUT( gcPrintBuff );
		arr_float_print( stdout, gpfUDSolution, guStages, gcOutputDelimiter );
		sprintf( gcPrintBuff, "%8s:%c", szOtherDecisionVariable, gcOutputDelimiter ); PRINT_STDOUT( gcPrintBuff );
		arr_float_print( stdout, gpfUDOtherSolution, guStages, gcOutputDelimiter );
		sprintf( gcPrintBuff, "%8s:%c", "H/K", gcOutputDelimiter ); PRINT_STDOUT( gcPrintBuff );
		arr_float_print( stdout, gpfUDHKSolution, guStages, gcOutputDelimiter );
		if( guDebugMode )
		{
		}
		putc( '\n', stdout );
	} else {
		FLOAT fSum1, fSum2;
		arr_float_sum( &fSum1, gpfUDSolution, guStages, 1 );
		float_print( stdout, fSum1, gcOutputDelimiter );
		arr_float_print_t2( stdout, gpfUDSolution, guStages, 1, gcOutputDelimiter, 0 );
		/**/
		arr_float_sum( &fSum2, gpfUDOtherSolution, guStages, 1 );
		float_print( stdout, fSum2, gcOutputDelimiter );
		arr_float_print_t2( stdout, gpfUDOtherSolution, guStages, 1, gcOutputDelimiter, 0 );
		/**/
		float_print( stdout, ( guSolveMode == DP_OPTIMIZE_FOR_POWER ) ? fSum1 / fSum2 : fSum2 / fSum1, gcOutputDelimiter );
		arr_float_print_t2( stdout, gpfUDHKSolution, guStages, 1, gcOutputDelimiter, 0 );
		if( guDebugMode )
		{
		}
		putc( '\n', stdout );
	}
}

/**********************************************/

void ui_parse_units( CHAR* szUnit )
{
	CHAR** pszUnit = 0;
	UINT32 uUnitMode = DP_UNITS_UNDEFINED;

	// reset measurement units
	if( !szUnit ) { guUnitMode = gfConvFactor = 0; return; }

	for( pszUnit = (CHAR**)&gsUniversalUnits; *pszUnit; pszUnit++ )
	{ if( strcasecmp( *pszUnit, szUnit ) == 0 ) { guMWMode = (pszUnit == &(gsUniversalUnits[1])) ? 1 : 0; return; } }

	for( pszUnit = (CHAR**)&gsImperialUnits; *pszUnit; pszUnit++ )
	{ if( strcasecmp( *pszUnit, szUnit ) == 0 ) { uUnitMode = DP_UNITS_IMPERIAL; gfConvFactor = DP_CONV_IMPERIAL; break; } }

	for( pszUnit = (CHAR**)&gsMetricUnits; *pszUnit; pszUnit++ )
	{ if( strcasecmp( *pszUnit, szUnit ) == 0 ) { uUnitMode = DP_UNITS_METRIC; gfConvFactor = DP_CONV_METRIC; break; } }

	if( guUnitMode == DP_UNITS_UNDEFINED ) { guUnitMode = uUnitMode; return; }

	if( guUnitMode != uUnitMode )
	{
		sprintf( gcPrintBuff, "%s:%d (" VERSIONED_NAME  ") Can't mix and match unit modes.\n", __FILE__, __LINE__ );
		PRINT_STDERR( gcPrintBuff );
		dp_cleanup_fatal();
	}
}

/**********************************************/

PVOID* gpTrackedBlocks = 0; /* array of pointers to malloc'd memory */
UINT32 guTrackedBlocks = 0;
#define DP_TRACKPOOLSIZE 20

#if 0 // delete when tested with linux build
#define parse_addblock( _p )\
	do{ \
		PVOID pVoid = realloc( (PVOID)gpTrackedBlocks, ( ++guTrackedBlocks ) * sizeof(PVOID) ); \
		if( pVoid == 0 ) { sprintf( gcPrintBuff, "%s:%d (" VERSIONED_NAME  ") realloc failed\n", __FILE__, __LINE__ ); exit(-1); } \
		gpTrackedBlocks = (PVOID*)pVoid; \
		gpTrackedBlocks[ guTrackedBlocks - 1 ] = _p; \
	}while(0)
#define parse_freeallblocks() \
	do{ \
		while( guTrackedBlocks > 0 ) { free( gpTrackedBlocks[ guTrackedBlocks-- ] ); }\
		if( gpTrackedBlocks ) free( gpTrackedBlocks ); \
		gpTrackedBlocks = 0; \
	}while(0)
#else
void parse_addblock( void* _p )
{
	++guTrackedBlocks;
	if( (guTrackedBlocks - 1) % (DP_TRACKPOOLSIZE - 1) == 0 )
	{
#ifdef DEBUG_MEMORY
		PRINT_STDERR( "memory: reallocation of tracking block\n" );
#endif //DEBUG_MEMORY
		PVOID pVoid = realloc( (PVOID)gpTrackedBlocks, ( guTrackedBlocks + DP_TRACKPOOLSIZE ) * sizeof(PVOID) );
		if( pVoid == 0 )
		{
			sprintf( gcPrintBuff, "%s:%d (" VERSIONED_NAME  ") realloc failed\n", __FILE__, __LINE__ );
			PRINT_STDERR( gcPrintBuff );
			exit(-1);
		}
		gpTrackedBlocks = (PVOID*)pVoid;
	}
	gpTrackedBlocks[ guTrackedBlocks - 1 ] = _p;
#ifdef DEBUG_MEMORY
	sprintf( gcPrintBuff, "memory: tracking block at %X\n", _p );
	PRINT_STDERR( gcPrintBuff );
#endif //DEBUG_MEMORY
}
void parse_freeallblocks()
{
	while( guTrackedBlocks > 0 )
	{
		--guTrackedBlocks;
#ifdef DEBUG_MEMORY
		sprintf( gcPrintBuff, "memory: freeing block at %X\n", gpTrackedBlocks[ guTrackedBlocks ] );
		PRINT_STDERR( gcPrintBuff );
#endif //DEBUG_MEMORY
		assert( gpTrackedBlocks[ guTrackedBlocks ] );
		free( gpTrackedBlocks[ guTrackedBlocks ] );
		gpTrackedBlocks[ guTrackedBlocks ] = 0;
	}
	if( gpTrackedBlocks )
	{
#ifdef DEBUG_MEMORY
		PRINT_STDERR( "memory: freeing tracking block\n" );
#endif //DEBUG_MEMORY
		free( gpTrackedBlocks );
	}
	gpTrackedBlocks = 0;
}
#endif

#ifndef _WIN32DLL

/********************/

CHAR* gszCommandHelp = "\
# This help\n\
> help\n\
# Quit program\n\
> end\n\
# Create a custom efficiency curve\n\
> curve <curvename> flo <n floats in ascending order>\n\
> curve <curvename> pow <n floats in ascending order>\n\
> curve <curvename> eff <n floats>\n\
# Create a unit from a reference efficiency curve\n\
> unit <unitname> ( Crossflow | FixedPropeller | Francis | Kaplan | Pelton | Turgo ) <design head> ft <design flow> cfs <generator capacity> kw\n\
# Create a unit from a custom efficiency curve\n\
> unit <unitname> <curvename> <design head> ft <design flow> cfs <design kw> kw capacity <float> [ weight <float> ] [ headloss <float> ] [ geneff <float> ]\n\
# Specify current head for optimization\n\
> head <current head> ft\n\
# Specify discretization of efficiency curve for optimization\n\
> unitsteps <integer>\n\
# Optimize for either power or flow\n\
> solve ( Power | Flow )\n\
# Print configuration or final solution\n\
> print ( Solution | Config | Units | Curves | Weights | Weighting ) \n\
# Transpose solution printout\n\
> transpose ( 0 | 1 ) \n\
# Change solution printout delimiter (use , to make *.csv import easier. Use with no arg to revert to blank.)\n\
> delimiter <anychar> \n\
# specify a set of operating points to determine the H/K of \n\
> op caps <n floats [0...1] in any order> \n\
# set the dependent variable from which a regression for the operating points will be determined \n\
> op dep <float> ft \n\
# perform a regression on the solution to determine the equations for each of the specified operating points \n\
> dp regress \n\
# print the regression equations for the set of operating points \n\
> dp print \n\
";

/* test symbols and test register */
UINT32 guSymbolCount = 0;
CHAR* gpSymbolList[ 20 ]; /* somewhat arbitrary sized array */
UINT32 guTestCondition = 0;

/* szProblem must be writable memory */
void parse_program( CHAR* szProblem )
{
	/* preparse problem file */
	CHAR* p;
	for( p = szProblem; *p; p++ )
	{
		if( *p == '#' ) { do { *p = ' '; p++; } while( *p != '\n' ); } /* remove 1-line comments */
		if( *p == '\n' ) { *p = ' '; } /* remove \n */
		if( *p == '\t' ) { *p = ' '; } /* remove \t */
	}

	/* now parse newline and comment free file */
 {
	FLOAT* pFloat = 0;
	FLOAT fCapVector[100], fPowVector[100], fEffVector[100];
	CHAR* tok = strtok( szProblem, " " );
	while( tok )
	{
		/******************************************/
		if( strcasecmp( tok, "curve" ) == 0 )
		{
			/* TODO: handle misordered curve parameter definition */
			/* curve c1 flo 0 .175 .22 .25 .285 .325 .375 .435 .485 .525 .60 .68 .75 .82 .91 1.00 1.08 */
			/* curve c1 pow 0 .175 .22 .25 .285 .325 .375 .435 .485 .525 .60 .68 .75 .82 .91 1.00 1.08 */
			/* curve c1 eff 0 .65 .70 .725 .75 .775 .80 .825 .84 .85 .86 .865 .8675 .8675 .865 .858 .85 */
			UINT32 u = 0;
			CHAR* szCurveName = strtok( 0, " " );
			tok = strtok( 0, " " );
			if( strcasecmp( tok, "flo" ) == 0 ) { pFloat = fCapVector; u = 0; }
			else if( strcasecmp( tok, "pow" ) == 0 ) { pFloat = fPowVector; u = 0; }
			else if( strcasecmp( tok, "eff" ) == 0 ) { pFloat = fEffVector; u = 0; }
			else { goto parseerror; }
			/* parse vector until we hit an alphabetic char, ie a keyword */
			while( (tok = strtok( 0, " " )) )
			{
				if( isalpha( *tok ) ) { break; }
				if( u < 100 ) { pFloat[ u++ ] = (FLOAT)atof( tok ); } /* bounds check */
			}
			if( pFloat == fEffVector )
			{
				CHAR *pCurveName = 0;
				FLOAT *pCap = 0; FLOAT *pPow = 0; FLOAT *pEff = 0;
				UINT32 uCurveName = strlen(szCurveName);
				DP_MALLOC( pCap, FLOAT, u, __FILE__, __LINE__ ); parse_addblock( pCap );
				DP_MALLOC( pPow, FLOAT, u, __FILE__, __LINE__ ); parse_addblock( pPow );
				DP_MALLOC( pEff, FLOAT, u, __FILE__, __LINE__ ); parse_addblock( pEff );
				DP_MALLOC( pCurveName, CHAR, uCurveName + 1, __FILE__, __LINE__ ); parse_addblock( pCurveName );
				arr_float_copy( pCap, fCapVector, u );
				arr_float_copy( pPow, fPowVector, u );
				arr_float_copy( pEff, fEffVector, u );
				strcpy( pCurveName, szCurveName );
				curve_register( pCurveName, u, pCap, pPow, pEff );
				pFloat = 0;
			}
		}
		/******************************************/
		else if( strcasecmp( tok, "op" ) == 0 )
		{
			tok = strtok( 0, " " ); if( tok == 0 ) { goto parseerror; }
			if( strcasecmp( tok, "caps" ) == 0 )
			{
				/* op caps 0.1 0.2 0.75 1.0 */
				UINT32 u = 0;
				/* parse vector until we hit an alphabetic char, ie a keyword */
				while( (tok = strtok( 0, " " )) )
				{
					if( isalpha( *tok ) ) { break; }
					if( u < 100 ) { fCapVector[ u++ ] = (FLOAT)atof( tok ); } /* bounds check */
				}
				op_set_operating_capacities( u, fCapVector );
			}
			else if( strcasecmp( tok, "dep" ) == 0 )
			{
				/* op dep 80 ft */
				FLOAT fDep = 0;
				tok = strtok( 0, " " ); if( tok == 0 ) { goto parseerror; }
				fDep = (FLOAT)atof( tok );
				op_set_dependent( fDep );
				tok = strtok( 0, " " ); if( tok == 0 ) { goto parseerror; }
				ui_parse_units( tok );
				tok = strtok( 0, " " ); /* grab next word */
			}
			else if( strcasecmp( tok, "regress" ) == 0 )
			{
				op_regress();
				tok = strtok( 0, " " ); /* grab next word */
			}
			else if( strcasecmp( tok, "print" ) == 0 )
			{
				op_print();
				tok = strtok( 0, " " ); /* grab next word */
			}
			else { goto parseerror; }
		}
		/******************************************/
		else if( strcasecmp( tok, "dispatch" ) == 0 )
		{
			tok = strtok( 0, " " ); if( tok == 0 ) { goto parseerror; }
			if( strcasecmp( tok, "for" ) == 0 )
			{
				/* for <value> */
				tok = strtok( 0, " " ); if( tok == 0 ) { goto parseerror; }
				gfDispatch = (FLOAT)atof( tok );
				ud_cleanup();
				ud_malloc();
				ud_dispatch();
			}
			else if( strcasecmp( tok, "print" ) == 0 )
			{
				ud_print();
				tok = strtok( 0, " " ); /* grab next word */
			}
			else { goto parseerror; }
		}
		/******************************************/
		else
		{
			if( strcasecmp( tok, "unit" ) == 0 )
			{
				/* unit <unitname> <curvename> 60 ft 1000 cfs 1000 kw [ capacity 1.2 ] [ weight 0.5 ] [ headloss 2.5 ] [ genloss 0.90 ] [ gencurve <curvename> ] */
				CHAR *szUnitName, *pUnitName, *szCurveName, *szGenCurveName;
				FLOAT ft, cfs, kw;
				tok = strtok( 0, " " ); if( tok == 0 ) { goto parseerror; } szUnitName = tok;
				tok = strtok( 0, " " ); if( tok == 0 ) { goto parseerror; } szCurveName = tok;
				tok = strtok( 0, " " ); if( tok == 0 ) { goto parseerror; } ft = (FLOAT)atof( tok );
				tok = strtok( 0, " " ); if( tok == 0 ) { goto parseerror; } ui_parse_units( tok );
				tok = strtok( 0, " " ); if( tok == 0 ) { goto parseerror; } cfs = (FLOAT)atof( tok );
				tok = strtok( 0, " " ); if( tok == 0 ) { goto parseerror; } ui_parse_units( tok );
				tok = strtok( 0, " " ); if( tok == 0 ) { goto parseerror; } kw = (FLOAT)atof( tok );
				tok = strtok( 0, " " ); if( tok == 0 ) { goto parseerror; } ui_parse_units( tok );
				pUnitName = 0;
				DP_MALLOC( pUnitName, CHAR, strlen( szUnitName ) + 1, __FILE__, __LINE__ ); parse_addblock( pUnitName );
				strcpy( pUnitName, szUnitName );
				turbine_register( pUnitName, szCurveName, ft, cfs, kw );
				{
					UINT32 uUnitNumber = turbine_find_err( szUnitName );
					if( uUnitNumber == MAX_UINT32 ) { goto parseerror; }
#if 1
					while( 1 )
					{
						FLOAT fValue;
						CHAR* tokKeyword = 0;
						CHAR* tokValue = 0;
						CHAR* tokRestore = tok;

						tok = strtok( 0, " " ); if( tok == 0 ) { goto parseerror; }
						if( strcasecmp( tok, "capacity" ) == 0 ) { tokKeyword = tok; }
						else if( strcasecmp( tok, "weight" ) == 0 ) { tokKeyword = tok; }
						else if( strcasecmp( tok, "headloss" ) == 0 ) { tokKeyword = tok; }
						else if( strcasecmp( tok, "geneff" ) == 0 ) { tokKeyword = tok; }
						else if( strcasecmp( tok, "gencurve" ) == 0 ) { tokKeyword = tok; }

						tok = strtok( 0, " " ); if( tok == 0 ) { goto parseerror; }
						fValue = (FLOAT)atof( tok );
						tokValue = tok;

						if( tokKeyword && strcasecmp( tokKeyword, "capacity" ) == 0 ) { turbine_gencap( uUnitNumber, fValue ); tokRestore = 0; }
						else if( tokKeyword && strcasecmp( tokKeyword, "weight" ) == 0 ) { turbine_weight( uUnitNumber, fValue ); tokRestore = 0; }
						else if( tokKeyword && strcasecmp( tokKeyword, "headloss" ) == 0 ) { turbine_headloss( uUnitNumber, fValue ); tokRestore = 0; }
						else if( tokKeyword && strcasecmp( tokKeyword, "geneff" ) == 0 ) { turbine_geneff( uUnitNumber, fValue ); tokRestore = 0; }
						else if( tokKeyword && strcasecmp( tokKeyword, "gencurve" ) == 0 )
						{
							szGenCurveName = 0;
							DP_MALLOC( szGenCurveName, CHAR, strlen( tokValue ) + 1, __FILE__, __LINE__ ); parse_addblock( szGenCurveName );
							strcpy( szGenCurveName, tokValue );
							turbine_gencurvename( uUnitNumber, szGenCurveName );
							tokRestore = 0;
						}

						/* unwind on parse error */
						if( tokRestore ) { tok = strtok( strtokRestore( tok, tokRestore, ' ' ), " " ); break; }
					}
#else
					/* optional capacity */
					{
						CHAR* tokRestore = tok;
						tok = strtok( 0, " " ); if( tok == 0 ) { goto parseerror; }
						if( strcasecmp( tok, "capacity" ) == 0 )
						{
							/* capacity 1.2 */
							FLOAT capacity;
							tok = strtok( 0, " " ); if( tok == 0 ) { goto parseerror; } capacity = (FLOAT)atof( tok );
							turbine_gencap( uUnitNumber, capacity );
							tokRestore = 0;
						}
						/* unwind if parse error */
						if( tokRestore ) { tok = strtok( strtokRestore( tok, tokRestore, ' ' ), " " ); }
					}
					/* optional weight */
					{
						CHAR* tokRestore = tok;
						tok = strtok( 0, " " ); if( tok == 0 ) { goto parseerror; }
						if( strcasecmp( tok, "weight" ) == 0 )
						{
							/* weight 0.9 */
							FLOAT weight;
							tok = strtok( 0, " " ); if( tok == 0 ) { goto parseerror; } weight = (FLOAT)atof( tok );
							turbine_weight( uUnitNumber, weight );
							tokRestore = 0;
						}
						/* unwind if parse error */
						if( tokRestore ) { tok = strtok( strtokRestore( tok, tokRestore, ' ' ), " " ); }
					}
					/* optional headloss */
					{
						CHAR* tokRestore = tok;
						tok = strtok( 0, " " ); if( tok == 0 ) { goto parseerror; }
						if( strcasecmp( tok, "headloss" ) == 0 )
						{
							/* headloss 2.5 */
							FLOAT loss;
							tok = strtok( 0, " " ); if( tok == 0 ) { goto parseerror; } loss = (FLOAT)atof( tok );
							turbine_headloss( uUnitNumber, loss );
							tokRestore = 0;
						}
						/* unwind if parse error */
						if( tokRestore ) { tok = strtok( strtokRestore( tok, tokRestore, ' ' ), " " ); }
					}
					/* optional genloss */
					{
						CHAR* tokRestore = tok;
						tok = strtok( 0, " " ); if( tok == 0 ) { goto parseerror; }
						if( strcasecmp( tok, "geneff" ) == 0 )
						{
							/* geneff 0.97 */
							FLOAT loss;
							tok = strtok( 0, " " ); if( tok == 0 ) { goto parseerror; } loss = (FLOAT)atof( tok );
							turbine_geneff( uUnitNumber, loss );
							tokRestore = 0;
						}
						/* unwind if parse error */
						if( tokRestore ) { tok = strtok( strtokRestore( tok, tokRestore, ' ' ), " " ); }
					}
#endif
				}
			}
			else if( strcasecmp( tok, "weight" ) == 0 )
			{
				/* weight u1 0.9 */
				CHAR *szUnitName;
				FLOAT weight;
				tok = strtok( 0, " " ); if( tok == 0 ) { goto parseerror; } szUnitName = tok;
				tok = strtok( 0, " " ); if( tok == 0 ) { goto parseerror; } weight = (FLOAT)atof( tok );
				turbine_weight( turbine_find( szUnitName ), weight );
			}
			else if( strcasecmp( tok, "weighting" ) == 0 )
			{
				/* weights [ equal ... ] */
				guWeightCode = DP_WEIGHT_DEFAULT;
				tok = strtok( 0, " " ); if( tok == 0 ) { goto parseerror; }
				if( strcasecmp( tok, "relative" ) == 0 )
				{
					guWeightCode |= DP_WEIGHT_RELATIVE;
					tok = strtok( 0, " " ); if( tok == 0 ) { goto parseerror; }
				}
				if( strcasecmp( tok, "equal" ) == 0 )			{ guWeightCode |= DP_WEIGHT_EQUAL; }
				else if( strcasecmp( tok, "maxpower" ) == 0 )	{ guWeightCode |= DP_WEIGHT_MAXPOWER; }
				else if( strcasecmp( tok, "maxflow" ) == 0 )	{ guWeightCode |= DP_WEIGHT_MAXFLOW; }
				else if( strcasecmp( tok, "minpower" ) == 0 )	{ guWeightCode |= DP_WEIGHT_MINPOWER; }
				else if( strcasecmp( tok, "minflow" ) == 0 )	{ guWeightCode |= DP_WEIGHT_MINFLOW; }
				else { goto parseerror; }
				/* group weights assigned during dp initalization */
			}
			else if( strcasecmp( tok, "head" ) == 0 ) {
				tok = strtok( 0, " " ); if( tok == 0 ) { goto parseerror; } gfHeadCurr = (FLOAT)atof( tok );
				tok = strtok( 0, " " ); if( tok == 0 ) { goto parseerror; } ui_parse_units( tok );
			}
			else if( strcasecmp( tok, "min" ) == 0 ) {
				tok = strtok( 0, " " ); if( tok == 0 ) { goto parseerror; } gfStateMin = (FLOAT)atof( tok );
				tok = strtok( 0, " " ); if( tok == 0 ) { goto parseerror; } ui_parse_units( tok );
			}
			else if( strcasecmp( tok, "max" ) == 0 ) {
				tok = strtok( 0, " " ); if( tok == 0 ) { goto parseerror; } gfStateMax = (FLOAT)atof( tok );
				tok = strtok( 0, " " ); if( tok == 0 ) { goto parseerror; } ui_parse_units( tok );
			}

			/***************************/
			/* global coefs */

			else if( strcasecmp( tok, "losscoef" ) == 0 )
			{ tok = strtok( 0, " " ); if( tok == 0 ) { goto parseerror; } gfPlantLossCoef = (FLOAT)atof( tok ); }
			else if( strcasecmp( tok, "coordinationfactora" ) == 0 )
			{ tok = strtok( 0, " " ); if( tok == 0 ) { goto parseerror; } gfCoordinationFactorA = (FLOAT)atof( tok ); }
			else if( strcasecmp( tok, "coordinationfactorb" ) == 0 )
			{ tok = strtok( 0, " " ); if( tok == 0 ) { goto parseerror; } gfCoordinationFactorB = (FLOAT)atof( tok ); }

			/***************************/
			/* solve and print */

			else if( strcasecmp( tok, "solve" ) == 0 ) {
				/* solve power | flow */
				UINT32 uSolve = 0;
				tok = strtok( 0, " " ); if( tok == 0 ) { goto parseerror; }
				if( strcasecmp( tok, "power" ) == 0 )		{ guSolveMode = DP_OPTIMIZE_FOR_POWER; uSolve = 1; }
				else if( strcasecmp( tok, "flow" ) == 0 )	{ guSolveMode = DP_OPTIMIZE_FOR_FLOW; uSolve = 1; }
				else { goto parseerror; }
				if( uSolve )
				{
					dp_cleanup();
					dp_assign_weights();
					dp_resize();
					PRINT_STDOUT( "DP starting...\n" );
					dp_malloc();
					dp();
				}
			}
			else if( strcasecmp( tok, "print" ) == 0 )
			{
				tok = strtok( 0, " " ); if( tok == 0 ) { goto parseerror; }
				if( strcasecmp( tok, "units" ) == 0 )			{ turbine_list(); }
				else if( strcasecmp( tok, "weights" ) == 0 )	{ dp_assign_weights(); dp_print_weights(); }
				else if( strcasecmp( tok, "allcurves" ) == 0 )	{ curve_list( 0 ); }
				else if( strcasecmp( tok, "curves" ) == 0 )		{ curve_list( 1 ); }
				else if( strcasecmp( tok, "config" ) == 0 )		{ dp_print_config(); }
				else if( strcasecmp( tok, "weighting" ) == 0 )	{ dp_print_weighting(); }
				else if( strcasecmp( tok, "solution" ) == 0 )	{ dp_print_solution(); }
				else { goto parseerror; }
			}
			else if( strcasecmp( tok, "echo" ) == 0 )
			{
				/* dear lord, what a hack! */
				tok = strtok( 0, "\n" ); if( tok == 0 ) { goto parseerror; }
				{
					CHAR sCharDelim[2] = {0,0}; sCharDelim[0] = *tok;
					tok = strtok( tok, (char*)&sCharDelim ); if( tok == 0 ) { goto parseerror; }
					fprintf( stdout, tok ); putc( '\n', stdout );
				}
			}

			/************************************/
			/* simple forward branching */

			else if( strcasecmp( tok, "skipto" ) == 0 )
			{
				CHAR *szLabel;
				tok = strtok( 0, " " ); if( tok == 0 ) { goto parseerror; } szLabel = tok;
				while( 1 )
				{
					tok = strtok( 0, ": " ); if( tok == 0 ) { goto parseerror; }
					if( strcasecmp( tok, szLabel ) == 0 ) { break; }
				}
			}
			else if( strcasecmp( tok, "define" ) == 0 )
			{
				tok = strtok( 0, " " ); if( tok == 0 ) { goto parseerror; }
				gpSymbolList[ guSymbolCount++ ] = tok;
			}
			else if( strcasecmp( tok, "skiptoif" ) == 0 )
			{
				tok = strtok( 0, " " ); if( tok == 0 ) { goto parseerror; }
				if( guTestCondition )
				{
					CHAR* szLabel = tok;
					while( 1 )
					{
						tok = strtok( 0, ": " ); if( tok == 0 ) { goto parseerror; }
						if( strcasecmp( tok, szLabel ) == 0 ) { break; }
					}
				}
			}
			else if( strcasecmp( tok, "test" ) == 0 )
			{
				/* test <symbol> */
				UINT32 u;
				CHAR *szSymbol;
				tok = strtok( 0, " " ); if( tok == 0 ) { goto parseerror; } szSymbol = tok;
				guTestCondition = 0;
				for( u = 0; u < guSymbolCount && !guTestCondition; u++ )
				{
					guTestCondition = (0 == stricmp( gpSymbolList[u], szSymbol ));
				}
			}

			/**********************************/
			/* simple commands */

			else if( strcasecmp( tok, "delimiter" ) == 0 )		{ gcOutputDelimiter = ( tok = strtok( 0, " " ) ) ? *tok : ' ';
			} else if( strcasecmp( tok, "unitsteps" ) == 0 )	{ tok = strtok( 0, " " ); if( tok == 0 ) { goto parseerror; } guUserStepCount = (UINT32)atoi( tok );
			} else if( strcasecmp( tok, "transpose" ) == 0 )	{ tok = strtok( 0, " " ); if( tok == 0 ) { goto parseerror; } guTransposeSolution = (UINT32)atoi( tok );
			} else if( strcasecmp( tok, "debug" ) == 0 )		{ tok = strtok( 0, " " ); if( tok == 0 ) { goto parseerror; } guDebugMode = (UINT32)atoi( tok );
			} else if( strcasecmp( tok, "help" ) == 0 )			{ fprintf( stdout, VERSIONED_NAME "\n" ); fprintf( stdout, gszCommandHelp );
			} else if( strcasecmp( tok, "end" ) == 0 )			{ giInteractiveMode = 0; break;
			} else if( strcasecmp( tok, "mem" ) == 0 )			{ fprintf( stdout, "%lu bytes allocated\n", guMALLOC );
			} else
			{
				if( tok[ strlen(tok) ] != ':' ) { ; /* skip label */ }
				else { goto parseerror; }
			}
			tok = strtok( 0, " " ); /* grab next word */
		}
	}
	return;
parseerror:
	if( tok ) {
		sprintf( gcPrintBuff, "%s:%d (" VERSIONED_NAME  ") Parse error '%s'.\n", __FILE__, __LINE__, tok );
		PRINT_STDERR( gcPrintBuff );
	} else {
		sprintf( gcPrintBuff, "%s:%d (" VERSIONED_NAME  ") Parse error: missing parameter.\n", __FILE__, __LINE__ );
		PRINT_STDERR( gcPrintBuff );
	}
	dp_cleanup_fatal();
  }	
}

/*********************************************/

#if 0

#define STDOUT_REDIRECT_FILENAME_LENGTH 1024
char gszRedirectionFilename[ STDOUT_REDIRECT_FILENAME_LENGTH + 1 ] = {0};
int gRedirectionHANDLE = 0;
int gRedirectionHANDLE_saved = 0;

EXTERNC void stdout_redirect( char* szOutfile )
{
	assert( !gRedirectionHANDLE );
	assert( !gRedirectionHANDLE_saved );

	strncpy( gszRedirectionFilename, szOutfile, STDOUT_REDIRECT_FILENAME_LENGTH - 1 );
	if( strlen( gszRedirectionFilename ) != 0 )
	{
		gRedirectionHANDLE = open( gszRedirectionFilename, O_RDWR | O_CREAT );
		if( gRedirectionHANDLE < 0 )
		{
			sprintf( gcPrintBuff, "%s:%d (" VERSIONED_NAME  ") Unable to open file '%s'.\n", __FILE__, __LINE__, gszRedirectionFilename );
			PRINT_STDERR( gcPrintBuff );
			dp_cleanup_fatal();
			exit(-1);
		}

		gRedirectionHANDLE_saved = dup( 1 );
		dup2( gRedirectionHANDLE, gRedirectionHANDLE_saved );
	}
}

EXTERNC void stdout_unredirect()
{
	fflush( stdout );
	if( gszRedirectionFilename[0] )
	{
		dup2( gRedirectionHANDLE_saved, 1 );
		gRedirectionHANDLE = 0;
		gRedirectionHANDLE_saved = 0;
	}
}

#else

#define STDOUT_REDIRECT_FILENAME_LENGTH 1024
char gszRedirectionFilename[ STDOUT_REDIRECT_FILENAME_LENGTH ] = {0};
FILE* gRedirectionFILE = 0;
int gRedirectionHANDLE = 0;
 
EXTERNC void stdout_redirect( char* szOutfile )
{
 	assert( !gRedirectionHANDLE );
	assert( !gRedirectionFILE );
 
 	strncpy( gszRedirectionFilename, szOutfile, STDOUT_REDIRECT_FILENAME_LENGTH - 1 );
	if( strlen( gszRedirectionFilename ) != 0 ) {
		gRedirectionHANDLE = dup( fileno( stdout ) );
		gRedirectionFILE = fopen( gszRedirectionFilename, "w" );
		if( !gRedirectionFILE ) { fprintf( stderr, "%s:%d (" VERSIONED_NAME  ") Unable to open file '%s'.\n", __FILE__, __LINE__, gszRedirectionFilename ); dp_cleanup_fatal(); exit(-1); }
		dup2( fileno( gRedirectionFILE ), fileno( stdout ) );
 	}
}
 
EXTERNC void stdout_unredirect()
{
 	fflush( stdout );
	if( gszRedirectionFilename[0] ) {
		fflush( stdout );
		dup2( gRedirectionHANDLE, fileno( stdout ) ); gRedirectionHANDLE = 0;
		fclose( gRedirectionFILE );	gRedirectionFILE = 0;
 	}
}

#endif

/*********************************************/

void code_sample()
{
	turbine_register( "u2", "Crossflow",      65 /*ft*/,  1400 /*cfs*/, 1700 /*kw*/ );
	turbine_register( "u3", "Francis",        55 /*ft*/,  730 /*cfs*/, 1500 /*kw*/ );
	turbine_register( "u4", "Kaplan",         50 /*ft*/,  1000 /*cfs*/, 1500 /*kw*/ );
	turbine_register( "u1", "FixedPropeller", 68 /*ft*/,  1250 /*cfs*/, 1250 /*kw*/ );
	guSolveMode = DP_OPTIMIZE_FOR_POWER;
	gfHeadCurr = 68;
	gfStateMin = gfStateMax = 0;
	guProblemStepCount = 5;
}

CHAR* gszWritableMemory = 0;
CHAR* gszSample = "\
curve c1 flo 0 .175 .22 .25 .285 .325 .375 .435 .485 .525 .60 .68 .75 .82 .91 1.00 1.08\n\
curve c1 pow 0 .175 .22 .25 .285 .325 .375 .435 .485 .525 .60 .68 .75 .82 .91 1.00 1.08\n\
curve c1 eff 0 .65 .70 .725 .75 .775 .80 .825 .84 .85 .86 .865 .8675 .8675 .865 .858 .85\n\
# comment\n\
unit u2 Crossflow 68 ft 1400 cfs 1700 kw\n\
unit u3 Francis   63 ft  730 cfs 1500 kw\n\
unit u4 Kaplan    65 ft 1000 cfs 1500 kw\n\
unit u1 Kaplan        65 ft 1250 cfs 1250 kw\n\
# comment\n\
head 65 ft min 0 cfs max 0 cfs unitsteps 5\n\
solve power\n\
print units\n\
print solution\n\
";
CHAR* gszSample2 = "\
unit u1 Pelton 65 ft 1000 cfs 1500 kw\n\
unit u2 Pelton 65 ft 1000 cfs 1500 kw\n\
unit u3 Pelton 65 ft 1000 cfs 1500 kw\n\
# comment\n\
head 65 ft min 0 cfs max 0 cfs unitsteps 5\n\
solve flow\n\
print units\n\
print solution\n\
";

UINT32 guTestLevel = 0;
CHAR* gszInfile = 0;
CHAR* gszOutfile = 0;

void commandline_help()
{
	printf( "ohdp [ -h ] [ -I ] [ -T ] [ -O int ] [ -t int ] [ -d char ] [ -o outfilename ] [ -i infilename ]\n" );
	printf( "-h             this help\n" );
	printf( "-I             interactive mode\n" );
	printf( "-T             transpose all output to vertical tables (ie. for spreadsheet programs)\n" );
	printf( "-D             Debug mode (extra details)\n" );
	printf( "-S identifier  Define symbol 'identifier'\n" );
	printf( "-t int         test level (1, 2)\n" );
	printf( "                 1 API test mode\n" );
	printf( "                 2 Input file test mode\n" );
	printf( "-d char        set output delimiter to single character 'char'\n" );
	printf( "-o outputfile  new file to put output of run\n" );
	printf( "-i inputfile   existing input file\n" );
	printf( "Example to create spreadsheet readable output:\n" );
	printf( "ohdp -T -d , -o basictest.csv -i basictest.dp\n" );
	exit( 0 );
}

int CDECLCALL main(int argc, char** argv)
{
	/* parse commandline */
	{
		int i = 1;
		while( i<argc && (*(argv[i] + 0) == '-') ) {
			if( *(argv[i] + 1) == 'i' ) { gszInfile = argv[++i]; }
			else if( *(argv[i] + 1) == 'o' ) { gszOutfile = argv[++i]; }
			else if( *(argv[i] + 1) == 'I' ) { giInteractiveMode = 1; }
			else if( *(argv[i] + 1) == 'T' ) { guTransposeSolution = 1; }
			else if( *(argv[i] + 1) == 'S' ) { gpSymbolList[ guSymbolCount++ ] = argv[++i]; }
			else if( *(argv[i] + 1) == 't' ) { guTestLevel = (UINT32)atoi( argv[++i] ); }
			else if( *(argv[i] + 1) == 'D' ) { guDebugMode = 1; }
			else if( *(argv[i] + 1) == 'd' ) { gcOutputDelimiter = *(argv[++i]); }
			else /* if( *(argv[i] + 1) == 'h' ) */ { commandline_help(); }
			i++;
		}
		if( i<argc ) { gszInfile = argv[i]; }
	}

	// redirect stdout
	if( gszOutfile ) { stdout_redirect( gszOutfile ); }

	/////////////////////////
	fprintf( stdout, VERSIONED_NAME "\n" );

#if defined(_WIN32) & defined(WIN32)
	if( _set_SSE2_enable(1) ) { PRINT_STDOUT( "SSE2 intrinsics enabled.\n" ); }
#endif

	curve_register_builtins();

	if( guTestLevel > 0 )
	{
		curve_test();
		turbine_test();
		if( guTestLevel == 1 )
		{
			code_sample();
		}
		else if( guTestLevel == 2 )
		{
			DP_MALLOC( gszWritableMemory, CHAR, strlen( gszSample2 ) + 1, __FILE__, __LINE__ ); parse_addblock( gszWritableMemory );
			strcpy( gszWritableMemory, gszSample2 );
			parse_program( gszWritableMemory );
		}
	}
	else if( gszInfile )
	{
		long iSize = 0;
		FILE* pFile = fopen( gszInfile, "r" );
		if( !pFile )
		{
			sprintf( gcPrintBuff, "%s:%d (" VERSIONED_NAME  ") Unable to open file '%s'.\n", __FILE__, __LINE__, gszInfile );
			PRINT_STDERR( gcPrintBuff );
			dp_cleanup();
			exit(-1);
		}
		fseek( pFile, 0, SEEK_END );
		iSize = ftell( pFile );
		rewind( pFile );
		DP_MALLOC( gszWritableMemory, CHAR, iSize + 1, __FILE__, __LINE__ ); parse_addblock( gszWritableMemory );
		fread( gszWritableMemory, 1, iSize, pFile );
		fclose( pFile );
		parse_program( gszWritableMemory );
	}
	else if( giInteractiveMode )
	{
		#define INTERACTIVE_BUFFER_SIZE 1024
		DP_MALLOC( gszWritableMemory, CHAR, INTERACTIVE_BUFFER_SIZE, __FILE__, __LINE__ ); parse_addblock( gszWritableMemory );
		while( giInteractiveMode )
		{
			fprintf( stdout, "> " );
			fgets( gszWritableMemory, INTERACTIVE_BUFFER_SIZE - 1, stdin );
			parse_program( gszWritableMemory );
		}
	}

	/* ensure cleanup */
	ud_cleanup();
	op_cleanup();
	dp_malloc_control( 2 );
	parse_freeallblocks();

	if( gszOutfile ) { stdout_unredirect(); }
	return 0;
}

#endif /* _WIN32DLL */
