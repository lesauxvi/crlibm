/* 
Beware to compile without optimizations

*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

#include "crlibm.h"
#include "crlibm_private.h"
#include "test_common.h"

#include "scs_lib/tests/tbx_timing.h"

#ifdef HAVE_MATHLIB_H
#include <MathLib.h>
#endif

#ifdef HAVE_LIBMCR_H
#include <libmcr.h>
#endif

#ifdef HAVE_MPFR_H
# include <gmp.h>
# include <mpfr.h>
# ifdef MPFR_VERSION
#  if MPFR_VERSION < MPFR_VERSION_NUM(2,2,0)
#   define mpfr_subnormalize(mp_res, inexact, mpfr_rnd_mode) 
#  endif
# else
#  define mpfr_get_version() "<2.1.0"
# endif
#endif


#define N1 20

#define TIMING_ITER 10000

#define DETAILED_REPORT 0

/* If set, the behaviour of the function with respect to cache memory
   will be tested*/
#define TEST_CACHE 0

/*
 * Rounding mode to test
 */




#if EVAL_PERF==1  
/* counter of calls to the second step */
extern int crlibm_second_step_taken; 
int crlibm_first_step_taken;
#endif

#ifdef HAVE_MPFR_H  
/* The rounding mode for mpfr function */
mp_rnd_t mpfr_rnd_mode;
#endif

/* Global variable for time stamping */
static unsigned long long tbx_time;

/* Unused random number generator*/
double (*randfun_soaktest) () = NULL;
/* The random number generator*/
double (*randfun)       () = NULL;
/* The function we test */
double (*testfun_crlibm)() = NULL;
/* The function we trust */
int    (*testfun_mpfr)  () = NULL;
/* The function to show off against for accuracy  */
double (*testfun_libm)  () = NULL;
/* The function to show off against for performance */
double (*testfun_libultim)   () = NULL;
/* The last competitor  */
double (*testfun_libmcr)   () = NULL;


/* TESTSIZE doubles should be enough to flush the cache */
#define TESTSIZE 200000
 
#if TEST_CACHE
static double inputs[TESTSIZE];
static double inputs2[TESTSIZE];
#endif /* TEST_CACHE */


/* indicate the number of argument taken by the function */
static int nbarg;          






static void usage(char *fct_name){
  /*fprintf (stderr, "\n%s: Performance test for crlibm and other mathematical libraries\n", fct_name);*/
  fprintf (stderr, "\nUsage: %s function (RN|RU|RD|RZ) iterations \n", fct_name);
  fprintf (stderr, " function      : name of function to test \n");
  fprintf (stderr, " (RN|RU|RD|RZ) : rounding mode, \n");
  fprintf (stderr, " iterations    : number of iterations, also seed for the random number generator \n");
  exit (EXIT_FAILURE);
}


#if TEST_CACHE

static void fill_and_flush(int seed, int args) {
  int i;
  srandom(seed);
  for (i=0; i<TESTSIZE; i++){
    if (args==1) {
      inputs[i] = randfun();
      inputs2[i] = randfun();
    } else {
      inputs[i] = (*((double (*)(double *))randfun))(&(inputs2[i]));
    }
  }
}

static void test_with_cache(const char *name, double (*testfun)(), int n, int args){
  int i, j, k;
  double i1, i2, rd;
  tbx_tick_t   t1, t2; 
  unsigned long long dt, min_dtsum, dtsum;

  if(testfun!=NULL) { /* test if some functions are missing */
    printf("\n%s\n",name);
    for(i=1; i<=10000; i*=10) { /* i=1,10,100...*/
      min_dtsum=1<<30; 
      for(k=0;k<10;k++) { /* do the whole test 10 times and take the min */
	fill_and_flush(n,args);
	dtsum=0;
	for (j=0; j<i; j++) {
	  i1 = inputs[i];
	  i2 = inputs2[i];

	  if (nbarg==1){
	    TBX_GET_TICK(t1);
	    rd = testfun(i1);
	    TBX_GET_TICK(t2);
	  }else{
	    TBX_GET_TICK(t1);
	    rd = testfun(i1, i2);
	    TBX_GET_TICK(t2);
	  }

	  dt = TBX_TICK_RAW_DIFF(t1, t2)-tbx_time; 
	  dtsum += dt;
	}
	if (dtsum < min_dtsum) min_dtsum=dtsum; 
      }
      printf("  %d loops: \t avg time = %f ticks\n",i, ((double)min_dtsum)/i);
    }
  }
}
#endif /* TEST_CACHE */

