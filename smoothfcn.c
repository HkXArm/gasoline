#include <math.h>
#include <assert.h>
#include "smoothfcn.h"

#ifdef COLLISIONS
#include "ssdefs.h"
#include "collision.h"
#endif /* COLLISIONS */

#ifdef M43D
/* M43D Creates a 3D kernel by convolution of 3D tophats the way M4(1D) is made in 1D */
#define BALL2(a) ((a)->fBall2)
#define KERNEL(ak,ar2) { \
		ak = sqrt(ar2); \
		if (ar2 < 1.0) ak = 6.*0.25/350./3. *(1360+ar2*(-2880 \
			 +ar2*(3528+ak*(-1890+ak*(-240+ak*(270-6*ar2)))))); \
		else ak = 6.*0.25/350./3. *(7040-1152/ak+ak*(-10080+ak*(2880+ak*(4200 \
	                 +ak*(-3528+ak*(630+ak*(240+ak*(-90+2*ar2)))))))); \
                }
#define DKERNEL(adk,ar2) { \
		adk = sqrt(ar2); \
		if (ar2 < 1.0) adk = 6.*0.25/350./3. * (-2880*2 \
	                 +ar2*(3528*4+ adk*(-1890*5 + adk*(-240*6+ adk*(270*7-6*9*ar2))))); \
		else adk = 6.*0.25/350./3. *((1152/ar2-10080)/adk+(2880*2+adk*(4200*3 \
	                 +adk*(-3528*4+adk*(630*5+adk*(240*6 +adk*(-90*7+2*9*ar2))))))); \
                }

#else
#ifdef HSHRINK
/* HSHRINK M4 Kernel uses an effective h of (pi/6)^(1/3) times h for nSmooth neighbours */
#define dSHRINKFACTOR        0.805995977
#define BALL2(a) ((a)->fBall2*(dSHRINKFACTOR*dSHRINKFACTOR))
#define KERNEL(ak,ar2) { \
		ak = 2.0 - sqrt(ar2); \
		if (ar2 < 1.0) ak = (1.0 - 0.75*ak*ar2); \
		else if (ar2 < 4.0) ak = 0.25*ak*ak*ak; \
		else ak = 0.0; \
                }
#define DKERNEL(adk,ar2) { \
		adk = sqrt(ar2); \
		if (ar2 < 1.0) { \
			adk = -3 + 2.25*adk; \
			} \
		else if (ar2 < 4.0) { \
			adk = -0.75*(2.0-adk)*(2.0-adk)/adk; \
			} \
		else adk = 0.0; \
                }

#else
/* Standard M_4 Kernel */
#define BALL2(a) ((a)->fBall2)
#define KERNEL(ak,ar2) { \
		ak = 2.0 - sqrt(ar2); \
		if (ar2 < 1.0) ak = (1.0 - 0.75*ak*ar2); \
		else ak = 0.25*ak*ak*ak; \
                }
#define DKERNEL(adk,ar2) { \
		adk = sqrt(ar2); \
		if (ar2 < 1.0) { \
			adk = -3 + 2.25*adk; \
			} \
		else { \
			adk = -0.75*(2.0-adk)*(2.0-adk)/adk; \
			} \
                }
#endif
#endif

void initDensity(void *p)
{
	((PARTICLE *)p)->fDensity = 0.0;
	}

void combDensity(void *p1,void *p2)
{
	((PARTICLE *)p1)->fDensity += ((PARTICLE *)p2)->fDensity;
	}

void Density(PARTICLE *p,int nSmooth,NN *nnList,SMF *smf)
{
	FLOAT ih2,r2,rs,fDensity;
	int i;

	ih2 = 4.0/BALL2(p);
	fDensity = 0.0;
	for (i=0;i<nSmooth;++i) {
		r2 = nnList[i].fDist2*ih2;
		KERNEL(rs,r2);
		fDensity += rs*nnList[i].pPart->fMass;
		}
	p->fDensity = M_1_PI*sqrt(ih2)*ih2*fDensity; 
	}

void DensitySym(PARTICLE *p,int nSmooth,NN *nnList,SMF *smf)
{
	PARTICLE *q;
	FLOAT fNorm,ih2,r2,rs;
	int i;

	ih2 = 4.0/(BALL2(p));
	fNorm = 0.5*M_1_PI*sqrt(ih2)*ih2;
	for (i=0;i<nSmooth;++i) {
		r2 = nnList[i].fDist2*ih2;
		KERNEL(rs,r2);
		rs *= fNorm;
		q = nnList[i].pPart;
		p->fDensity += rs*q->fMass;
		q->fDensity += rs*p->fMass;
		}
	}

void initParticleMarkDensity(void *p)
{
	((PARTICLE *)p)->fDensity = 0.0;
	TYPESet((PARTICLE *) p, TYPE_DensZeroed);
	}

void initMarkDensity(void *p)
{
	((PARTICLE *)p)->fDensity = 0.0;
	}

void combMarkDensity(void *p1,void *p2)
{
        if (TYPETest((PARTICLE *) p1, TYPE_DensZeroed)) 
  	        ((PARTICLE *)p1)->fDensity += ((PARTICLE *)p2)->fDensity;
        else if (TYPETest((PARTICLE *) p2, TYPE_DensZeroed)) {
  	        ((PARTICLE *)p1)->fDensity = ((PARTICLE *)p2)->fDensity;
	        }
	((PARTICLE *)p1)->iActive |= ((PARTICLE *)p2)->iActive;
	}

void MarkDensity(PARTICLE *p,int nSmooth,NN *nnList,SMF *smf)
{
	assert(0);
}

void MarkDensitySym(PARTICLE *p,int nSmooth,NN *nnList,SMF *smf)
{
	PARTICLE *q;
	FLOAT fNorm,ih2,r2,rs;
	int i;
	unsigned int qiActive;

	ih2 = 4.0/(BALL2(p));
	fNorm = 0.5*M_1_PI*sqrt(ih2)*ih2;
        if (TYPETest(p, TYPE_ACTIVE)) {
	        TYPESet( p, TYPE_NbrOfACTIVE );
  	        for (i=0;i<nSmooth;++i) {
		       r2 = nnList[i].fDist2*ih2;
		       KERNEL(rs,r2);
		       rs *= fNorm;
		       q = nnList[i].pPart;
	               p->fDensity += rs*q->fMass;
     	               if (TYPETest(q, TYPE_DensZeroed)) 
   	                        q->fDensity += rs*p->fMass;
		       else {
   	                        q->fDensity = rs*p->fMass;
           	                TYPESet(q, TYPE_DensZeroed);
		                }
		       TYPESet( q, TYPE_NbrOfACTIVE );
		       }
                } 
        else {
	        qiActive = 0;
                for (i=0;i<nSmooth;++i) {
		       r2 = nnList[i].fDist2*ih2;
		       KERNEL(rs,r2);
                       rs *= fNorm;
		       q = nnList[i].pPart;
     	               if (TYPETest(p, TYPE_DensZeroed)) 
   	                        p->fDensity += rs*q->fMass;
		       else {
   	                        p->fDensity = rs*q->fMass;
           	                TYPESet(p, TYPE_DensZeroed);
		                }
     	               if (TYPETest(q, TYPE_DensZeroed)) 
   	                        q->fDensity += rs*p->fMass;
		       else {
   	                        q->fDensity = rs*p->fMass;
           	                TYPESet(q, TYPE_DensZeroed);
		                }
		       qiActive |= q->iActive;
		       }
		if (qiActive & TYPE_ACTIVE) TYPESet( p, TYPE_NbrOfACTIVE );
	        }
        }

void initParticleMarkIIDensity(void *p)
{
	if (TYPEFilter((PARTICLE *) p, TYPE_DensACTIVE|TYPE_DensZeroed, TYPE_DensACTIVE)) {
                ((PARTICLE *)p)->fDensity = 0.0;
        	TYPESet((PARTICLE *)p, TYPE_DensZeroed);
	        }
	}

void initMarkIIDensity(void *p)
{
        ((PARTICLE *) p)->fDensity = 0.0;
	}

