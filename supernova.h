
#ifndef SUPERNOVA_HINCLUDED
#define SUPERNOVA_HINCLUDED
#include "startime.h"
#include "millerscalo.h"
#include "feedback.h"

typedef struct snContext 
{
  double dMinMassFrac;        /* minimum fraction of average mass of neighbour 
				 particles required for particle to avoid deletion */
  double dGmUnit;		/* system mass in grams */
  double dGmPerCcUnit;	/* system density in gm/cc */
  double dSecUnit;		/* system time in seconds */
  double dErgUnit;		/* system energy in ergs */
  
  double dESN;		        /* SN energy released per SN */
  int iNSNIIQuantum;         /* Minimum # of SN allowed per timestep */
  double dRadPreFactor;         /* unit conversion for blast radius */
  double dTimePreFactor;        /* unit conversion for shell lifetime */
  double dMSNrem;		/* mass of supernovae remnant */
  
  double dMSNIImin;           /* minimum mass of star that goes SNII */
  double dMSNIImax;           /* maximum mass of star that goes SNII */
  
  double dMBmin;              /* minimum mass of binary that goes SNIa */
  double dMBmax;              /* maximum mass of binary that goes SNIa */
  
  double dFracBinSNIa;        /* fraction of binaries that go SNIa */
  
  double dMEjconst;           /* normalization constant for formula
				 for total mass ejected as a
				 function of stellar mass */
  double dMEjexp;             /* power of stellar mass in formula
				 for total mass ejected as a
				 function of stellar mass */
  double dMFeconst;           /* normalization constant for formula
				 for mass of ejected Fe as a
				 function of stellar mass */
  double dMFeexp;             /* power of stellar mass in formula
				 for mass of ejected Fe as a
				 function of stellar mass */
  double dMOxconst;           /* same for oxygen */
  double dMOxexp;
  
  double dSNIaMetals;		/* Solar masses of metals generated by
				   a single SNIa (independent of mass) */
  
  struct MillerScaloContext MSparam;
  struct PadovaContext ppdva;
  
  int bInitialized;
} * SN;

void snInitialize(SN *psn);

void snInitConstants(SN sn);

void snFree(SN sn);

void snCalcSNIIFeedback(SN sn, SFEvent sfEvent,
			double dTime, /* current time in years */
			double dDelta, /* length of timestep (years) */
			FBEffects *fbEffects);

void snCalcSNIaFeedback(SN sn, SFEvent sfEvent,
			double dTime, /* current time in years */
			double dDelta, /* length of timestep (years) */
			FBEffects *fbEffects);

#endif
