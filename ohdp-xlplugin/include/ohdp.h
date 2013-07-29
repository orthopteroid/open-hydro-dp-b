#pragma once

#ifdef __cplusplus
	extern "C" {
#endif

#define DP_SOLVE_POWER_USE_HK (1)
#define DP_SOLVE_FLOW_USE_HK  (2)

#define DP_CONV_UNDEFINED (0)
#define DP_CONV_IMPERIAL  (62.4 /* POUNDSPERCUBICFT */ * 0.746 /* KWPERHP */ / 550 /* FTPOUNDSPERHP */ ) /* from cfs */
#define DP_CONV_METRIC    (1000.0 /* WATERDENSITYINKGPERM3 */ * 9.81 /* ACCELDUETOGRAVITY */ / 1000 /* WATTSPERKW */ ) /* from cms */

#define DP_WEIGHT_RELATIVE		(1<<30)
#define DP_WEIGHT_CODEMASK		((1<<16) - 1)
#define DP_WEIGHT_DEFAULT		(0)
#define DP_WEIGHT_EQUAL			(1)
#define DP_WEIGHT_MAXPOWER		(2) /* preference given to larger units */
#define DP_WEIGHT_MAXFLOW		(3)
#define DP_WEIGHT_MINPOWER		(4) /* preference given to smaller units */
#define DP_WEIGHT_MINFLOW		(5)

#define UINT32 unsigned long int

char* dp_gsVERSION;

UINT32 guUserStepCount;
UINT32 guProblemStepCount;
DP_FLOAT_TYPE gfConvFactor;
UINT32 guSolveMode;
UINT32 guWeightCode;
DP_FLOAT_TYPE gfHeadCurr;
UINT32 guStates;
UINT32 guStages;

void stdout_redirect( char* szOutfile );
void stdout_unredirect( void );

void curve_register_builtins( void );
UINT32 curve_register_n( UINT32 np, DP_FLOAT_TYPE* pCap, DP_FLOAT_TYPE* pPow, DP_FLOAT_TYPE* pEff );
UINT32 turbine_register_n( UINT32 uType, DP_FLOAT_TYPE h, DP_FLOAT_TYPE q, DP_FLOAT_TYPE p );
void turbine_weight( UINT32 uTurbine, DP_FLOAT_TYPE fWeight );
void turbine_headloss( UINT32 uTurbine, DP_FLOAT_TYPE fLossCoef );
void turbine_geneff( UINT32 uTurbine, DP_FLOAT_TYPE fGeneff );
void turbine_gencap( UINT32 uTurbine, DP_FLOAT_TYPE fGenCapacity );
void turbine_gencurve( UINT32 uTurbine, UINT32 uGenCurveNum );

void dp_malloc_control( UINT32 uControl ); /* 0=cache off, 1=cache on, 2=cache flush */
void dp_malloc( void );
void dp_cleanup( void );

void dp_assign_weights( void );
void dp_resize( void );
void dp( void );

void curve_cleanup( void );
void turbine_cleanup( void );

DP_FLOAT_TYPE* gpfSolutionAllocations;
DP_FLOAT_TYPE* gpfOtherSolutionAllocations;
DP_FLOAT_TYPE* gpfHKSolutionAverages;

DP_FLOAT_TYPE* gpfSolution;
DP_FLOAT_TYPE* gpfOtherSolution;
DP_FLOAT_TYPE* gpfHKSolution;

/***************/

UINT32 guOPoints;
DP_FLOAT_TYPE* gpfOPCoefM;
DP_FLOAT_TYPE* gpfOPCoefB;
void op_set_operating_capacities( UINT32 u, DP_FLOAT_TYPE* pCap );
void op_set_dependent( DP_FLOAT_TYPE fDep );
void op_regress( void );
void op_cleanup( void );

/***************/

DP_FLOAT_TYPE gfDispatch;
DP_FLOAT_TYPE* gpfUDSolution;
DP_FLOAT_TYPE* gpfUDOtherSolution;
DP_FLOAT_TYPE* gpfUDHKSolution;

void ud_malloc( void );
void ud_cleanup( void );
void ud_dispatch( void );

/***************/

void ex_clear();
void ex_set();
UINT32 ex_didFail();

#ifdef __cplusplus
	}
#endif