void combMarkIIDensity(void *p1,void *p2)
{
	if (TYPETest((PARTICLE *) p1, TYPE_DensACTIVE)) {
                if (TYPETest((PARTICLE *) p1, TYPE_DensZeroed)) 
  	                ((PARTICLE *)p1)->fDensity += ((PARTICLE *)p2)->fDensity;
                else if (TYPETest((PARTICLE *) p2, TYPE_DensZeroed)) {
  	                ((PARTICLE *)p1)->fDensity = ((PARTICLE *)p2)->fDensity;
	                }
	        }
	((PARTICLE *)p1)->iActive |= ((PARTICLE *)p2)->iActive;
	}

void MarkIIDensity(PARTICLE *p,int nSmooth,NN *nnList,SMF *smf)
{
	assert(0);
}

void MarkIIDensitySym(PARTICLE *p,int nSmooth,NN *nnList,SMF *smf)
{
	PARTICLE *q;
	FLOAT fNorm,ih2,r2,rs;
	int i;
	unsigned int qiActive;

	ih2 = 4.0/(BALL2(p));
	fNorm = 0.5*M_1_PI*sqrt(ih2)*ih2;
        if (TYPETest(p, TYPE_DensACTIVE)) {
	        qiActive = 0;
  	        for (i=0;i<nSmooth;++i) {
		       q = nnList[i].pPart;
		       qiActive |= q->iActive;
		       r2 = nnList[i].fDist2*ih2;
		       KERNEL(rs,r2);
		       rs *= fNorm;
	               p->fDensity += rs*q->fMass;
	               if (TYPETest(q, TYPE_DensACTIVE)) {
     	                        if (TYPETest(q, TYPE_DensZeroed)) 
   	                                q->fDensity += rs*p->fMass;
		                else {
   	                                q->fDensity = rs*p->fMass;
           	                        TYPESet(q, TYPE_DensZeroed);
				        }
		                }
		       if (TYPETest(p, TYPE_ACTIVE)) TYPESet( q, TYPE_NbrOfACTIVE );
		       }
		if (qiActive & TYPE_ACTIVE) TYPESet( p, TYPE_NbrOfACTIVE );
                } 
        else if (TYPETest(p, TYPE_ACTIVE)) {
	        TYPESet( p, TYPE_NbrOfACTIVE );
                for (i=0;i<nSmooth;++i) {
		       q = nnList[i].pPart;
		       TYPESet( q, TYPE_NbrOfACTIVE );
                       if (!TYPETest(q, TYPE_DensACTIVE)) continue;
		       r2 = nnList[i].fDist2*ih2;
		       KERNEL(rs,r2);
                       rs *= fNorm;
     	               if (TYPETest(q, TYPE_DensZeroed)) 
   	                        q->fDensity += rs*p->fMass;
		       else {
   	                        q->fDensity = rs*p->fMass;
           	                TYPESet(q, TYPE_DensZeroed);
		                }
		       }
	        }
        else {
	        qiActive = 0;
                for (i=0;i<nSmooth;++i) {
		       q = nnList[i].pPart;
		       qiActive |= q->iActive;
		       if (!TYPETest(q, TYPE_DensACTIVE)) continue;
		       r2 = nnList[i].fDist2*ih2;
		       KERNEL(rs,r2);
                       rs *= fNorm;
     	               if (TYPETest(q, TYPE_DensZeroed)) 
   	                        q->fDensity += rs*p->fMass;
		       else {
   	                        q->fDensity = rs*p->fMass;
           	                TYPESet(q, TYPE_DensZeroed);
		                }
		       }
		if (qiActive & TYPE_ACTIVE) TYPESet( p, TYPE_NbrOfACTIVE );
	        }
        }

void combMark(void *p1,void *p2)
{
	((PARTICLE *)p1)->iActive |= ((PARTICLE *)p2)->iActive;
	}


#ifdef SUPERCOOL
void initMeanVel(void *p)
{
	int j;

	for (j=0;j<3;++j) {
		((PARTICLE *)p)->vMean[j] = 0.0;
		}
	}

void combMeanVel(void *p1,void *p2)
{
	int j;

	for (j=0;j<3;++j) {
		((PARTICLE *)p1)->vMean[j] += ((PARTICLE *)p2)->vMean[j];
		}
	}

void MeanVel(PARTICLE *p,int nSmooth,NN *nnList,SMF *smf)
{
	PARTICLE *q;
	FLOAT fNorm,ih2,r2,rs;
	int i,j;

	ih2 = 4.0/BALL2(p);
	fNorm = M_1_PI*sqrt(ih2)*ih2;
	for (i=0;i<nSmooth;++i) {
		r2 = nnList[i].fDist2*ih2;
		KERNEL(rs,r2);
		rs *= fNorm;
		q = nnList[i].pPart;
		for (j=0;j<3;++j) {
			p->vMean[j] += rs*q->fMass/q->fDensity*q->v[j];
			}
		}
	}

void MeanVelSym(PARTICLE *p,int nSmooth,NN *nnList,SMF *smf)
{
	PARTICLE *q;
	FLOAT fNorm,ih2,r2,rs;
	int i,j;

	ih2 = 4.0/BALL2(p);
	fNorm = 0.5*M_1_PI*sqrt(ih2)*ih2;
	for (i=0;i<nSmooth;++i) {
		r2 = nnList[i].fDist2*ih2;
		KERNEL(rs,r2);
		rs *= fNorm;
		q = nnList[i].pPart;
		for (j=0;j<3;++j) {
			p->vMean[j] += rs*q->fMass/q->fDensity*q->v[j];
			q->vMean[j] += rs*p->fMass/p->fDensity*p->v[j];
			}
		}
	}
#endif


#ifdef GASOLINE
/* Original Particle */
void initSphPressureTermsParticle(void *p)
{
        if (TYPEQueryACTIVE((PARTICLE *) p)) {
                ((PARTICLE *)p)->mumax = 0.0;
	        ((PARTICLE *)p)->PdV = 0.0;
#ifdef DEBUG
		((PARTICLE *)p)->PdVvisc = 0.0;
		((PARTICLE *)p)->PdVpres = 0.0;
#endif
	        }
	}

/* Cached copies of particle */
void initSphPressureTerms(void *p)
{
        if (TYPEQueryACTIVE((PARTICLE *) p)) {
	        ((PARTICLE *)p)->mumax = 0.0;
		((PARTICLE *)p)->PdV = 0.0;
#ifdef DEBUG
		((PARTICLE *)p)->PdVvisc = 0.0;
		((PARTICLE *)p)->PdVpres = 0.0;
#endif
		((PARTICLE *)p)->a[0] = 0.0;
		((PARTICLE *)p)->a[1] = 0.0;
		((PARTICLE *)p)->a[2] = 0.0;
	        }
	}

void combSphPressureTerms(void *p1,void *p2)
{
        if (TYPEQueryACTIVE((PARTICLE *) p1)) {
	        ((PARTICLE *)p1)->PdV += ((PARTICLE *)p2)->PdV;
#ifdef DEBUG
		((PARTICLE *)p1)->PdVvisc += ((PARTICLE *)p2)->PdVvisc;
		((PARTICLE *)p1)->PdVpres += ((PARTICLE *)p2)->PdVpres;
#endif
		if (((PARTICLE *)p2)->mumax > ((PARTICLE *)p1)->mumax)
		  ((PARTICLE *)p1)->mumax = ((PARTICLE *)p2)->mumax;
		((PARTICLE *)p1)->a[0] += ((PARTICLE *)p2)->a[0];
		((PARTICLE *)p1)->a[1] += ((PARTICLE *)p2)->a[1];
		((PARTICLE *)p1)->a[2] += ((PARTICLE *)p2)->a[2];	
	        }
	}

