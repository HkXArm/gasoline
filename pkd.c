#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <malloc.h>
#include <math.h>
#include <assert.h>
#include <sys/time.h>

#include <rpc/types.h>
#include <rpc/xdr.h>

#include "pkd.h"
#include "ewald.h"
#include "grav.h"
#include "walk.h"
#include "opentype.h"
#include "mdl.h"
#include "tipsydefs.h"


double pkdGetTimer(PKD pkd,int iTimer)
{
	return(pkd->ti[iTimer].sec);
	}


void pkdClearTimer(PKD pkd,int iTimer)
{
	int i;

	if (iTimer >= 0) {
		pkd->ti[iTimer].sec = 0.0;
		}
	else {
		for (i=0;i<MAX_TIMERS;++i) {
			pkd->ti[i].sec = 0.0;
			}
		}
	}


void pkdStartTimer(PKD pkd,int iTimer)
{
	pkd->ti[iTimer].stamp = mdlCpuTimer(pkd->mdl);
	}


void pkdStopTimer(PKD pkd,int iTimer)
{
	double sec;
	sec = mdlCpuTimer(pkd->mdl) - pkd->ti[iTimer].stamp;
	if (sec < 0.0) sec = 0.0;
	pkd->ti[iTimer].sec += sec;
	}


void pkdInitialize(PKD *ppkd,MDL mdl,int iOrder,int nStore,int nLvl,
				   float *fPeriod,int nDark,int nGas,int nStar)
{
	PKD pkd;
	int j;
	
	pkd = (PKD)malloc(sizeof(struct pkdContext));
	assert(pkd != NULL);
	pkd->mdl = mdl;
	pkd->iOrder = iOrder;
	pkd->idSelf = mdlSelf(mdl);
	pkd->nThreads = mdlThreads(mdl);
	pkd->nStore = nStore;
	pkd->nLocal = 0;
	pkd->nDark = nDark;
	pkd->nGas = nGas;
	pkd->nStar = nStar;
	pkd->nRejects = 0;
	for (j=0;j<3;++j) {
		pkd->fPeriod[j] = fPeriod[j];
		}
	/*
	 ** Allocate the main particle store.
	 ** Need to use mdlMalloc() since the particles will need to be
	 ** visible to all other processors thru mdlAquire() later on.
	 */
	pkd->pStore = mdlMalloc(pkd->mdl,nStore*sizeof(PARTICLE));
	assert(pkd->pStore != NULL);
	pkd->kdNodes = NULL;
	pkd->piLeaf = NULL;
	pkd->kdTop = NULL;
	/*
	 ** Allocate initial interaction lists
	 */
	pkd->nMaxPart = 500;
	pkd->nMaxCellSoft = 500;
	pkd->nMaxCellNewt = 500;
	pkd->nSqrtTmp = 500;
	pkd->ilp = malloc(pkd->nMaxPart*sizeof(ILP));
	assert(pkd->ilp != NULL);
	pkd->ilcs = malloc(pkd->nMaxCellSoft*sizeof(ILCS));
	assert(pkd->ilcs != NULL);
	pkd->ilcn = malloc(pkd->nMaxCellNewt*sizeof(ILCN));
	assert(pkd->ilcn != NULL);
	pkd->sqrttmp = malloc(pkd->nSqrtTmp*sizeof(double));
	assert(pkd->sqrttmp != NULL);
	pkd->d2a = malloc(pkd->nSqrtTmp*sizeof(double));
	assert(pkd->d2a != NULL);
	/*
	 ** Ewald stuff!
	 */
	pkd->nMaxEwhLoop = 100;
	pkd->ewt = malloc(pkd->nMaxEwhLoop*sizeof(EWT));
	assert(pkd->ewt != NULL);
	*ppkd = pkd;
	}


void pkdFinish(PKD pkd)
{
	if (pkd->kdNodes) {
		/*
		 ** Close caching space and free up nodes.
		 */
		mdlFinishCache(pkd->mdl,CID_CELL);
		mdlFree(pkd->mdl,pkd->kdNodes);
		}
	if (pkd->kdTop) free(pkd->kdTop);
	if (pkd->piLeaf) free(pkd->piLeaf);
	free(pkd->ilp);
	free(pkd->ilcs);
	free(pkd->ilcn);
	free(pkd->sqrttmp);
	free(pkd->d2a);
	free(pkd->ewt);
	mdlFree(pkd->mdl,pkd->pStore);
	free(pkd);
	}


void pkdSeek(PKD pkd,FILE *fp,int nStart,int bStandard)
{
	long lStart;

	/*
	 ** Seek according to true XDR size structures when bStandard is true.
	 ** This may be a bit dicey, but it should work as long
	 ** as no one changes the tipsy binary format!
	 */
	if (bStandard) lStart = 32;
	else lStart = sizeof(struct dump);
	if (nStart > pkd->nGas) {
		if (bStandard) lStart += pkd->nGas*48;
		else lStart += pkd->nGas*sizeof(struct gas_particle);
		nStart -= pkd->nGas;
		if (nStart > pkd->nDark) {
			if (bStandard) lStart += pkd->nDark*36;
			else lStart += pkd->nDark*sizeof(struct dark_particle);
			nStart -= pkd->nDark;
			if (bStandard) lStart += nStart*44;
			lStart += nStart*sizeof(struct star_particle);
			}
		else {
			if (bStandard) lStart += nStart*36;
			else lStart += nStart*sizeof(struct dark_particle);
			}
		}
	else {
		if (bStandard) lStart += nStart*48;
		else lStart += nStart*sizeof(struct gas_particle);
		}
	fseek(fp,lStart,0);
	}


void pkdReadTipsy(PKD pkd,char *pszFileName,int nStart,int nLocal,
				  int bStandard,double dvFac,double dTuFac)
{
	FILE *fp;
	int i,j;
	struct dark_particle dp;
	struct gas_particle gp;
	struct star_particle sp;

	pkd->nLocal = nLocal;
	pkd->nActive = nLocal;
	/*
	 ** General initialization.
	 */
	for (i=0;i<nLocal;++i) {
		pkd->pStore[i].iActive = 1;
		pkd->pStore[i].iRung = 0;
		pkd->pStore[i].fWeight = 1.0;
		pkd->pStore[i].fDensity = 0.0;
		pkd->pStore[i].fBall2 = 0.0;
#ifdef GASOLINE
		for (j=0;j<3;++j) {
			pkd->pStore[i].vPred[j] = 0.0;
			}
		pkd->pStore[i].fHsmDivv = 0.0;
		pkd->pStore[i].fRhoDivv = 0.0;
		pkd->pStore[i].fCutVisc = 0.0;
		pkd->pStore[i].u = 0.0;
		pkd->pStore[i].du = 0.0;
		pkd->pStore[i].uOld = 0.0;
		pkd->pStore[i].A = 0.0;
		pkd->pStore[i].B = 0.0;		
		pkd->pStore[i].fMetals = 0.0;
		pkd->pStore[i].fTimeForm = 0.0;
#endif
		}
	/*
	 ** Seek past the header and up to nStart.
	 */
	fp = fopen(pszFileName,"r");
	assert(fp != NULL);
	/*
	 ** Seek to right place in file.
	 */
	pkdSeek(pkd,fp,nStart,bStandard);
	/*
	 ** Read Stuff!
	 */
	if (bStandard) {
		float vTemp;
		XDR xdrs;
		xdrstdio_create(&xdrs,fp,XDR_DECODE);
		for (i=0;i+nStart < pkd->nGas && i < nLocal;++i) {
			xdr_float(&xdrs,&pkd->pStore[i].fMass);
			for (j=0;j<3;++j) {
				xdr_float(&xdrs,&pkd->pStore[i].r[j]);
				}
			for (j=0;j<3;++j) {
				xdr_float(&xdrs,&vTemp);
				pkd->pStore[i].v[j] = dvFac*vTemp;			
#ifdef GASOLINE
				pkd->pStore[i].vPred[j] = dvFac*vTemp;
#endif
				}
			xdr_float(&xdrs,&pkd->pStore[i].fDensity);
#ifdef GASOLINE
			/*
			 ** Convert Temperature to Thermal energy.
			 */
			xdr_float(&xdrs,&vTemp);
			pkd->pStore[i].u = dTuFac*vTemp;
			xdr_float(&xdrs,&pkd->pStore[i].fSoft);
			xdr_float(&xdrs,&pkd->pStore[i].fMetals);
#else
			xdr_float(&xdrs,&vTemp);
			xdr_float(&xdrs,&pkd->pStore[i].fSoft);
			xdr_float(&xdrs,&vTemp);
#endif
			xdr_float(&xdrs,&pkd->pStore[i].fPot);
			pkd->pStore[i].iOrder = nStart + i;
			}
		for (;i+nStart < pkd->nGas+pkd->nDark && i < nLocal;++i) {
			xdr_float(&xdrs,&pkd->pStore[i].fMass);
			for (j=0;j<3;++j) {
				xdr_float(&xdrs,&pkd->pStore[i].r[j]);
				}
			for (j=0;j<3;++j) {
				xdr_float(&xdrs,&vTemp);
				pkd->pStore[i].v[j] = dvFac*vTemp;			
				}
			xdr_float(&xdrs,&pkd->pStore[i].fSoft);
			xdr_float(&xdrs,&pkd->pStore[i].fPot);
			pkd->pStore[i].iOrder = nStart + i;
			}
		for (;i+nStart < pkd->nGas+pkd->nDark+pkd->nStar && i < nLocal;++i) {
			xdr_float(&xdrs,&pkd->pStore[i].fMass);
			for (j=0;j<3;++j) {
				xdr_float(&xdrs,&pkd->pStore[i].r[j]);
				}
			for (j=0;j<3;++j) {
				xdr_float(&xdrs,&vTemp);
				pkd->pStore[i].v[j] = dvFac*vTemp;			
				}
#ifdef GASOLINE
			xdr_float(&xdrs,&pkd->pStore[i].fMetals);
			xdr_float(&xdrs,&pkd->pStore[i].fTimeForm);
#else
			xdr_float(&xdrs,&vTemp);
			xdr_float(&xdrs,&vTemp);
#endif
			xdr_float(&xdrs,&pkd->pStore[i].fSoft);
			xdr_float(&xdrs,&pkd->pStore[i].fPot);
			pkd->pStore[i].iOrder = nStart + i;
			}
		xdr_destroy(&xdrs);
		}
	else {
		for (i=0;i+nStart < pkd->nGas && i < nLocal;++i) {
			fread(&gp,sizeof(struct gas_particle),1,fp);
			for (j=0;j<3;++j) {
				pkd->pStore[i].r[j] = gp.pos[j];
				pkd->pStore[i].v[j] = dvFac*gp.vel[j];
#ifdef GASOLINE
				pkd->pStore[i].vPred[j] = dvFac*gp.vel[j];
#endif
				}
			pkd->pStore[i].fMass = gp.mass;
			pkd->pStore[i].fSoft = gp.hsmooth;
#ifdef GASOLINE
			pkd->pStore[i].fDensity = gp.rho;
			pkd->pStore[i].u = dTuFac*gp.temp;
			pkd->pStore[i].fMetals = gp.metals;
#endif
			pkd->pStore[i].iOrder = nStart + i;
			}
		for (;i+nStart < pkd->nGas+pkd->nDark && i < nLocal;++i) {
			fread(&dp,sizeof(struct dark_particle),1,fp);
			for (j=0;j<3;++j) {
				pkd->pStore[i].r[j] = dp.pos[j];
				pkd->pStore[i].v[j] = dvFac*dp.vel[j];
				}
			pkd->pStore[i].fMass = dp.mass;
			pkd->pStore[i].fSoft = dp.eps;
			pkd->pStore[i].iOrder = nStart + i;
			}
		for (;i+nStart < pkd->nGas+pkd->nDark+pkd->nStar && i < nLocal;++i) {
			fread(&sp,sizeof(struct star_particle),1,fp);
			for (j=0;j<3;++j) {
				pkd->pStore[i].r[j] = sp.pos[j];
				pkd->pStore[i].v[j] = dvFac*sp.vel[j];
				}
			pkd->pStore[i].fMass = sp.mass;
			pkd->pStore[i].fSoft = sp.eps;
#ifdef GASOLINE
			pkd->pStore[i].fMetals = sp.metals;
			pkd->pStore[i].fTimeForm = sp.tform;		
#endif
			pkd->pStore[i].iOrder = nStart + i;
			}
		}
	fclose(fp);
	}


