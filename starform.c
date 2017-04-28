
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

#include <rpc/types.h>
#include <rpc/xdr.h>

#include "pkd.h"
#include "starform.h"
#include "millerscalo.h"

#define max(A,B) ((A) > (B) ? (A) : (B))
#define min(A,B) ((A) < (B) ? (A) : (B))

#define MHYDR 1.67e-24 	/* mass of hydrogen atom in grams */
#ifdef STARFORM

#ifdef NOCOOLING
#error "STARFORM requires Cooling on"
#endif


/*
 * Star forming module for GASOLINE
 */

void stfmInitialize(STFM *pstfm)
{
    STFM stfm;
    
    stfm = (STFM) malloc(sizeof(struct stfmContext));
    assert(stfm != NULL);
    
    stfm->dPhysDenMin = 0;
    stfm->dOverDenMin = 0;
    stfm->dTempMax = 0;
    stfm->dCStar = 0;
    stfm->dSecUnit = 0;
    stfm->dGmPerCcUnit = 0;
    stfm->dGmUnit = 0;
    stfm->dStarEff = 0.0;
	stfm->dInitStarMass = 0.0;
    stfm->dMinGasMass = 0.0;
    stfm->dMinMassFrac = 0.0;
    stfm->dMaxStarMass = 0.0;
    stfm->dZAMSDelayTime = 0.0;
    stfm->dBHFormProb = 0.0; /* new BH params */
    stfm->bBHForm = 0;
    stfm->dInitBHMass = 0.0;
    stfm->dStarClusterMass = 0;
    *pstfm = stfm;
    }

void pkdStarLogInit(PKD pkd)
{
    STARLOG *pStarLog = &pkd->starLog;
    
    pStarLog->nLog = 0;
    pStarLog->nMaxLog = 1000;	/* inital size of buffer */
    pStarLog->nOrdered = 0;
    pStarLog->seTab = malloc(pStarLog->nMaxLog*sizeof(SFEVENT));
    srand(pkd->idSelf);
}

void pkdStarLogFlush(PKD pkd, char *pszFileName)
{
    FILE *fp;
    int iLog;
    XDR xdrs;
    
    if(pkd->starLog.nLog == 0)
	return;
    
    assert(pkd->starLog.nLog == pkd->starLog.nOrdered);
    
    fp = fopen(pszFileName, "a");
    assert(fp != NULL);
    xdrstdio_create(&xdrs,fp,XDR_ENCODE);
    for(iLog = 0; iLog < pkd->starLog.nLog; iLog++){
	SFEVENT *pSfEv = &(pkd->starLog.seTab[iLog]);
	xdr_int(&xdrs, &(pSfEv->iOrdStar));
	xdr_int(&xdrs, &(pSfEv->iOrdGas));
	xdr_double(&xdrs, &(pSfEv->timeForm));
	xdr_double(&xdrs, &(pSfEv->rForm[0]));
	xdr_double(&xdrs, &(pSfEv->rForm[1]));
	xdr_double(&xdrs, &(pSfEv->rForm[2]));
	xdr_double(&xdrs, &(pSfEv->vForm[0]));
	xdr_double(&xdrs, &(pSfEv->vForm[1]));
	xdr_double(&xdrs, &(pSfEv->vForm[2]));
	xdr_double(&xdrs, &(pSfEv->massForm));
	xdr_double(&xdrs, &(pSfEv->rhoForm));
	xdr_double(&xdrs, &(pSfEv->TForm));
#ifdef COOLING_MOLECULARH
	xdr_double(&xdrs, &(pSfEv->H2fracForm));
#endif
	}
    xdr_destroy(&xdrs);
    fclose(fp);
    pkd->starLog.nLog = 0;
    pkd->starLog.nOrdered = 0;
    }
    
/*
     taken from TREESPH and modified greatly.
     Uses the following formula for the star formation rate:

              d(ln(rhostar))/dt=cstar*rhogas/tdyn

*/

