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

#define _CRT_SECURE_NO_WARNINGS

#include <assert.h>
#include <ctype.h>
#include <malloc.h>
#include <windows.h>

#define DLLEXPORT __declspec(dllexport)

#ifdef _DEBUG
	#undef ASSERT
	#define ASSERT(x) do{if(!(x)){__debugbreak();}}while(0)
	#define BUILDTYPE "D"
#else
	#undef ASSERT
	#define ASSERT(x) do {} while(0)
	#define BUILDTYPE " "
#endif // _DEBUG

#ifdef _WINDDK

//int _finite( double f ) { return 1; }

#else

#include <stdio.h>

#endif

#include "xlcall.h"

#include "hdp_vnum.h"
#include "dp_vnum.h"

/*******************************************************************/

#define ENABLE_MIXED_TYPES

#ifdef ENABLE_MIXED_TYPES
	#define XL_FLOAT_TYPE double
	#define DP_FLOAT_TYPE float
#else
	#define XL_FLOAT_TYPE double
	#define DP_FLOAT_TYPE double
#endif

#include "ohdp.h"

/*******************************************************************/

#include "XLCALL.cpp"

/*******************************************************************/

#ifdef _DEBUG
	#define BUILDTYPE "D"
#else
	#define BUILDTYPE " "
#endif // _DEBUG

#define MAX_UINT (0-1)
#define MAX_EXCEL_ROWS 32767
#define MAX_DP_RESOLUTION 100

#define ENABLE_PROPERREGISTERARGS

typedef unsigned short int UINT16;
#define ADDIN_NAME "HydroDP"
#define ADDIN_VERSIONED_NAME "HydroDP " HDP_VNUM
#define ADDIN_DESCRIPTION "Dynamic Programming for Unit Dispatch"
#define ADDIN_COPYRIGHT "John Howard, Apache License Ver2"
#define ADDIN_VERSION "Version: " HDP_VNUM "/" DP_VNUM " " BUILDTYPE
#define ADDIN_DATE "Date: " __DATE__ ", " __TIME__

static LPSTR rgFuncs[] = {
	"HydroAbout", "I", "HydroAbout",
		"", /* arg text */
		"1" /* macrotype */, ADDIN_NAME /* category */, "ST" /* shortcut text */,
		"About the HydroDP Plugin" /* helptopic */,
		"About the HydroDP Plugin" /* functionhelp */,
		"" /* arg help 2 */,

	"HydroVersion", "P", "HydroVersion",
		"", /* arg text */
		"1" /* macrotype */, ADDIN_NAME /* category */, "ST" /* shortcut text */,
		"HydroDP Version Information" /* helptopic */,
		"HydroDP Version Information" /* functionhelp */,
		"" /* arg help 2 */,

	"HydroOptPTable", "RKKBHPP", "HydroOptPTable",
	"Eff Curves, Unit Descriptions, Current Head, # Eff Samples, [Weights], [Metric mode]",
		"1" /* macrotype */, ADDIN_NAME /* category */, "" /* shortcut text */,
		"Optimize Power Allocation Decisions" /* helptopic */,
		"Optimize Power Allocation Decisions" /* functionhelp */,
		"" /* arg help 2 */,

	"HydroOptQTable", "RKKBHPP", "HydroOptQTable",
	"Eff Curves, Unit Descriptions, Current Head, # Eff Samples, [Weights], [Metric mode]",
		"1" /* macrotype */, ADDIN_NAME /* category */, "" /* shortcut text */,
		"Optimize Discharge Allocation Decisions" /* helptopic */,
		"Optimize Discharge Allocation Decisions" /* functionhelp */,
		"" /* arg help 2 */,

	"HydroOptQValue", "RKKBHBPP", "HydroOptQValue",
	"Curves, Units, Head, SampleCount, Value, [Weights], [Metric mode]",
		"1" /* macrotype */, ADDIN_NAME /* category */, "" /* shortcut text */,
		"Determine Plant Operation for a Specific Discharge" /* helptopic */,
		"Determine Plant Operation for a Specific Discharge" /* functionhelp */,
		"" /* arg help 2 */,

	"HydroOptQValue2", "RKKBHBPPP", "HydroOptQValue2",
	"Curves, Units, Head, SampleCount, Value, [GenCaps], [Weights], [Metric mode]",
		"1" /* macrotype */, ADDIN_NAME /* category */, "" /* shortcut text */,
		"Determine Plant Operation for a Specific Discharge" /* helptopic */,
		"Determine Plant Operation for a Specific Discharge" /* functionhelp */,
		"" /* arg help 2 */,

	"HydroOptPValue", "RKKBHBPP", "HydroOptPValue",
	"Curves, Units, Head, SampleCount, Value, [Weights], [Metric mode]",
		"1" /* macrotype */, ADDIN_NAME /* category */, "" /* shortcut text */,
		"Determine Plant Operation for a Specific Power Output" /* helptopic */,
		"Determine Plant Operation for a Specific Power Output" /* functionhelp */,
		"" /* arg help 2 */,

	"HydroOptPValue2", "RKKBHBPPP", "HydroOptPValue2",
	"Curves, Units, Head, SampleCount, Value, [GenCaps], [Weights], [Metric mode]",
		"1" /* macrotype */, ADDIN_NAME /* category */, "" /* shortcut text */,
		"Determine Plant Operation for a Specific Power Output" /* helptopic */,
		"Determine Plant Operation for a Specific Power Output" /* functionhelp */,
		"" /* arg help 2 */,

	"HydroOptQValueHK", "RKKBHBPP", "HydroOptQValueHK",
	"Curves, Units, Head, SampleCount, Value, [Weights], [Metric mode]",
		"1" /* macrotype */, ADDIN_NAME /* category */, "" /* shortcut text */,
		"Determine Plant HK for a Specific Discharge" /* helptopic */,
		"Determine Plant HK for a Specific Discharge" /* functionhelp */,
		"" /* arg help 2 */,

	"HydroOptQValueHK2", "RKKBHBPPP", "HydroOptQValueHK2",
	"Curves, Units, Head, SampleCount, Value, [GenCaps], [Weights], [Metric mode]",
		"1" /* macrotype */, ADDIN_NAME /* category */, "" /* shortcut text */,
		"Determine Plant HK for a Specific Discharge" /* helptopic */,
		"Determine Plant HK for a Specific Discharge" /* functionhelp */,
		"" /* arg help 2 */,

	"HydroOptQPoints", "RKKKBBHPP", "HydroOptQPoints",
	"Curves, Units, OPCapacities, Head1, Head2, SampleCount, [Weights], [Metric mode]",
		"1" /* macrotype */, ADDIN_NAME /* category */, "" /* shortcut text */,
		"Determine Plant H/K at Specific Capacities" /* helptopic */,
		"Determine Plant H/K at Specific Capacities" /* functionhelp */,
		"" /* arg help 2 */,

	"HydroOptQPoints2", "RKKKBBHPPP", "HydroOptQPoints2",
	"Curves, Units, OPCapacities, Head1, Head2, SampleCount, [GenCaps], [Weights], [Metric mode]",
		"1" /* macrotype */, ADDIN_NAME /* category */, "" /* shortcut text */,
		"Determine Plant H/K at Specific Capacities" /* helptopic */,
		"Determine Plant H/K at Specific Capacities" /* functionhelp */,
		"" /* arg help 2 */,

	"HydroOptimizeSpecificQ", "RKKBHBPP", "HydroOptimizeSpecificQ",
	"Eff Curves, Unit Descriptions, Current Head, # Eff Samples, Value, [Weights], [Metric mode]",
		"1" /* macrotype */, ADDIN_NAME /* category */, "" /* shortcut text */,
		"Determine Plant Operation for a Specific Discharge" /* helptopic */,
		"Determine Plant Operation for a Specific Discharge" /* functionhelp */,
		"" /* arg help 2 */,

	"HydroOptimizeOPQ", "RKKKBBHPP", "HydroOptimizeOPQ",
	"Eff Curves, Unit Descriptions, OPCapacities, Head1, Head2, # Eff Samples, [Weights], [Metric mode]",
		"1" /* macrotype */, ADDIN_NAME /* category */, "" /* shortcut text */,
		"Determine Plant H/K at Specific Capacities" /* helptopic */,
		"Determine Plant H/K at Specific Capacities" /* functionhelp */,
		"" /* arg help 2 */,

#if 0
	"HydroTest", "I", "HydroTest",
		"", /* arg text */
		"1" /* macrotype */, ADDIN_NAME /* category */, "" /* shortcut text */,
		"test" /* helptopic */,
		"test" /* functionhelp */,
		"" /* arg help 2 */,
#endif
	0
};