static void test_without_cache(const char *name, 
			double (*testfun)(), 
			double i1,
			double i2,
			unsigned long long *lib_dtmin,
			unsigned long long *lib_dtmax,
			unsigned long long *lib_dtsum,
			int func_type){
  double result;
  unsigned long long dt, dtmin;
  tbx_tick_t   t1, t2; 
  int j;
#ifdef TIMING_USES_GETTIMEOFDAY
  int k;
#endif 
#ifdef HAVE_MPFR_H  
  mpfr_t mp_res, mp_inpt;
  mpfr_t mp_inpt2; /* For the pow function */
  int inexact;

  mpfr_init2(mp_res,  53);
  mpfr_init2(mp_inpt, 53);
  mpfr_init2(mp_inpt2, 53);
#endif

  if(testfun!=NULL) { /* test if some functions are missing */
    dtmin=1<<30;
    /* take the min of N1 consecutive calls */
    for(j=0; j<N1; j++) {

      if (func_type == 0){              /* func_type = normal function */
	if (nbarg==1){
	  TBX_GET_TICK(t1);
#ifdef TIMING_USES_GETTIMEOFDAY /* use inaccurate timer, do many loops */
	  for(k=0; k<TIMING_ITER;k++)
#endif
	    result = testfun(i1);
	  TBX_GET_TICK(t2);
	}else{
	  TBX_GET_TICK(t1);
#ifdef TIMING_USES_GETTIMEOFDAY /* use inaccurate timer, do many loops */
	  for(k=0; k<TIMING_ITER;k++)
#endif
	    result = testfun(i1,i2);
	  TBX_GET_TICK(t2);	  
	}
      }else{                             /* func_type = MPFR function  */
#ifdef   HAVE_MPFR_H 
	if (nbarg==1){
	  TBX_GET_TICK(t1);
#ifdef TIMING_USES_GETTIMEOFDAY /* use inaccurate timer, do many loops */
	  for(k=0; k<TIMING_ITER;k++){
#endif    
	    mpfr_set_d(mp_inpt, i1, GMP_RNDN);
	    inexact = testfun(mp_res, mp_inpt, mpfr_rnd_mode);
            mpfr_subnormalize (mp_res, inexact, mpfr_rnd_mode);
	    result = mpfr_get_d(mp_res, GMP_RNDN);
#ifdef TIMING_USES_GETTIMEOFDAY /* use inaccurate timer, do many loops */
	  }
#endif
	  TBX_GET_TICK(t2);
	}else{
	  TBX_GET_TICK(t1);
#ifdef TIMING_USES_GETTIMEOFDAY /* use inaccurate timer, do many loops */
	  for(k=0; k<TIMING_ITER;k++){
#endif    
	    mpfr_set_d(mp_inpt, i1, GMP_RNDN);
	    mpfr_set_d(mp_inpt2, i2, GMP_RNDN);
	    inexact = testfun(mp_res, mp_inpt, mp_inpt2, mpfr_rnd_mode);
            mpfr_subnormalize (mp_res, inexact, mpfr_rnd_mode);
	    result = mpfr_get_d(mp_res, GMP_RNDN);
#ifdef TIMING_USES_GETTIMEOFDAY /* use inaccurate timer, do many loops */
	  }
#endif 
	  TBX_GET_TICK(t2);
	}
#endif /*HAVE_MPFR_H*/
      }

      dt = TBX_TICK_RAW_DIFF(t1, t2)-tbx_time;
      if (dt<dtmin)  dtmin=dt;
    }
    *lib_dtsum+=dtmin;
    if (dtmin<*lib_dtmin)  *lib_dtmin=dtmin;
    if (dtmin>*lib_dtmax)  *lib_dtmax=dtmin;
#if      DETAILED_REPORT
    printf("\n input=%1.15e\tT%s=%lld", i1, name, dtmin);
#endif /*DETAILED_REPORT*/
  }

  /* release memory */
#ifdef   HAVE_MPFR_H
  mpfr_clear(mp_inpt2);
  mpfr_clear(mp_inpt);
  mpfr_clear(mp_res);
#endif /*HAVE_MPFR_H*/

}