void pkdCalcBound(PKD pkd,BND *pbnd,BND *pbndActive)
{
	int i,j;

	/*
	 ** Initialize the bounds to 0 at the beginning
	 */
	for (j=0;j<3;++j) {
		pbnd->fMin[j] = FLT_MAX;
		pbnd->fMax[j] = -FLT_MAX;
		pbndActive->fMin[j] = FLT_MAX;
		pbndActive->fMax[j] = -FLT_MAX;
		}
	/*
	 ** Calculate Local Bounds.
	 */
	for (i=0;i<pkd->nLocal;++i) {
		for (j=0;j<3;++j) {
			if (pkd->pStore[i].r[j] < pbnd->fMin[j]) 
				pbnd->fMin[j] = pkd->pStore[i].r[j];
			if (pkd->pStore[i].r[j] > pbnd->fMax[j])
				pbnd->fMax[j] = pkd->pStore[i].r[j];
			}
		}
	/*
	 ** Calculate Active Bounds.
	 */
	for (i=0;i<pkd->nLocal;++i) {
		if (pkd->pStore[i].iActive) {
			for (j=0;j<3;++j) {
				if (pkd->pStore[i].r[j] < pbndActive->fMin[j]) 
					pbndActive->fMin[j] = pkd->pStore[i].r[j];
				if (pkd->pStore[i].r[j] > pbndActive->fMax[j])
					pbndActive->fMax[j] = pkd->pStore[i].r[j];
				}
			}
		}
	}


/*
 ** Partition particles between iFrom and iTo into those < fSplit and
 ** those >= to fSplit.  Find number and weight in each partition.
 */
int pkdWeight(PKD pkd,int d,float fSplit,int iSplitSide,int iFrom,int iTo,
			  int *pnLow,int *pnHigh,float *pfLow,float *pfHigh)
{
	int i,iPart;
	float fLower,fUpper;
	
	/*
	 ** First partition the memory about fSplit for particles iFrom to iTo.
	 */
	if (iSplitSide) {
		iPart = pkdLowerPart(pkd,d,fSplit,iFrom,iTo);
		*pnLow = pkdLocal(pkd)-iPart;
		*pnHigh = iPart;
		}
	else {
		iPart = pkdUpperPart(pkd,d,fSplit,iFrom,iTo);
		*pnLow = iPart;
		*pnHigh = pkdLocal(pkd)-iPart;
		}
	/*
	 ** Calculate the lower weight and upper weight BETWEEN the particles
	 ** iFrom to iTo!
	 */
	fLower = 0.0;
	for (i=iFrom;i<iPart;++i) {
		fLower += pkd->pStore[i].fWeight;
		}
	fUpper = 0.0;
	for (i=iPart;i<=iTo;++i) {
		fUpper += pkd->pStore[i].fWeight;
		}
	if (iSplitSide) {
		*pfLow = fUpper;
		*pfHigh = fLower;
		}
	else {
		*pfLow = fLower;
		*pfHigh = fUpper;
		}
	return(iPart);
	}


int pkdLowerPart(PKD pkd,int d,float fSplit,int i,int j)
{
	PARTICLE pTemp;

	if (i > j) goto done;
    while (1) {
        while (pkd->pStore[i].r[d] >= fSplit)
            if (++i > j) goto done;
        while (pkd->pStore[j].r[d] < fSplit)
            if (i > --j) goto done;
		pTemp = pkd->pStore[i];
		pkd->pStore[i] = pkd->pStore[j];
		pkd->pStore[j] = pTemp;
        }
 done:
    return(i);
	}


int pkdUpperPart(PKD pkd,int d,float fSplit,int i,int j)
{
	PARTICLE pTemp;

	if (i > j) goto done;
    while (1) {
        while (pkd->pStore[i].r[d] < fSplit)
            if (++i > j) goto done;
        while (pkd->pStore[j].r[d] >= fSplit)
            if (i > --j) goto done;
		pTemp = pkd->pStore[i];
		pkd->pStore[i] = pkd->pStore[j];
		pkd->pStore[j] = pTemp;
        }
 done:
    return(i);
	}


int pkdLowerOrdPart(PKD pkd,int nOrdSplit,int i,int j)
{
	PARTICLE pTemp;

	if (i > j) goto done;
    while (1) {
        while (pkd->pStore[i].iOrder >= nOrdSplit)
            if (++i > j) goto done;
        while (pkd->pStore[j].iOrder < nOrdSplit)
            if (i > --j) goto done;
		pTemp = pkd->pStore[i];
		pkd->pStore[i] = pkd->pStore[j];
		pkd->pStore[j] = pTemp;
        }
 done:
    return(i);
	}


int pkdUpperOrdPart(PKD pkd,int nOrdSplit,int i,int j)
{
	PARTICLE pTemp;

	if (i > j) goto done;
    while (1) {
        while (pkd->pStore[i].iOrder < nOrdSplit)
            if (++i > j) goto done;
        while (pkd->pStore[j].iOrder >= nOrdSplit)
            if (i > --j) goto done;
		pTemp = pkd->pStore[i];
		pkd->pStore[i] = pkd->pStore[j];
		pkd->pStore[j] = pTemp;
        }
 done:
    return(i);
	}


void pkdActiveOrder(PKD pkd)
{
	PARTICLE pTemp;
	int i=0;
	int j=pkdLocal(pkd)-1;

	if (i > j) goto done;
    while (1) {
        while (pkd->pStore[i].iActive)
            if (++i > j) goto done;
        while (!pkd->pStore[j].iActive)
            if (i > --j) goto done;
		pTemp = pkd->pStore[i];
		pkd->pStore[i] = pkd->pStore[j];
		pkd->pStore[j] = pTemp;
        }
 done:
	pkd->nActive = i;
	}


int pkdColRejects(PKD pkd,int d,float fSplit,float fSplitInactive,
				  int iSplitSide)
{
	PARTICLE pTemp;
	int nSplit,nSplitInactive,iRejects,i,j;

	assert(pkd->nRejects == 0);
	if (iSplitSide) {
		nSplit = pkdLowerPart(pkd,d,fSplit,0,pkdActive(pkd)-1);
		}
	else {
		nSplit = pkdUpperPart(pkd,d,fSplit,0,pkdActive(pkd)-1);
		}
	if (iSplitSide) {
		nSplitInactive = pkdLowerPart(pkd,d,fSplitInactive,
					      pkdActive(pkd),pkdLocal(pkd)-1);
		}
	else {
		nSplitInactive = pkdUpperPart(pkd,d,fSplitInactive,
					      pkdActive(pkd),pkdLocal(pkd)-1);
		}
	for(i = 0; i < nSplit; ++i)
	    assert(pkd->pStore[i].iActive == 1);
	for(i = pkdActive(pkd); i < nSplitInactive; ++i)
	    assert(pkd->pStore[i].iActive == 0);

	nSplitInactive -= pkdActive(pkd);
	/*
	 ** Now do some fancy rearrangement.
	 */
	i = nSplit;
	j = pkdActive(pkd);
	while (j < pkdActive(pkd) + nSplitInactive) {
		pTemp = pkd->pStore[i];
		pkd->pStore[i] = pkd->pStore[j];
		pkd->pStore[j] = pTemp;
		++i; ++j;
		}
	pkd->nRejects = pkdLocal(pkd) - nSplit - nSplitInactive;
	iRejects = pkdFreeStore(pkd) - pkd->nRejects;
	pkd->nActive = nSplit;
	pkd->nLocal = nSplit + nSplitInactive;
	/*
	 ** Move rejects to High memory.
	 */
	for (i=pkd->nRejects-1;i>=0;--i)
		pkd->pStore[iRejects+i] = pkd->pStore[pkd->nLocal+i];
	return(pkd->nRejects);
	}


int pkdSwapRejects(PKD pkd,int idSwap)
{
	int nBuf;
	int nOutBytes,nSndBytes,nRcvBytes;

	if (idSwap != -1) {
		nBuf = (pkdSwapSpace(pkd))*sizeof(PARTICLE);
		nOutBytes = pkd->nRejects*sizeof(PARTICLE);
		assert(pkdLocal(pkd) + pkd->nRejects <= pkdFreeStore(pkd));
		mdlSwap(pkd->mdl,idSwap,nBuf,&pkd->pStore[pkdLocal(pkd)],
				nOutBytes,&nSndBytes,&nRcvBytes);
		pkd->nLocal += nRcvBytes/sizeof(PARTICLE);
		pkd->nRejects -= nSndBytes/sizeof(PARTICLE);
		}
	return(pkd->nRejects);
	}