HANDLE g_hInst = NULL;
char* gszInfo = 0;
DP_FLOAT_TYPE* gpCurveData = 0;

/*******************************************************************/

#if 0

// can be used for excel 2003 builds
void CriticalSection( UINT32 uCode ) {}

#else

// should be used for excel 2007+ builds that have multithreaded evaluation
void CriticalSection( UINT32 uCode )
{
	static CRITICAL_SECTION cs;
	switch( uCode ) {
	case 0: InitializeCriticalSection( &cs ); break;
	case 1: DeleteCriticalSection( &cs ); break;
	case 2: EnterCriticalSection( &cs ); break;
	case 3: LeaveCriticalSection( &cs ); break;
	}
}

#endif

#ifdef _DEBUG

void cdecl debugPrintf(LPSTR lpFormat, ...)
{
	char rgch[256];
	va_list argList;

	va_start(argList,lpFormat);
	wvsprintfA(rgch,lpFormat,argList);
	va_end(argList);
	OutputDebugStringA(rgch);
}

#endif // _DEBUG

int lpstricmp(LPSTR szStr, LPSTR pasStr)
{
	int i;

	if (strlen(szStr) != *pasStr)
		return 1;

	for (i = 1; i <= szStr[0]; i++)
	{
		if (tolower(szStr[i-1]) != tolower(pasStr[i]))
			return 1;
	}
	return 0;
}

void interpret_xlret( int xlret )
{
#ifdef _DEBUG
	if (xlret & xlretAbort)     debugPrintf(" Macro Halted\r");
	if (xlret & xlretInvXlfn)   debugPrintf(" Invalid Function Number\r");
	if (xlret & xlretInvCount)  debugPrintf(" Invalid Number of Arguments\r");
	if (xlret & xlretInvXloper) debugPrintf(" Invalid XLOPER12\r");
	if (xlret & xlretStackOvfl) debugPrintf(" Stack Overflow\r");
	if (xlret & xlretFailed)    debugPrintf(" Command failed\r");
	if (xlret & xlretUncalced)  debugPrintf(" Uncalced cell\r");
#endif
}

/*******************************************************************/

#ifdef false

LPXLOPER xloper_handle()
{
	LPXLOPER pxloper;
	HANDLE hArray;
	hArray = GlobalAlloc( GMEM_ZEROINIT, sizeof(XLOPER) );
	pxloper = (LPXLOPER)GlobalLock( hArray );
	return pxloper;
}
LPXLOPER xloper_static()
{
	static XLOPER xloper;
	memset( (LPVOID)&xloper, 0, sizeof(XLOPER) );
	return &xloper;
}

#endif

LPXLOPER xloper_init( LPXLOPER p )
{
	memset( p, 0, sizeof( XLOPER ) );
	return p;
}
LPXLOPER xloper_malloc_array( UINT32 u )
{
	LPXLOPER p;
	CriticalSection( 2 );
		p = (LPXLOPER)malloc( sizeof(XLOPER) * u );
	CriticalSection( 3 );
	memset( (LPVOID)p, 0, sizeof(XLOPER) * u );
	p->xltype = xlbitDLLFree;
	return p;
}
LPXLOPER xloper_malloc()
{
	LPXLOPER p;
	CriticalSection( 2 );
		p = (LPXLOPER)malloc( sizeof(XLOPER) );
	CriticalSection( 3 );
	memset( (LPVOID)p, 0, sizeof(XLOPER) );
	p->xltype = xlbitDLLFree;
	return p;
}
void xloper_free( LPXLOPER p )
{
	if( p->xltype & xltypeStr )
	{
		memset( (LPVOID)(p->val.str + 1), 0, (BYTE)(p->val.str[0]) );
		CriticalSection( 2 );
			free( p->val.str );
		CriticalSection( 3 );
		p->val.str = (LPSTR)0xDEADBEEF;
	}
	else if( p->xltype & xltypeMulti )
	{
		// semantics here are that the data for the xltypeMulti is never malloced by itself...
		// it is malloced with a leading container which, when we free it, also frees the data.
		memset( (LPVOID)(p->val.array.lparray), 0xDD, p->val.array.rows * p->val.array.columns * sizeof(XLOPER) );
		p->val.array.lparray = (LPXLOPER)0xDEADBEEF;
	}
	// free the container?
	if( p->xltype & xlbitDLLFree )
	{
		memset( (LPVOID)p, 0xDD, sizeof(XLOPER) );
		CriticalSection( 2 );
			free( p );
		CriticalSection( 3 );
	}
}

LPXLOPER xloper_err( LPXLOPER p )
{
	p->xltype |= xltypeErr;
	p->val.err = xlerrValue;
	return p;
}
int xloper_iserr( LPXLOPER p )
{
	return (p->xltype & xltypeErr);
}
int xloper_ismissing( LPXLOPER p )
{
	return (p->xltype & xltypeMissing);
}
LPXLOPER xloper_str( LPXLOPER p, LPSTR sz )
{
	p->xltype |= xltypeStr;
	CriticalSection( 2 );
		p->val.str = malloc( strlen( sz ) + 1 );
	CriticalSection( 3 );
	strncpy( p->val.str + 1, sz, strlen( sz ) );
	p->val.str[0] = (BYTE)( strlen( sz ) );
	return p;
}
LPXLOPER xloper_missing( LPXLOPER p )
{
	p->xltype |= xltypeMissing;
	return p;
}
LPXLOPER xloper_num( LPXLOPER p, XL_FLOAT_TYPE d )
{
	p->xltype |= xltypeNum;
	p->val.num = d;
	return p;
}
LPXLOPER xloper_int( LPXLOPER p, UINT16 u )
{
	p->xltype |= xltypeInt;
	p->val.w = u;
	return p;
}
LPXLOPER xloper_sref( LPXLOPER p, WORD r0, BYTE c0, WORD r1, BYTE c1 )
{
	p->xltype |= xltypeSRef;
	p->val.sref.count = 1;
	p->val.sref.ref.rwFirst = r0;
	p->val.sref.ref.rwLast = r1;
	p->val.sref.ref.colFirst = c0;
	p->val.sref.ref.colLast = c1;
	return p;
}
LPXLOPER xloper_fqref( LPXLOPER pFQ, LPXLMREF pPQ, WORD r0, BYTE c0, WORD r1, BYTE c1 )
{
	pPQ->count = 1;
	pPQ->reftbl[0].rwFirst = r0;
	pPQ->reftbl[0].rwLast = r1;
	pPQ->reftbl[0].colFirst = c0;
	pPQ->reftbl[0].colLast = c1;
	pFQ->val.mref.lpmref = pPQ;
	return pFQ;
}

LPXLOPER xloper_multi( LPXLOPER p, UINT16 uRows, UINT16 uCols, LPXLOPER pxArray )
{
	p->xltype |= xltypeMulti;
	p->val.array.rows = uRows;
	p->val.array.columns = uCols;
	p->val.array.lparray = pxArray;
	return p;
}

/*********************************************************************/

static BYTE* g_pDLLMemChain = 0;
void* dll_allocblock( unsigned int uBytes )
{
	UINT32 uLinkSize = sizeof(void*);
	BYTE* p = 0;
	CriticalSection( 2 );
		p = malloc( uLinkSize + uBytes );
	CriticalSection( 3 );
	if( !p ) { return 0; }
	memset( p, 0, uLinkSize + uBytes );
	*(BYTE**)p = g_pDLLMemChain;
	g_pDLLMemChain = p;
	return &p[ uLinkSize ];
}
void dll_freeallblocks()
{
	while( g_pDLLMemChain )
	{
		BYTE* pNext = *(BYTE**)g_pDLLMemChain;
		CriticalSection( 2 );
			free( g_pDLLMemChain );
		CriticalSection( 3 );
		g_pDLLMemChain = pNext;
	}
}

/*********************************************************************/

#define DLL_CACHE_ADDR_INFO( _pData )  ((int*) (  ((int*)_pData)-1 ))
#define DLL_CACHE_ADDR_DATA( _pInfo )  ((void*)(  ((int*)_pInfo)+1 ))