/* Returns probability of converting all gas to star in this step */
double stfmFormStarProb(STFM stfm, PKD pkd, PARTICLE *p, 
    double dTime /* current time */)
    {
    double tdyn;
    double tform;
    double tcool;
    double T;
    COOL *cl = pkd->Cool;
    double dMProb;
 #ifdef COOLING_MOLECULARH
    double correL = 1.0;/*correlation length used for H2 shielding, CC*/
    double yH;
#endif
   
    /*  This version of the code has only three conditions unless 
	-D SFCONDITIONS is set:
	  converging flow (p->divv < 0)
	  T < dTempMax
	  density > dOverdenmin && density > dPhysDenMin
	Anil - Nov. 2003
    */

    /*
     * Is particle in convergent part of flow?
     */

#ifdef STARCLUSTERFORM
#define DIVVOFF
#endif

#ifndef DIVVOFF
    if(p->divv >= 0.0) return 0;
#endif /*DIVVOFF*/
#ifdef JEANSSF
    double l_jeans2;
    T = CoolCodeEnergyToTemperature( cl, &p->CoolParticle, p->u, p->fMetals );
    l_jeans2 = M_PI*p->c*p->c/p->fDensity*stfm->dCosmoFac;
#endif
    /*
     * Determine dynamical time.
     */
    tdyn = 1.0/sqrt(4.0*M_PI*p->fDensity/stfm->dCosmoFac);

    /*
     * Determine cooling time.
     */

#ifdef UNONCOOL
    if (stfm->bTempInclHot) 
        T = CoolCodeEnergyToTemperature( cl, &p->CoolParticle, p->u+p->uHot, p->fDensity, p->fMetals );
    else
#endif
    T = CoolCodeEnergyToTemperature( cl, &p->CoolParticle, p->u, p->fDensity, p->fMetals );
#ifdef TWOPHASE
    if (p->fMassHot > 0) {
        return 0;
    }
#endif

#ifdef  COOLING_MOLECULARH
    /*Made using the smoothing length the default, as it has been used that way in all production runs to Jun 4th, 2012, CC*/
    /***** From particle smoothing.  This works best for determining the correlation length.  CC 7/20/11 ******/
    correL = sqrt(0.25*p->fBall2);
#endif/* COOLING_MOLECULARH */


    tcool = p->u/(
#ifdef  COOLING_MOLECULARH
		  -CoolEdotInstantCode( cl, &p->CoolParticle, p->u, p->fDensity, p->fMetals, p->r, correL )
#else
		  -CoolEdotInstantCode( cl, &p->CoolParticle, p->u, p->fDensity, p->fMetals,  p->r)
#endif
          -UDOT_HYDRO(p) );
    if(T > stfm->dTempMax) return 0;

    /*
     * Determine if this particle satisfies all conditions.
     */
    
    if(p->fDensity < stfm->dOverDenMin ||
        p->fDensity/stfm->dCosmoFac < stfm->dPhysDenMin) return 0;

    if(tcool < 0.0 || tdyn > tcool || T < stfm->dTempMax)
        tform = tdyn;
    else
        tform = tcool;



#ifdef COOLING_MOLECULARH
    if (p->fMetals <= 0.1) yH = 1.0 - 4.0*((0.236 + 2.1*p->fMetals)/4.0) - p->fMetals;
    else yH = 1.0 - 4.0*((-0.446*(p->fMetals - 0.1)/0.9 + 0.446)/4.0) - p->fMetals;

    /* For non-zero values of dStarFormEfficiencyH2, set SF efficiency as a multiple of H2 fractional abundance and dStarFormEfficiencyH2, CC*/
    if (stfm->dStarFormEfficiencyH2 == 0) 
         dMProb = 1.0 - exp(-stfm->dCStar*stfm->dDeltaT/tform);
    else dMProb = 1.0 - exp(-stfm->dCStar*stfm->dDeltaT/tform*
         stfm->dStarFormEfficiencyH2*(2.0*p->CoolParticle.f_H2/yH));
#else  /* COOLING_MOLECULARH */  
    dMProb = 1.0 - exp(-stfm->dCStar*stfm->dDeltaT/tform);
#endif /* COOLING_MOLECULARH */    

    return dMProb;
    }