void pkdSwapAll(PKD pkd, int idSwap)
{
    int nBuf;
    int nOutBytes,nSndBytes,nRcvBytes;
    int i;
    int iBuf;
    
    /*
     ** Move particles to High memory.
     */
    iBuf = pkdSwapSpace(pkd);
    for (i=pkdLocal(pkd)-1;i>=0;--i)
	pkd->pStore[iBuf+i] = pkd->pStore[i];

    nBuf = pkdFreeStore(pkd)*sizeof(PARTICLE);
    nOutBytes = pkdLocal(pkd)*sizeof(PARTICLE);
    mdlSwap(pkd->mdl,idSwap,nBuf,&pkd->pStore[0], nOutBytes,
	    &nSndBytes, &nRcvBytes);
    assert(nSndBytes/sizeof(PARTICLE) == pkdLocal(pkd));
    pkd->nLocal = nRcvBytes/sizeof(PARTICLE);
    }

int pkdSwapSpace(PKD pkd)
{
	return(pkdFreeStore(pkd) - pkdLocal(pkd));
	}


int pkdFreeStore(PKD pkd)
{
	return(pkd->nStore);
	}

int pkdActive(PKD pkd)
{
	return(pkd->nActive);
	}

int pkdInactive(PKD pkd)
{
	return(pkd->nLocal - pkd->nActive);
	}

int pkdLocal(PKD pkd)
{
	return(pkd->nLocal);
	}

int pkdNodes(PKD pkd)
{
	return(pkd->nNodes);
	}


void pkdDomainColor(PKD pkd)
{
#ifdef COLORCODE
	int i;
	
	for (i=0;i<pkd->nLocal;++i) {
		pkd->pStore[i].fColor = (float)pkd->idSelf;
		}
#endif
	}


int pkdColOrdRejects(PKD pkd,int nOrdSplit,int iSplitSide)
{
	int nSplit,iRejects,i;

	if (iSplitSide) nSplit = pkdLowerOrdPart(pkd,nOrdSplit,0,pkdLocal(pkd)-1);
	else nSplit = pkdUpperOrdPart(pkd,nOrdSplit,0,pkdLocal(pkd)-1);
	pkd->nRejects = pkdLocal(pkd) - nSplit;
	iRejects = pkdFreeStore(pkd) - pkd->nRejects;
	pkd->nLocal = nSplit;
	/*
	 ** Move rejects to High memory.
	 */
	for (i=pkd->nRejects-1;i>=0;--i)
		pkd->pStore[iRejects+i] = pkd->pStore[pkd->nLocal+i];
	return(pkd->nRejects);
	}


int cmpParticles(const void *pva,const void *pvb)
{
	PARTICLE *pa = (PARTICLE *)pva;
	PARTICLE *pb = (PARTICLE *)pvb;

	return(pa->iOrder - pb->iOrder);
	}


void _pkdOrder(PKD pkd,PARTICLE *pTemp,int nStart,int i,int j)
{
	int nSplit,k,iOrd;

	if (j-i+1 > PKD_ORDERTEMP) {
		nSplit = (i+j+1)/2 + nStart;
		nSplit = pkdUpperOrdPart(pkd,nSplit,i,j);
		_pkdOrder(pkd,pTemp,nStart,i,nSplit-1);
		_pkdOrder(pkd,pTemp,nStart,nSplit,j);
		}
	else {
		/*
		 ** Reorder using the temporary storage!
		 */
		for (k=i;k<=j;++k) {
			iOrd = pkd->pStore[k].iOrder - nStart - i;
			pTemp[iOrd] = pkd->pStore[k];
			}
		for (k=i;k<=j;++k) {
			pkd->pStore[k] = pTemp[k-i];
			}
		}
	}


void pkdLocalOrder(PKD pkd,int nStart)
{
	/*
	 ** My ordering function seems to break on some machines, but
	 ** I still don't understand why. Use qsort instead.
	 **	PARTICLE pTemp[PKD_ORDERTEMP];
	 ** _pkdOrder(pkd,pTemp,nStart,0,pkd->nLocal-1);
	 */
	qsort(pkd->pStore,pkdLocal(pkd),sizeof(PARTICLE),cmpParticles);
	}


void pkdWriteTipsy(PKD pkd,char *pszFileName,int nStart,int nEnd,
				   int bStandard,double dvFac,double duTFac)
{
	FILE *fp;
	int i,j;
	struct dark_particle dp;
	struct gas_particle gp;
	struct star_particle sp;
	int nout;

	/*
	 ** First verify order of the particles!
	 */
	for (i=0;i<pkd->nLocal;++i) {
		if (pkd->pStore[i].iOrder != i+nStart) {
			mdlDiag(pkd->mdl,"Order error\n");
			break;
			}
		}
	if (nEnd - nStart + 1 != pkd->nLocal) mdlDiag(pkd->mdl,"Number error\n");
	/*
	 ** Seek past the header and up to nStart.
	 */
	fp = fopen(pszFileName,"r+");
	assert(fp != NULL);
	pkdSeek(pkd,fp,nStart,bStandard);
	if (bStandard) {
		float vTemp;
		XDR xdrs;
		/* 
		 ** Write Stuff!
		 */
		xdrstdio_create(&xdrs,fp,XDR_ENCODE);
		for (i=0;i+nStart < pkd->nGas && i < pkd->nLocal;++i) {
			xdr_float(&xdrs,&pkd->pStore[i].fMass);
			for (j=0;j<3;++j) {
				xdr_float(&xdrs,&pkd->pStore[i].r[j]);
				}
			for (j=0;j<3;++j) {
				vTemp = dvFac*pkd->pStore[i].v[j];			
				xdr_float(&xdrs,&vTemp);
				}
			xdr_float(&xdrs,&pkd->pStore[i].fDensity);
#ifdef GASOLINE
			/*
			 ** Convert thermal energy to tempertature.
			 */
			vTemp = duTFac*pkd->pStore[i].u;
			xdr_float(&xdrs,&vTemp);
			xdr_float(&xdrs,&pkd->pStore[i].fSoft);
			xdr_float(&xdrs,&pkd->pStore[i].fMetals);
#else
			vTemp = 0.0;
			xdr_float(&xdrs,&vTemp);
			xdr_float(&xdrs,&pkd->pStore[i].fSoft);
			xdr_float(&xdrs,&vTemp);
#endif
			xdr_float(&xdrs,&pkd->pStore[i].fPot);
			}
		for (;i+nStart < pkd->nGas+pkd->nDark && i < pkd->nLocal;++i) {
			xdr_float(&xdrs,&pkd->pStore[i].fMass);
			for (j=0;j<3;++j) {
				xdr_float(&xdrs,&pkd->pStore[i].r[j]);
				}
			for (j=0;j<3;++j) {
				vTemp = dvFac*pkd->pStore[i].v[j];			
				xdr_float(&xdrs,&vTemp);
				}
			xdr_float(&xdrs,&pkd->pStore[i].fSoft);
			xdr_float(&xdrs,&pkd->pStore[i].fPot);
			}
		for (;i+nStart < pkd->nGas+pkd->nDark+pkd->nStar && i < pkd->nLocal;++i) {
			xdr_float(&xdrs,&pkd->pStore[i].fMass);
			for (j=0;j<3;++j) {
				xdr_float(&xdrs,&pkd->pStore[i].r[j]);
				}
			for (j=0;j<3;++j) {
				vTemp = dvFac*pkd->pStore[i].v[j];			
				xdr_float(&xdrs,&vTemp);
				}
#ifdef GASOLINE
			xdr_float(&xdrs,&pkd->pStore[i].fMetals);
			xdr_float(&xdrs,&pkd->pStore[i].fTimeForm);
#else
			vTemp = 0.0;
			xdr_float(&xdrs,&vTemp);
			xdr_float(&xdrs,&vTemp);			
#endif
			xdr_float(&xdrs,&pkd->pStore[i].fSoft);
			xdr_float(&xdrs,&pkd->pStore[i].fPot);
			}
		xdr_destroy(&xdrs);
		}
	else {
		/* 
		 ** Write Stuff!
		 */
		for (i=0;i+nStart < pkd->nGas && i < pkd->nLocal;++i) {
			for (j=0;j<3;++j) {
				gp.pos[j] = pkd->pStore[i].r[j];
				gp.vel[j] = dvFac*pkd->pStore[i].v[j];
				}
			gp.mass = pkd->pStore[i].fMass;
			gp.hsmooth = pkd->pStore[i].fSoft;
			gp.phi = pkd->pStore[i].fPot;
			gp.rho = pkd->pStore[i].fDensity;
#ifdef GASOLINE
			gp.temp = duTFac*pkd->pStore[i].u;
			gp.metals = pkd->pStore[i].fMetals;
#else
			gp.temp = 0.0;
			gp.metals = 0.0;
#endif
			nout = fwrite(&gp,sizeof(struct gas_particle),1,fp);
			assert(nout == 1);
			}
		for (;i+nStart < pkd->nGas+pkd->nDark && i < pkd->nLocal;++i) {
			for (j=0;j<3;++j) {
				dp.pos[j] = pkd->pStore[i].r[j];
				dp.vel[j] = dvFac*pkd->pStore[i].v[j];
				}
			dp.mass = pkd->pStore[i].fMass;
			dp.eps = pkd->pStore[i].fSoft;
			dp.phi = pkd->pStore[i].fPot;
			nout = fwrite(&dp,sizeof(struct dark_particle),1,fp);
			assert(nout == 1);
			}
		for (;i+nStart < pkd->nGas+pkd->nDark+pkd->nStar && i < pkd->nLocal;++i) {
			for (j=0;j<3;++j) {
				sp.pos[j] = pkd->pStore[i].r[j];
				sp.vel[j] = dvFac*pkd->pStore[i].v[j];
				}
			sp.mass = pkd->pStore[i].fMass;
			sp.eps = pkd->pStore[i].fSoft;
			sp.phi = pkd->pStore[i].fPot;
#ifdef GASOLINE
			sp.metals = pkd->pStore[i].fMetals;
			sp.tform = pkd->pStore[i].fTimeForm;
#else
			sp.metals = 0.0;
			sp.tform = 0.0;
#endif
			nout = fwrite(&sp,sizeof(struct dark_particle),1,fp);
			assert(nout == 1);
			}
		}
	nout = fclose(fp);
	assert(nout == 0);
	}