void* dll_cache_alloc( void* _pData, int _n )
{
	int* _pInfo = _pData ? DLL_CACHE_ADDR_INFO( _pData ) : 0;
	if( _pInfo )
	{
		if( ( *_pInfo >= _n ) ) { memset( _pData, 0, _n ); return _pData; }
		CriticalSection( 2 );
			free( _pInfo );
		CriticalSection( 3 );
	}
	CriticalSection( 2 );
		_pInfo = (int*)malloc( _n + sizeof(int) );
	CriticalSection( 3 );
	*_pInfo = _n;
	_pData = DLL_CACHE_ADDR_DATA( _pInfo );
	memset( _pData, 0, _n );
	return _pData;
}

void* dll_cache_free( void* _pData, int _iFlush )
{
	if( _pData )
	{
		memset( _pData, 0, *DLL_CACHE_ADDR_INFO( _pData ) );
		if( !_iFlush ) { return _pData; }
		CriticalSection( 2 );
			free( DLL_CACHE_ADDR_INFO( _pData ) );
		CriticalSection( 3 );
	}
	return 0;
}

/*****************************************/

#define CLEANUP_UD (1 << 0)
#define CLEANUP_OP (1 << 1) 
#define CLEANUP_CURVE (1 << 2)
#define CLEANUP_TURBINE (1 << 3)
#define CLEANUP_DLL (1 << 4)
#define CLEANUP_ALL 0xFF

void DllCleanup( UINT32 uCode )
{
	if( uCode & CLEANUP_UD ) { ud_cleanup(); }
	if( uCode & CLEANUP_OP ) { op_cleanup(); }
	if( uCode & CLEANUP_CURVE ) { curve_cleanup(); }
	if( uCode & CLEANUP_TURBINE ) { turbine_cleanup(); }
	if( uCode & CLEANUP_DLL ) { dll_freeallblocks(); }
}

BOOL WINAPI DllMain(HANDLE hInstance, ULONG dwReason, LPVOID lpReserved)
{
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:
//		_set_SSE2_enable(1);
		//
		CriticalSection( 0 );
		g_hInst = hInstance;
		dp_malloc_control( 1 );
		//
		if( !gszInfo )
		{
			CriticalSection( 2 );
				gszInfo = malloc( strlen( ADDIN_NAME " " HDP_VNUM "/" DP_VNUM " " BUILDTYPE ) + 1 );
			CriticalSection( 3 );
			gszInfo[ 0 ] = 0;
			strcat( gszInfo, ADDIN_NAME " " HDP_VNUM "/" DP_VNUM " " BUILDTYPE );
		}
		break;
	case DLL_PROCESS_DETACH:
		CriticalSection( 1 );
		dp_malloc_control( 2 );
		DllCleanup( CLEANUP_ALL );
		//
		CriticalSection( 2 );
			free( gszInfo );
		CriticalSection( 3 );
		gszInfo = 0;
		//
		gpCurveData = dll_cache_free( gpCurveData, 1 );
		break;
	case DLL_THREAD_ATTACH:
		break;
	case DLL_THREAD_DETACH:
		break;
	default:
		break;
	}
	return TRUE;
}

DLLEXPORT int WINAPI xlAutoOpen(void)
{
	static XLOPER xDLL;
	LPSTR* pszArg;
	int xlret;

	xlret = Excel4(xlGetName, &xDLL, 0);
	ASSERT( xlret == xlretSuccess );
	interpret_xlret( xlret );

    for( pszArg = rgFuncs; *pszArg != 0; pszArg += 10 )
	{
		static XLOPER xoper[10];
		int i;

		for( i = 0; i < 10; i++ )
		{
#ifdef ENABLE_PROPERREGISTERARGS
			if( i == 4 )
			{
				// pxMacroType is of xltypeNum or xltypeInt type
				xloper_int( &(xoper[i]), *((char*)(pszArg[i])) - '0' );
			}
			else
#endif // ENABLE_PROPERREGISTERARGS
			{
				xloper_str( &(xoper[i]), pszArg[i] );
			}
		}

		xlret = Excel4(
			xlfRegister, 0, 1 + 10,
			(LPXLOPER)&xDLL,
			(LPXLOPER)&(xoper[0]), (LPXLOPER)&(xoper[1]), (LPXLOPER)&(xoper[2]),
			(LPXLOPER)&(xoper[3]), (LPXLOPER)&(xoper[4]), (LPXLOPER)&(xoper[5]),
			(LPXLOPER)&(xoper[6]), (LPXLOPER)&(xoper[7]), (LPXLOPER)&(xoper[8]),
			(LPXLOPER)&(xoper[9])
		);
		ASSERT( xlret == xlretSuccess );
		interpret_xlret( xlret );

		for( i = 0; i < 10; i++ )
		{
#ifdef ENABLE_PROPERREGISTERARGS
			if( i == 4 )
			{
				// pxMacroType is of xltypeNum or xltypeInt type
			}
			else
#endif // ENABLE_PROPERREGISTERARGS
			{
				xloper_free( &(xoper[i]) );
			}
		}
	}

	/* Free the XLL filename?? */
	xlret = Excel4(xlFree, 0, 1, (LPXLOPER)&xDLL);
	ASSERT( xlret == xlretSuccess );
	interpret_xlret( xlret );

	return 1;
}

DLLEXPORT int WINAPI xlAutoClose(void)
{
	LPSTR* pszArg;

    for( pszArg = rgFuncs; *pszArg != 0; pszArg += 10 )
	{
		static XLOPER xFUNC;
		int xlret;

		xloper_init( &xFUNC );
		xloper_str( &xFUNC, pszArg[2] ); // leaks?
		xlret = Excel4( xlfSetName, 0, 1, (LPXLOPER)&xFUNC );
		ASSERT( xlret == xlretSuccess );
		interpret_xlret( xlret );
	}

	return 1;
}

DLLEXPORT int WINAPI xlAutoAdd(void)
{
#if 0
	LPXLOPER pxText;
	CHAR szBuf[1024];
	int xlret;

	_snprintf( (LPSTR)szBuf, 1023, ADDIN_NAME ": " ADDIN_DESCRIPTION "\n" ADDIN_COPYRIGHT "\n" ADDIN_VERSION "\n" ADDIN_DATE );

	pxText = xloper_str( xloper_malloc(), szBuf );
	xlret = Excel4( xlcAlert, 0, 1, pxText );
	ASSERT( xlret == xlretSuccess );
	interpret_xlret( xlret );
	xloper_free( pxText );
#else
	static XLOPER xlStr;
	int xlret;
	xloper_init( &xlStr );
	xloper_str( &xlStr, ADDIN_NAME ": " ADDIN_DESCRIPTION "\n" ADDIN_COPYRIGHT "\n" ADDIN_VERSION "\n" ADDIN_DATE );
	xlret = Excel4( xlcAlert, 0, 1, &xlStr );
	ASSERT( xlret == xlretSuccess );
	interpret_xlret( xlret );
	xloper_free( &xlStr );
#endif
	curve_register_builtins();

	return 1;
}

DLLEXPORT int WINAPI xlAutoRemove(void)
{
/*
	static XLOPER xText;
	int xlret;
	// kinda verbose - don't really need
	xloper_str( &xText, ADDIN_NAME " removed" );
	xlret = Excel4( xlcAlert, 0, 1, &xText );
	ASSERT( xlret == xlretSuccess );
	interpret_xlret( xlret );
	xloper_free( &xText );
*/
	return 1;
}

DLLEXPORT LPXLOPER WINAPI xlAddInManagerInfo(LPXLOPER pxAction)
{
#if 0
	XLOPER xIntVal;
	XLOPER xIntTypeRequest;
	int xlret;

	xloper_int( &xIntTypeRequest, xltypeInt );
	xlret = Excel4(xlCoerce, &xIntVal, 2, pxAction, &xIntTypeRequest );
	ASSERT( xlret == xlretSuccess );
	interpret_xlret( xlret );
	if(xIntVal.val.w == 1)
#else
	// hack courtesy of
	// Financial applications using Excel add-in development in C/C++ By Steve Dalton
	if(
		(pxAction->xltype == xltypeNum && pxAction->val.num == 1.0 )
		||
		(pxAction->xltype == xltypeInt && pxAction->val.w == 1 )
	)
#endif
	{
		// show name and version in addinmanager dialog box
		return xloper_str( xloper_malloc(), gszInfo );
	}
	return xloper_err( xloper_malloc() );
}

DLLEXPORT void WINAPI xlAutoFree(LPXLOPER pxFree)
{
	xloper_free( pxFree );
	return;
}

/*********************************************************************/

