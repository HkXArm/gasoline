#include <stdio.h>
#include <malloc.h>
#include <math.h>
#include <assert.h>
#include "ewald.h"
#include "pkd.h"
#include "meval.h"
#include "qeval.h"


void pkdBucketEwald(PKD pkd,int iBucket,int nReps,double fEwCut,int iOrder)
{
	KDN *pkdn;
	PARTICLE *p;
	struct ilCellNewt mom;
	int i,j,n,ix,iy,iz,nEwReps,bInHolex,bInHolexy,bInHole;
	double L,alpha,alpha2,alphan,k1,ka;
	double fEwCut2;
	double fPot,ax,ay,az;
	double dx,dy,dz,dxo,dyo,dzo,r2,r,dir,dir2,a;
	double gam[6];
	double hdotx,s,c;
	
	if(!iOrder) return;
	mom = pkd->ilcnRoot;
	pkdn = &pkd->kdNodes[iBucket];
	n = pkdn->pUpper - pkdn->pLower + 1;
	p = &pkd->pStore[pkdn->pLower];
	nEwReps = ceil(fEwCut);
	L = pkd->fPeriod[0];
	fEwCut2 = fEwCut*fEwCut*L*L;
	nEwReps = nEwReps > nReps ? nEwReps : nReps;
	alpha = 2.0/L;
	alpha2 = alpha*alpha;
	k1 = M_PI/(alpha2*L*L*L);
	ka = 2.0*alpha/sqrt(M_PI);
	for(j=0;j<n;++j) {
		fPot = mom.m*k1;
		ax = 0.0;
		ay = 0.0;
		az = 0.0;
		dx = p[j].r[0] - mom.x;
		dy = p[j].r[1] - mom.y;
		dz = p[j].r[2] - mom.z;
		for (ix=-nEwReps;ix<=nEwReps;++ix) {
			bInHolex = (ix >= -nReps && ix <= nReps);
			dxo = dx + ix*L;
			for(iy=-nEwReps;iy<=nEwReps;++iy) {
				bInHolexy = (bInHolex && iy >= -nReps && iy <= nReps);
				dyo = dy + iy*L;
				for(iz=-nEwReps;iz<=nEwReps;++iz) {
					bInHole = (bInHolexy && iz >= -nReps && iz <= nReps);
					dzo = dz + iz*L;
					r2 = dxo*dxo + dyo*dyo + dzo*dzo;
					if (r2 == 0.0) continue;
					if (r2 > fEwCut2 && !bInHole) continue;
					r = sqrt(r2);
					dir = 1/r;
					dir2 = dir*dir;
					a = exp(-r2*alpha2);
					a *= ka*dir2;
					if (bInHole) gam[0] = -erf(alpha*r);
					else gam[0] = erfc(alpha*r);
					gam[0] *= dir;
					gam[1] = gam[0]*dir2 + a;
					alphan = 2*alpha2;
					gam[2] = 3*gam[1]*dir2 + alphan*a;
					alphan *= 2*alpha2;
					gam[3] = 5*gam[2]*dir2 + alphan*a;
					alphan *= 2*alpha2;
					gam[4] = 7*gam[3]*dir2 + alphan*a;
					alphan *= 2*alpha2;
					gam[5] = 9*gam[4]*dir2 + alphan*a;
#ifdef REDUCED_EWALD
					QEVAL(iOrder,mom,gam,dxo,dyo,dzo,ax,ay,az,fPot);
#else
					MEVAL(iOrder,mom,gam,dxo,dyo,dzo,ax,ay,az,fPot);
#endif
					}
				}
			}
		for (i=0;i<pkd->nEwhLoop;++i) {
			hdotx = pkd->ewt[i].hx*dx + pkd->ewt[i].hy*dy + pkd->ewt[i].hz*dz;
			c = cos(hdotx);
			s = sin(hdotx);
			fPot += pkd->ewt[i].hCfac*c + pkd->ewt[i].hSfac*s;
			ax += pkd->ewt[i].hx*(pkd->ewt[i].hCfac*s - pkd->ewt[i].hSfac*c);
			ay += pkd->ewt[i].hy*(pkd->ewt[i].hCfac*s - pkd->ewt[i].hSfac*c);
			az += pkd->ewt[i].hz*(pkd->ewt[i].hCfac*s - pkd->ewt[i].hSfac*c);
			}
		p[j].fPot += fPot;
		p[j].a[0] += ax;
		p[j].a[1] += ay;
		p[j].a[2] += az;
	    }
	}



void pkdEwaldInit(PKD pkd,double fhCut,int iOrder)
{
	struct ilCellNewt mom;
	int i,hReps,hx,hy,hz,h2;
	double alpha,k4,L;
	double gam[6],mfacc,mfacs;
	double ax,ay,az;

	mom = pkd->ilcnRoot;
	/*
	 ** Now setup stuff for the h-loop.
	 */
	hReps = ceil(fhCut);
	L = pkd->fPeriod[0];
	alpha = 2.0/L;
	k4 = M_PI*M_PI/(alpha*alpha*L*L);
	i = 0;
	for (hx=-hReps;hx<=hReps;++hx) {
		for (hy=-hReps;hy<=hReps;++hy) {
			for (hz=-hReps;hz<=hReps;++hz) {
				h2 = hx*hx + hy*hy + hz*hz;
				if (h2 == 0) continue;
				if (h2 > fhCut*fhCut) continue;
				if (i == pkd->nMaxEwhLoop) {
					pkd->nMaxEwhLoop *= 2;
					pkd->ewt = realloc(pkd->ewt,pkd->nMaxEwhLoop*sizeof(EWT));
					assert(pkd->ewt != NULL);
					}
				gam[0] = exp(-k4*h2)/(M_PI*h2*L);
				gam[1] = 2*M_PI/L*gam[0];
				gam[2] = -2*M_PI/L*gam[1];
				gam[3] = 2*M_PI/L*gam[2];
				gam[4] = -2*M_PI/L*gam[3];
				gam[5] = 2*M_PI/L*gam[4];
				gam[1] = 0.0;
				gam[3] = 0.0;
				gam[5] = 0.0;
				ax = 0.0;
				ay = 0.0;
				az = 0.0;
				mfacc = 0.0;
				QEVAL(iOrder,mom,gam,hx,hy,hz,ax,ay,az,mfacc);
				gam[0] = exp(-k4*h2)/(M_PI*h2*L);
				gam[1] = 2*M_PI/L*gam[0];
				gam[2] = -2*M_PI/L*gam[1];
				gam[3] = 2*M_PI/L*gam[2];
				gam[4] = -2*M_PI/L*gam[3];
				gam[5] = 2*M_PI/L*gam[4];
				gam[0] = 0.0;
				gam[2] = 0.0;
				gam[4] = 0.0;
				ax = 0.0;
				ay = 0.0;
				az = 0.0;
				mfacs = 0.0;
				QEVAL(iOrder,mom,gam,hx,hy,hz,ax,ay,az,mfacs);
				pkd->ewt[i].hx = 2*M_PI/L*hx;
				pkd->ewt[i].hy = 2*M_PI/L*hy;
				pkd->ewt[i].hz = 2*M_PI/L*hz;
				pkd->ewt[i].hCfac = mfacc;
				pkd->ewt[i].hSfac = mfacs;
				++i;
				}
			}
		}
	pkd->nEwhLoop = i;
	}