/* Gather only version -- untested */
void SphPressureTerms(PARTICLE *p,int nSmooth,NN *nnList,SMF *smf)
{
	PARTICLE *q;
	FLOAT ih2,r2,rs1;
	FLOAT dx,dy,dz,dvx,dvy,dvz,dvdotdr;
	FLOAT pPoverRho2,pPdV,pa[3],pmumax;
	FLOAT ph,pc,pDensity,visc,hav,vFac,absmu;
	FLOAT fNorm,fNorm1,fNorm2,fTmp;
	int i;

	if (!TYPEQueryACTIVE(p)) return;

	pc = p->c;
	pDensity = p->fDensity;
	pPoverRho2 = p->PoverRho2;
	pmumax = p->mumax;
	ph = sqrt(0.25*BALL2(p));
	ih2 = 4.0/BALL2(p);
	fNorm = M_1_PI*ih2/ph;
	fNorm1 = fNorm*ih2;	
	fNorm2 = fNorm1*(smf->a);    /* Comoving accelerations */
	vFac = (smf->bCannonical ? 1./(smf->a*smf->a) : 1.0); /* converts v to xdot */

	pPdV=0.0;
	pa[0]=0.0;
	pa[1]=0.0;
	pa[2]=0.0;
	for (i=0;i<nSmooth;++i) {
		q = nnList[i].pPart;
		r2 = nnList[i].fDist2*ih2;
		DKERNEL(rs1,r2);
		rs1 *= q->fMass;

		dx = nnList[i].dx;
		dy = nnList[i].dy;
		dz = nnList[i].dz;
		dvx = p->vPred[0] - q->vPred[0];
		dvy = p->vPred[1] - q->vPred[1];
		dvz = p->vPred[2] - q->vPred[2];
		dvdotdr = vFac*(dvx*dx + dvy*dy + dvz*dz) + nnList[i].fDist2*smf->H;
		if (dvdotdr>0.0) {
		        pPdV += rs1 * pPoverRho2 * dvdotdr;
			rs1 *= (pPoverRho2 + q->PoverRho2);
		        pa[0] -= rs1 * dx;
		        pa[1] -= rs1 * dy;
		        pa[2] -= rs1 * dz;
              		}
		else {
		        hav = 0.5*(ph+sqrt(0.25*BALL2(q)));
			/* mu 
			   multiply by a to be consistent with physical c */
		        absmu = -hav*dvdotdr*smf->a 
			    / (nnList[i].fDist2+0.01*hav*hav);
			/* mu terms for gas time step */
			if (absmu>pmumax) pmumax=absmu;

                        /* viscosity term */
		        visc = 0.5*(p->BalsaraSwitch+q->BalsaraSwitch)*(smf->alpha * (pc + q->c) 
			    +   smf->beta  * 2 * absmu ) 
			    * absmu / (pDensity + q->fDensity);
		        pPdV += rs1 * (pPoverRho2 + 0.5*visc) * dvdotdr;
			rs1 *= (pPoverRho2 + q->PoverRho2 + visc);
		        pa[0] -= rs1 * dx;
		        pa[1] -= rs1 * dy;
		        pa[2] -= rs1 * dz;
              		}
 		}
	p->PdV += fNorm1*pPdV;
	p->mumax = pmumax;
	p->a[0] += fNorm2*pa[0];
	p->a[1] += fNorm2*pa[1];
	p->a[2] += fNorm2*pa[2];
	}