LPXLOPER privateParse(
	FP* pfpCurves, // K
	/* 2 rows per curve:
	 * 1st row is % capacity
	 * 2nd row is % efficiency
	 */

	FP* pfpUnits, // K
	/* 1 row per unit:
	 * 1st column is curve number from pfpCurves table
	 * 2nd column is design head
	 * 3rd column is design discharge
	 * 4th column is turbine kw output
	 * 5th column is unit headloss
	 * 6th column is generator efficiency
	 * 7th column is generator capacity (%) WRT turbine kw output
	 */

	XL_FLOAT_TYPE fHead, // B
	/* current head
	 */

	UINT16 uSteps, // H
	/* requested number of steps in the objective function matrix
	   figure may be rounded up to improve solution depending upon:
	   - capacity of units in relation to plant
	   - differing capacities of units
	   - difference of head from design head of units
	 */

	LPXLOPER pxOptionalWeights, // P

	LPXLOPER pxOptionalUnits // a
	/* optional boolean
	 * if true sets metric mode, default is imperial
	 */
)
{
	int xlret;
	UINT16 uCurves;

	//////////////////////////////////
	// simple validation

	/* odd number of curve rows */
	if( pfpCurves->rows & 0x01 ) { goto cleanup_and_exit; }

	/* mismatch in number of weights and number of units */
	if( !xloper_ismissing( pxOptionalWeights ) && (pfpUnits->rows != pxOptionalWeights->val.array.rows) ) { goto cleanup_and_exit; }

	/* impractical H/K resolution */
	if( uSteps < 5 || uSteps > MAX_DP_RESOLUTION ) { goto cleanup_and_exit; }

	/* bad head */
	if( fHead < 1.0 ) { goto cleanup_and_exit; }

	//////////////////////////////////
	// parse scalar args

	gfConvFactor = (DP_FLOAT_TYPE)DP_CONV_IMPERIAL;
	gfHeadCurr = (DP_FLOAT_TYPE)fHead;
	guUserStepCount = uSteps;

	if( !xloper_ismissing( pxOptionalUnits ) )
	{
		XLOPER xBoolVal;
		XLOPER xIntTypeRequest;

		xloper_int( &xIntTypeRequest, xltypeBool );
		xlret = Excel4(xlCoerce, &xBoolVal, 2, pxOptionalUnits, &xIntTypeRequest );
		ASSERT( xlret == xlretSuccess );
		interpret_xlret( xlret );
		if( xBoolVal.val.bool )
		{
			gfConvFactor = (DP_FLOAT_TYPE)DP_CONV_METRIC;
		}
	}

	//////////////////////////////////
	// parse curve matrix

	uCurves = pfpCurves->rows / 2;
	{
		UINT16 uCurve;
		UINT16 uPoints;
		UINT16 uMaxPoints = pfpCurves->columns;
		DP_FLOAT_TYPE fTol = (DP_FLOAT_TYPE)1E-6;

		// invalid column count
		if( uMaxPoints < 2 ) { goto cleanup_and_exit; }

		// check for blank rows... (they sum to zero)
		for( uCurve = 0; uCurve < uCurves; uCurve++ )
		{
			XL_FLOAT_TYPE fSum = 0;
			for( uPoints = 0; uPoints < uMaxPoints; uPoints++ )
			{
				fSum += pfpCurves->array[ 2 * uCurve * uMaxPoints + uPoints ];
			}
			if( fSum <= fTol ) { goto cleanup_and_exit; } // invalid (empty line?)
		}

		gpCurveData = dll_cache_alloc( (void*)gpCurveData, sizeof( DP_FLOAT_TYPE ) * uCurves * 2 * uMaxPoints );
		if( !gpCurveData ) { goto cleanup_and_exit; }

		for( uCurve = 0; uCurve < uCurves; uCurve++ )
		{
			// copy curve data into new memory
			for( uPoints = 0; uPoints < uMaxPoints; uPoints++ )
			{
				DP_FLOAT_TYPE fCap = (DP_FLOAT_TYPE)(pfpCurves->array[ 2 * uCurve * uMaxPoints + uPoints ]);
				DP_FLOAT_TYPE fEff = (DP_FLOAT_TYPE)(pfpCurves->array[ ( 2 * uCurve + 1 ) * uMaxPoints + uPoints  ]);
				if( fCap <= fTol && fEff <= fTol ) { break; }
				gpCurveData[ (2 * uCurve + 0) * uMaxPoints + uPoints ] = fCap;
				gpCurveData[ (2 * uCurve + 1) * uMaxPoints + uPoints ] = fEff;
			}

			// throw curve data at dp
			curve_register_n(
				uPoints,
				&(gpCurveData[ (2 * uCurve + 0) * uMaxPoints ]),
				&(gpCurveData[ (2 * uCurve + 0) * uMaxPoints ]),
				&(gpCurveData[ (2 * uCurve + 1) * uMaxPoints ]) /* peff */
			);
		}
	}

	//////////////////////////////////
	// parse unit matrix

	{
		DP_FLOAT_TYPE fPlantDischarge = 0;
		UINT16 uUnit;
		UINT16 uCols = pfpUnits->columns;
		UINT16 uStages = pfpUnits->rows;
		if( uStages > 255 ) { goto cleanup_and_exit; } // impractical!
		if( uCols < 4 ) { goto cleanup_and_exit; } // must have these cols at least
		for( uUnit = 0; uUnit < uStages; uUnit++ )
		{
			/* non-optional arg: mandatory columns */
			UINT32 uCurveNumber = (UINT32)(pfpUnits->array[ uUnit * uCols + 0 ]);
			if( uCurveNumber > uCurves ) { goto cleanup_and_exit; } // too many curves
			{
				UINT32 uUnitNumber = turbine_register_n(
					uCurveNumber - 1, // zero based
					(DP_FLOAT_TYPE)pfpUnits->array[ uUnit * uCols + 1 ],
					(DP_FLOAT_TYPE)pfpUnits->array[ uUnit * uCols + 2 ],
					(DP_FLOAT_TYPE)pfpUnits->array[ uUnit * uCols + 3 ]
				);

				/* non-optional arg: optional columns */
				if( uCols > 4 ) { turbine_headloss( uUnitNumber, (DP_FLOAT_TYPE)pfpUnits->array[ uUnit * uCols + 4 ] ); }
				if( uCols > 5 ) { turbine_geneff( uUnitNumber, (DP_FLOAT_TYPE)pfpUnits->array[ uUnit * uCols + 5 ] ); }
				if( uCols > 6 ) { turbine_gencap( uUnitNumber, (DP_FLOAT_TYPE)pfpUnits->array[ uUnit * uCols + 6 ] ); }

				/* optional args */
				if( !xloper_ismissing( pxOptionalWeights ) )
				{
					LPXLOPER pxWeights = pxOptionalWeights->val.array.lparray;
					turbine_weight( uUnitNumber, (DP_FLOAT_TYPE)pxWeights[ uUnit ].val.num );
				}
			}
		}
	}

	return 0;

	//////////////////////////////////

cleanup_and_exit:
	return xloper_err( xloper_malloc() );
}