void pkdSetSoft(PKD pkd,double dSoft)
{
	PARTICLE *p;
	int i,n;

	p = pkd->pStore;
	n = pkdLocal(pkd);
	for (i=0;i<n;++i) {
             p[i].fSoft = dSoft;
             }
        }


void pkdCombine(KDN *p1,KDN *p2,KDN *pOut)
{
	int j;

	/*
	 ** Combine the bounds.
	 */
	for (j=0;j<3;++j) {
		if (p2->bnd.fMin[j] < p1->bnd.fMin[j])
			pOut->bnd.fMin[j] = p2->bnd.fMin[j];
		else
			pOut->bnd.fMin[j] = p1->bnd.fMin[j];
		if (p2->bnd.fMax[j] > p1->bnd.fMax[j])
			pOut->bnd.fMax[j] = p2->bnd.fMax[j];
		else
			pOut->bnd.fMax[j] = p1->bnd.fMax[j];
		}
	/*
	 ** Find the center of mass and mass weighted softening.
	 */
    pOut->fMass = p1->fMass + p2->fMass;
	pOut->fSoft = (p1->fMass*p1->fSoft + p2->fMass*p2->fSoft)/pOut->fMass;
	for (j=0;j<3;++j) {
		pOut->r[j] = (p1->fMass*p1->r[j] + p2->fMass*p2->r[j])/pOut->fMass;
		}
	}


void pkdCalcCell(PKD pkd,KDN *pkdn,float *rcm,int iOrder,
				 struct pkdCalcCellStruct *pcc)
{
	int pj;
	double m,dx,dy,dz,d2,d1;

	/*
	 ** Initialize moments.
	 ** Initialize various B numbers.
	 */
	switch (iOrder) {	
	case 4:
		pcc->Hxxxx = 0.0;
		pcc->Hxyyy = 0.0;
		pcc->Hxxxy = 0.0;
		pcc->Hyyyy = 0.0;
		pcc->Hxxxz = 0.0;
		pcc->Hyyyz = 0.0;
		pcc->Hxxyy = 0.0;
		pcc->Hxxyz = 0.0;
		pcc->Hxyyz = 0.0;
		pcc->Hxxzz = 0.0;
		pcc->Hxyzz = 0.0;
		pcc->Hxzzz = 0.0;
		pcc->Hyyzz = 0.0;
		pcc->Hyzzz = 0.0;
		pcc->Hzzzz = 0.0;
		pcc->B6 = 0.0;
	case 3:
		pcc->Oxxx = 0.0;
		pcc->Oxyy = 0.0;
		pcc->Oxxy = 0.0;
		pcc->Oyyy = 0.0;
		pcc->Oxxz = 0.0;
		pcc->Oyyz = 0.0;
		pcc->Oxyz = 0.0;
		pcc->Oxzz = 0.0;
		pcc->Oyzz = 0.0;
		pcc->Ozzz = 0.0;
		pcc->B5 = 0.0;
	default:
		pcc->Qxx = 0.0;
		pcc->Qyy = 0.0;
		pcc->Qzz = 0.0;
		pcc->Qxy = 0.0;
		pcc->Qxz = 0.0;
		pcc->Qyz = 0.0;
		pcc->B2 = 0.0;
		pcc->B3 = 0.0;
		pcc->B4 = 0.0;
		}
	pcc->Bmax = 0.0;
	/*
	 ** Calculate moments and B numbers about center-of-mass.
	 */
	if (pkdn == NULL) pkdn = &pkd->kdNodes[pkd->iRoot];
	for (pj=pkdn->pLower;pj<=pkdn->pUpper;++pj) {
		m = pkd->pStore[pj].fMass;
		dx = pkd->pStore[pj].r[0] - rcm[0];
		dy = pkd->pStore[pj].r[1] - rcm[1];
		dz = pkd->pStore[pj].r[2] - rcm[2];
		d2 = dx*dx + dy*dy + dz*dz;
		d1 = sqrt(d2);
		if (d1 > pcc->Bmax) pcc->Bmax = d1;
		pcc->B2 += m*d2;
		pcc->B3 += m*d2*d1;
		pcc->B4 += m*d2*d2;
		pcc->B5 += m*d2*d2*d1;
		pcc->B6 += m*d2*d2*d2;
#ifdef COMPLETE_LOCAL
		d2 = 0.0;
#endif
		switch (iOrder) {
		case 4:
			/*
			 ** Calculate reduced hexadecapole moment...
			 */
			pcc->Hxxxx += m*(dx*dx*dx*dx - 6.0/7.0*d2*(dx*dx - 0.1*d2));
			pcc->Hxyyy += m*(dx*dy*dy*dy - 3.0/7.0*d2*dx*dy);
			pcc->Hxxxy += m*(dx*dx*dx*dy - 3.0/7.0*d2*dx*dy);
			pcc->Hyyyy += m*(dy*dy*dy*dy - 6.0/7.0*d2*(dy*dy - 0.1*d2));
			pcc->Hxxxz += m*(dx*dx*dx*dz - 3.0/7.0*d2*dx*dz);
			pcc->Hyyyz += m*(dy*dy*dy*dz - 3.0/7.0*d2*dy*dz);
			pcc->Hxxyy += m*(dx*dx*dy*dy - 1.0/7.0*d2*(dx*dx + dy*dy - 0.2*d2));
			pcc->Hxxyz += m*(dx*dx*dy*dz - 1.0/7.0*d2*dy*dz);
			pcc->Hxyyz += m*(dx*dy*dy*dz - 1.0/7.0*d2*dx*dz);
			pcc->Hxxzz += m*(dx*dx*dz*dz - 1.0/7.0*d2*(dx*dx + dz*dz - 0.2*d2));
			pcc->Hxyzz += m*(dx*dy*dz*dz - 1.0/7.0*d2*dx*dy);
			pcc->Hxzzz += m*(dx*dz*dz*dz - 3.0/7.0*d2*dx*dz);
			pcc->Hyyzz += m*(dy*dy*dz*dz - 1.0/7.0*d2*(dy*dy + dz*dz - 0.2*d2));
			pcc->Hyzzz += m*(dy*dz*dz*dz - 3.0/7.0*d2*dy*dz);
			pcc->Hzzzz += m*(dz*dz*dz*dz - 6.0/7.0*d2*(dz*dz - 0.1*d2));
		case 3:
			/*
			 ** Calculate reduced octopole moment...
			 */
			pcc->Oxxx += m*(dx*dx*dx - 0.6*d2*dx);
			pcc->Oxyy += m*(dx*dy*dy - 0.2*d2*dx);
			pcc->Oxxy += m*(dx*dx*dy - 0.2*d2*dy);
			pcc->Oyyy += m*(dy*dy*dy - 0.6*d2*dy);
			pcc->Oxxz += m*(dx*dx*dz - 0.2*d2*dz);
			pcc->Oyyz += m*(dy*dy*dz - 0.2*d2*dz);
			pcc->Oxyz += m*dx*dy*dz;
			pcc->Oxzz += m*(dx*dz*dz - 0.2*d2*dx);
			pcc->Oyzz += m*(dy*dz*dz - 0.2*d2*dy);
			pcc->Ozzz += m*(dz*dz*dz - 0.6*d2*dz);;
		default:
			/*
			 ** Calculate quadrupole moment...
			 */
			pcc->Qxx += m*dx*dx;
			pcc->Qyy += m*dy*dy;
			pcc->Qzz += m*dz*dz;
			pcc->Qxy += m*dx*dy;
			pcc->Qxz += m*dx*dz;
			pcc->Qyz += m*dy*dz;
			}
		}
	}


double fcnAbsMono(KDN *pkdn,double r)
{
	double t;

	t = r*(r - pkdn->mom.Bmax);
	t *= t;
	t = (3.0*pkdn->mom.B2 - 2.0*pkdn->mom.B3/r)/t;
	return t;
	}


double fcnAbsQuad(KDN *pkdn,double r)
{
	double t;

	t = r*(r - pkdn->mom.Bmax);
	t *= r*t;
	t = (4.0*pkdn->mom.B3 - 3.0*pkdn->mom.B4/r)/t;
	return t;
	}


double fcnAbsOct(KDN *pkdn,double r)
{
	double t;

	t = r*r*(r - pkdn->mom.Bmax);
	t *= t;
	t = (5.0*pkdn->mom.B4 - 4.0*pkdn->mom.B5/r)/t;
	return t;
	}


double fcnAbsHex(KDN *pkdn,double r)
{
	double t;

	t = r*r*(r - pkdn->mom.Bmax);
	t *= r*t;
	t = (6.0*pkdn->mom.B5 - 5.0*pkdn->mom.B6/r)/t;
	return t;
	}


double dRootBracket(KDN *pkdn,double dErrBnd,double (*fcn)(KDN *,double))
{
	double dLower,dUpper,dMid;
	double dLowerErr,dErrDif,dErrCrit;
	int iter = 0;

	dErrCrit = 1e-6*dErrBnd;
	dLower = (1.0 + 1e-6)*pkdn->mom.Bmax;
	dLowerErr = (*fcn)(pkdn,dLower);
	if (dLowerErr < dErrBnd) {
/*
		printf("iter:%d %g\n",iter,dLower);
*/
		return dLower;
		}
	/*
	 ** Hunt phase for upper bound.
	 */
	dUpper = 2*dLower;
	while (1) {
		dErrDif = dErrBnd - (*fcn)(pkdn,dUpper);
		if (dErrDif > dErrCrit) break;
		dUpper = 2*dUpper;
		}
	/*
	 ** Bracket root.
	 */
	while (1) {
		dMid = 0.5*(dLower + dUpper);
		dErrDif = dErrBnd - (*fcn)(pkdn,dMid);
		if (dErrDif < 0) {
			dLower = dMid;
			}
		else {
			dUpper = dMid;
			if (dErrDif < dErrCrit) break;
			}
		if (++iter > 32) break;
		}
/*
	printf("iter:%d %g\n",iter,dMid);
*/
	return dMid;
	}