static void test_worst_case(double (*testfun)(), 
		     double i1, 
		     double i2, 
		     unsigned long long *lib_dtwc, 
		     int func_type){


  double res;
  tbx_tick_t   t1, t2; 
  unsigned long long dtmin, dt;
  int j;
#ifdef TIMING_USES_GETTIMEOFDAY
  int k;
#endif 
#ifdef HAVE_MPFR_H  
  mpfr_t mp_res, mp_inpt;
  mpfr_t mp_inpt2; /* For the pow function */
  int inexact;

  mpfr_init2(mp_res,  53);
  mpfr_init2(mp_inpt, 53);
  mpfr_init2(mp_inpt2, 53);
#endif

  if(testfun!=NULL) { /* test if some functions are missing  */
    dtmin=1<<30;
    for(j=0; j<N1; j++) {
      if (func_type == 0){    /* func_type = normal function */
	if (nbarg==1){
	  TBX_GET_TICK(t1);
#ifdef TIMING_USES_GETTIMEOFDAY
	  for(k=0; k<TIMING_ITER;k++)
#endif
	    res = testfun(i1);
	  TBX_GET_TICK(t2);
	}else{
	  TBX_GET_TICK(t1);
#ifdef TIMING_USES_GETTIMEOFDAY
	  for(k=0; k<TIMING_ITER;k++)
#endif
	    res = testfun(i1,i2);
	  TBX_GET_TICK(t2);
	}
      }else{                   /* func_type = MPFR function  */
#ifdef HAVE_MPFR_H 
	if (nbarg==1){
	  TBX_GET_TICK(t1);
#ifdef TIMING_USES_GETTIMEOFDAY
	  for(k=0; k<TIMING_ITER;k++){
#endif
	    mpfr_set_d (mp_inpt, i1, GMP_RNDN);
	    inexact = testfun (mp_res, mp_inpt, mpfr_rnd_mode);
            mpfr_subnormalize (mp_res, inexact, mpfr_rnd_mode);
	    res = mpfr_get_d (mp_res, GMP_RNDN);
#ifdef TIMING_USES_GETTIMEOFDAY
	  }
#endif
	  TBX_GET_TICK(t2);
	}else{
	  TBX_GET_TICK(t1);
#ifdef TIMING_USES_GETTIMEOFDAY
	  for(k=0; k<TIMING_ITER;k++){
#endif
	    mpfr_set_d (mp_inpt, i1, GMP_RNDN);
	    mpfr_set_d (mp_inpt2, i2, GMP_RNDN);
	    inexact = testfun (mp_res, mp_inpt, mp_inpt2, mpfr_rnd_mode);
            mpfr_subnormalize (mp_res, inexact, mpfr_rnd_mode);
	    res = mpfr_get_d (mp_res, GMP_RNDN);
#ifdef TIMING_USES_GETTIMEOFDAY
	  }
#endif
	  TBX_GET_TICK(t2);
	}
#endif /*HAVE_MPFR_H*/
      }
      dt = TBX_TICK_RAW_DIFF(t1, t2)-tbx_time; 
      if (dt<dtmin)  dtmin=dt;
    }
    *lib_dtwc = dtmin;
  }

  /* release memory */
#ifdef   HAVE_MPFR_H
  mpfr_clear(mp_inpt2);
  mpfr_clear(mp_inpt);
  mpfr_clear(mp_res);
#endif /*HAVE_MPFR_H*/
}



static void normal_output(const char *name,
		   double (*testfun)(),
		   unsigned long long lib_dtmin,
		   unsigned long long lib_dtmax,
		   unsigned long long lib_dtsum,
		   unsigned long long lib_dtwc,
		   int n){
  if(testfun!=NULL) { /* some functions are missing in libultim (cosh, ...  */
    printf("\n%s\nTmin = %lld ticks,\t Tmax = %lld ticks\t avg = %f\tT worst case = %lld\n",
	   name, lib_dtmin, lib_dtmax, ((double)lib_dtsum) / ((double) n), lib_dtwc);
  }
}

static void latex_output(const char *name,
		  double (*testfun)(),
		  unsigned long long lib_dtmin,
		  unsigned long long lib_dtmax,
		  unsigned long long lib_dtsum,
		  unsigned long long lib_dtwc,
		  int n){
    if(testfun!=NULL) { /* some functions are missing in libultim (cosh, ...  */
      if (lib_dtwc > lib_dtmax) lib_dtmax=lib_dtwc;
      printf(" %s  \t& %lld    \t& %10.0f   \t& %lld      \\\\ \n \\hline\n",  
	     name, lib_dtmin, ((double)lib_dtsum) / ((double) n), lib_dtmax);
    }
}