LPXLOPER privateParse2(
	FP* pfpCurves, // K
	/* 2 rows per curve:
	 * 1st row is % capacity
	 * 2nd row is % efficiency
	 */

	FP* pfpUnits, // K
	/* 1 row per unit:
	 * 1st column is turbine curve number from pfpCurves table
	 * 2nd column is design head
	 * 3rd column is design discharge
	 * 4th column is turbine kw output
	 * 5th column is unit headloss
	 * 6th column is generator curve number from pfpCurves table
	 * 7th column is generator capacity (%) WRT turbine kw output (optional)
	 */

	XL_FLOAT_TYPE fHead, // B
	/* current head
	 */

	UINT16 uSteps, // H
	/* requested number of steps in the objective function matrix
	   figure may be rounded up to improve solution depending upon:
	   - capacity of units in relation to plant
	   - differing capacities of units
	   - difference of head from design head of units
	 */

	LPXLOPER pxOptionalGenCaps, // P

	LPXLOPER pxOptionalWeights, // P

	LPXLOPER pxOptionalUnits // a
	/* optional boolean
	 * if true sets metric mode, default is imperial
	 */
)
{
	int xlret;
	UINT16 uCurves;

	//////////////////////////////////
	// simple validations

	/* odd number of curve rows */
	if( pfpCurves->rows & 0x01 ) { goto cleanup_and_exit; }

	/* mismatch in number of items and number of units */
	if( !xloper_ismissing( pxOptionalGenCaps ) && (pfpUnits->rows != pxOptionalGenCaps->val.array.rows) ) { goto cleanup_and_exit; }
	if( !xloper_ismissing( pxOptionalWeights ) && (pfpUnits->rows != pxOptionalWeights->val.array.rows) ) { goto cleanup_and_exit; }

	/* impractical H/K resolution */
	if( uSteps < 5 || uSteps > MAX_DP_RESOLUTION ) { goto cleanup_and_exit; }

	/* bad head */
	if( fHead < 1.0 ) { goto cleanup_and_exit; }

	//////////////////////////////////
	// parse scalar args

	gfConvFactor = (DP_FLOAT_TYPE)DP_CONV_IMPERIAL;
	gfHeadCurr = (DP_FLOAT_TYPE)fHead;
	guUserStepCount = uSteps;

	if( !xloper_ismissing( pxOptionalUnits ) )
	{
		XLOPER xBoolVal;
		XLOPER xIntTypeRequest;

		xloper_init( &xIntTypeRequest );
		xloper_int( &xIntTypeRequest, xltypeBool );
		xlret = Excel4(xlCoerce, &xBoolVal, 2, pxOptionalUnits, &xIntTypeRequest );
		ASSERT( xlret == xlretSuccess );
		interpret_xlret( xlret );
		if( xBoolVal.val.bool )
		{
			gfConvFactor = (DP_FLOAT_TYPE)DP_CONV_METRIC;
		}
	}

	//////////////////////////////////
	// parse curve matrix

	uCurves = pfpCurves->rows / 2;
	{
		UINT16 uCurve;
		UINT16 uPoints;
		UINT16 uMaxPoints = pfpCurves->columns;
		DP_FLOAT_TYPE fTol = (DP_FLOAT_TYPE)1E-6;

		// invalid column count
		if( uMaxPoints < 2 ) { goto cleanup_and_exit; }

		// check for blank rows... (they sum to zero)
		for( uCurve = 0; uCurve < uCurves; uCurve++ )
		{
			XL_FLOAT_TYPE fSum = 0;
			for( uPoints = 0; uPoints < uMaxPoints; uPoints++ )
			{
				fSum += pfpCurves->array[ 2 * uCurve * uMaxPoints + uPoints ];
			}
			if( fSum <= fTol ) { goto cleanup_and_exit; } // invalid (empty line?)
		}

		gpCurveData = dll_cache_alloc( (void*)gpCurveData, sizeof( DP_FLOAT_TYPE ) * uCurves * 2 * uMaxPoints );
		if( !gpCurveData ) { goto cleanup_and_exit; }

		for( uCurve = 0; uCurve < uCurves; uCurve++ )
		{
			// copy curve data into new memory
			for( uPoints = 0; uPoints < uMaxPoints; uPoints++ )
			{
				DP_FLOAT_TYPE fCap = (DP_FLOAT_TYPE)(pfpCurves->array[ 2 * uCurve * uMaxPoints + uPoints ]);
				DP_FLOAT_TYPE fEff = (DP_FLOAT_TYPE)(pfpCurves->array[ ( 2 * uCurve + 1 ) * uMaxPoints + uPoints  ]);
				if( fCap <= fTol && fEff <= fTol ) { break; }
				gpCurveData[ (2 * uCurve + 0) * uMaxPoints + uPoints ] = fCap;
				gpCurveData[ (2 * uCurve + 1) * uMaxPoints + uPoints ] = fEff;
			}

			// throw curve data at dp
			curve_register_n(
				uPoints,
				&(gpCurveData[ (2 * uCurve + 0) * uMaxPoints ]),
				&(gpCurveData[ (2 * uCurve + 0) * uMaxPoints ]),
				&(gpCurveData[ (2 * uCurve + 1) * uMaxPoints ]) /* peff */
			);
		}
	}

	//////////////////////////////////
	// parse unit matrix

	{
		DP_FLOAT_TYPE fPlantDischarge = 0;
		UINT16 uUnit;
		UINT16 uCols = pfpUnits->columns;
		UINT16 uStages = pfpUnits->rows;
		if( uStages > 255 ) { goto cleanup_and_exit; } // impractical!
		if( uCols < 4 ) { goto cleanup_and_exit; } // must have these cols at least
		for( uUnit = 0; uUnit < uStages; uUnit++ )
		{
			/* non-optional arg: mandatory columns */
			UINT32 uCurveNumber = (UINT32)(pfpUnits->array[ uUnit * uCols + 0 ]);
			if( uCurveNumber > uCurves ) { goto cleanup_and_exit; } // too many curves
			{
				UINT32 uUnitNumber = turbine_register_n(
					uCurveNumber - 1, // zero based
					(DP_FLOAT_TYPE)pfpUnits->array[ uUnit * uCols + 1 ],
					(DP_FLOAT_TYPE)pfpUnits->array[ uUnit * uCols + 2 ],
					(DP_FLOAT_TYPE)pfpUnits->array[ uUnit * uCols + 3 ]
				);

				/* non-optional arg: optional columns */
				if( uCols > 4 ) { turbine_headloss( uUnitNumber, (DP_FLOAT_TYPE)pfpUnits->array[ uUnit * uCols + 4 ] ); }
				if( uCols > 5 )
				{
					UINT32 uGenCurve = (UINT32)pfpUnits->array[ uUnit * uCols + 5 ];
					if( uGenCurve > uCurves ) { goto cleanup_and_exit; } // too many curves
					turbine_gencurve( uUnitNumber, uGenCurve - 1 ); /* zero based */
				}
				if( uCols > 6 ) { turbine_gencap( uUnitNumber, (DP_FLOAT_TYPE)pfpUnits->array[ uUnit * uCols + 6 ] ); }

				/* optional args */
				if( !xloper_ismissing( pxOptionalWeights ) )
				{
					LPXLOPER pxWeights = pxOptionalWeights->val.array.lparray;
					turbine_weight( uUnitNumber, (DP_FLOAT_TYPE)pxWeights[ uUnit ].val.num );
				}
				if( !xloper_ismissing( pxOptionalGenCaps ) )
				{
					LPXLOPER pxGenCaps = pxOptionalGenCaps->val.array.lparray;
					turbine_gencap( uUnitNumber, (DP_FLOAT_TYPE)pxGenCaps[ uUnit ].val.num );
				}
			}
		}
	}

	return 0;

	//////////////////////////////////

cleanup_and_exit:
	return xloper_err( xloper_malloc() );
}


LPXLOPER privateSolve()
{
	//////////////////////////////////
	// run

	ex_clear();
	dp_cleanup();
	if( ex_didFail() ) { goto cleanup_and_exit; }
	dp_assign_weights();
	if( ex_didFail() ) { goto cleanup_and_exit; }
	dp_resize();
	if( guStates > MAX_EXCEL_ROWS ) { goto cleanup_and_exit; }
	if( ex_didFail() ) { goto cleanup_and_exit; }
	dp_malloc();
	if( ex_didFail() ) { goto cleanup_and_exit; }
	dp();
	if( ex_didFail() ) { goto cleanup_and_exit; }

	return 0;

	//////////////////////////////////

cleanup_and_exit:
	return xloper_err( xloper_malloc() );
}

LPXLOPER privatePackageAllResults()
{
	LPXLOPER pxReturn, pxReturnData;

	//////////////////////////////////
	// fill return array

	{
		UINT16 uBlock0, uBlock1, uBlock2, uCols, uStates;
		UINT32 uCells;

		// 3 blocks: solution, other solution and H/K solution
		uBlock0 = 0;
		uBlock1 = (UINT16)guStages + 1; // +1 for summary column
		uBlock2 = uBlock1 * 2;
		uCols = uBlock1 * 3;

		uStates = (unsigned short)guStates;
		uCells = uCols * uStates; // use UINT32 to handle wraparound

		pxReturn = xloper_malloc_array( 1 + uCells ); // + 1 for root
		xloper_multi( pxReturn, uStates, uCols, &(pxReturn[1]) );
		pxReturnData = &(pxReturn[1]);

		{
			UINT16 uStage, uState;
			for( uState = 0; uState < uStates; uState++ )
			{
				xloper_num( &(pxReturnData[ uState * uCols + uBlock0 ]), (XL_FLOAT_TYPE) gpfSolutionAllocations[ uState ] );
				xloper_num( &(pxReturnData[ uState * uCols + uBlock1 ]), (XL_FLOAT_TYPE) gpfOtherSolutionAllocations[ uState ] );
				xloper_num( &(pxReturnData[ uState * uCols + uBlock2 ]), (XL_FLOAT_TYPE) gpfHKSolutionAverages[ uState ] );

				for( uStage = 0; uStage < guStages; uStage++ )
				{
					UINT32 uSRC, uDST;

					uDST = uState * uCols + uStage; // row major
					uSRC = uStage * uStates + uState; // column major

					// allways add + 1 for the block summary column
					xloper_num( &(pxReturnData[ uDST + (uBlock0 + 1) ]), (XL_FLOAT_TYPE) gpfSolution[ uSRC ] );
					xloper_num( &(pxReturnData[ uDST + (uBlock1 + 1) ]), (XL_FLOAT_TYPE) gpfOtherSolution[ uSRC ] );
					xloper_num( &(pxReturnData[ uDST + (uBlock2 + 1) ]), (XL_FLOAT_TYPE) gpfHKSolution[ uSRC ] );
				}
			}
		}
	}

	return pxReturn;
}