double pkdCalcOpen(KDN *pkdn,int iOpenType,double dCrit,int iOrder)
{
	double dOpen;

	if (iOpenType == OPEN_ABSPAR) {
		switch (iOrder) {
		case 1:
			dOpen = dRootBracket(pkdn,dCrit,fcnAbsMono);
			break;
		case 2:
			dOpen = dRootBracket(pkdn,dCrit,fcnAbsQuad);
			break;
		case 3:
			dOpen = dRootBracket(pkdn,dCrit,fcnAbsOct);
			break;
		case 4:
			dOpen = dRootBracket(pkdn,dCrit,fcnAbsHex);
			break;
			}
		}
	else if (iOpenType == OPEN_JOSH) {
		/*
		 ** Set openening criterion to an approximation of Josh's theta.
		 */
		dOpen = 2/sqrt(3.0)*pkdn->mom.Bmax/dCrit;
		if (dOpen < pkdn->mom.Bmax) dOpen = pkdn->mom.Bmax;
		}
	else {
		/*
		 ** Set opening criterion to the minimal, i.e., Bmax.
		 */
		dOpen = pkdn->mom.Bmax;
		}
	return(dOpen);
	}


void pkdUpPass(PKD pkd,int iCell,int iOpenType,double dCrit,int iOrder)
{
	KDN *c;
	PARTICLE *p;
	int l,u,pj,j;
	double dOpen;

	c = pkd->kdNodes;
	p = pkd->pStore;
	l = c[iCell].pLower;
	u = c[iCell].pUpper;
	if (c[iCell].iDim >= 0) {
		pkdUpPass(pkd,LOWER(iCell),iOpenType,dCrit,iOrder);
		pkdUpPass(pkd,UPPER(iCell),iOpenType,dCrit,iOrder);
		pkdCombine(&c[LOWER(iCell)],&c[UPPER(iCell)],&c[iCell]);
		}
	else {
		c[iCell].fMass = 0.0;
		c[iCell].fSoft = 0.0;
		for (j=0;j<3;++j) {
			c[iCell].bnd.fMin[j] = p[u].r[j];
			c[iCell].bnd.fMax[j] = p[u].r[j];
			c[iCell].r[j] = 0.0;
			}
		for (pj=l;pj<=u;++pj) {
			for (j=0;j<3;++j) {
				if (p[pj].r[j] < c[iCell].bnd.fMin[j])
					c[iCell].bnd.fMin[j] = p[pj].r[j];
				if (p[pj].r[j] > c[iCell].bnd.fMax[j])
					c[iCell].bnd.fMax[j] = p[pj].r[j];
				}
			/*
			 ** Find center of mass and total mass and mass weighted softening.
			 */
			c[iCell].fMass += p[pj].fMass;
			c[iCell].fSoft += p[pj].fMass*p[pj].fSoft;
			for (j=0;j<3;++j) {
				c[iCell].r[j] += p[pj].fMass*p[pj].r[j];
				}
			}
		for (j=0;j<3;++j) c[iCell].r[j] /= c[iCell].fMass;
		c[iCell].fSoft /= c[iCell].fMass;
		}
	/*
	 ** Calculate multipole moments.
	 */
	pkdCalcCell(pkd,&c[iCell],c[iCell].r,iOrder,&c[iCell].mom);
	dOpen = pkdCalcOpen(&c[iCell],iOpenType,dCrit,iOrder);
	c[iCell].fOpen2 = dOpen*dOpen;
	}


/*
 ** JST's Select
 */
void pkdSelect(PKD pkd,int d,int k,int l,int r)
{
	PARTICLE *p,t;
	float v;
	int i,j;

	p = pkd->pStore;
	while (r > l) {
		v = p[k].r[d];
		t = p[r];
		p[r] = p[k];
		p[k] = t;
		i = l - 1;
		j = r;
		while (1) {
			while (i < j) if (p[++i].r[d] >= v) break;
			while (i < j) if (p[--j].r[d] <= v) break;
			t = p[i];
			p[i] = p[j];
			p[j] = t;
			if (j <= i) break;
			}
		p[j] = p[i];
		p[i] = p[r];
		p[r] = t;
		if (i >= k) r = i - 1;
		if (i <= k) l = i + 1;
		}
	}


int NumBinaryNodes(PKD pkd,int nBucket,int pLower,int pUpper)
{
	BND bnd;
	int i,j,m,d,l,u;
	float fSplit;

	if (pLower > pUpper) return(0);
	else {
		if (pUpper-pLower+1 > nBucket) {
			/*
			 ** We need to find the bounding box.
			 */
			for (j=0;j<3;++j) {
				bnd.fMin[j] = pkd->pStore[pLower].r[j];
				bnd.fMax[j] = pkd->pStore[pLower].r[j];
				}
			for (i=pLower+1;i<=pUpper;++i) {
				for (j=0;j<3;++j) {
					if (pkd->pStore[i].r[j] < bnd.fMin[j]) 
						bnd.fMin[j] = pkd->pStore[i].r[j];
					else if (pkd->pStore[i].r[j] > bnd.fMax[j])
						bnd.fMax[j] = pkd->pStore[i].r[j];
					}
				}	
			/*
			 ** Now we need to determine the longest axis.
			 */
			d = 0;
			for (j=1;j<3;++j) {
				if (bnd.fMax[j]-bnd.fMin[j] > bnd.fMax[d]-bnd.fMin[d]) {
					d = j;
					}
				}
			/*
			 ** Now we do the split.
			 */
			fSplit = 0.5*(bnd.fMin[d]+bnd.fMax[d]);
			m = pkdUpperPart(pkd,d,fSplit,pLower,pUpper);
			/*
			 ** Recursive node count.
			 */
			l = NumBinaryNodes(pkd,nBucket,pLower,m-1);
			u = NumBinaryNodes(pkd,nBucket,m,pUpper);
			/*
			 ** Careful, this assert only applies when we are doing the
			 ** squeezing!
			 */
			assert(l > 0 && u > 0);
			return(l+u+1);
			}
		else {
			return(1);
			}
		}
	}


int BuildBinary(PKD pkd,int nBucket,int pLower,int pUpper,int iOpenType,
				double dCrit,int iOrder)
{
	KDN *pkdn;
	int i,j,m,d,c;
	float fm;
	double dOpen;

	if (pLower > pUpper) return(-1);
	else {
		/*
		 ** Grab a cell from the cell storage!
		 */
		assert(pkd->iFreeCell < pkd->nNodes);
		c = pkd->iFreeCell++;
		pkdn = &pkd->kdNodes[c];
		pkdn->pLower = pLower;
		pkdn->pUpper = pUpper;
		/*
		 ** We need to find the bounding box.
		 */
		for (j=0;j<3;++j) {
			pkdn->bnd.fMin[j] = pkd->pStore[pLower].r[j];
			pkdn->bnd.fMax[j] = pkd->pStore[pLower].r[j];
			}
		for (i=pLower+1;i<=pUpper;++i) {
			for (j=0;j<3;++j) {
				if (pkd->pStore[i].r[j] < pkdn->bnd.fMin[j]) 
					pkdn->bnd.fMin[j] = pkd->pStore[i].r[j];
				else if (pkd->pStore[i].r[j] > pkdn->bnd.fMax[j])
					pkdn->bnd.fMax[j] = pkd->pStore[i].r[j];
				}
			}	
		if (pUpper-pLower+1 > nBucket) {
			/*
			 ** Now we need to determine the longest axis.
			 */
			d = 0;
			for (j=1;j<3;++j) {
				if (pkdn->bnd.fMax[j]-pkdn->bnd.fMin[j] > 
					pkdn->bnd.fMax[d]-pkdn->bnd.fMin[d]) {
					d = j;
					}
				}
			pkdn->iDim = d;
			/*
			 ** Now we do the split.
			 */
			pkdn->fSplit = 0.5*(pkdn->bnd.fMin[d]+pkdn->bnd.fMax[d]);
			m = pkdUpperPart(pkd,d,pkdn->fSplit,pLower,pUpper);
			pkdn->iLower = BuildBinary(pkd,nBucket,pLower,m-1,iOpenType,
									   dCrit,iOrder);
			pkdn->iUpper = BuildBinary(pkd,nBucket,m,pUpper,iOpenType,
									   dCrit,iOrder);
			/*
			 ** Careful, this assert only applies when we are doing the
			 ** squeezing!
			 */
			assert(pkdn->iLower != -1 && pkdn->iUpper != -1);
			/*
			 ** Now calculate the mass, center of mass and mass weighted
			 ** softening radius.
			 */
			pkdn->fMass = 0.0;
			pkdn->fSoft = 0.0;
			for (j=0;j<3;++j) {
				pkdn->r[j] = 0.0;
				}
			if (pkdn->iLower != -1) {
				fm = pkd->kdNodes[pkdn->iLower].fMass;
				pkdn->fMass += fm;
				pkdn->fSoft += fm*pkd->kdNodes[pkdn->iLower].fSoft;
				for (j=0;j<3;++j) {
					pkdn->r[j] += fm*pkd->kdNodes[pkdn->iLower].r[j];
					}
				}
			if (pkdn->iUpper != -1) {
				fm = pkd->kdNodes[pkdn->iUpper].fMass;
				pkdn->fMass += fm;
				pkdn->fSoft += fm*pkd->kdNodes[pkdn->iUpper].fSoft;
				for (j=0;j<3;++j) {
					pkdn->r[j] += fm*pkd->kdNodes[pkdn->iUpper].r[j];
					}
				}
			pkdn->fSoft /= pkdn->fMass;
			for (j=0;j<3;++j) {
				pkdn->r[j] /= pkdn->fMass;
				}
			}
		else {
			pkdn->iDim = -1; /* it is a bucket! */
			pkdn->fSplit = 0.0;
			pkdn->iLower = -1;
			pkdn->iUpper = -1;
			/*
			 ** Calculate the bucket quantities.
			 */
			fm = pkd->pStore[pkdn->pLower].fMass;
			pkdn->fMass = fm;
			pkdn->fSoft = fm*pkd->pStore[pkdn->pLower].fSoft;
			for (j=0;j<3;++j) {
				pkdn->r[j] = fm*pkd->pStore[pLower].r[j];
				}
			for (i=pkdn->pLower+1;i<=pkdn->pUpper;++i) {
				fm = pkd->pStore[i].fMass;
				pkdn->fMass += fm;
				pkdn->fSoft += fm*pkd->pStore[i].fSoft;
				for (j=0;j<3;++j) {
					pkdn->r[j] += fm*pkd->pStore[i].r[j];
					}
				}
			pkdn->fSoft /= pkdn->fMass;
			for (j=0;j<3;++j) {
				pkdn->r[j] /= pkdn->fMass;
				}
			}
		/*
		 ** Calculate multipole moments.
		 */
		pkdCalcCell(pkd,pkdn,pkdn->r,iOrder,&pkdn->mom);
		dOpen = pkdCalcOpen(pkdn,iOpenType,dCrit,iOrder);
		pkdn->fOpen2 = dOpen*dOpen;
		return(c);
		}
	}