int main (int argc, char *argv[]){ 
  int i, j, n;
  double i1, i2;
  char* rounding_mode;
  char* function_name;
  double worstcase;
  tbx_tick_t   t1, t2; 
  unsigned long long 
    dt,
    libm_dtmin, libm_dtmax, libm_dtsum, libm_dtwc,
    crlibm_dtmin, crlibm_dtmax, crlibm_dtsum, crlibm_dtwc,
    mpfr_dtmin, mpfr_dtmax, mpfr_dtsum, mpfr_dtwc,
    libultim_dtmin, libultim_dtmax, libultim_dtsum, libultim_dtwc,
    libmcr_dtmin, libmcr_dtmax, libmcr_dtsum, libmcr_dtwc;
#ifdef   HAVE_MATHLIB_H
  short Original_Mode;
#endif


  if ((argc !=4)) usage(argv[0]);

  function_name = argv[1];
  rounding_mode = argv[2];
  sscanf(argv[3],"%d", &n);
    

#ifdef HAVE_MPFR_H  
  if      (strcmp(rounding_mode,"RU")==0) mpfr_rnd_mode = GMP_RNDU;
  else if (strcmp(rounding_mode,"RD")==0) mpfr_rnd_mode = GMP_RNDD;
  else if (strcmp(rounding_mode,"RZ")==0) mpfr_rnd_mode = GMP_RNDZ;
  else {
    mpfr_rnd_mode = GMP_RNDN; 
    rounding_mode="RN" ;
  }
#endif
    
  if (strcmp(function_name,"pow")==0) nbarg=2;
  else nbarg=1;

  crlibm_init();

  test_init(/* pointers to returned value */
	    &randfun, 
	    &randfun_soaktest, /* unused here */ 
	    &testfun_crlibm, 
	    &testfun_mpfr,
	    &testfun_libultim,
	    &testfun_libmcr,
	    &testfun_libm,
	    &worstcase,
	    /* arguments */
	    function_name,
	    rounding_mode ) ;
  


  crlibm_dtmin=1<<30; crlibm_dtmax=0; crlibm_dtsum=0; crlibm_dtwc=0;
  libm_dtmin=1<<30;   libm_dtmax=0;   libm_dtsum=0; libm_dtwc=0;
  libultim_dtmin=1<<30;    libultim_dtmax=0;    libultim_dtsum=0; libultim_dtwc=0;
  libmcr_dtmin=1<<30;    libmcr_dtmax=0;    libmcr_dtsum=0; libmcr_dtwc=0;
  mpfr_dtmin=1<<30;   mpfr_dtmax=0;   mpfr_dtsum=0; mpfr_dtwc=0;

  
  /* take the min of N1 consecutive calls */
  tbx_time=1<<30;
  for(j=0; j<1000*N1; j++) {
    TBX_GET_TICK(t1);
    TBX_GET_TICK(t2);
    dt = TBX_TICK_RAW_DIFF(t1, t2);
    if(dt<tbx_time) tbx_time = dt;
  }
#if HAVE_MPFR_H
  printf ("GMP version %s MPFR version %s ",
          gmp_version, mpfr_get_version ());
#endif
  printf("tbx_time=%llu\n", tbx_time);

#if TEST_CACHE
  /************  TESTS WITH CACHES  *********************/
  /* First tests in real conditions, where cache considerations
     matter */

  /* libm */
  printf("TEST WITH CACHE CONSIDERATION \n");
  test_with_cache("LIBM", testfun_libm, n, nbarg);
  test_with_cache("CRLIBM", testfun_crlibm, n, nbarg);
#ifdef HAVE_MATHLIB_H
  Original_Mode = Init_Lib();
  test_with_cache("IBM", testfun_libultim, n, nbarg);
  Exit_Lib(Original_Mode);
#endif
#ifdef HAVE_LIBMCR_H
  test_with_cache("SUN", testfun_libmcr, n, nbarg);
#endif

#endif /* TEST_CACHE*/

  /************  TESTS WITHOUT CACHES  *******************/
  srandom(n);
#if EVAL_PERF==1  
  crlibm_second_step_taken=0; 
#endif

  /* take the min of N1 identical calls to leverage interruptions */
  /* As a consequence, the cache impact of these calls disappear...*/
  for(i=0; i< n; i++){ 
    if (nbarg==1) {
      i1 = randfun();
      i2 = randfun();
    } else {
      i1 = (*((double (*)(double *))randfun))(&i2);
    }
    
    test_without_cache("libm", testfun_libm, i1, i2, &libm_dtmin, &libm_dtmax, &libm_dtsum, 0);
    test_without_cache("crlibm", testfun_crlibm, i1, i2, &crlibm_dtmin, &crlibm_dtmax, &crlibm_dtsum, 0);
#ifdef   HAVE_MATHLIB_H
    Original_Mode = Init_Lib(); 
    test_without_cache("ultim", testfun_libultim, i1, i2, &libultim_dtmin, &libultim_dtmax, &libultim_dtsum, 0);
    Exit_Lib(Original_Mode);
#endif /*HAVE_MATHLIB_H*/
#ifdef   HAVE_LIBMCR_H
    test_without_cache("libmcr", testfun_libmcr, i1, i2, &libmcr_dtmin, &libmcr_dtmax, &libmcr_dtsum, 0);
#endif /*HAVE_LIBMCR_H*/
#ifdef   HAVE_MPFR_H
    test_without_cache("mpfr", (double(*)()) testfun_mpfr, i1, i2, &mpfr_dtmin, &mpfr_dtmax, &mpfr_dtsum, 1);
#endif /*HAVE_MPFR_H*/
  } 

#if EVAL_PERF==1  
#ifdef TIMING_USES_GETTIMEOFDAY /* use inaccurate timer, do many loops */
	 printf("\nCRLIBM : Second step taken %d times out of %d\n",
		crlibm_second_step_taken/(N1 * TIMING_ITER), n );
#else
	 printf("\nCRLIBM : Second step taken %d times out of %d\n",
		crlibm_second_step_taken/N1, n );
#endif

#endif


  /************  WORST CASE TESTS   *********************/
  /* worst case test */
  i1 = worstcase;
  i2 = 0.156250000000000000000000000000000000000000000000000000e1; /* TODO when we have worst cases for power...*/

  test_worst_case(testfun_libm, i1, i2, &libm_dtwc, 0);
  test_worst_case(testfun_crlibm, i1, i2, &crlibm_dtwc, 0);
#ifdef   HAVE_MPFR_H
  test_worst_case((double(*)())testfun_mpfr, i1, i2, &mpfr_dtwc, 1);
#endif /*HAVE_MPFR_H*/
#ifdef   HAVE_MATHLIB_H
  Original_Mode = Init_Lib(); 
  test_worst_case(testfun_libultim, i1, i2, &libultim_dtwc, 0);
  Exit_Lib(Original_Mode);
#endif /*HAVE_MATHLIB_H*/
#ifdef   HAVE_LIBMCR_H
  test_worst_case(testfun_libmcr, i1, i2, &libmcr_dtwc, 0);
#endif /*HAVE_LIBMCR_H*/

    /*************Normal output*************************/
  normal_output("LIBM", testfun_libm, libm_dtmin, libm_dtmax, libm_dtsum, libm_dtwc, n);
#ifdef   HAVE_MPFR_H
  normal_output("MPFR", (double(*)())testfun_mpfr, mpfr_dtmin, mpfr_dtmax, mpfr_dtsum, mpfr_dtwc, n);
#endif /*HAVE_MPFR_H*/
#ifdef   HAVE_MATHLIB_H
  normal_output("IBM", testfun_libultim, libultim_dtmin, libultim_dtmax, libultim_dtsum, libultim_dtwc, n);
#endif /*HAVE_MATHLIB_H*/
#ifdef   HAVE_LIBMCR_H
  normal_output("SUN", testfun_libmcr, libmcr_dtmin, libmcr_dtmax, libmcr_dtsum, libmcr_dtwc, n);
#endif /*HAVE_LIBMCR_H*/
  normal_output("CRLIBM", testfun_crlibm, crlibm_dtmin, crlibm_dtmax, crlibm_dtsum, crlibm_dtwc, n);


  /******************* Latex output ****************/
  printf("\\multicolumn{4}{|c|}{Processor / system / compiler}   \\\\ \n \\hline");
  printf("\n                             & min time \t & avg time \t& max time \t  \\\\ \n \\hline\n");
  latex_output("default \\texttt{libm}  ", testfun_libm, libm_dtmin, libm_dtmax, libm_dtsum, libm_dtwc, n);
#ifdef   HAVE_MPFR_H
  latex_output("MPFR                   ", (double(*)())testfun_mpfr, mpfr_dtmin, mpfr_dtmax, mpfr_dtsum, mpfr_dtwc, n);
#endif /*HAVE_MPFR_H*/
#ifdef   HAVE_MATHLIB_H
  latex_output("IBM's \\texttt{libultim}", testfun_libultim, libultim_dtmin, libultim_dtmax, libultim_dtsum, libultim_dtwc, n);
#endif /*HAVE_MATHLIB_H*/
#ifdef   HAVE_LIBMCR_H
  latex_output("Sun's \\texttt{libmcr}  ", testfun_libmcr, libmcr_dtmin, libmcr_dtmax, libmcr_dtsum, libmcr_dtwc, n);
#endif /*HAVE_LIBMCR_H*/
  latex_output("\\texttt{crlibm}        ", testfun_crlibm, crlibm_dtmin, crlibm_dtmax, crlibm_dtsum, crlibm_dtwc, n);

  return 0;
}