void SphPressureTermsSym(PARTICLE *p,int nSmooth,NN *nnList,SMF *smf)
{
	PARTICLE *q;
	FLOAT ih2,r2,r,rs1,rq,rp;
	FLOAT dx,dy,dz,dvx,dvy,dvz,dvdotdr;
	FLOAT pPoverRho2,pPdV,pa[3],pMass,pmumax;
	FLOAT ph,pc,pDensity,visc,hav,absmu;
	FLOAT fNorm,fNorm1,aFac,vFac,fTmp;
	int i;

#ifdef PDVCHECK
	char ach[456];
#endif

	pc = p->c;
	pDensity = p->fDensity;
	pMass = p->fMass;
	pPoverRho2 = p->PoverRho2;
	ph = sqrt(0.25*BALL2(p));
	ih2 = 4.0/BALL2(p);
	fNorm = 0.5*M_1_PI*ih2/ph;
	fNorm1 = fNorm*ih2;	/* converts to physical u */
	aFac = (smf->a);        /* comoving acceleration factor */
	vFac = (smf->bCannonical ? 1./(smf->a*smf->a) : 1.0); /* converts v to xdot */

	if (TYPEQueryACTIVE(p)) {
	  /* p active */
	  pmumax = p->mumax;
	  pPdV=0.0;
	  pa[0]=0.0;
	  pa[1]=0.0;
	  pa[2]=0.0;
	  for (i=0;i<nSmooth;++i) {
	        q = nnList[i].pPart;
	        r2 = nnList[i].fDist2*ih2;
		DKERNEL(rs1,r2);
		rs1 *= fNorm1;
		rq = rs1 * q->fMass;

		dx = nnList[i].dx;
		dy = nnList[i].dy;
		dz = nnList[i].dz;
		dvx = p->vPred[0] - q->vPred[0];
		dvy = p->vPred[1] - q->vPred[1];
		dvz = p->vPred[2] - q->vPred[2];
		dvdotdr = vFac*(dvx*dx + dvy*dy + dvz*dz) + nnList[i].fDist2*smf->H;

		if (TYPEQueryACTIVE(q)) {
		  /* q active */
		  rp = rs1 * pMass;
		  if (dvdotdr>0.0) {
#ifdef PDVCHECK
		    if (p->iOrder==880556 || q->iOrder==880556 || !finite(rq * pPoverRho2 * dvdotdr) || !finite(rp * q->PoverRho2 * dvdotdr) || fabs(rq * pPoverRho2 * dvdotdr * 1e-5) > p->u || fabs(rp * q->PoverRho2 * dvdotdr *1e-5) > q->u) {
			  sprintf(ach,"PDV-ERR-1 %d - %d: Den %g - %g u %g - %g PdV+ %g v %g %g %g Pred %g %g %g v %g %g %g Pred %g %g %g fB %g %g - %g %g \n",p->iOrder,q->iOrder,p->fDensity,q->fDensity,p->u,q->u,rq * pPoverRho2 * dvdotdr,p->v[0],p->v[1],p->v[2],p->vPred[0],p->vPred[1],p->vPred[2],q->v[0],q->v[1],q->v[2],q->vPred[0],q->vPred[1],q->vPred[2],sqrt(p->fBall2),p->fBallMax,sqrt(q->fBall2),q->fBallMax);
			  mdlDiag(smf->pkd->mdl,ach);
		          }
#endif

		        pPdV += rq * pPoverRho2 * dvdotdr;
			q->PdV += rp * q->PoverRho2 * dvdotdr;
			rq *= (pPoverRho2 + q->PoverRho2);
			rp *= (pPoverRho2 + q->PoverRho2);
#ifdef DEBUG
		        p->PdVpres += rq * (pPoverRho2) * dvdotdr;
			q->PdVpres += rp * (q->PoverRho2) * dvdotdr;
#endif
			rp *= aFac; /* convert to comoving acceleration */
			rq *= aFac;
		        pa[0] -= rq * dx;
		        pa[1] -= rq * dy;
		        pa[2] -= rq * dz;
		        q->a[0] += rp * dx;
		        q->a[1] += rp * dy;
		        q->a[2] += rp * dz;
#ifdef PDVCHECK
			if (p->iOrder==880556 || q->iOrder==880556 || fabs(rq) > 1e50 || fabs(rp) > 1e50 || fabs(p->a[0])+fabs(p->a[1])+fabs(p->a[2])+fabs(pa[0])+fabs(pa[1])+fabs(pa[2]) > 1e50 || fabs(q->a[0])+fabs(q->a[1])+fabs(q->a[2]) > 1e50) {
			  sprintf(ach,"PDV-ACC-1 %d - %d: Den %g - %g u %g - %g PdV+ %g v %g %g %g Pred %g %g %g v %g %g %g Pred %g %g %g fB %g %g - %g %g a %g - %g\n",p->iOrder,q->iOrder,p->fDensity,q->fDensity,p->u,q->u,rq * pPoverRho2 * dvdotdr,p->v[0],p->v[1],p->v[2],p->vPred[0],p->vPred[1],p->vPred[2],q->v[0],q->v[1],q->v[2],q->vPred[0],q->vPred[1],q->vPred[2],sqrt(p->fBall2),p->fBallMax,sqrt(q->fBall2),q->fBallMax,rp,rq);
			  mdlDiag(smf->pkd->mdl,ach);
		          }
#endif
              		}
		  else {
             		/* h mean - using just hp probably ok */
		        hav=0.5*(ph+sqrt(0.25*BALL2(q)));
			/* mu 
			   multiply by a to be consistent with physical c */
		        absmu = -hav*dvdotdr*smf->a 
			    / (nnList[i].fDist2+0.01*hav*hav);
			/* mu terms for gas time step */
			if (absmu>pmumax) pmumax=absmu;
			if (absmu>q->mumax) q->mumax=absmu;
                        /* viscosity term */
		        visc = 0.5*(p->BalsaraSwitch+q->BalsaraSwitch)*(smf->alpha * (pc + q->c) 
			    +   smf->beta  * 2 * absmu ) 
			    * absmu / (pDensity + q->fDensity);

#ifdef PDVCHECK
			if (p->iOrder==880556 || q->iOrder==880556 || !finite(rq * (pPoverRho2 + 0.5*visc) * dvdotdr) || !finite(rp * (q->PoverRho2 + 0.5*visc) * dvdotdr) || fabs(rq * (pPoverRho2 + 0.5*visc) * dvdotdr * 1e-5) > p->u || fabs(rp * (q->PoverRho2 + 0.5*visc) * dvdotdr *1e-5) > q->u) {
			  sprintf(ach,"PDV-ERR-2 %d - %d: Den %g - %g u %g %g PdV+ %g %g %g v %g %g %g Pred %g %g %g v %g %g %g Pred %g %g %g fB %g %g - %g %g\n",p->iOrder,q->iOrder,p->fDensity,q->fDensity,p->u,q->u,rq * (pPoverRho2 + 0.5*visc) * dvdotdr,rq * (pPoverRho2) * dvdotdr,rq * (0.5*visc) * dvdotdr,p->v[0],p->v[1],p->v[2],p->vPred[0],p->vPred[1],p->vPred[2],q->v[0],q->v[1],q->v[2],q->vPred[0],q->vPred[1],q->vPred[2],sqrt(p->fBall2),p->fBallMax,sqrt(q->fBall2),q->fBallMax);
			  mdlDiag(smf->pkd->mdl,ach);

			  sprintf(ach,"PDV-ERR-2 PdV %g %g %g Parts %g %g %g %g %g %g %g %g %g uPred %g %g %g %g %d %d \n",rq * (pPoverRho2 + 0.5*visc) * dvdotdr,rq * (pPoverRho2) * dvdotdr,rq * (0.5*visc) * dvdotdr, visc, dvdotdr, pc, q->c, absmu, hav, smf->a,p->BalsaraSwitch,q->BalsaraSwitch,p->uPred,q->uPred,p->uDot,q->uDot,p->iRung,q->iRung);
			  mdlDiag(smf->pkd->mdl,ach);
		          }
#endif

		        pPdV += rq * (pPoverRho2 + 0.5*visc) * dvdotdr;
			q->PdV += rp * (q->PoverRho2 + 0.5*visc) * dvdotdr;
			rq *= (pPoverRho2 + q->PoverRho2 + visc);
			rp *= (pPoverRho2 + q->PoverRho2 + visc);
#ifdef DEBUG			
		        p->PdVpres += rq * (pPoverRho2) * dvdotdr;
			q->PdVpres += rp * (q->PoverRho2) * dvdotdr;
		        p->PdVvisc += rq * (0.5*visc) * dvdotdr;
			q->PdVvisc += rp * (0.5*visc) * dvdotdr;
#endif
			rp *= aFac; /* convert to comoving acceleration */
			rq *= aFac;

		        pa[0] -= rq * dx;
		        pa[1] -= rq * dy;
		        pa[2] -= rq * dz;
		        q->a[0] += rp * dx;
		        q->a[1] += rp * dy;
		        q->a[2] += rp * dz;
#ifdef PDVCHECK
			if (p->iOrder==880556 || q->iOrder==880556 || fabs(rq) > 1e50 || fabs(rp) > 1e50 || fabs(p->a[0])+fabs(p->a[1])+fabs(p->a[2])+fabs(pa[0])+fabs(pa[1])+fabs(pa[2]) > 1e50 || fabs(q->a[0])+fabs(q->a[1])+fabs(q->a[2]) > 1e50) {
			  sprintf(ach,"PDV-ACC-2 %d - %d: Den %g - %g u %g - %g PdV+ %g a %g %g %g %g %g %g vPred %g %g %g a %g %g %g vPred %g %g %g fB %g %g - %g %g a %g - %g\n",p->iOrder,q->iOrder,p->fDensity,q->fDensity,p->u,q->u,rq * pPoverRho2 * dvdotdr,p->a[0],p->a[1],p->a[2],p->vPred[0],p->vPred[1],p->vPred[2],q->a[0],q->a[1],q->a[2],q->vPred[0],q->vPred[1],q->vPred[2],sqrt(p->fBall2),p->fBallMax,sqrt(q->fBall2),q->fBallMax,rp,rq);
			  mdlDiag(smf->pkd->mdl,ach);
		          }
#endif
              		}
                  }
		else {
		  /* q not active */
		  if (dvdotdr>0.0) {
#ifdef PDVCHECK
		    if (p->iOrder==880556 || q->iOrder==880556 || !finite(rq * pPoverRho2 * dvdotdr) || !finite(rp * q->PoverRho2 * dvdotdr) || fabs(rq * pPoverRho2 * dvdotdr * 1e-5) > p->u || fabs(rp * q->PoverRho2 * dvdotdr *1e-5) > q->u) {
			  sprintf(ach,"PDV-ERR-3 %d - %d: Den %g - %g u %g - %g PdV+ %g v %g %g %g Pred %g %g %g v %g %g %g Pred %g %g %g fB %g %g - %g %g \n",p->iOrder,q->iOrder,p->fDensity,q->fDensity,p->u,q->u,rq * pPoverRho2 * dvdotdr,p->v[0],p->v[1],p->v[2],p->vPred[0],p->vPred[1],p->vPred[2],q->v[0],q->v[1],q->v[2],q->vPred[0],q->vPred[1],q->vPred[2],sqrt(p->fBall2),p->fBallMax,sqrt(q->fBall2),q->fBallMax);
			  mdlDiag(smf->pkd->mdl,ach);
		          }
#endif

		        pPdV += rq * pPoverRho2 * dvdotdr;
			rq *= (pPoverRho2 + q->PoverRho2);
			rq *= aFac; /* convert to comoving acceleration */

		        pa[0] -= rq * dx;
		        pa[1] -= rq * dy;
		        pa[2] -= rq * dz;
#ifdef PDVCHECK
			if (p->iOrder==880556 || q->iOrder==880556 || fabs(rq) > 1e50 || fabs(p->a[0])+fabs(p->a[1])+fabs(p->a[2])+fabs(pa[0])+fabs(pa[1])+fabs(pa[2]) > 1e50) {
			  sprintf(ach,"PDV-ACC-3 %d - %d: Den %g - %g u %g - %g PdV+ %g v %g %g %g Pred %g %g %g v %g %g %g Pred %g %g %g fB %g %g - %g %g a %g - %g\n",p->iOrder,q->iOrder,p->fDensity,q->fDensity,p->u,q->u,rq * pPoverRho2 * dvdotdr,p->v[0],p->v[1],p->v[2],p->vPred[0],p->vPred[1],p->vPred[2],q->v[0],q->v[1],q->v[2],q->vPred[0],q->vPred[1],q->vPred[2],sqrt(p->fBall2),p->fBallMax,sqrt(q->fBall2),q->fBallMax,rp,rq);
			  mdlDiag(smf->pkd->mdl,ach);
		          }
#endif
              		}
		  else {
             		/* h mean */
		        hav = 0.5*(ph+sqrt(0.25*BALL2(q)));
			/* mu 
			   multiply by a to be consistent with physical c */
		        absmu = -hav*dvdotdr*smf->a 
			    / (nnList[i].fDist2+0.01*hav*hav);
			/* mu terms for gas time step */
			if (absmu>pmumax) pmumax=absmu;
                        /* viscosity term */
		        visc = 0.5*(p->BalsaraSwitch+q->BalsaraSwitch)*(smf->alpha * (pc + q->c) 
			    +   smf->beta  * 2 * absmu ) 
			    * absmu / (pDensity + q->fDensity);
#ifdef PDVCHECK
			if (p->iOrder==880556 || q->iOrder==880556 || !finite(rq * (pPoverRho2 + 0.5*visc) * dvdotdr) || !finite(rp * (q->PoverRho2 + 0.5*visc) * dvdotdr) || fabs(rq * (pPoverRho2 + 0.5*visc) * dvdotdr * 1e-5) > p->u || fabs(rp * (q->PoverRho2 + 0.5*visc) * dvdotdr *1e-5) > q->u) {
			  sprintf(ach,"PDV-ERR-4 %d - %d: Den %g - %g u %g - %g PdV+ %g %g %g v %g %g %g Pred %g %g %g v %g %g %g Pred %g %g %g fB %g %g - %g %g \n",p->iOrder,q->iOrder,p->fDensity,q->fDensity,p->u,q->u,rq * (pPoverRho2 + 0.5*visc) * dvdotdr,rq * (pPoverRho2) * dvdotdr,rq * (0.5*visc) * dvdotdr,p->v[0],p->v[1],p->v[2],p->vPred[0],p->vPred[1],p->vPred[2],q->v[0],q->v[1],q->v[2],q->vPred[0],q->vPred[1],q->vPred[2],sqrt(p->fBall2),p->fBallMax,sqrt(q->fBall2),q->fBallMax);
			  mdlDiag(smf->pkd->mdl,ach);
		          }
#endif

		        pPdV += rq * (pPoverRho2 + 0.5*visc) * dvdotdr;
			rq *= (pPoverRho2 + q->PoverRho2 + visc);
#ifdef DEBUG			
		        p->PdVpres += rq * (pPoverRho2) * dvdotdr;
		        p->PdVvisc += rq * (0.5*visc) * dvdotdr;
#endif
			rq *= aFac; /* convert to comoving acceleration */

		        pa[0] -= rq * dx;
		        pa[1] -= rq * dy;
		        pa[2] -= rq * dz; 
#ifdef PDVCHECK
			if (p->iOrder==880556 || q->iOrder==880556 || fabs(rq) > 1e50 || fabs(p->a[0])+fabs(p->a[1])+fabs(p->a[2])+fabs(pa[0])+fabs(pa[1])+fabs(pa[2]) > 1e50 ) {
			  sprintf(ach,"PDV-ACC-4 %d - %d: Den %g - %g u %g - %g PdV+ %g v %g %g %g Pred %g %g %g v %g %g %g Pred %g %g %g fB %g %g - %g %g a %g - %g\n",p->iOrder,q->iOrder,p->fDensity,q->fDensity,p->u,q->u,rq * pPoverRho2 * dvdotdr,p->v[0],p->v[1],p->v[2],p->vPred[0],p->vPred[1],p->vPred[2],q->v[0],q->v[1],q->v[2],q->vPred[0],q->vPred[1],q->vPred[2],sqrt(p->fBall2),p->fBallMax,sqrt(q->fBall2),q->fBallMax,rp,rq);
			  mdlDiag(smf->pkd->mdl,ach);
		          }
#endif
             		}
 		  }
	        }
          p->PdV += pPdV;
	  p->mumax = pmumax;
	  p->a[0] += pa[0];
	  p->a[1] += pa[1];
	  p->a[2] += pa[2];
	  }
        else {
          /* p not active */
	  for (i=0;i<nSmooth;++i) {
	        q = nnList[i].pPart;
		if (!TYPEQueryACTIVE(q)) continue; /* neither active */

	        r2 = nnList[i].fDist2*ih2;
		DKERNEL(rs1,r2);
		rs1 *= fNorm1;
		rp = rs1 * pMass;

		dx = nnList[i].dx;
		dy = nnList[i].dy;
		dz = nnList[i].dz;
		dvx = p->vPred[0] - q->vPred[0];
		dvy = p->vPred[1] - q->vPred[1];
		dvz = p->vPred[2] - q->vPred[2];
		dvdotdr = vFac*(dvx*dx + dvy*dy + dvz*dz) + nnList[i].fDist2*smf->H;
		if (dvdotdr>0.0) {
#ifdef PDVCHECK
#endif
			q->PdV += rp * q->PoverRho2 * dvdotdr;
			rp *= (pPoverRho2 + q->PoverRho2);
			rp *= aFac; /* convert to comoving acceleration */

		        q->a[0] += rp * dx;
		        q->a[1] += rp * dy;
		        q->a[2] += rp * dz;
#ifdef PDVCHECK
			if (p->iOrder==880556 || q->iOrder==880556 || fabs(rp) > 1e50 || fabs(q->a[0])+fabs(q->a[1])+fabs(q->a[2]) > 1e50) {
			  sprintf(ach,"PDV-ACC-5 %d - %d: Den %g - %g u %g - %g PdV+ %g v %g %g %g Pred %g %g %g v %g %g %g Pred %g %g %g fB %g %g - %g %g a %g - %g\n",p->iOrder,q->iOrder,p->fDensity,q->fDensity,p->u,q->u,rq * pPoverRho2 * dvdotdr,p->v[0],p->v[1],p->v[2],p->vPred[0],p->vPred[1],p->vPred[2],q->v[0],q->v[1],q->v[2],q->vPred[0],q->vPred[1],q->vPred[2],sqrt(p->fBall2),p->fBallMax,sqrt(q->fBall2),q->fBallMax,rp,rq);
			  mdlDiag(smf->pkd->mdl,ach);
		          }
#endif
              		}
		else {
             		/* h mean */
		        hav = 0.5*(ph+sqrt(0.25*BALL2(q)));
			/* mu 
			   multiply by a to be consistent with physical c */
		        absmu = -hav*dvdotdr*smf->a 
			    / (nnList[i].fDist2+0.01*hav*hav);
			/* mu terms for gas time step */
			if (absmu>q->mumax) q->mumax=absmu;
                        /* viscosity */
		        visc = 0.5*(p->BalsaraSwitch+q->BalsaraSwitch)*(smf->alpha * (pc + q->c) 
			    +   smf->beta  * 2 * absmu ) 
			    * absmu / (pDensity + q->fDensity);
#ifdef PDVCHECK
#endif
			q->PdV += rp * (q->PoverRho2 + 0.5*visc) * dvdotdr;
			rp *= (pPoverRho2 + q->PoverRho2 + visc);
#ifdef DEBUG			
			q->PdVpres += rp * (q->PoverRho2) * dvdotdr;
			q->PdVvisc += rp * (0.5*visc) * dvdotdr;
#endif
			rp *= aFac; /* convert to comoving acceleration */

		        q->a[0] += rp * dx;
		        q->a[1] += rp * dy;
		        q->a[2] += rp * dz;
#ifdef PDVCHECK
			if (p->iOrder==880556 || q->iOrder==880556 || fabs(rp) > 1e50 || fabs(q->a[0])+fabs(q->a[1])+fabs(q->a[2]) > 1e50) {
			  sprintf(ach,"PDV-ACC-6 %d - %d: Den %g - %g u %g - %g PdV+ %g v %g %g %g Pred %g %g %g v %g %g %g Pred %g %g %g fB %g %g - %g %g a %g - %g\n",p->iOrder,q->iOrder,p->fDensity,q->fDensity,p->u,q->u,rq * pPoverRho2 * dvdotdr,p->v[0],p->v[1],p->v[2],p->vPred[0],p->vPred[1],p->vPred[2],q->v[0],q->v[1],q->v[2],q->vPred[0],q->vPred[1],q->vPred[2],sqrt(p->fBall2),p->fBallMax,sqrt(q->fBall2),q->fBallMax,rp,rq);
			  mdlDiag(smf->pkd->mdl,ach);
		          }
#endif
              		}
	        }
	  } 

        }