void pkdThreadTree(PKD pkd,int iCell,int iNext)
{
	int l,u;

	if (iCell == -1) return;
	else if (pkd->kdNodes[iCell].iDim != -1) {
		l = pkd->kdNodes[iCell].iLower;
		u = pkd->kdNodes[iCell].iUpper;
		if (u == -1) {
			assert(l != -1);
			pkdThreadTree(pkd,l,iNext);
			}
		else if (l == -1) {
			assert(u != -1);
			pkdThreadTree(pkd,u,iNext);
			/* 
			 ** It is convenient to change the "down" pointer in this case.
			 */
			pkd->kdNodes[iCell].iLower = u;
			}
		else {
			pkdThreadTree(pkd,l,u);
			pkdThreadTree(pkd,u,iNext);
			}
		pkd->kdNodes[iCell].iUpper = iNext;
		}
	else {
		pkd->kdNodes[iCell].iLower = -1;	/* Just make sure! */
		pkd->kdNodes[iCell].iUpper = iNext;
		}
	}


void pkdBuildBinary(PKD pkd,int nBucket,int iOpenType,double dCrit,
					int iOrder,int bActiveOnly,KDN *pRoot)
{
	/*
	 ** Make sure the particles are in Active/Inactive order.
	 */
	pkdActiveOrder(pkd);
	if (pkd->kdNodes) {
		/*
		 ** Close caching space and free up nodes.
		 */
		mdlFinishCache(pkd->mdl,CID_CELL);
		mdlFree(pkd->mdl,pkd->kdNodes);
		}
	/*
	 ** First problem is to figure out how many cells we need to
	 ** allocate. We need to do this in advance because we need to
	 ** synchronize to allocate the memory with mdlMalloc().
	 */
	if (bActiveOnly) {
		pkd->nNodes = NumBinaryNodes(pkd,nBucket,0,pkd->nActive-1);
		}
	else {
		pkd->nNodes = NumBinaryNodes(pkd,nBucket,0,pkd->nLocal-1);
		}
	/*
	 ** We need at least one particle per processor.
	 */
	assert(pkd->nNodes > 0);

	pkd->kdNodes = mdlMalloc(pkd->mdl,pkd->nNodes*sizeof(KDN));
	assert(pkd->kdNodes != NULL);
	/*
	 ** Now we really build the tree.
	 */
	pkd->iFreeCell = 0;
	if (bActiveOnly) {
		pkd->iRoot = BuildBinary(pkd,nBucket,0,pkd->nActive-1,
								 iOpenType,dCrit,iOrder);
		}
	else {
		pkd->iRoot = BuildBinary(pkd,nBucket,0,pkd->nLocal-1,
								 iOpenType,dCrit,iOrder);
		}
	assert(pkd->iFreeCell == pkd->nNodes);
	/*
	 ** Thread the tree.
	 */
	pkdThreadTree(pkd,pkd->iRoot,-1);
	*pRoot = pkd->kdNodes[pkd->iRoot];
	/*
	 ** Finally activate a read only cache for remote access.
	 */
	mdlROcache(pkd->mdl,CID_CELL,pkd->kdNodes,sizeof(KDN),pkdNodes(pkd));
	}


void pkdBuildLocal(PKD pkd,int nBucket,int iOpenType,double dCrit,
				   int iOrder,int bActiveOnly,KDN *pRoot)
{
	int l,n,i,d,m,j,diff;
	KDN *c;
	char ach[256];
	BND bndDum;

	/*
	 ** Make sure the particles are in Active/Inactive order.
	 */
	pkdActiveOrder(pkd);
	pkd->nBucket = nBucket;
	if (bActiveOnly) n = pkd->nActive;
	else n = pkd->nLocal;
	pkd->nLevels = 1;
	l = 1;
	while (n > nBucket) {
		n = n>>1;
		l = l<<1;
		++pkd->nLevels;
		}
	pkd->nSplit = l;
	pkd->nNodes = l<<1;
	if (pkd->kdNodes) {
		/*
		 ** Close caching space and free up nodes.
		 */
		mdlFinishCache(pkd->mdl,CID_CELL);
		mdlFree(pkd->mdl,pkd->kdNodes);
		}
	if(n == 0) {
	    pkd->kdNodes = NULL;
	    return;
	    }
	pkd->kdNodes = mdlMalloc(pkd->mdl,pkd->nNodes*sizeof(KDN));
	assert(pkd->kdNodes != NULL);
	sprintf(ach,"nNodes:%d nSplit:%d nLevels:%d nBucket:%d\n",
			pkd->nNodes,pkd->nSplit,pkd->nLevels,nBucket);
	mdlDiag(pkd->mdl,ach);
	/*
	 ** Set up ROOT node
	 */
	c = pkd->kdNodes;
	pkd->iRoot = ROOT;
	c[pkd->iRoot].pLower = 0;
	if (bActiveOnly) {
		c[pkd->iRoot].pUpper = pkd->nActive-1;
		}
	else {
		c[pkd->iRoot].pUpper = pkd->nLocal-1;
		}
	/*
	 ** determine the local bound of the particles.
	 */
	if (bActiveOnly) {
		pkdCalcBound(pkd,&bndDum,&c[pkd->iRoot].bnd);
		}
	else {
		pkdCalcBound(pkd,&c[pkd->iRoot].bnd,&bndDum);
		}
	i = pkd->iRoot;
	while (1) {
		assert(c[i].pUpper - c[i].pLower + 1 > 0);
		if (i < pkd->nSplit && (c[i].pUpper - c[i].pLower) > 0) {
			d = 0;
			for (j=1;j<3;++j) {
				if (c[i].bnd.fMax[j]-c[i].bnd.fMin[j] > 
					c[i].bnd.fMax[d]-c[i].bnd.fMin[d]) d = j;
				}
			c[i].iDim = d;
			m = (c[i].pLower + c[i].pUpper)/2;
			pkdSelect(pkd,d,m,c[i].pLower,c[i].pUpper);
			c[i].fSplit = pkd->pStore[m].r[d];
			c[i].iLower = LOWER(i);
			c[i].iUpper = UPPER(i);
			c[LOWER(i)].bnd = c[i].bnd;
			c[LOWER(i)].bnd.fMax[d] = c[i].fSplit;
			c[LOWER(i)].pLower = c[i].pLower;
			c[LOWER(i)].pUpper = m;
			c[UPPER(i)].bnd = c[i].bnd;
			c[UPPER(i)].bnd.fMin[d] = c[i].fSplit;
			c[UPPER(i)].pLower = m+1;
			c[UPPER(i)].pUpper = c[i].pUpper;
			diff = (m-c[i].pLower+1)-(c[i].pUpper-m);
			assert(diff == 0 || diff == 1);
			i = LOWER(i);
			}
		else {
			c[i].iDim = -1;		/* to indicate a bucket! */
			c[i].iLower = -1;
			c[i].iUpper = -1;
			SETNEXT(i);
			if (i == pkd->iRoot) break;
			}
		}
	pkdUpPass(pkd,pkd->iRoot,iOpenType,dCrit,iOrder);
	/*
	 ** Thread the tree.
	 */
	pkdThreadTree(pkd,pkd->iRoot,-1);
	*pRoot = c[pkd->iRoot];
	/*
	 ** Finally activate a read only cache for remote access.
	 */
	mdlROcache(pkd->mdl,CID_CELL,pkd->kdNodes,sizeof(KDN),pkdNodes(pkd));
	}


void pkdBucketWeight(PKD pkd,int iBucket,float fWeight)
{
	KDN *pbuc;
	int pj;
	
	pbuc = &pkd->kdNodes[iBucket];
	for (pj=pbuc->pLower;pj<=pbuc->pUpper;++pj) {
		if (pkd->pStore[pj].iActive) 
			pkd->pStore[pj].fWeight = fWeight;
		}
	}


void pkdColorCell(PKD pkd,int iCell,float fColor)
{
#ifdef COLORCODE
	KDN *pkdn;
	int pj;
	
	pkdn = &pkd->kdNodes[iCell];
	for (pj=pkdn->pLower;pj<=pkdn->pUpper;++pj) {
		pkd->pStore[pj].fColor = fColor;
		}
#endif	
	}


void pkdGravAll(PKD pkd,int nReps,int bPeriodic,int iOrder,int iEwOrder,
				double fEwCut,double fEwhCut,int *nActive,
				double *pdPartSum,double *pdCellSum,CASTAT *pcs,
				double *pdFlop)
{
	KDN *c = pkd->kdNodes;
	int iCell,n;
	float fWeight;
	double dFlopI, dFlopE;
	int i;

	*pdFlop = 0.0;
	pkdClearTimer(pkd,1);
	pkdClearTimer(pkd,2);
	pkdClearTimer(pkd,3);
	/*
	 ** Set up Ewald tables and stuff.
	 */
	if (bPeriodic) {
	    pkdEwaldInit(pkd,fEwhCut,iEwOrder);		/* ignored in Flop count! */
		}
	/*
	 ** Start particle caching space (cell cache already active).
	 */
	mdlROcache(pkd->mdl,CID_PARTICLE,pkd->pStore,sizeof(PARTICLE),
			   pkdLocal(pkd));
	/*
	 ** Walk over the local buckets!
	 */
	*nActive = 0;
	*pdPartSum = 0.0;
	*pdCellSum = 0.0;
	iCell = pkd->iRoot;
	while (iCell != -1) {
		if (c[iCell].iLower != -1) {
			iCell = c[iCell].iLower;
			continue;
			}
		n = 0;
		for (i=c[iCell].pLower;i<=c[iCell].pUpper;++i) {
			if (pkd->pStore[i].iActive) ++n;
			}
		if (n > 0) {
			/*
			 ** Calculate gravity on this bucket.
			 */
			pkdStartTimer(pkd,1);
			pkdBucketWalk(pkd,iCell,nReps,iOrder);		/* ignored in Flop count! */
			pkdStopTimer(pkd,1);
			*nActive += n;
			*pdPartSum += n*pkd->nPart + 
				n*(2*(c[iCell].pUpper-c[iCell].pLower) - n + 1)/2;
			*pdCellSum += n*(pkd->nCellSoft + pkd->nCellNewt);
			pkdStartTimer(pkd,2);
			dFlopI = pkdBucketInteract(pkd,iCell,iOrder);
			*pdFlop += dFlopI;
			pkdStopTimer(pkd,2);
			/*
			 * Now do Ewald part.
			 */
			if (bPeriodic) {
				pkdStartTimer(pkd,3);
				dFlopE = pkdBucketEwald(pkd,iCell,nReps,fEwCut,iEwOrder);
				*pdFlop += dFlopE;
				pkdStopTimer(pkd,3);
				}
			fWeight = dFlopI + dFlopE;
			/*
			 ** pkdBucketWeight, only updates the weights of the active
			 ** particles. Although this isn't really a requirement it
			 ** might be a good idea, if weights correspond to different 
			 ** tasks at different times.
			 */
			pkdBucketWeight(pkd,iCell,fWeight);
			}
		iCell = c[iCell].iUpper;
		}
	/*
	 ** Get caching statistics.
	 */
	pcs->dcNumAccess = mdlNumAccess(pkd->mdl,CID_CELL);
	pcs->dcMissRatio = mdlMissRatio(pkd->mdl,CID_CELL);
	pcs->dcCollRatio = mdlCollRatio(pkd->mdl,CID_CELL);
	pcs->dcMinRatio = mdlMinRatio(pkd->mdl,CID_CELL);
	pcs->dpNumAccess = mdlNumAccess(pkd->mdl,CID_PARTICLE);
	pcs->dpMissRatio = mdlMissRatio(pkd->mdl,CID_PARTICLE);
	pcs->dpCollRatio = mdlCollRatio(pkd->mdl,CID_PARTICLE);
	pcs->dpMinRatio = mdlMinRatio(pkd->mdl,CID_PARTICLE);
	/*
	 ** Stop particle caching space.
	 */
	mdlFinishCache(pkd->mdl,CID_PARTICLE);
	}