LPXLOPER privatePackageHKResult(
	XL_FLOAT_TYPE fValue // B
	/* return solution for specific value
	 */
)
{
	XL_FLOAT_TYPE fTol = 1e-6;

	//////////////////////////////////
	// simple validation

	/* bad Q */
	if( fValue < 0 ) { goto cleanup_and_exit; }

	//////////////////////////////////
	// call dispatch

	gfDispatch = (DP_FLOAT_TYPE) fValue;

	ud_malloc();
	ud_dispatch();

	if( ex_didFail() ) { goto cleanup_and_exit; }

	//////////////////////////////////

	{
		XL_FLOAT_TYPE fSumSoln = 0;
		XL_FLOAT_TYPE fSumOtherSoln = 0;
		UINT16 uStage;
		for( uStage = 0; uStage < guStages; uStage++ )
		{
			fSumSoln += (XL_FLOAT_TYPE) gpfUDSolution[ uStage ];
			fSumOtherSoln += (XL_FLOAT_TYPE) gpfUDOtherSolution[ uStage ];
		}

		if( fSumOtherSoln <= fTol || fSumSoln <= fTol ) {
			fSumSoln = 0;
		} else {
			fSumSoln = ( fSumOtherSoln > fSumSoln ) ? fSumOtherSoln / fSumSoln : fSumSoln / fSumOtherSoln; // hack?
		}

		return xloper_num( xloper_malloc(), fSumSoln );
	}

	//////////////////////////////////

cleanup_and_exit:
	return xloper_err( xloper_malloc() );
}

LPXLOPER privatePackageSpecificResult(
	XL_FLOAT_TYPE fValue // B
	/* return solution for specific value
	 */
)
{
	LPXLOPER pxReturn, pxReturnData;
	XL_FLOAT_TYPE fTol = 1e-6;

	//////////////////////////////////
	// simple validation

	/* bad Q */
	if( fValue < 0 ) { goto cleanup_and_exit; }

	//////////////////////////////////
	// call dispatch

	gfDispatch = (DP_FLOAT_TYPE) fValue;

	ud_malloc();
	ud_dispatch();

	if( ex_didFail() ) { goto cleanup_and_exit; }

	//////////////////////////////////
	// fill return array

	{
		UINT16 uBlock0, uBlock1, uBlock2, uCols, uRows;
		UINT32 uCells;
		// 3 blocks: solution, other solution and H/K solution
		uRows = 1;
		uBlock0 = 0;
		uBlock1 = (UINT16)guStages + 1; // +1 for summary column
		uBlock2 = uBlock1 * 2;
		uCells = uCols = uBlock1 * 3;

		pxReturn = xloper_malloc_array( 1 + uCells ); // + 1 for root
		xloper_multi( pxReturn, uRows, uCols, &(pxReturn[1]) );
		pxReturnData = &(pxReturn[1]);

		{
			XL_FLOAT_TYPE fSumSoln = 0;
			XL_FLOAT_TYPE fSumOtherSoln = 0;
			UINT16 uStage;
			for( uStage = 0; uStage < guStages; uStage++ )
			{
				// allways add + 1 for the block summary column
				xloper_num( &(pxReturnData[ uStage + (uBlock0 + 1) ]), (XL_FLOAT_TYPE) gpfUDSolution[ uStage ] );
				xloper_num( &(pxReturnData[ uStage + (uBlock1 + 1) ]), (XL_FLOAT_TYPE) gpfUDOtherSolution[ uStage ] );
				xloper_num( &(pxReturnData[ uStage + (uBlock2 + 1) ]), (XL_FLOAT_TYPE) gpfUDHKSolution[ uStage ] );
				//
				fSumSoln += (XL_FLOAT_TYPE) gpfUDSolution[ uStage ];
				fSumOtherSoln += (XL_FLOAT_TYPE) gpfUDOtherSolution[ uStage ];
			}

			xloper_num( &(pxReturnData[ uBlock0 ]), (XL_FLOAT_TYPE) fSumSoln );
			xloper_num( &(pxReturnData[ uBlock1 ]), (XL_FLOAT_TYPE) fSumOtherSoln );

			if( fSumOtherSoln <= fTol || fSumSoln <= fTol ) {
				fSumSoln = 0;
			} else {
				fSumSoln = ( fSumOtherSoln > fSumSoln ) ? fSumOtherSoln / fSumSoln : fSumSoln / fSumOtherSoln; // hack?
			}
			xloper_num( &(pxReturnData[ uBlock2 ]), fSumSoln );
		}
	}

	return pxReturn;

	//////////////////////////////////

cleanup_and_exit:
	return xloper_err( xloper_malloc() );
}

LPXLOPER privatePackageOPResult()
{
	LPXLOPER pxReturn, pxReturnData;

	//////////////////////////////////
	// fill return array

	pxReturn = xloper_malloc_array( 1 + 2 * (UINT16)guOPoints ); // + 1 for root
	xloper_multi( pxReturn, 1, 2 * (UINT16)guOPoints, &(pxReturn[1]) );
	pxReturnData = &(pxReturn[1]);

	{
		UINT16 u;
		for( u = 0; u < (UINT16)guOPoints; u++ )
		{
			xloper_num( &(pxReturnData[ 2 * u + 0 ]), (XL_FLOAT_TYPE) gpfOPCoefB[ u ] );
			xloper_num( &(pxReturnData[ 2 * u + 1 ]), (XL_FLOAT_TYPE) gpfOPCoefM[ u ] );
		}
	}

	return pxReturn;
}

/*********************************************************************/

#if 0

/* using xlcSelect calls, back into excel, may only be possible when WE are called from
   a toolbar/menu. This kind of use of the excel API doesnt appear to work from the
   worksheet calculation context... it appears. */

void SelectCells(int xStartRow, int xEndRow,int  xStartCol,int xEndCol)
{
	XLOPER xSheet;
	XLMREF arg1,arg2;
	Excel4(xlSheetId,&xSheet,0);
	xSheet.xltype=xltypeRef;	xSheet.val.mref.lpmref=(LPXLMREF)&arg1;
	arg1.reftbl[0].rwFirst=xStartRow;	arg1.reftbl[0].rwLast=xEndRow;
	arg1.reftbl[0].colFirst=xStartCol;	arg1.reftbl[0].colLast=xEndCol;
	arg1.count=1;
	Excel4(xlcSelect,0,1,(LPXLOPER)&xSheet);
	Excel4(xlFree,0,1,(LPXLOPER)&xSheet);
}

DLLEXPORT short WINAPI
	HydroTest( void )
{
	static XLOPER xlStr;
	xloper_init( &xlStr );
	xloper_str( &xlStr, ADDIN_NAME ": " ADDIN_DESCRIPTION "\n" ADDIN_COPYRIGHT "\n" ADDIN_VERSION );
	Excel4( xlcAlert, 0, 1, &xlStr );
	xloper_free( &xlStr );

	{
		XLOPER xlFQRef, xlValue, xlResult;
		XLMREF xlRef;
		int rc;

		rc = Excel4( xlSheetId, xloper_init( &xlFQRef ), 0 );
		xloper_fqref( &xlFQRef, &xlRef, 2, 2, 2, 2 );
		xloper_int( xloper_init( &xlValue ), 33 );
		rc = Excel4( xlSet, xloper_init( &xlResult ), 2, &xlFQRef, &xlValue );
		rc += 0;

//		Excel4( xlcSelect, 0, 1, (LPXLOPER)&xSheet );
//		Excel4( xlFree, 0, 1, (LPXLOPER)&xSheet );
	}
	return 1;
}

#endif // 0