void initDivVort(void *p)
{
        if (TYPEQueryACTIVE((PARTICLE *) p )) {
	        ((PARTICLE *)p)->divv = 0.0;
		((PARTICLE *)p)->curlv[0] = 0.0;
		((PARTICLE *)p)->curlv[1] = 0.0;
		((PARTICLE *)p)->curlv[2] = 0.0;
	        }
	}

void combDivVort(void *p1,void *p2)
{
        if (TYPEQueryACTIVE((PARTICLE *) p1 )) {
 	        ((PARTICLE *)p1)->divv += ((PARTICLE *)p2)->divv;
		((PARTICLE *)p1)->curlv[0] += ((PARTICLE *)p2)->curlv[0];
		((PARTICLE *)p1)->curlv[1] += ((PARTICLE *)p2)->curlv[1];
		((PARTICLE *)p1)->curlv[2] += ((PARTICLE *)p2)->curlv[2];
	        }
	}

/* Gather only version -- untested */
void DivVort(PARTICLE *p,int nSmooth,NN *nnList,SMF *smf)
{
	PARTICLE *q;
	FLOAT ih2,r2,rs1;
	FLOAT dx,dy,dz,dvx,dvy,dvz,dvdotdr;
	FLOAT pcurlv[3],pdivv;
	FLOAT pDensity;
	FLOAT fNorm,vFac,a2;
	int i;

	if (!TYPEQueryACTIVE( p )) return;

	pDensity = p->fDensity;
	ih2 = 4.0/BALL2(p);
        a2 = (smf->a*smf->a);
	fNorm = M_1_PI*ih2*ih2; 
	vFac = (smf->bCannonical ? 1./a2 : 1.0); /* converts v to xdot */

	pdivv=0.0;
	pcurlv[0]=0.0;
	pcurlv[1]=0.0;
	pcurlv[2]=0.0;
	for (i=0;i<nSmooth;++i) {
		q = nnList[i].pPart;
		r2 = nnList[i].fDist2*ih2;
		DKERNEL(rs1,r2);
		rs1 *= q->fMass/q->fDensity;

		dx = nnList[i].dx;
		dy = nnList[i].dy;
		dz = nnList[i].dz;
		dvx = p->vPred[0] - q->vPred[0];
		dvy = p->vPred[1] - q->vPred[1];
		dvz = p->vPred[2] - q->vPred[2];
		dvdotdr = vFac*(dvx*dx + dvy*dy + dvz*dz) + nnList[i].fDist2*smf->H;
		pdivv += rs1*dvdotdr;
		pcurlv[0] += rs1*(dvz*dy - dvy*dz);
		pcurlv[1] += rs1*(dvx*dz - dvz*dx);
		pcurlv[2] += rs1*(dvy*dx - dvx*dy);
 		}
	p->divv -=  fNorm*pdivv;  /* physical */
	p->curlv[0] += fNorm*vFac*pcurlv[0];
	p->curlv[1] += fNorm*vFac*pcurlv[1];
	p->curlv[2] += fNorm*vFac*pcurlv[2];
	}