void pkdCalcE(PKD pkd,double *T,double *U)
{
	PARTICLE *p;
	int i,n;
	float vx,vy,vz;

	p = pkd->pStore;
	n = pkdLocal(pkd);
	*T = 0.0;
	*U = 0.0;
	for (i=0;i<n;++i) {
		*U += 0.5*p[i].fMass*p[i].fPot;
		vx = p[i].v[0];
		vy = p[i].v[1];
		vz = p[i].v[2];
		*T += 0.5*p[i].fMass*(vx*vx + vy*vy + vz*vz);
		}
	}


void pkdDrift(PKD pkd,double dDelta,float fCenter[3],int bPeriodic)
{
	PARTICLE *p;
	int i,j,n;

	p = pkd->pStore;
	n = pkdLocal(pkd);
	for (i=0;i<n;++i) {
		for (j=0;j<3;++j) {
			p[i].r[j] += dDelta*p[i].v[j];
			if (bPeriodic) {
				if (p[i].r[j] >= fCenter[j]+0.5*pkd->fPeriod[j])
					p[i].r[j] -= pkd->fPeriod[j];
				if (p[i].r[j] < fCenter[j]-0.5*pkd->fPeriod[j])
					p[i].r[j] += pkd->fPeriod[j];
				}
			}
		}
	}


void pkdKick(PKD pkd,double dvFacOne,double dvFacTwo, double dvPredFacOne,
	     double dvPredFacTwo)
{
	PARTICLE *p;
	int i,j,n;

	p = pkd->pStore;
	n = pkdLocal(pkd);
	for (i=0;i<n;++i) {
	    if(p[i].iActive) {
#ifdef GASOLINE
	        if(pkdIsGas(pkd, &p[i])) {
		    for (j=0;j<3;++j) {
			p[i].vPred[j] = p[i].v[j]*dvPredFacOne + p[i].a[j]*dvPredFacTwo;
			}
		    }
	      
#endif
		for (j=0;j<3;++j) {
			p[i].v[j] = p[i].v[j]*dvFacOne + p[i].a[j]*dvFacTwo;
			}
		}
	    }
	}


void pkdReadCheck(PKD pkd,char *pszFileName,int iVersion,int iOffset,
				  int nStart,int nLocal)
{
	FILE *fp;
	CHKPART cp;
	long lStart;
	int i,j;

	pkd->nLocal = nLocal;
	/*
	 ** Seek past the header and up to nStart.
	 */
	fp = fopen(pszFileName,"r");
	assert(fp != NULL);
	lStart = iOffset+nStart*sizeof(CHKPART);
	fseek(fp,lStart,SEEK_SET);
	/*
	 ** Read Stuff!
	 */
	for (i=0;i<nLocal;++i) {
		fread(&cp,sizeof(CHKPART),1,fp);
		pkd->pStore[i].iOrder = cp.iOrder;
		pkd->pStore[i].fMass = cp.fMass;
		pkd->pStore[i].fSoft = cp.fSoft;
		for (j=0;j<3;++j) {
			pkd->pStore[i].r[j] = cp.r[j];
			pkd->pStore[i].v[j] = cp.v[j];
			}
		pkd->pStore[i].iActive = 1;
		pkd->pStore[i].iRung = 0;
		pkd->pStore[i].fWeight = 1.0;	/* set the initial weight to 1.0 */
		pkd->pStore[i].fDensity = 0.0;
		pkd->pStore[i].fBall2 = 0.0;
		}
	fclose(fp);	
	}


void pkdWriteCheck(PKD pkd,char *pszFileName,int iOffset,int nStart)
{
	FILE *fp;
	CHKPART cp;
	long lStart;
	int i,j,nLocal;

	/*
	 ** Seek past the header and up to nStart.
	 */
	fp = fopen(pszFileName,"r+");
	assert(fp != NULL);
	lStart = iOffset+nStart*sizeof(CHKPART);
	fseek(fp,lStart,0);
	/* 
	 ** Write Stuff!
	 */
	nLocal = pkdLocal(pkd);
	for (i=0;i<nLocal;++i) {
		cp.iOrder = pkd->pStore[i].iOrder;
		cp.fMass = pkd->pStore[i].fMass;
		cp.fSoft = pkd->pStore[i].fSoft;
		for (j=0;j<3;++j) {
			cp.r[j] = pkd->pStore[i].r[j];
			cp.v[j] = pkd->pStore[i].v[j];
			}
		fwrite(&cp,sizeof(CHKPART),1,fp);
		}
	fclose(fp);
	}


void pkdDistribCells(PKD pkd,int nCell,KDN *pkdn)
{
	int i;

	if (pkd->kdTop != NULL) free(pkd->kdTop);
	if (pkd->piLeaf != NULL) free(pkd->piLeaf);
	pkd->kdTop = malloc(nCell*sizeof(KDN));
	assert(pkd->kdTop != NULL);
	pkd->piLeaf = malloc(pkd->nThreads*sizeof(int));
	assert(pkd->piLeaf != NULL);
	for (i=1;i<nCell;++i) {
		if (pkdn[i].pUpper) {
			pkd->kdTop[i] = pkdn[i];
			if (pkdn[i].pLower >= 0) pkd->piLeaf[pkdn[i].pLower] = i;
			}
		}
	}


void pkdCalcRoot(PKD pkd,struct ilCellNewt *pcc)
{
	KDN *pkdn;
	int pj;
	double m,dx,dy,dz;
	double d2;

	/*
	 ** Initialize moments.
	 */
	pcc->xxxx = 0.0;
	pcc->xyyy = 0.0;
	pcc->xxxy = 0.0;
	pcc->yyyy = 0.0;
	pcc->xxxz = 0.0;
	pcc->yyyz = 0.0;
	pcc->xxyy = 0.0;
	pcc->xxyz = 0.0;
	pcc->xyyz = 0.0;
	pcc->xxzz = 0.0;
	pcc->xyzz = 0.0;
	pcc->xzzz = 0.0;
	pcc->yyzz = 0.0;
	pcc->yzzz = 0.0;
	pcc->zzzz = 0.0;
	pcc->xxx = 0.0;
	pcc->xyy = 0.0;
	pcc->xxy = 0.0;
	pcc->yyy = 0.0;
	pcc->xxz = 0.0;
	pcc->yyz = 0.0;
	pcc->xyz = 0.0;
	pcc->xzz = 0.0;
	pcc->yzz = 0.0;
	pcc->zzz = 0.0;
	/*
	 ** Calculate moments.
	 */
	pkdn = &pkd->kdNodes[pkd->iRoot];
	for (pj=pkdn->pLower;pj<=pkdn->pUpper;++pj) {
		m = pkd->pStore[pj].fMass;
		dx = pkd->pStore[pj].r[0] - pkdn->r[0];
		dy = pkd->pStore[pj].r[1] - pkdn->r[1];
		dz = pkd->pStore[pj].r[2] - pkdn->r[2];
#ifdef REDUCED_EWALD
		d2 = dx*dx + dy*dy + dz*dz;
#else
		d2 = 0.0;
#endif
		pcc->xxxx += m*(dx*dx*dx*dx - 6.0/7.0*d2*(dx*dx - 0.1*d2));
		pcc->xyyy += m*(dx*dy*dy*dy - 3.0/7.0*d2*dx*dy);
		pcc->xxxy += m*(dx*dx*dx*dy - 3.0/7.0*d2*dx*dy);
		pcc->yyyy += m*(dy*dy*dy*dy - 6.0/7.0*d2*(dy*dy - 0.1*d2));
		pcc->xxxz += m*(dx*dx*dx*dz - 3.0/7.0*d2*dx*dz);
		pcc->yyyz += m*(dy*dy*dy*dz - 3.0/7.0*d2*dy*dz);
		pcc->xxyy += m*(dx*dx*dy*dy - 1.0/7.0*d2*(dx*dx + dy*dy - 0.2*d2));
		pcc->xxyz += m*(dx*dx*dy*dz - 1.0/7.0*d2*dy*dz);
		pcc->xyyz += m*(dx*dy*dy*dz - 1.0/7.0*d2*dx*dz);
		pcc->xxzz += m*(dx*dx*dz*dz - 1.0/7.0*d2*(dx*dx + dz*dz - 0.2*d2));
		pcc->xyzz += m*(dx*dy*dz*dz - 1.0/7.0*d2*dx*dy);
		pcc->xzzz += m*(dx*dz*dz*dz - 3.0/7.0*d2*dx*dz);
		pcc->yyzz += m*(dy*dy*dz*dz - 1.0/7.0*d2*(dy*dy + dz*dz - 0.2*d2));
		pcc->yzzz += m*(dy*dz*dz*dz - 3.0/7.0*d2*dy*dz);
		pcc->zzzz += m*(dz*dz*dz*dz - 6.0/7.0*d2*(dz*dz - 0.1*d2));
		/*
		 ** Calculate octopole moment...
		 */
		pcc->xxx += m*(dx*dx*dx - 0.6*d2*dx);
		pcc->xyy += m*(dx*dy*dy - 0.2*d2*dx);
		pcc->xxy += m*(dx*dx*dy - 0.2*d2*dy);
		pcc->yyy += m*(dy*dy*dy - 0.6*d2*dy);
		pcc->xxz += m*(dx*dx*dz - 0.2*d2*dz);
		pcc->yyz += m*(dy*dy*dz - 0.2*d2*dz);
		pcc->xyz += m*dx*dy*dz;
		pcc->xzz += m*(dx*dz*dz - 0.2*d2*dx);
		pcc->yzz += m*(dy*dz*dz - 0.2*d2*dy);
		pcc->zzz += m*(dz*dz*dz - 0.6*d2*dz);
		}
	}