void stfmFormStarParticle(STFM stfm, PKD pkd, PARTICLE *p,
    double dDeltaM, /* Star mass */
    double dTime, /* current time */
    int *nFormed, /* number of stars formed */
    double *dMassFormed,	/* mass of stars formed */
    int *nDeleted) /* gas particles deleted */
    { 
    /* This code makes stars and black holes (some of the time) */
    PARTICLE starp;
    int newbh; /* tracking whether a new seed BH has formed JMB  */
    int j;
 
    /* 
     * Note on number of stars formed:
     * n = log(dMinGasMass/dInitMass)/log(1-dStarEff) = max no. stars 
     * formed per gas particle, e.g. if min gas mass = 10% initial mass,
     * dStarEff = 1/3, max no. stars formed = 6 (round up so gas mass 
     * goes below min gas mass)
     */

    starp = *p; 		/* grab copy before possible deletion */

    /*
     * form star
     */

    starp.fTimeForm = dTime + stfm->dZAMSDelayTime;
/*    printf("star   KDK %g %g %g\n",dTime,starp.fTimeForm,stfm->dZAMSDelayTime); //DEBUG dTime for SF */
    starp.fBallMax = 0.0;
    starp.iGasOrder = starp.iOrder; /* iOrder gets reassigned in
				       NewParticle() */

    /* Seed BH Formation JMB 1/19/09*/
     newbh = 0;  /* BH tracker */
     if (stfm->bBHForm == 1 && starp.fMetals <= 1.0e-6 && stfm->dBHFormProb > (rand()/((double) RAND_MAX ))) {
         starp.fTimeForm = -1.0*starp.fTimeForm;
         newbh = 1;      
         /* Decrement mass of particle.*/
         if (stfm->dInitBHMass > 0) 
             dDeltaM = stfm->dInitBHMass;  /* reassigning dDeltaM to be initBHmass JMB 6/16/09 */
         else 
             dDeltaM = p->fMass*stfm->dStarEff;
         /* No negative or very tiny masses please! */
         if ( (dDeltaM > p->fMass) ) dDeltaM = p->fMass;
         p->fMass -= dDeltaM;
         assert(p->fMass >= 0.0);
         starp.fMass = dDeltaM;
         starp.fMassForm = dDeltaM;
         }
     else {
         p->fMass -= dDeltaM;
         assert(p->fMass >= -dDeltaM*1e-6);
         starp.fMass = dDeltaM;
         starp.fMassForm = dDeltaM;
         }

    if(p->fMass < stfm->dMinGasMass) {
		(*nDeleted)++;
		pkdDeleteParticle(pkd, p);
		}


    /*
     * Log Star formation quantities
     */
    {
	STARLOG *pStarLog = &pkd->starLog;
	SFEVENT *pSfEv;
	if(pStarLog->nLog >= pStarLog->nMaxLog) {
	    /* Grow table */
	    pStarLog->nMaxLog *= 1.4;
	    pStarLog->seTab = realloc(pStarLog->seTab,
				      pStarLog->nMaxLog*sizeof(SFEVENT));
	    assert(pStarLog->seTab != NULL);
	}
	/* take care of iOrder assignment later */
	pSfEv = &(pStarLog->seTab[pStarLog->nLog]);
	pSfEv->timeForm = starp.fTimeForm;
	pSfEv->iOrdGas = starp.iOrder;
	for(j = 0; j < 3; j++) {
	    pSfEv->rForm[j] = starp.r[j];
	    pSfEv->vForm[j] = starp.v[j];
	    }
	pSfEv->massForm = starp.fMassForm;
	pSfEv->rhoForm = starp.fDensity/stfm->dCosmoFac;
    pSfEv->TForm = CoolCodeEnergyToTemperature( pkd->Cool, &p->CoolParticle, p->u, p->fDensity, p->fMetals );
#ifdef COOLING_MOLECULARH /* Output the H2 fractional abundance in the gas particle*/
	pSfEv->H2fracForm = 2.0*p->CoolParticle.f_H2/yH;
#endif
	pStarLog->nLog++;
	}
    
    starp.fNSNtot = 0.0;

	/* NB: It is important that the star inherit special properties of the gas
	   particle such as being a target for movies or other tracing
	   Thus: Do not remove all the TYPE properties -- just the gas specific ones */
    TYPEReset(&starp, TYPE_GAS|TYPE_TREEACTIVE|TYPE_SMOOTHACTIVE|TYPE_ACTIVE|TYPE_DensACTIVE|TYPE_Scatter);
    if(newbh == 0) TYPESet(&starp, TYPE_STAR) ; /* if it's a BH make it a SINK  JMB  */
    else TYPESet(&starp, TYPE_SINK);
    TYPEReset(&starp, TYPE_NbrOfACTIVE); /* just a precaution */
    
    (*nFormed)++;
    *dMassFormed += dDeltaM;
    
    pkdNewParticle(pkd, starp);    
}