/* Output is physical divv and curlv -- thus a*h_co*divv is physical */
void DivVortSym(PARTICLE *p,int nSmooth,NN *nnList,SMF *smf)
{
	PARTICLE *q;
	FLOAT ih2,r2,rs1,rq,rp;
	FLOAT dx,dy,dz,dvx,dvy,dvz,dvdotdr;
	FLOAT pMass,pDensity;
	FLOAT fNorm,dv,vFac,a2;
	FLOAT pcurlv[3],pdivv;
	int i;
 
	pDensity = p->fDensity;
	pMass = p->fMass;
	ih2 = 4.0/BALL2(p);
	a2 = (smf->a*smf->a);
	fNorm = 0.5*M_1_PI*ih2*ih2*sqrt(ih2); 
	vFac = (smf->bCannonical ? 1./a2 : 1.0); /* converts v to xdot */

	if (TYPEQueryACTIVE( p )) {
	  /* p active */
	  for (i=0;i<nSmooth;++i) {
	        q = nnList[i].pPart;
	        r2 = nnList[i].fDist2*ih2;
		DKERNEL(rs1,r2);
		rs1 *= fNorm;
		rq = rs1 * q->fMass/q->fDensity;

		dx = nnList[i].dx;
		dy = nnList[i].dy;
		dz = nnList[i].dz;
		dvx = p->vPred[0] - q->vPred[0];
		dvy = p->vPred[1] - q->vPred[1];
		dvz = p->vPred[2] - q->vPred[2];
		dvdotdr = vFac*(dvx*dx + dvy*dy + dvz*dz) + nnList[i].fDist2*smf->H;

		if (TYPEQueryACTIVE( q )) {
     		        /* q active */
		        rp = rs1 * pMass/pDensity;
			p->divv -= rq*dvdotdr;
			q->divv -= rp*dvdotdr;
			dv=vFac*(dvz*dy - dvy*dz);
			p->curlv[0] += rq*dv;
			q->curlv[0] += rp*dv;
			dv=vFac*(dvx*dz - dvz*dx);
			p->curlv[1] += rq*dv;
			q->curlv[1] += rp*dv;
			dv=vFac*(dvy*dx - dvx*dy);
			p->curlv[2] += rq*dv;
			q->curlv[2] += rp*dv;
		        }
 		else {
		        /* q inactive */
     		        p->divv -= rq*dvdotdr;
			dv=vFac*(dvz*dy - dvy*dz);
			p->curlv[0] += rq*dv;
			dv=vFac*(dvx*dz - dvz*dx);
			p->curlv[1] += rq*dv;
			dv=vFac*(dvy*dx - dvx*dy);
			p->curlv[2] += rq*dv;
		        }
	        }
	  } 
        else {
          /* p not active */
	  for (i=0;i<nSmooth;++i) {
	        q = nnList[i].pPart;
		if (!TYPEQueryACTIVE( q )) continue; /* neither active */

	        r2 = nnList[i].fDist2*ih2;
		DKERNEL(rs1,r2);
		rp = rs1 * fNorm*pMass/pDensity;

		dx = nnList[i].dx;
		dy = nnList[i].dy;
		dz = nnList[i].dz;
		dvx = p->vPred[0] - q->vPred[0];
		dvy = p->vPred[1] - q->vPred[1];
		dvz = p->vPred[2] - q->vPred[2];
		dvdotdr = vFac*(dvx*dx + dvy*dy + dvz*dz) + nnList[i].fDist2*smf->H;
		/* q active */
		q->divv -= rp*dvdotdr;
		dv=vFac*(dvz*dy - dvy*dz);
		q->curlv[0] += rp*dv;
		dv=vFac*(dvx*dz - dvz*dx);
		q->curlv[1] += rp*dv;
		dv=vFac*(dvy*dx - dvx*dy);
		q->curlv[2] += rp*dv;
	        }
	  } 
        }

/* Original Particle */
void initHKPressureTermsParticle(void *p)
{
        if (TYPEQueryACTIVE((PARTICLE *) p)) {
 	         ((PARTICLE *)p)->mumax = 0.0;
		 ((PARTICLE *)p)->PdV = 0.0;
#ifdef DEBUG
		 ((PARTICLE *)p)->PdVvisc = 0.0;
		 ((PARTICLE *)p)->PdVpres = 0.0;
#endif
	         }
	}

/* Cached copies of particle */
void initHKPressureTerms(void *p)
{
        if (TYPEQueryACTIVE((PARTICLE *) p)) {
	         ((PARTICLE *)p)->mumax = 0.0;
		 ((PARTICLE *)p)->PdV = 0.0;
#ifdef DEBUG
		 ((PARTICLE *)p)->PdVvisc = 0.0;
		 ((PARTICLE *)p)->PdVpres = 0.0;
#endif
		 ((PARTICLE *)p)->a[0] = 0.0;
		 ((PARTICLE *)p)->a[1] = 0.0;
		 ((PARTICLE *)p)->a[2] = 0.0;
	         }
	}

void combHKPressureTerms(void *p1,void *p2)
{
        if (TYPEQueryACTIVE((PARTICLE *) p1)) {
	         ((PARTICLE *)p1)->PdV += ((PARTICLE *)p2)->PdV;
#ifdef DEBUG
		 ((PARTICLE *)p1)->PdVvisc += ((PARTICLE *)p2)->PdVvisc;
		 ((PARTICLE *)p1)->PdVpres += ((PARTICLE *)p2)->PdVpres;
#endif
		 if (((PARTICLE *)p2)->mumax > ((PARTICLE *)p1)->mumax)
		   ((PARTICLE *)p1)->mumax = ((PARTICLE *)p2)->mumax;
		 ((PARTICLE *)p1)->a[0] += ((PARTICLE *)p2)->a[0];
		 ((PARTICLE *)p1)->a[1] += ((PARTICLE *)p2)->a[1];
		 ((PARTICLE *)p1)->a[2] += ((PARTICLE *)p2)->a[2];	
	         }
	}