void pkdDistribRoot(PKD pkd,struct ilCellNewt *pcc)
{
	KDN *pkdn;
	double tr;

	pkd->ilcnRoot = *pcc;
	/*
	 ** Must set the quadrupole, mass and cm.
	 */
	pkdn = &pkd->kdTop[ROOT];
#ifdef REDUCED_EWALD
	tr = pkdn->mom.Qxx + pkdn->mom.Qyy + pkdn->mom.Qzz;
#else
	tr = 0.0;
#endif
	pkd->ilcnRoot.m = pkdn->fMass;
	pkd->ilcnRoot.x = pkdn->r[0];
	pkd->ilcnRoot.y = pkdn->r[1];
	pkd->ilcnRoot.z = pkdn->r[2];
	pkd->ilcnRoot.xx = pkdn->mom.Qxx - tr/3.0;
	pkd->ilcnRoot.xy = pkdn->mom.Qxy;
	pkd->ilcnRoot.xz = pkdn->mom.Qxz;
	pkd->ilcnRoot.yy = pkdn->mom.Qyy - tr/3.0;
	pkd->ilcnRoot.yz = pkdn->mom.Qyz;
	pkd->ilcnRoot.zz = pkdn->mom.Qzz - tr/3.0;
	}


double pkdMassCheck(PKD pkd) 
{
	double dMass=0.0;
	int i,iRej;

	for (i=0;i<pkdLocal(pkd);++i) {
		dMass += pkd->pStore[i].fMass;
		}
	iRej = pkdFreeStore(pkd) - pkd->nRejects;
	for (i=0;i<pkd->nRejects;++i) {
		dMass += pkd->pStore[iRej+i].fMass;
		}
	return(dMass);
	}

void
pkdSetRung(PKD pkd, int iRung)
{
    int i;
    
    for(i = 0; i < pkdLocal(pkd); ++i) {
	pkd->pStore[i].iRung = iRung;
	}
    }

void
pkdActiveRung(PKD pkd, int iRung, int bGreater)
{
    int i;
    int nActive;
    char out[128];
    
    nActive = 0;
    for(i = 0; i < pkdLocal(pkd); ++i) {
	if(pkd->pStore[i].iRung == iRung
	   || (bGreater && pkd->pStore[i].iRung > iRung)) {
	    pkd->pStore[i].iActive = 1;
	    ++nActive;
	    }
	else
	    pkd->pStore[i].iActive = 0;
	}
    sprintf(out, "nActive: %d\n", nActive);
    mdlDiag(pkd->mdl, out);
    }


int
pkdCurrRung(PKD pkd, int iRung)
{
    int i;
    int iCurrent;
    
    iCurrent = 0;
    for(i = 0; i < pkdLocal(pkd); ++i) {
	if(pkd->pStore[i].iRung == iRung) {
	    iCurrent = 1;
	    break;
	    }
	}
    return iCurrent;
    }

int
pkdDensityRung(PKD pkd, int iRung, double dDelta, double dEta,
	       double dRhoFac, int bAll)
{
    int i;
    int iMaxRung;
    int iTempRung;
    int iSteps;
    
    iMaxRung = 0;
    for(i = 0; i < pkdLocal(pkd); ++i) {
	if(pkd->pStore[i].iRung >= iRung) {
	    assert(pkd->pStore[i].iActive == 1);
	    if(bAll) {          /* Assign all rungs at iRung and above */
		iSteps =
		    dDelta*sqrt(pkd->pStore[i].fDensity*dRhoFac)/dEta;
		iTempRung = iRung;
		while(iSteps) {
		    ++iTempRung;
		    iSteps >>= 1;
		    }
		pkd->pStore[i].iRung = iTempRung; /* XXX need MaxRung */
						  /* limit */
                }
            else {
		if(dDelta*sqrt(pkd->pStore[i].fDensity*dRhoFac) < dEta) {
		    pkd->pStore[i].iRung = iRung;
		    }
		else {
		    pkd->pStore[i].iRung = iRung+1;
		    }
		}
	    }
	if(pkd->pStore[i].iRung > iMaxRung)
	    iMaxRung = pkd->pStore[i].iRung;
	}
    return iMaxRung;
    }

int
pkdVelocityRung(PKD pkd, int iRung, double dDelta, double dEta, int iMaxRung,
		double dVelFac, double dAccFac, int bAll)
{
    int i;
    int iMaxRungOut;
    int iTempRung;
    int iSteps;
    int iStepsv;
    int iStepsa;
    double vel;
    double acc;
    int j;
    
    iMaxRungOut = 0;
    for(i = 0; i < pkdLocal(pkd); ++i) {
	if(pkd->pStore[i].iRung >= iRung) {
	    assert(pkd->pStore[i].iActive == 1);
	    vel = 0;
            acc = 0;
	    for(j = 0; j < 3; j++) {
		vel += pkd->pStore[i].v[j]*pkd->pStore[i].v[j];
                acc += pkd->pStore[i].a[j]*pkd->pStore[i].a[j];
		}
	    vel = sqrt(vel)*dVelFac;
	    acc = sqrt(acc)*dAccFac;
	    if(bAll) {          /* Assign all rungs at iRung and above */
		iStepsv = dDelta*vel/(dEta*pkd->pStore[i].fSoft);
		iStepsa = dDelta*sqrt(acc/pkd->pStore[i].fSoft)/dEta;
		/* iSteps = iStepsv > iStepsa ? iStepsv : iStepsa; */
		iSteps = iStepsa;	/* Just use a --trq */
		iTempRung = iRung;
		while(iSteps) {
		    ++iTempRung;
		    iSteps >>= 1;
		    }
		if(iTempRung >= iMaxRung)
		    iTempRung = iMaxRung-1;
		pkd->pStore[i].iRung = iTempRung;
		}
	    else {
		if(dDelta*vel < dEta*pkd->pStore[i].fSoft) {
		    pkd->pStore[i].iRung = iRung;
		    }
		else {
		    pkd->pStore[i].iRung = iRung+1;
		    }
                }
	    }
	if(pkd->pStore[i].iRung > iMaxRungOut)
	    iMaxRungOut = pkd->pStore[i].iRung;
	}
    return iMaxRungOut;
    }

int pkdRungParticles(PKD pkd,int iRung)
{
	int i,n;

	n = 0;
	for (i=0;i<pkdLocal(pkd);++i) {
		if (pkd->pStore[i].iRung == iRung) ++n;
		}
	return n;
	}


void pkdCoolVelocity(PKD pkd,int nSuperCool,double dCoolFac,
					 double dCoolDens,double dCoolMaxDens)
{
	PARTICLE *p;
	int i,j,n;

	p = pkd->pStore;
	n = pkdLocal(pkd);
	for (i=0;i<n;++i) {
		if (p[i].iOrder < nSuperCool && p[i].fDensity >= dCoolDens &&
			p[i].fDensity < dCoolMaxDens) {
			for (j=0;j<3;++j) {
#ifdef SUPERCOOL
				p[i].v[j] = p[i].vMean[j] + (p[i].v[j]-p[i].vMean[j])*dCoolFac;
#else
				p[i].v[j] *= dCoolFac;
#endif
				}
			}
		}
	}


void pkdActiveCool(PKD pkd,int nSuperCool)
{
    int i;
    
    for(i=0;i<pkdLocal(pkd);++i) {
		if (pkd->pStore[i].iOrder < nSuperCool) {
			pkd->pStore[i].iActive = 1;
			}
		else {
			pkd->pStore[i].iActive = 0;
			}
		}
    }

#ifdef GASOLINE

int pkdIsGas(PKD pkd,PARTICLE *p) {
	if (p->iOrder < pkd->nGas) return 1;
	else return 0;
	}

int pkdIsDark(PKD pkd,PARTICLE *p) {
	if (p->iOrder >= pkd->nGas && p->iOrder < pkd->nGas+pkd->nDark) return 1;
	else return 0;
	}

int pkdIsStar(PKD pkd,PARTICLE *p) {
	if (p->iOrder >= pkd->nGas+pkd->nDark) return 1;
	else return 0;
	}

void pkdActiveGas(PKD pkd)
{
    int i;
    
    for(i=0;i<pkdLocal(pkd);++i) {
		if (pkdIsGas(pkd,&pkd->pStore[i])) {
			pkd->pStore[i].iActive = 1;
			}
		else {
			pkd->pStore[i].iActive = 0;
			}
		}
    }

void pkdCalcEthdot(PKD pkd)
{
	PARTICLE *p;
    int i;
    
    for(i=0;i<pkdLocal(pkd);++i) {
		p = &pkd->pStore[i];
		if (p->iActive) {
			p->du = p->A*sqrt(p->u) + p->B;
			}
		}
    }

void pkdKickVpred(PKD pkd, double dvFacOne, double dvFacTwo)
{
	PARTICLE *p;
	int i,j,n;

	p = pkd->pStore;
	n = pkdLocal(pkd);
	for (i=0;i<n;++i) {
	    if(pkdIsGas(pkd, &p[i])) {
		for (j=0;j<3;++j) {
		    p[i].vPred[j] = p[i].vPred[j]*dvFacOne + p[i].a[j]*dvFacTwo;
		    }
		}
	    }
	}

int
pkdSphCurrRung(PKD pkd, int iRung)
{
    int i;
    int iCurrent;
    
    iCurrent = 0;
    for(i = 0; i < pkdLocal(pkd); ++i) {
	if(pkdIsGas(pkd, &pkd->pStore[i]) && pkd->pStore[i].iRung == iRung) {
	    iCurrent = 1;
	    break;
	    }
	}
    return iCurrent;
    }

#endif
