#ifdef STARFORM
#ifndef STARFORM_HINCLUDED
#define STARFORM_HINCLUDED

typedef struct stfmContext 
{
    double dMinMassFrac;        /* minimum fraction of average mass of neighbour 
                                   particles required for particle to avoid deletion */
    double dDeltaT;		/* timestep in system units */
    double dGmUnit;		/* system mass in grams */
    double dGmPerCcUnit;	/* system density in gm/cc */
    double dSecUnit;		/* system time in seconds */
    double dErgUnit;		/* system energy in ergs */
    double dPhysDenMin;		/* Physical density minimum for star
				   formation (in system units) */
    double dOverDenMin;		/* Overdensity minimum for star formation */
    double dTempMax;		/* Form stars below this temperature
				   EVEN IF the gas is not cooling. */
    double dSoftMin;		/* Jean's length as a fraction of
				   softening at which to form stars*/
    double dCStar;		/* Star formation constant */
    double dStarEff;		/* Fraction of gas mass converted into
				 star mass per timestep. */
    double dInitStarMass;       /* Fixed Initial Star Mass */
    double dMinSpawnStarMass;   /* Minimum Initial Star Mass */
    double dMinGasMass;		/* minimum mass gas before we delete
				   the particle. */
    double dMaxGasMass;		/* maximum mass gas particle. */
    double dMaxStarMass;	/* maximum mass star particle. */
    double dMinPot;             /* minimum low metallicity potential
				   in simulation */
    double dMaxPot;             /* max low metallicity potential in
				   simulation */
    double dBHFormProb;         /* Probability star will become a BH */
    int bBHForm;                /* are BH seeds allowed to form */ 
    double dInitBHMass;         /* Initial BH mass */
    double dMaxBHMetallicity;	/* maximum BH metallicity. */
#ifdef COOLING_MOLECULARH
  double dStarFormEfficiencyH2; /* Star formation efficiency, CStar, is multiplied by dStarFormEfficiencyH2 times the fraction of hydrogen in molecular form  */
#endif

} * STFM;
	
void stfmInitialize(STFM *pstfm);

void pkdStarLogInit(PKD pkd);
void pkdStarLogFlush(PKD pkd, char *pszFileName);
void pkdMinMaxPot(PKD pkd, STFM stfm, double *dMinPot, double *dMaxPot);

void stfmFormStars(STFM stfm, PKD pkd, PARTICLE *p,
		   double dTime, /* current time */
		   int *nFormed, /* number of stars formed */
		   double *dMassFormed,	/* mass of stars formed */
		   int *nDeleted); /* gas particles deleted */
void pkdFormStars(PKD pkd, STFM stfm, double dTime,
		   int *nFormed, /* number of stars formed */
		   double *dMassFormed,	/* mass of stars formed */
		   int *nDeleted); /* gas particles deleted */

#endif
#endif

#ifdef SIMPLESF
void pkdSimpleStarForm(PKD pkd, double dRateCoeff, double dTMax, double dDenMin, double dDelta, double dTime,
					   double dInitStarMass, double dESNPerStarMass, double dtCoolingShutoff, int bdivv,
                                           int *nFormed, /* number of stars formed */
                                           double *dMassFormed, /* mass of stars formed */
                                           int *nDeleted); /* gas particles deleted */
#endif