/* Gather only version -- (untested)  */
void HKPressureTerms(PARTICLE *p,int nSmooth,NN *nnList,SMF *smf)
{
	PARTICLE *q;
	FLOAT ih2,r2,rs1,rq,rp;
	FLOAT dx,dy,dz,dvx,dvy,dvz,dvdotdr;
	FLOAT pPoverRho2,pQonRho2,qQonRho2,qhdivv;
	FLOAT ph,pc,pDensity,visc,absmu,qh,pMass,hav;
	FLOAT fNorm,fNorm1,aFac,vFac,fTmp;
	int i;

	if (!TYPEQueryACTIVE(p)) return;

	pc = p->c;
	pDensity = p->fDensity;
	pMass = p->fMass;
	pPoverRho2 = p->PoverRho2;
	ph = sqrt(0.25*BALL2(p));
	/* QonRho2 given same scaling with a as PonRho2 */
	pQonRho2 = (p->divv>0.0 ? 0.0 : fabs(p->divv)*ph*smf->a
		    *(smf->alpha*pc + smf->beta*fabs(p->divv)*ph*smf->a)/pDensity );
	ih2 = 4.0/BALL2(p);
	fNorm = 0.5*M_1_PI*ih2/ph;
	fNorm1 = fNorm*ih2;	/* converts to physical u */
	aFac = (smf->a);        /* comoving acceleration factor */
	vFac = (smf->bCannonical ? 1./(smf->a*smf->a) : 1.0); /* converts v to xdot */

	for (i=0;i<nSmooth;++i) {
	        q = nnList[i].pPart;
	        r2 = nnList[i].fDist2*ih2;
		DKERNEL(rs1,r2);
		rs1 *= fNorm1 * q->fMass;;

		dx = nnList[i].dx;
		dy = nnList[i].dy;
		dz = nnList[i].dz;
		dvx = p->vPred[0] - q->vPred[0];
		dvy = p->vPred[1] - q->vPred[1];
		dvz = p->vPred[2] - q->vPred[2];
		dvdotdr = vFac*(dvx*dx + dvy*dy + dvz*dz) + nnList[i].fDist2*smf->H;

		if (dvdotdr>0.0) {
		  p->PdV += rs1 * pPoverRho2 * dvdotdr;
		  rs1 *= (pPoverRho2 + q->PoverRho2);
		  rs1 *= aFac;
		  p->a[0] -= rs1 * dx;
		  p->a[1] -= rs1 * dy;
		  p->a[2] -= rs1 * dz;
		  }
 	        else {
		  qh=sqrt(0.25*BALL2(q));
		  qhdivv = qh*fabs(q->divv)*smf->a; /* units of physical velocity */
		  qQonRho2 = ( qhdivv>0.0 ? 0.0 : 
			qhdivv*(smf->alpha*q->c + smf->beta*qhdivv)/q->fDensity );
		  visc = pQonRho2 + qQonRho2;
		  /* mu -- same timestep criteria as standard sph above (for now) */
		  hav=0.5*(qh+ph);
		  absmu = -hav*dvdotdr*smf->a 
			    / (nnList[i].fDist2+0.01*hav*hav);
		  if (absmu>p->mumax) p->mumax=absmu;
		  p->PdV += rs1 * (pPoverRho2 + 0.5*visc) * dvdotdr;
		  rs1 *= (pPoverRho2 + q->PoverRho2 + visc);
		  rs1 *= aFac; /* convert to comoving acceleration */
		  p->a[0] -= rs1 * dx;
		  p->a[1] -= rs1 * dy;
		  p->a[2] -= rs1 * dz;
		  }
                }
        }

/* Bulk viscosity and standard pressure forces */
void HKPressureTermsSym(PARTICLE *p,int nSmooth,NN *nnList,SMF *smf)
{
	PARTICLE *q;
	FLOAT ih2,r2,rs1,rq,rp;
	FLOAT dx,dy,dz,dvx,dvy,dvz,dvdotdr;
	FLOAT pPoverRho2,pQonRho2,qQonRho2,qhdivv;
	FLOAT ph,pc,pDensity,visc,absmu,qh,pMass,hav;
	FLOAT fNorm,fNorm1,aFac,vFac,fTmp;
	int i;

	pc = p->c;
	pDensity = p->fDensity;
	pMass = p->fMass;
	pPoverRho2 = p->PoverRho2;
	ph = sqrt(0.25*BALL2(p));
	/* QonRho2 given same scaling with a as PonRho2 */
	pQonRho2 = (p->divv>0.0 ? 0.0 : fabs(p->divv)*ph*smf->a
		    *(smf->alpha*pc + smf->beta*fabs(p->divv)*ph*smf->a)/pDensity );
	ih2 = 4.0/BALL2(p);
	fNorm = 0.5*M_1_PI*ih2/ph;
	fNorm1 = fNorm*ih2;	/* converts to physical u */
	aFac = (smf->a);        /* comoving acceleration factor */
	vFac = (smf->bCannonical ? 1./(smf->a*smf->a) : 1.0); /* converts v to xdot */

	for (i=0;i<nSmooth;++i) {
	        q = nnList[i].pPart;
	        r2 = nnList[i].fDist2*ih2;
		DKERNEL(rs1,r2);
		rs1 *= fNorm1;
		rq = rs1 * q->fMass;
		rp = rs1 * pMass;

		dx = nnList[i].dx;
		dy = nnList[i].dy;
		dz = nnList[i].dz;
		dvx = p->vPred[0] - q->vPred[0];
		dvy = p->vPred[1] - q->vPred[1];
		dvz = p->vPred[2] - q->vPred[2];
		dvdotdr = vFac*(dvx*dx + dvy*dy + dvz*dz) + nnList[i].fDist2*smf->H;

		if (dvdotdr>0.0) {
		  if (TYPEQueryACTIVE(p)) {
		        p->PdV += rq * pPoverRho2 * dvdotdr;
			rq *= (pPoverRho2 + q->PoverRho2);
			rq *= aFac;
		        p->a[0] -= rq * dx;
		        p->a[1] -= rq * dy;
		        p->a[2] -= rq * dz;
		        }
		  if (TYPEQueryACTIVE(q)) {
			q->PdV += rp * q->PoverRho2 * dvdotdr;
			rp *= (pPoverRho2 + q->PoverRho2);
			rp *= aFac; /* convert to comoving acceleration */
		        q->a[0] += rp * dx;
		        q->a[1] += rp * dy;
		        q->a[2] += rp * dz;
              		}
		  }
 	        else {
		  qh=sqrt(0.25*BALL2(q));
		  qhdivv = qh*fabs(q->divv)*smf->a; /* units of physical velocity */
		  qQonRho2 = ( qhdivv>0.0 ? 0.0 : 
			qhdivv*(smf->alpha*q->c + smf->beta*qhdivv)/q->fDensity );
		  visc = pQonRho2 + qQonRho2;
		  /* mu -- same timestep criteria as standard sph above (for now) */
		  hav=0.5*(qh+ph);
		  absmu = -hav*dvdotdr*smf->a 
			    / (nnList[i].fDist2+0.01*hav*hav);
		  if (TYPEQueryACTIVE(p)) {
			if (absmu>p->mumax) p->mumax=absmu;
		        p->PdV += rq * (pPoverRho2 + 0.5*visc) * dvdotdr;
			rq *= (pPoverRho2 + q->PoverRho2 + visc);
			rq *= aFac; /* convert to comoving acceleration */
		        p->a[0] -= rq * dx;
		        p->a[1] -= rq * dy;
		        p->a[2] -= rq * dz;
		        }
		  if (TYPEQueryACTIVE(q)) {
			if (absmu>q->mumax) q->mumax=absmu;
			q->PdV += rp * (q->PoverRho2 + 0.5*visc) * dvdotdr;
			rp *= (pPoverRho2 + q->PoverRho2 + visc);
			rp *= aFac; /* convert to comoving acceleration */
		        q->a[0] += rp * dx;
		        q->a[1] += rp * dy;
		        q->a[2] += rp * dz;
              		}
		  }
                }
        }