DLLEXPORT short WINAPI
	HydroAbout( void )
{
	static XLOPER xlStr;
	int xlret;
	xloper_init( &xlStr );
	xloper_str( &xlStr, ADDIN_NAME ": " ADDIN_DESCRIPTION "\n" ADDIN_COPYRIGHT "\n" ADDIN_VERSION "\n" ADDIN_DATE );
	xlret = Excel4( xlcAlert, 0, 1, &xlStr );
	ASSERT( xlret == xlretSuccess );
	interpret_xlret( xlret );
	xloper_free( &xlStr );
	return 1;
}

DLLEXPORT LPXLOPER WINAPI
	HydroVersion( void )
{
	return xloper_str( xloper_malloc(), gszInfo );
}

DLLEXPORT LPXLOPER WINAPI
	HydroOptPTable(
		FP* pfpCurves, // K
		FP* pfpUnits, // K
		XL_FLOAT_TYPE fHead, // B
		UINT16 uSteps, // H
		LPXLOPER pxOptionalWeights, // P
		LPXLOPER pxOptionalUnits // P
	)
{
	LPXLOPER pxRC;
	guSolveMode = DP_SOLVE_POWER_USE_HK;
	pxRC = privateParse( pfpCurves, pfpUnits, fHead, uSteps, pxOptionalWeights, pxOptionalUnits );
	if( !pxRC || !xloper_iserr( pxRC ) )
	{
		pxRC = privateSolve();
		if( !pxRC || !xloper_iserr( pxRC ) )
		{
			pxRC = privatePackageAllResults();
		}
	}

	DllCleanup( CLEANUP_ALL );
	return pxRC;
}

DLLEXPORT LPXLOPER WINAPI
	HydroOptQTable(
		FP* pfpCurves, // K
		FP* pfpUnits, // K
		XL_FLOAT_TYPE fHead, // B
		UINT16 uSteps, // H
		LPXLOPER pxOptionalWeights, // P
		LPXLOPER pxOptionalUnits // P
	)
{
	LPXLOPER pxRC;
	guSolveMode = DP_SOLVE_FLOW_USE_HK;
	pxRC = privateParse( pfpCurves, pfpUnits, fHead, uSteps, pxOptionalWeights, pxOptionalUnits );
	if( !pxRC || !xloper_iserr( pxRC ) )
	{
		pxRC = privateSolve();
		if( !pxRC || !xloper_iserr( pxRC ) )
		{
			pxRC = privatePackageAllResults();
		}
	}

	DllCleanup( CLEANUP_ALL );
	return pxRC;
}

DLLEXPORT LPXLOPER WINAPI
	HydroOptimizeSpecificQ(
		FP* pfpCurves, // K
		FP* pfpUnits, // K
		XL_FLOAT_TYPE fHead, // B
		UINT16 uSteps, // H
		XL_FLOAT_TYPE fValue, // B
		LPXLOPER pxOptionalWeights, // P
		LPXLOPER pxOptionalUnits // P
	)
{
	LPXLOPER pxRC;
	guSolveMode = DP_SOLVE_FLOW_USE_HK;
	pxRC = privateParse( pfpCurves, pfpUnits, fHead, uSteps, pxOptionalWeights, pxOptionalUnits );
	if( !pxRC || !xloper_iserr( pxRC ) )
	{
		pxRC = privateSolve();
		if( !pxRC || !xloper_iserr( pxRC ) || !ex_didFail() )
		{
			pxRC = privatePackageSpecificResult( fValue );
		}
	}

	DllCleanup( CLEANUP_ALL );
	return pxRC;
}

DLLEXPORT LPXLOPER WINAPI
	HydroOptQValue(
		FP* pfpCurves, // K
		FP* pfpUnits, // K
		XL_FLOAT_TYPE fHead, // B
		UINT16 uSteps, // H
		XL_FLOAT_TYPE fValue, // B
		LPXLOPER pxOptionalWeights, // P
		LPXLOPER pxOptionalUnits // P
	)
{
	XLOPER xlMissing;
	LPXLOPER pxRC;
	guSolveMode = DP_SOLVE_FLOW_USE_HK;
	xloper_init( &xlMissing );
	xloper_missing( &xlMissing );
	pxRC = privateParse2( pfpCurves, pfpUnits, fHead, uSteps, &xlMissing, pxOptionalWeights, pxOptionalUnits );
	if( !pxRC || !xloper_iserr( pxRC ) )
	{
		pxRC = privateSolve();
		if( !pxRC || !xloper_iserr( pxRC ) || !ex_didFail() )
		{
			pxRC = privatePackageSpecificResult( fValue );
		}
	}

	DllCleanup( CLEANUP_ALL );
	return pxRC;
}

DLLEXPORT LPXLOPER WINAPI
	HydroOptQValue2(
		FP* pfpCurves, // K
		FP* pfpUnits, // K
		XL_FLOAT_TYPE fHead, // B
		UINT16 uSteps, // H
		XL_FLOAT_TYPE fValue, // B
		LPXLOPER pxOptionalGenCaps, // P
		LPXLOPER pxOptionalWeights, // P
		LPXLOPER pxOptionalUnits // P
	)
{
	LPXLOPER pxRC;
	guSolveMode = DP_SOLVE_FLOW_USE_HK;
	pxRC = privateParse2( pfpCurves, pfpUnits, fHead, uSteps, pxOptionalGenCaps, pxOptionalWeights, pxOptionalUnits );
	if( !pxRC || !xloper_iserr( pxRC ) )
	{
		pxRC = privateSolve();
		if( !pxRC || !xloper_iserr( pxRC ) || !ex_didFail() )
		{
			pxRC = privatePackageSpecificResult( fValue );
		}
	}

	DllCleanup( CLEANUP_ALL );
	return pxRC;
}

DLLEXPORT LPXLOPER WINAPI
	HydroOptPValue(
		FP* pfpCurves, // K
		FP* pfpUnits, // K
		XL_FLOAT_TYPE fHead, // B
		UINT16 uSteps, // H
		XL_FLOAT_TYPE fValue, // B
		LPXLOPER pxOptionalWeights, // P
		LPXLOPER pxOptionalUnits // P
	)
{
	XLOPER xlMissing;
	LPXLOPER pxRC;
	guSolveMode = DP_SOLVE_POWER_USE_HK;
	xloper_init( &xlMissing );
	xloper_missing( &xlMissing );
	pxRC = privateParse2( pfpCurves, pfpUnits, fHead, uSteps, &xlMissing, pxOptionalWeights, pxOptionalUnits );
	if( !pxRC || !xloper_iserr( pxRC ) )
	{
		pxRC = privateSolve();
		if( !pxRC || !xloper_iserr( pxRC ) || !ex_didFail() )
		{
			pxRC = privatePackageSpecificResult( fValue );
		}
	}

	DllCleanup( CLEANUP_ALL );
	return pxRC;
}

DLLEXPORT LPXLOPER WINAPI
	HydroOptPValue2(
		FP* pfpCurves, // K
		FP* pfpUnits, // K
		XL_FLOAT_TYPE fHead, // B
		UINT16 uSteps, // H
		XL_FLOAT_TYPE fValue, // B
		LPXLOPER pxOptionalGenCaps, // P
		LPXLOPER pxOptionalWeights, // P
		LPXLOPER pxOptionalUnits // P
	)
{
	LPXLOPER pxRC;
	guSolveMode = DP_SOLVE_POWER_USE_HK;
	pxRC = privateParse2( pfpCurves, pfpUnits, fHead, uSteps, pxOptionalGenCaps, pxOptionalWeights, pxOptionalUnits );
	if( !pxRC || !xloper_iserr( pxRC ) )
	{
		pxRC = privateSolve();
		if( !pxRC || !xloper_iserr( pxRC ) || !ex_didFail() )
		{
			pxRC = privatePackageSpecificResult( fValue );
		}
	}

	DllCleanup( CLEANUP_ALL );
	return pxRC;
}

DLLEXPORT LPXLOPER WINAPI
	HydroOptQValueHK(
		FP* pfpCurves, // K
		FP* pfpUnits, // K
		XL_FLOAT_TYPE fHead, // B
		UINT16 uSteps, // H
		XL_FLOAT_TYPE fValue, // B
		LPXLOPER pxOptionalWeights, // P
		LPXLOPER pxOptionalUnits // P
	)
{
	XLOPER xlMissing;
	LPXLOPER pxRC;
	guSolveMode = DP_SOLVE_FLOW_USE_HK;
	xloper_init( &xlMissing );
	xloper_missing( &xlMissing );
	pxRC = privateParse2( pfpCurves, pfpUnits, fHead, uSteps, &xlMissing, pxOptionalWeights, pxOptionalUnits );
	if( !pxRC || !xloper_iserr( pxRC ) )
	{
		pxRC = privateSolve();
		if( !pxRC || !xloper_iserr( pxRC ) || !ex_didFail() )
		{
			pxRC = privatePackageHKResult( fValue );
		}
	}

	DllCleanup( CLEANUP_ALL );
	return pxRC;
}