void stfmFormStars(STFM stfm, PKD pkd, PARTICLE *p,
		   double dTime, /* current time */
		   int *nFormed, /* number of stars formed */
		   double *dMassFormed,	/* mass of stars formed */
		   int *nDeleted) /* gas particles deleted */
{
    double dDeltaM,dMProb;

#ifdef STARCLUSTERFORM
    if (TYPETest(p, TYPE_STARFORM)) {
        dMProb = stfmFormStarProb(stfm, pkd, p, dTime);
        if (dMProb > 0) {
            dDeltaM = p->fMass;
            printf("SCF Clus: %8d %12g %12g STAR from %d\n",p->iOrder,p->fDensity,dMProb,(int) StarClusterFormiOrder(p));
            stfmFormStarParticle(stfm, pkd, p, dDeltaM, dTime, nFormed,dMassFormed,nDeleted);
            }
        else printf("SCF Clus: %8d %12g %12g NO from %d\n",p->iOrder,p->fDensity,dMProb,(int) StarClusterFormiOrder(p));
        }      
#else
    /* probability of converting all gas to star in this step */
    dMProb = stfmFormStarProb(stfm, pkd, p, dTime);
    if (dMProb > 0) {
        /* mass of star particle. */
        if (stfm->dInitStarMass > 0) 
            dDeltaM = stfm->dInitStarMass;

        else 
            dDeltaM = p->fMass*stfm->dStarEff;
        /* No negative or very tiny masses please! */
        if ( (dDeltaM > p->fMass) ) dDeltaM = p->fMass;
        /* Reduce probability for just dDeltaM/p->fMass becoming star */
        if(dMProb*p->fMass < dDeltaM*(rand()/((double) RAND_MAX))) return; /* no star */

        stfmFormStarParticle(stfm, pkd, p, dDeltaM, dTime, nFormed,dMassFormed,nDeleted);
    }
#endif

}

void pkdFormStars(PKD pkd, STFM stfm, double dTime, int *nFormed,
		  double *dMassFormed, int *nDeleted)
{
    int i;
    PARTICLE *p;
    int n = pkdLocal(pkd);
    
    
    *nFormed = 0;
    *nDeleted = 0;
    *dMassFormed = 0.0;
    
    for(i = 0; i < n; ++i) {
        p = &pkd->pStore[i];
        if(pkdIsGas(pkd, p))
            stfmFormStars(stfm, pkd, p, dTime, nFormed, dMassFormed, nDeleted);
        assert(p->u >= 0);
        assert(p->uPred >= 0);
        assert(p->fMass >= 0);
        }

    }

void pkdStarClusterFormPrecondition(PKD pkd, struct inStarClusterFormPrecondition in) {
    int i;
    PARTICLE *p;
    int n = pkdLocal(pkd);
    
    for(i = 0; i < n; ++i) {
        p = &pkd->pStore[i];
        TYPEReset(p,TYPE_SMOOTHACTIVE|TYPE_ACTIVE|TYPE_STARFORM);
        if(pkdIsGas(pkd, p) && TYPETest(p, TYPE_DENMAX)) { 
            double dMProb;
            dMProb = stfmFormStarProb(&in.stfm, pkd, p,  in.dTime);
            /* See if all gas becomes star(s) */
            if(dMProb < (rand()/((double) RAND_MAX))) {
                printf("SCFPre: %8d %12g %12g DENMAX: NO STAR CLUSTER\n",p->iOrder,p->fDensity,dMProb);
                continue; /* no star */
                }
            printf("SCFPre: %8d %12g %12g DENMAX: STAR CLUSTER ?\n",p->iOrder,p->fDensity,dMProb);

            StarClusterFormfBall2Save(p) = p->fBall2;
            StarClusterFormiOrder(p) = -1;
            p->fBall2 = pow(in.stfm.dStarClusterMass/(p->fDensity*(4./3.*M_PI)),0.66666666666667);
            TYPESet(p,TYPE_SMOOTHACTIVE|TYPE_ACTIVE|TYPE_STARFORM);
            }
        }
    }
#endif