#endif

#ifdef COLLISIONS

void
FindRejects(PARTICLE *p,int nSmooth,NN *nnList,SMF *smf)
{
	/*
	 ** Checks "nSmooth" neighbours of "p" for Hill radius (or physical
	 ** radius) overlap. Note that only planetesimals with "iOrder"s larger
	 ** than "p->iOrder" are considered. This means at most N/2 particles
	 ** will be rejected.
	 */

	PARTICLE *pn;
	double a,r2,sh,sr;
	int i;

	if (p->iColor != PLANETESIMAL) return;

	a = sqrt(p->r[0]*p->r[0] + p->r[1]*p->r[1]); /* approx. orbital distance */
	for (i=0;i<nSmooth;++i) {
		pn = nnList[i].pPart;
		if (pn->iColor != PLANETESIMAL || pn->iOrder <= p->iOrder) continue;
		r2 = nnList[i].fDist2;
		sh = a*(p->fRedHill + pn->fRedHill);
		sr = 2*(p->fSoft + pn->fSoft); /* radius = 2 * softening */
		if (r2 < sh*sh || r2 < sr*sr) {
			p->dTEnc = -1.0; /* rejects have dTEnc < 0 (see REJECT() macro) */
			return; /* one reject is enough */
			}
		}
	}

void
SetTimeStep(PARTICLE *p,int nSmooth,NN *nnList,SMF *smf)
{
	/*DEBUG out of date
	 * Determines time-step for particle "p" based on distance and relative
	 * radial velocity of the "nSmooth" nearby particles given in "nnList"
	 * [that are currently approaching the target particle]. The time step is
	 * returned in p->dt and has a value no greater than the initial value of
	 * p->dt on entry.
	 *
	 */

	PARTICLE *pn;
	double r2,dt;
	int i;

	for (i=0;i<nSmooth;++i) {
		pn = nnList[i].pPart;
		if (pn == p)
			continue;
		r2 = nnList[i].fDist2;
		dt = 0.2 * sqrt(r2 * sqrt(r2) / (p->fMass + pn->fMass));
		if (dt < p->dt) p->dt = dt;
		}
	}

void
CheckForCollision(PARTICLE *p,int nSmooth,NN *nnList,SMF *smf)
{
	/*
	 ** Checks whether particle "p" will collide with any of its "nSmooth"
	 ** nearest neighbours in "nnList" during drift interval smf->dStart to
	 ** smf->dEnd, relative to current time. If any collisions are found,
	 ** the relative time to the one that will occur first is noted in
	 ** pkd->dImpactTime and relevant info of the particle and its collider
	 ** are stored in the pkd->Collider1 & pkd->Collider2 structures.
	 ** NOTE: the iIndex of particle "p" is calculated as (p - pkd->pStore).
	 */

	PKD pkd = smf->pkd;
	PARTICLE *pn;
	FLOAT vx,vy,vz,rdotv,v2,sr,D,dt;
	int i;

	assert(TYPEQueryACTIVE(p));

	for (i=0;i<nSmooth;++i) {
		pn = nnList[i].pPart;
		if (pn == p || !TYPEQueryACTIVE(pn)))
			continue;
		if (COLLISION(pkd->dImpactTime) &&
			(pkd->Collider1.id.iPid == nnList[i].iPid &&
			 pkd->Collider1.id.iIndex == nnList[i].iIndex &&
			 pkd->Collider2.id.iPid == pkd->idSelf &&
			 pkd->Collider2.id.iIndex == p - pkd->pStore))
			continue; /* skip if same colliders but order reversed */
		vx = p->v[0] - pn->v[0];
		vy = p->v[1] - pn->v[1];
		vz = p->v[2] - pn->v[2];
		rdotv = nnList[i].dx*vx + nnList[i].dy*vy + nnList[i].dz*vz;
		if (rdotv >= 0)
			continue; /* skip if particle not approaching */
		v2 = vx*vx + vy*vy + vz*vz;
		sr = 2*(p->fSoft + pn->fSoft); /* softening = 0.5 * particle radius */
		D = 1 - v2*(nnList[i].fDist2 - sr*sr)/(rdotv*rdotv);
		if (D < 0)
			continue; /* no real solutions ==> no collision */
		D = sqrt(D);
		if (smf->dStart == 0) {
#ifdef FIX_COLLAPSE
			if (D >= 1) {
				dt = rdotv*(D - 1)/v2;
				if (dt < pkd->dImpactTime) {/* take most negative */
					if (sqrt(nnList[i].fDist2)/sr - 1 > 0.01)
						(void) fprintf(stderr,"***LARGE OVERLAP (%f%%)***\n",
									   100*(sqrt(nnList[i].fDist2)/sr - 1));
					fprintf(stderr,"POSITION FIX %i & %i D=%e dt=%e\n",
							p->iOrder,pn->iOrder,D,dt);
					pkd->dImpactTime = dt;
					partToCollider(p,pkd->idSelf,p - pkd->pStore,&pkd->Collider1);
					partToCollider(pn,nnList[i].iPid,nnList[i].iIndex,&pkd->Collider2);
					continue;
					}
				}
#else /* FIX_COLLAPSE */
			if (D >= 1)
				fprintf(stderr,"OVERLAP! %i (r=%e,%e,%e,iRung=%i) &"
						" %i (r=%e,%e,%e,iRung=%i)"
						" v=%e,%e,%e rv=%e sr=%e d=%e D=%e\n",
						p->iOrder,p->r[0],p->r[1],p->r[2],p->iRung,
						pn->iOrder,pn->r[0],pn->r[1],pn->r[2],pn->iRung,
						vx,vy,vz,rdotv,sr,sqrt(nnList[i].fDist2) - sr,D);
			assert(D < 1); /* particles must not touch or overlap initially */
#endif /* !FIX_COLLAPSE */
			}
		dt = rdotv*(D - 1)/v2; /* minimum time to surface contact */
		if (dt > smf->dStart && dt <= smf->dEnd) {
			if (COLLISION(pkd->dImpactTime)) { /* if not first collision */
				if (dt > pkd->dImpactTime)
					continue; /* skip if this one will happen later */
				/*
				 ** ASSERT: Can't handle multiple simultaneous collisions...
				 */
				assert(dt < pkd->dImpactTime);
				}
			/* currently only support collisions between planetesimals... */
			assert(p->iColor == PLANETESIMAL && pn->iColor == PLANETESIMAL);
			pkd->dImpactTime = dt;
			partToCollider(p,pkd->idSelf,p - pkd->pStore,&pkd->Collider1);
			partToCollider(pn,nnList[i].iPid,nnList[i].iIndex,&pkd->Collider2);
			}
		}

#ifdef SAND_PILE
	if (p->v[2] < 0) { /* check for floor collision */
		dt = (2*p->fSoft - p->r[2]) / p->v[2];
		if (dt > smf->dStart && dt <= smf->dEnd && dt < pkd->dImpactTime) {
			pkd->dImpactTime = dt;
			partToCollider(p,pkd->idSelf,p - pkd->pStore,&pkd->Collider1);
			pkd->Collider2.id.iPid = -1;
			}
		}
#endif /* SAND_PILE */

#ifdef IN_A_BOX
	{
	int j;
	for (i=0;i<3;i++) {
		for (j=-1;j<=1;j+=2) {
			if (j*p->v[i] > 0) { /* possible wall collision */
				dt = (BOX_HALF_SIZE - j*p->r[i] - 2*p->fSoft) / (j*p->v[i]);
				if (dt > smf->dStart && dt <= smf->dEnd && dt < pkd->dImpactTime) {
					pkd->dImpactTime = dt;
					partToCollider(p,pkd->idSelf,p - pkd->pStore,&pkd->Collider1);
					pkd->Collider2.id.iPid = -1 - i;
					}
				}
			}
		}
	}
#endif /* IN_A_BOX */
	}

#endif /* COLLISIONS */