DLLEXPORT LPXLOPER WINAPI
	HydroOptQValueHK2(
		FP* pfpCurves, // K
		FP* pfpUnits, // K // without gen limits
		XL_FLOAT_TYPE fHead, // B
		UINT16 uSteps, // H
		XL_FLOAT_TYPE fValue, // B
		LPXLOPER pxOptionalGenCaps, // P
		LPXLOPER pxOptionalWeights, // P
		LPXLOPER pxOptionalUnits // P
	)
{
	LPXLOPER pxRC;
	guSolveMode = DP_SOLVE_FLOW_USE_HK;
	pxRC = privateParse2( pfpCurves, pfpUnits, fHead, uSteps, pxOptionalGenCaps, pxOptionalWeights, pxOptionalUnits );
	if( !pxRC || !xloper_iserr( pxRC ) )
	{
		pxRC = privateSolve();
		if( !pxRC || !xloper_iserr( pxRC ) || !ex_didFail() )
		{
			pxRC = privatePackageHKResult( fValue );
		}
	}

	DllCleanup( CLEANUP_ALL );
	return pxRC;
}

DLLEXPORT LPXLOPER WINAPI
	HydroOptQPoints(
		FP* pfpCurves, // K
		FP* pfpUnits, // K
		FP* pfpCapacities, // K
		XL_FLOAT_TYPE fHead1, // B
		XL_FLOAT_TYPE fHead2, // B
		UINT16 uSteps, // H
		LPXLOPER pxOptionalWeights, // P
		LPXLOPER pxOptionalUnits // P
	)
{
	LPXLOPER pxRC;
	DP_FLOAT_TYPE fOPCaps[20];
	UINT16 u;

	guSolveMode = DP_SOLVE_FLOW_USE_HK;

	for( u = 0; u < pfpCapacities->columns; u++ )
	{ fOPCaps[ u ] = (DP_FLOAT_TYPE)pfpCapacities->array[ u ]; }
	op_set_operating_capacities( pfpCapacities->columns, fOPCaps );
#if 0
	op_set_dependent( (DP_FLOAT_TYPE) fHead1 );
	pxRC = privateParse2( pfpCurves, pfpUnits, fHead1, uSteps, xloper_missing( &xlMissing ), pxOptionalWeights, pxOptionalUnits );
	if( !pxRC || !xloper_iserr( pxRC ) )
	{
		pxRC = privateSolve();
		if( !pxRC || !xloper_iserr( pxRC ) )
		{
			op_regress();
			DllCleanup( CLEANUP_ALL & ( ~CLEANUP_OP ) );
			op_set_dependent( (DP_FLOAT_TYPE) fHead2 );
			pxRC = privateParse( pfpCurves, pfpUnits, fHead2, uSteps, pxOptionalWeights, pxOptionalUnits );
			if( !pxRC || !xloper_iserr( pxRC ) )
			{
				pxRC = privateSolve();
				if( !pxRC || !xloper_iserr( pxRC ) )
				{
					op_regress();
					pxRC = privatePackageOPResult();
				}
			}
		}
	}
#else
	{
		XLOPER xlMissing;
		UINT32 uIter = 0;

		xloper_init( &xlMissing );
		xloper_missing( &xlMissing );
		for( uIter = 0; uIter < 2; uIter++ )
		{
			XL_FLOAT_TYPE fHeadTrial = (uIter == 0)? fHead1 : fHead2;

			op_set_dependent( (DP_FLOAT_TYPE) fHeadTrial );
			pxRC = privateParse2( pfpCurves, pfpUnits, fHeadTrial, uSteps, &xlMissing, pxOptionalWeights, pxOptionalUnits );
			if( !pxRC || !xloper_iserr( pxRC ) )
			{
				pxRC = privateSolve();
				if( !pxRC || !xloper_iserr( pxRC ) )
				{
					op_regress();
				} else { break; }
			} else { break; }
			if( uIter == 0 ) {
				DllCleanup( CLEANUP_ALL & ( ~CLEANUP_OP ) );
			} else {
				pxRC = privatePackageOPResult();
			}
		}
	}
#endif
	DllCleanup( CLEANUP_ALL );
	return pxRC;
}

DLLEXPORT LPXLOPER WINAPI
	HydroOptQPoints2(
		FP* pfpCurves, // K
		FP* pfpUnits, // K
		FP* pfpCapacities, // K
		XL_FLOAT_TYPE fHead1, // B
		XL_FLOAT_TYPE fHead2, // B
		UINT16 uSteps, // H
		LPXLOPER pxOptionalGenCaps, // P
		LPXLOPER pxOptionalWeights, // P
		LPXLOPER pxOptionalUnits // P
	)
{
	LPXLOPER pxRC;
	DP_FLOAT_TYPE fOPCaps[20];
	UINT16 u;

	guSolveMode = DP_SOLVE_FLOW_USE_HK;

	for( u = 0; u < pfpCapacities->columns; u++ )
	{ fOPCaps[ u ] = (DP_FLOAT_TYPE)pfpCapacities->array[ u ]; }
	op_set_operating_capacities( pfpCapacities->columns, fOPCaps );
	{
		UINT32 uIter = 0;

		for( uIter = 0; uIter < 2; uIter++ )
		{
			XL_FLOAT_TYPE fHeadTrial = (uIter == 0)? fHead1 : fHead2;

			op_set_dependent( (DP_FLOAT_TYPE) fHeadTrial );
			pxRC = privateParse2( pfpCurves, pfpUnits, fHeadTrial, uSteps, pxOptionalGenCaps, pxOptionalWeights, pxOptionalUnits );
			if( !pxRC || !xloper_iserr( pxRC ) )
			{
				pxRC = privateSolve();
				if( !pxRC || !xloper_iserr( pxRC ) )
				{
					op_regress();
				} else { break; }
			} else { break; }
			if( uIter == 0 ) {
				DllCleanup( CLEANUP_ALL & ( ~CLEANUP_OP ) );
			} else {
				pxRC = privatePackageOPResult();
			}
		}
	}
	DllCleanup( CLEANUP_ALL );
	return pxRC;
}

DLLEXPORT LPXLOPER WINAPI
	HydroOptimizeOPQ(
		FP* pfpCurves, // K
		FP* pfpUnits, // K
		FP* pfpCapacities, // K
		XL_FLOAT_TYPE fHead1, // B
		XL_FLOAT_TYPE fHead2, // B
		UINT16 uSteps, // H
		LPXLOPER pxOptionalWeights, // P
		LPXLOPER pxOptionalUnits // P
	)
{
	LPXLOPER pxRC;
	DP_FLOAT_TYPE fOPCaps[20];
	UINT16 u;

	guSolveMode = DP_SOLVE_FLOW_USE_HK;

	for( u = 0; u < pfpCapacities->columns; u++ )
	{ fOPCaps[ u ] = (DP_FLOAT_TYPE)pfpCapacities->array[ u ]; }
	op_set_operating_capacities( pfpCapacities->columns, fOPCaps );

	op_set_dependent( (DP_FLOAT_TYPE) fHead1 );
	pxRC = privateParse( pfpCurves, pfpUnits, fHead1, uSteps, pxOptionalWeights, pxOptionalUnits );
	if( !pxRC || !xloper_iserr( pxRC ) )
	{
		pxRC = privateSolve();
		if( !pxRC || !xloper_iserr( pxRC ) )
		{
			op_regress();
			DllCleanup( CLEANUP_ALL & ( ~CLEANUP_OP ) );
			op_set_dependent( (DP_FLOAT_TYPE) fHead2 );
			pxRC = privateParse( pfpCurves, pfpUnits, fHead2, uSteps, pxOptionalWeights, pxOptionalUnits );
			if( !pxRC || !xloper_iserr( pxRC ) )
			{
				pxRC = privateSolve();
				if( !pxRC || !xloper_iserr( pxRC ) )
				{
					op_regress();
					pxRC = privatePackageOPResult();
				}
			}
		}
	}

	DllCleanup( CLEANUP_ALL );
	return pxRC;
}

