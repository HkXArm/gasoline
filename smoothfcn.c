#include <math.h>
#include <assert.h>
#include "smoothfcn.h"

#ifdef COLLISIONS
#include "ssdefs.h"
#include "collision.h"
#endif /* COLLISIONS */

/*
 Change the way the Balsara Switch is applied:
*/
/*
#define SWITCHCOMBINE(a,b) (0.5*(a->BalsaraSwitch+b->BalsaraSwitch))
#define SWITCHCOMBINE(a,b) (a->BalsaraSwitch > b->BalsaraSwitch ? a->BalsaraSwitch : b->BalsaraSwitch)
#define SWITCHCOMBINE(a,b) (a->BalsaraSwitch*b->BalsaraSwitch)
#define SWITCHCOMBINE(a,b) ((a->BalsaraSwitch*b->BalsaraSwitch > 0.5 || \
           (a->BalsaraSwitch > 0.5 && (dx*a->aPres[0]+dy*a->aPres[1]+dz*a->aPres[2]) > 0) || \
           (b->BalsaraSwitch > 0.5 && (dx*b->aPres[0]+dy*b->aPres[1]+dz*b->aPres[2]) < 0)) ? 1 : 0)
#define SWITCHCOMBINEA(a,b) (a->BalsaraSwitch >= 1 || b->BalsaraSwitch >= 1 ? 1 : 0)
#define SWITCHCOMBINEA(a,b) ((a->BalsaraSwitch*b->BalsaraSwitch)*(a->ShockTracker > b->ShockTracker ? a->ShockTracker : b->ShockTracker))
*/

#define ACCEL(p,j) (((PARTICLE *)(p))->a[j])

#ifdef SHOCKTRACK
/* Shock Tracking on: p->ShockTracker and p->aPres are defined */

#define SWITCHCOMBINE(a,b) (0.5*(a->BalsaraSwitch+b->BalsaraSwitch))
#define SWITCHCOMBINEA(a,b) ((a->BalsaraSwitch*b->BalsaraSwitch)*a->ShockTracker*b->ShockTracker)
#define SWITCHCOMBINEB(a,b) (a->BalsaraSwitch*b->BalsaraSwitch)

#define ACCEL_PRES(p,j) (((PARTICLE *)(p))->aPres[j])
#define ACCEL_COMB_PRES(p,j) ((((PARTICLE *)(p))->a[j])+=(((PARTICLE *)(p))->aPres[j]))

#else

#define SWITCHCOMBINE(a,b) (0.5*(a->BalsaraSwitch+b->BalsaraSwitch))
#define SWITCHCOMBINEA(a,b) SWITCHCOMBINE(a,b)
#define SWITCHCOMBINEB(a,b) SWITCHCOMBINE(a,b)

#define ACCEL_PRES(p,j) (((PARTICLE *)(p))->a[j])
#define ACCEL_COMB_PRES(p,j) 

#endif

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

void NullSmooth(PARTICLE *p,int nSmooth,NN *nnList,SMF *smf) {
}

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
	TYPESet((PARTICLE *) p,TYPE_DensZeroed);
	}

void initMarkDensity(void *p)
{
	((PARTICLE *)p)->fDensity = 0.0;
	}

void combMarkDensity(void *p1,void *p2)
{
	if (TYPETest((PARTICLE *) p1,TYPE_DensZeroed)) 
		((PARTICLE *)p1)->fDensity += ((PARTICLE *)p2)->fDensity;
	else if (TYPETest((PARTICLE *) p2,TYPE_DensZeroed)) {
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

	assert(TYPETest(p,TYPE_GAS));
	ih2 = 4.0/(BALL2(p));
	fNorm = 0.5*M_1_PI*sqrt(ih2)*ih2;
	if (TYPETest(p,TYPE_ACTIVE)) {
		TYPESet(p,TYPE_NbrOfACTIVE);
		for (i=0;i<nSmooth;++i) {
			r2 = nnList[i].fDist2*ih2;
			KERNEL(rs,r2);
			rs *= fNorm;
			q = nnList[i].pPart;
	assert(TYPETest(q,TYPE_GAS));
			p->fDensity += rs*q->fMass;
			if (TYPETest(q,TYPE_DensZeroed)) 
				q->fDensity += rs*p->fMass;
			else {
				q->fDensity = rs*p->fMass;
				TYPESet(q, TYPE_DensZeroed);
				}
			TYPESet(q,TYPE_NbrOfACTIVE);
			}
		} 
	else {
		qiActive = 0;
		for (i=0;i<nSmooth;++i) {
			r2 = nnList[i].fDist2*ih2;
			KERNEL(rs,r2);
			rs *= fNorm;
			q = nnList[i].pPart;
	assert(TYPETest(q,TYPE_GAS));
			if (TYPETest(p,TYPE_DensZeroed)) 
				p->fDensity += rs*q->fMass;
			else {
				p->fDensity = rs*q->fMass;
				TYPESet(p,TYPE_DensZeroed);
				}
			if (TYPETest(q,TYPE_DensZeroed)) 
				q->fDensity += rs*p->fMass;
			else {
				q->fDensity = rs*p->fMass;
				TYPESet(q, TYPE_DensZeroed);
				}
			qiActive |= q->iActive;
			}
		if (qiActive & TYPE_ACTIVE) TYPESet(p,TYPE_NbrOfACTIVE);
		}
	}

void initParticleMarkIIDensity(void *p)
{
	if (TYPEFilter((PARTICLE *) p,TYPE_DensACTIVE|TYPE_DensZeroed,
				   TYPE_DensACTIVE)) {
		((PARTICLE *)p)->fDensity = 0.0;
		TYPESet((PARTICLE *)p,TYPE_DensZeroed);
		}
	}

void initMarkIIDensity(void *p)
{
	((PARTICLE *) p)->fDensity = 0.0;
	}

void combMarkIIDensity(void *p1,void *p2)
{
	if (TYPETest((PARTICLE *) p1,TYPE_DensACTIVE)) {
		if (TYPETest((PARTICLE *) p1,TYPE_DensZeroed)) 
			((PARTICLE *)p1)->fDensity += ((PARTICLE *)p2)->fDensity;
		else if (TYPETest((PARTICLE *) p2,TYPE_DensZeroed)) {
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
	if (TYPETest(p,TYPE_DensACTIVE)) {
		qiActive = 0;
		for (i=0;i<nSmooth;++i) {
			q = nnList[i].pPart;
			qiActive |= q->iActive;
			r2 = nnList[i].fDist2*ih2;
			KERNEL(rs,r2);
			rs *= fNorm;
			p->fDensity += rs*q->fMass;
			if (TYPETest(q,TYPE_DensACTIVE)) {
				if (TYPETest(q,TYPE_DensZeroed)) 
					q->fDensity += rs*p->fMass;
				else {
					q->fDensity = rs*p->fMass;
					TYPESet(q,TYPE_DensZeroed);
					}
				}
			if (TYPETest(p,TYPE_ACTIVE)) TYPESet(q,TYPE_NbrOfACTIVE);
			}
		if (qiActive & TYPE_ACTIVE) TYPESet(p,TYPE_NbrOfACTIVE);
		}
	else if (TYPETest(p,TYPE_ACTIVE)) {
		TYPESet( p,TYPE_NbrOfACTIVE);
		for (i=0;i<nSmooth;++i) {
			q = nnList[i].pPart;
			TYPESet(q,TYPE_NbrOfACTIVE);
			if (!TYPETest(q,TYPE_DensACTIVE)) continue;
			r2 = nnList[i].fDist2*ih2;
			KERNEL(rs,r2);
			rs *= fNorm;
			if (TYPETest(q,TYPE_DensZeroed)) 
				q->fDensity += rs*p->fMass;
			else {
				q->fDensity = rs*p->fMass;
				TYPESet(q,TYPE_DensZeroed);
				}
			}
		}
	else {
		qiActive = 0;
		for (i=0;i<nSmooth;++i) {
			q = nnList[i].pPart;
			qiActive |= q->iActive;
			if (!TYPETest(q,TYPE_DensACTIVE)) continue;
			r2 = nnList[i].fDist2*ih2;
			KERNEL(rs,r2);
			rs *= fNorm;
			if (TYPETest(q,TYPE_DensZeroed)) 
				q->fDensity += rs*p->fMass;
			else {
				q->fDensity = rs*p->fMass;
				TYPESet(q,TYPE_DensZeroed);
				}
			}
		if (qiActive & TYPE_ACTIVE) TYPESet(p,TYPE_NbrOfACTIVE);
		}
	}

void initMark(void *p)
{
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
#endif /* SUPER_COOL */


#ifdef GASOLINE
/* Original Particle */
void initSphPressureTermsParticle(void *p)
{
	if (TYPEQueryACTIVE((PARTICLE *) p)) {
		((PARTICLE *)p)->mumax = 0.0;
		((PARTICLE *)p)->PdV = 0.0;
#ifdef PDVDEBUG
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
#ifdef PDVDEBUG
		((PARTICLE *)p)->PdVvisc = 0.0;
		((PARTICLE *)p)->PdVpres = 0.0;
#endif
		ACCEL(p,0) = 0.0;
		ACCEL(p,1) = 0.0;
		ACCEL(p,2) = 0.0;
		}
	}

void combSphPressureTerms(void *p1,void *p2)
{
	if (TYPEQueryACTIVE((PARTICLE *) p1)) {
		((PARTICLE *)p1)->PdV += ((PARTICLE *)p2)->PdV;
#ifdef PDVDEBUG
		((PARTICLE *)p1)->PdVvisc += ((PARTICLE *)p2)->PdVvisc;
		((PARTICLE *)p1)->PdVpres += ((PARTICLE *)p2)->PdVpres;
#endif
		if (((PARTICLE *)p2)->mumax > ((PARTICLE *)p1)->mumax)
			((PARTICLE *)p1)->mumax = ((PARTICLE *)p2)->mumax;
		ACCEL(p1,0) += ACCEL(p2,0);
		ACCEL(p1,1) += ACCEL(p2,1);
		ACCEL(p1,2) += ACCEL(p2,2);
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
	FLOAT fNorm,fNorm1,fNorm2;
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
			pPdV += rs1 * PRES_PDV(pPoverRho2,q->PoverRho2) * dvdotdr;
			rs1 *= PRES_ACC(pPoverRho2,q->PoverRho2);
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

			visc = SWITCHCOMBINE(p,q)*
				(smf->alpha*(pc + q->c) + smf->beta*2*absmu) 
					*absmu/(pDensity + q->fDensity);
		        pPdV += rs1 * (PRES_PDV(pPoverRho2,q->PoverRho2) + 0.5*visc)*dvdotdr;
			rs1 *= PRES_ACC(pPoverRho2,q->PoverRho2) + visc;
			pa[0] -= rs1 * dx;
			pa[1] -= rs1 * dy;
			pa[2] -= rs1 * dz;
			}
 		}
	p->PdV += fNorm1*pPdV;
	p->mumax = pmumax;
	ACCEL(p,0) += fNorm2*pa[0];
	ACCEL(p,0) += fNorm2*pa[1];
	ACCEL(p,0) += fNorm2*pa[2];
	}

void SphPressureTermsSym(PARTICLE *p,int nSmooth,NN *nnList,SMF *smf)
{
	PARTICLE *q;
	FLOAT ih2,r2,rs1,rq,rp;
	FLOAT dx,dy,dz,dvx,dvy,dvz,dvdotdr;
	FLOAT pPoverRho2,pPdV,pa[3],pMass,pmumax;
	FLOAT ph,pc,pDensity,visc,hav,absmu;
	FLOAT fNorm,fNorm1,aFac,vFac;
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
	aFac = (smf->a);    /* comoving acceleration factor */
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
			dvdotdr = vFac*(dvx*dx + dvy*dy + dvz*dz)
				+ nnList[i].fDist2*smf->H;

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
					pPdV += rq*PRES_PDV(pPoverRho2,q->PoverRho2)*dvdotdr;
					q->PdV += rp*PRES_PDV(q->PoverRho2,pPoverRho2)*dvdotdr;
#ifdef PDVDEBUG
					p->PdVpres += rq*PRES_PDV(pPoverRho2,q->PoverRho2)*dvdotdr;
					q->PdVpres += rp*PRES_PDV(q->PoverRho2,pPoverRho2)*dvdotdr;
#endif
					rq *= (PRES_ACC(pPoverRho2,q->PoverRho2));
					rp *= (PRES_ACC(pPoverRho2,q->PoverRho2));
					rp *= aFac; /* convert to comoving acceleration */
					rq *= aFac;
					pa[0] -= rq * dx;
					pa[1] -= rq * dy;
					pa[2] -= rq * dz;
					ACCEL(q,0) += rp * dx;
					ACCEL(q,1) += rp * dy;
					ACCEL(q,2) += rp * dz;
#ifdef PDVCHECK
					if (p->iOrder==880556 || q->iOrder==880556 || fabs(rq) > 1e50 || fabs(rp) > 1e50 || fabs(ACCEL(p,0))+fabs(ACCEL(p,1))+fabs(ACCEL(p,2))+fabs(pa[0])+fabs(pa[1])+fabs(pa[2]) > 1e50 || fabs(ACCEL(q,0))+fabs(ACCEL(q,1))+fabs(ACCEL(q,2)) > 1e50) {
						sprintf(ach,"PDV-ACC-1 %d - %d: Den %g - %g u %g - %g PdV+ %g v %g %g %g Pred %g %g %g v %g %g %g Pred %g %g %g fB %g %g - %g %g a %g - %g\n",p->iOrder,q->iOrder,p->fDensity,q->fDensity,p->u,q->u,rq * pPoverRho2 * dvdotdr,p->v[0],p->v[1],p->v[2],p->vPred[0],p->vPred[1],p->vPred[2],q->v[0],q->v[1],q->v[2],q->vPred[0],q->vPred[1],q->vPred[2],sqrt(p->fBall2),p->fBallMax,sqrt(q->fBall2),q->fBallMax,rp,rq);
						mdlDiag(smf->pkd->mdl,ach);
						}
#endif
              		}
				else {
             		/* h mean - using just hp probably ok */
					hav=0.5*(ph+sqrt(0.25*BALL2(q)));
					/* mu multiply by a to be consistent with physical c */
					absmu = -hav*dvdotdr*smf->a 
						/(nnList[i].fDist2+0.01*hav*hav);
					/* mu terms for gas time step */
					if (absmu>pmumax) pmumax=absmu;
					if (absmu>q->mumax) q->mumax=absmu;
					/* viscosity term */

					visc = SWITCHCOMBINE(p,q)*
					  (smf->alpha*(pc + q->c) + smf->beta*2*absmu) 
					  *absmu/(pDensity + q->fDensity);
#ifdef PDVCHECK
					if (p->iOrder==880556 || q->iOrder==880556 || !finite(rq * (pPoverRho2 + 0.5*visc) * dvdotdr) || !finite(rp * (q->PoverRho2 + 0.5*visc) * dvdotdr) || fabs(rq * (pPoverRho2 + 0.5*visc) * dvdotdr * 1e-5) > p->u || fabs(rp * (q->PoverRho2 + 0.5*visc) * dvdotdr *1e-5) > q->u) {
						sprintf(ach,"PDV-ERR-2 %d - %d: Den %g - %g u %g %g PdV+ %g %g %g v %g %g %g Pred %g %g %g v %g %g %g Pred %g %g %g fB %g %g - %g %g\n",p->iOrder,q->iOrder,p->fDensity,q->fDensity,p->u,q->u,rq * (pPoverRho2 + 0.5*visc) * dvdotdr,rq * (pPoverRho2) * dvdotdr,rq * (0.5*visc) * dvdotdr,p->v[0],p->v[1],p->v[2],p->vPred[0],p->vPred[1],p->vPred[2],q->v[0],q->v[1],q->v[2],q->vPred[0],q->vPred[1],q->vPred[2],sqrt(p->fBall2),p->fBallMax,sqrt(q->fBall2),q->fBallMax);
						mdlDiag(smf->pkd->mdl,ach);
						sprintf(ach,"PDV-ERR-2 PdV %g %g %g Parts %g %g %g %g %g %g %g %g %g uPred %g %g %g %g %d %d \n",rq * (pPoverRho2 + 0.5*visc) * dvdotdr,rq * (pPoverRho2) * dvdotdr,rq * (0.5*visc) * dvdotdr, visc, dvdotdr, pc, q->c, absmu, hav, smf->a,p->BalsaraSwitch,q->BalsaraSwitch,p->uPred,q->uPred,p->uDot,q->uDot,p->iRung,q->iRung);
						mdlDiag(smf->pkd->mdl,ach);
						}
#endif
					pPdV += rq*(PRES_PDV(pPoverRho2,q->PoverRho2) + 0.5*visc)*dvdotdr;
					q->PdV += rp*(PRES_PDV(q->PoverRho2,pPoverRho2) + 0.5*visc)*dvdotdr;
#ifdef PDVDEBUG			
					p->PdVpres += rq*(PRES_PDV(pPoverRho2,q->PoverRho2))*dvdotdr;
					q->PdVpres += rp*(PRES_PDV(q->PoverRho2,pPoverRho2))*dvdotdr;
					p->PdVvisc += rq*(0.5*visc)*dvdotdr;
					q->PdVvisc += rp*(0.5*visc)*dvdotdr;
#endif
					rq *= (PRES_ACC(pPoverRho2,q->PoverRho2) + visc);
					rp *= (PRES_ACC(pPoverRho2,q->PoverRho2) + visc);
					rp *= aFac; /* convert to comoving acceleration */
					rq *= aFac;

					pa[0] -= rq*dx;
					pa[1] -= rq*dy;
					pa[2] -= rq*dz;
					ACCEL(q,0) += rp*dx;
					ACCEL(q,1) += rp*dy;
					ACCEL(q,2) += rp*dz;
#ifdef PDVCHECK
					if (p->iOrder==880556 || q->iOrder==880556 || fabs(rq) > 1e50 || fabs(rp) > 1e50 || fabs(ACCEL(p,0))+fabs(ACCEL(p,1))+fabs(ACCEL(p,2))+fabs(pa[0])+fabs(pa[1])+fabs(pa[2]) > 1e50 || fabs(ACCEL(q,0))+fabs(ACCEL(q,1))+fabs(ACCEL(q,2)) > 1e50) {
						sprintf(ach,"PDV-ACC-2 %d - %d: Den %g - %g u %g - %g PdV+ %g a %g %g %g %g %g %g vPred %g %g %g a %g %g %g vPred %g %g %g fB %g %g - %g %g a %g - %g\n",p->iOrder,q->iOrder,p->fDensity,q->fDensity,p->u,q->u,rq * pPoverRho2 * dvdotdr,ACCEL(p,0),ACCEL(p,1),ACCEL(p,2),p->vPred[0],p->vPred[1],p->vPred[2],ACCEL(q,0),ACCEL(q,1),ACCEL(q,2),q->vPred[0],q->vPred[1],q->vPred[2],sqrt(p->fBall2),p->fBallMax,sqrt(q->fBall2),q->fBallMax,rp,rq);
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

					pPdV += rq*PRES_PDV(pPoverRho2,q->PoverRho2)*dvdotdr;
#ifdef PDVDEBUG
  					p->PdVpres += rq*(PRES_PDV(pPoverRho2,q->PoverRho2))*dvdotdr;
#endif
					rq *= (PRES_ACC(pPoverRho2,q->PoverRho2));
					rq *= aFac; /* convert to comoving acceleration */

					pa[0] -= rq*dx;
					pa[1] -= rq*dy;
					pa[2] -= rq*dz;
#ifdef PDVCHECK
					if (p->iOrder==880556 || q->iOrder==880556 || fabs(rq) > 1e50 || fabs(ACCEL(p,0))+fabs(ACCEL(p,1))+fabs(ACCEL(p,2))+fabs(pa[0])+fabs(pa[1])+fabs(pa[2]) > 1e50) {
						sprintf(ach,"PDV-ACC-3 %d - %d: Den %g - %g u %g - %g PdV+ %g v %g %g %g Pred %g %g %g v %g %g %g Pred %g %g %g fB %g %g - %g %g a %g - %g\n",p->iOrder,q->iOrder,p->fDensity,q->fDensity,p->u,q->u,rq * pPoverRho2 * dvdotdr,p->v[0],p->v[1],p->v[2],p->vPred[0],p->vPred[1],p->vPred[2],q->v[0],q->v[1],q->v[2],q->vPred[0],q->vPred[1],q->vPred[2],sqrt(p->fBall2),p->fBallMax,sqrt(q->fBall2),q->fBallMax,rp,rq);
						mdlDiag(smf->pkd->mdl,ach);
						}
#endif
              		}
				else {
             		/* h mean */
					hav = 0.5*(ph+sqrt(0.25*BALL2(q)));
					/* mu multiply by a to be consistent with physical c */
					absmu = -hav*dvdotdr*smf->a 
						/(nnList[i].fDist2+0.01*hav*hav);
					/* mu terms for gas time step */
					if (absmu>pmumax) pmumax=absmu;
					/* viscosity term */

					visc = SWITCHCOMBINE(p,q)*
					  (smf->alpha*(pc + q->c) + smf->beta*2*absmu) 
					  *absmu/(pDensity + q->fDensity);
#ifdef PDVCHECK
					if (p->iOrder==880556 || q->iOrder==880556 || !finite(rq * (pPoverRho2 + 0.5*visc) * dvdotdr) || !finite(rp * (q->PoverRho2 + 0.5*visc) * dvdotdr) || fabs(rq * (pPoverRho2 + 0.5*visc) * dvdotdr * 1e-5) > p->u || fabs(rp * (q->PoverRho2 + 0.5*visc) * dvdotdr *1e-5) > q->u) {
						sprintf(ach,"PDV-ERR-4 %d - %d: Den %g - %g u %g - %g PdV+ %g %g %g v %g %g %g Pred %g %g %g v %g %g %g Pred %g %g %g fB %g %g - %g %g \n",p->iOrder,q->iOrder,p->fDensity,q->fDensity,p->u,q->u,rq * (pPoverRho2 + 0.5*visc) * dvdotdr,rq * (pPoverRho2) * dvdotdr,rq * (0.5*visc) * dvdotdr,p->v[0],p->v[1],p->v[2],p->vPred[0],p->vPred[1],p->vPred[2],q->v[0],q->v[1],q->v[2],q->vPred[0],q->vPred[1],q->vPred[2],sqrt(p->fBall2),p->fBallMax,sqrt(q->fBall2),q->fBallMax);
						mdlDiag(smf->pkd->mdl,ach);
						}
#endif

					pPdV += rq*(PRES_PDV(pPoverRho2,q->PoverRho2) + 0.5*visc)*dvdotdr;
#ifdef PDVDEBUG			
  					p->PdVpres += rq*(PRES_PDV(pPoverRho2,q->PoverRho2))*dvdotdr;
					p->PdVvisc += rq*(0.5*visc)*dvdotdr;
#endif
					rq *= (PRES_ACC(pPoverRho2,q->PoverRho2) + visc);
					rq *= aFac; /* convert to comoving acceleration */

					pa[0] -= rq*dx;
					pa[1] -= rq*dy;
					pa[2] -= rq*dz; 
#ifdef PDVCHECK
					if (p->iOrder==880556 || q->iOrder==880556 || fabs(rq) > 1e50 || fabs(ACCEL(p,0))+fabs(ACCEL(p,1))+fabs(ACCEL(p,2))+fabs(pa[0])+fabs(pa[1])+fabs(pa[2]) > 1e50 ) {
						sprintf(ach,"PDV-ACC-4 %d - %d: Den %g - %g u %g - %g PdV+ %g v %g %g %g Pred %g %g %g v %g %g %g Pred %g %g %g fB %g %g - %g %g a %g - %g\n",p->iOrder,q->iOrder,p->fDensity,q->fDensity,p->u,q->u,rq * pPoverRho2 * dvdotdr,p->v[0],p->v[1],p->v[2],p->vPred[0],p->vPred[1],p->vPred[2],q->v[0],q->v[1],q->v[2],q->vPred[0],q->vPred[1],q->vPred[2],sqrt(p->fBall2),p->fBallMax,sqrt(q->fBall2),q->fBallMax,rp,rq);
						mdlDiag(smf->pkd->mdl,ach);
						}
#endif
             		}
				}
	        }
		p->PdV += pPdV;
		p->mumax = pmumax;
		ACCEL(p,0) += pa[0];
		ACCEL(p,1) += pa[1];
		ACCEL(p,2) += pa[2];
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
			dvdotdr = vFac*(dvx*dx + dvy*dy + dvz*dz) +
				nnList[i].fDist2*smf->H;
			if (dvdotdr>0.0) {
#ifdef PDVCHECK
#endif
				q->PdV += rp*PRES_PDV(q->PoverRho2,pPoverRho2)*dvdotdr;
#ifdef PDVDEBUG			
				q->PdVpres += rp*(PRES_PDV(q->PoverRho2,pPoverRho2))*dvdotdr;
#endif

				rp *= (PRES_ACC(pPoverRho2,q->PoverRho2));
				rp *= aFac; /* convert to comoving acceleration */

		        ACCEL(q,0) += rp*dx;
		        ACCEL(q,1) += rp*dy;
		        ACCEL(q,2) += rp*dz;
#ifdef PDVCHECK
				if (p->iOrder==880556 || q->iOrder==880556 || fabs(rp) > 1e50 || fabs(ACCEL(q,0))+fabs(ACCEL(q,1))+fabs(ACCEL(q,2)) > 1e50) {
					sprintf(ach,"PDV-ACC-5 %d - %d: Den %g - %g u %g - %g PdV+ %g v %g %g %g Pred %g %g %g v %g %g %g Pred %g %g %g fB %g %g - %g %g a %g - %g\n",p->iOrder,q->iOrder,p->fDensity,q->fDensity,p->u,q->u,rq * pPoverRho2 * dvdotdr,p->v[0],p->v[1],p->v[2],p->vPred[0],p->vPred[1],p->vPred[2],q->v[0],q->v[1],q->v[2],q->vPred[0],q->vPred[1],q->vPred[2],sqrt(p->fBall2),p->fBallMax,sqrt(q->fBall2),q->fBallMax,rp,rq);
					mdlDiag(smf->pkd->mdl,ach);
					}
#endif
				}
			else {
				/* h mean */
		        hav = 0.5*(ph+sqrt(0.25*BALL2(q)));
			/* mu multiply by a to be consistent with physical c */
		        absmu = -hav*dvdotdr*smf->a 
					/(nnList[i].fDist2+0.01*hav*hav);
				/* mu terms for gas time step */
				if (absmu>q->mumax) q->mumax=absmu;
				/* viscosity */

				visc = SWITCHCOMBINE(p,q)*
				  (smf->alpha*(pc + q->c) + smf->beta*2*absmu) 
				  *absmu/(pDensity + q->fDensity);
#ifdef PDVCHECK
#endif
				q->PdV += rp*(PRES_PDV(q->PoverRho2,pPoverRho2) + 0.5*visc)*dvdotdr;
#ifdef PDVDEBUG			
				q->PdVpres += rp*(PRES_PDV(q->PoverRho2,pPoverRho2))*dvdotdr;
				q->PdVvisc += rp*(0.5*visc)*dvdotdr;
#endif
				rp *= (PRES_ACC(pPoverRho2,q->PoverRho2) + visc);
				rp *= aFac; /* convert to comoving acceleration */

		        ACCEL(q,0) += rp*dx;
		        ACCEL(q,1) += rp*dy;
		        ACCEL(q,2) += rp*dz;
#ifdef PDVCHECK
				if (p->iOrder==880556 || q->iOrder==880556 || fabs(rp) > 1e50 || fabs(ACCEL(q,0))+fabs(ACCEL(q,1))+fabs(ACCEL(q,2)) > 1e50) {
					sprintf(ach,"PDV-ACC-6 %d - %d: Den %g - %g u %g - %g PdV+ %g v %g %g %g Pred %g %g %g v %g %g %g Pred %g %g %g fB %g %g - %g %g a %g - %g\n",p->iOrder,q->iOrder,p->fDensity,q->fDensity,p->u,q->u,rq * pPoverRho2 * dvdotdr,p->v[0],p->v[1],p->v[2],p->vPred[0],p->vPred[1],p->vPred[2],q->v[0],q->v[1],q->v[2],q->vPred[0],q->vPred[1],q->vPred[2],sqrt(p->fBall2),p->fBallMax,sqrt(q->fBall2),q->fBallMax,rp,rq);
					mdlDiag(smf->pkd->mdl,ach);
					}
#endif
				}
	        }
		} 
	}

/* NB: ACCEL_PRES used here -- 
   with shock tracking disabled: #define NOSHOCKTRACK
   it is: a->aPres
   otherwise it is identical to p->a 
   The postSphPressure function combines p->a and p->aPres
*/

/* Original Particle */
void initSphPressureParticle(void *p)
{
	if (TYPEQueryACTIVE((PARTICLE *) p)) {
		((PARTICLE *)p)->mumax = 0.0;
		((PARTICLE *)p)->PdV = 0.0;
#ifdef PDVDEBUG
		((PARTICLE *)p)->PdVpres = 0.0;
#endif
		}
	}

/* Cached copies of particle */
void initSphPressure(void *p)
{
	if (TYPEQueryACTIVE((PARTICLE *) p)) {
		((PARTICLE *)p)->mumax = 0.0;
		((PARTICLE *)p)->PdV = 0.0;
#ifdef PDVDEBUG
		((PARTICLE *)p)->PdVpres = 0.0;
#endif
		ACCEL_PRES(p,0) = 0.0;
		ACCEL_PRES(p,1) = 0.0;
		ACCEL_PRES(p,2) = 0.0;
		}
	}

void combSphPressure(void *p1,void *p2)
{
	if (TYPEQueryACTIVE((PARTICLE *) p1)) {
		((PARTICLE *)p1)->PdV += ((PARTICLE *)p2)->PdV;
#ifdef PDVDEBUG
		((PARTICLE *)p1)->PdVpres += ((PARTICLE *)p2)->PdVpres;
#endif
		if (((PARTICLE *)p2)->mumax > ((PARTICLE *)p1)->mumax)
			((PARTICLE *)p1)->mumax = ((PARTICLE *)p2)->mumax;
		ACCEL_PRES(p1,0) += ACCEL_PRES(p2,0);
		ACCEL_PRES(p1,1) += ACCEL_PRES(p2,1);
		ACCEL_PRES(p1,2) += ACCEL_PRES(p2,2);
		}
	}

void postSphPressure(PARTICLE *p, SMF *smf)
{
	if (TYPEQueryACTIVE((PARTICLE *) p)) {
		ACCEL_COMB_PRES(p,0);
		ACCEL_COMB_PRES(p,1);
		ACCEL_COMB_PRES(p,2);
		}
	}

/* Gather only version -- untested */
void SphPressure(PARTICLE *p,int nSmooth,NN *nnList,SMF *smf)
{
	PARTICLE *q;
	FLOAT ih2,r2,rs1;
	FLOAT dx,dy,dz,dvx,dvy,dvz,dvdotdr;
	FLOAT pPoverRho2,pPdV,pa[3],pmumax;
	FLOAT ph,pc,pDensity,visc,hav,vFac,absmu;
	FLOAT fNorm,fNorm1,fNorm2;
	int i;

	assert(0);

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
			pPdV += rs1 * PRES_PDV(pPoverRho2,q->PoverRho2) * dvdotdr;
#ifdef PDVDEBUG
			p->PdVpres += fNorm1*rs1 * (PRES_PDV(pPoverRho2,q->PoverRho2))*dvdotdr;
#endif
			rs1 *= (PRES_ACC(pPoverRho2,q->PoverRho2));
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

			visc = SWITCHCOMBINE(p,q)*
			  (smf->alpha*(pc + q->c) + smf->beta*2*absmu) 
			  *absmu/(pDensity + q->fDensity);
		        pPdV += rs1 * (PRES_PDV(pPoverRho2,q->PoverRho2) + 0.5*visc)*dvdotdr;
#ifdef PDVDEBUG
			p->PdVpres += fNorm1*rs1 * (PRES_PDV(pPoverRho2,q->PoverRho2) + 0.5*visc)*dvdotdr;
#endif
			rs1 *= (PRES_ACC(pPoverRho2,q->PoverRho2) + visc);
			pa[0] -= rs1 * dx;
			pa[1] -= rs1 * dy;
			pa[2] -= rs1 * dz;
			}
 		}
	p->PdV += fNorm1*pPdV;
	p->mumax = pmumax;
	ACCEL_PRES(p,0) += fNorm2*pa[0];
	ACCEL_PRES(p,0) += fNorm2*pa[1];
	ACCEL_PRES(p,0) += fNorm2*pa[2];
	}

void SphPressureSym(PARTICLE *p,int nSmooth,NN *nnList,SMF *smf)
{
	PARTICLE *q;
	FLOAT ih2,r2,rs1,rq,rp;
	FLOAT dx,dy,dz,dvx,dvy,dvz,dvdotdr;
	FLOAT pPoverRho2,pPdV,pa[3],pMass,pmumax;
	FLOAT ph,pc,pDensity,visc,hav,absmu;
	FLOAT fNorm,fNorm1,aFac,vFac;
	int i;

	pc = p->c;
	pDensity = p->fDensity;
	pMass = p->fMass;
	pPoverRho2 = p->PoverRho2;
	ph = sqrt(0.25*BALL2(p));
	ih2 = 4.0/BALL2(p);
	fNorm = 0.5*M_1_PI*ih2/ph;
	fNorm1 = fNorm*ih2;	/* converts to physical u */
	aFac = (smf->a);    /* comoving acceleration factor */
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
			dvdotdr = vFac*(dvx*dx + dvy*dy + dvz*dz)
				+ nnList[i].fDist2*smf->H;

			if (TYPEQueryACTIVE(q)) {
				/* q active */
			        rp = rs1 * pMass;
				pPdV += rq*PRES_PDV(pPoverRho2,q->PoverRho2)*dvdotdr;
				q->PdV += rp*PRES_PDV(q->PoverRho2,pPoverRho2)*dvdotdr;
#ifdef PDVDEBUG
				p->PdVpres += rq*PRES_PDV(pPoverRho2,q->PoverRho2)*dvdotdr;
				q->PdVpres += rp*PRES_PDV(q->PoverRho2,pPoverRho2)*dvdotdr;
#endif
				rq *= (PRES_ACC(pPoverRho2,q->PoverRho2));
				rp *= (PRES_ACC(pPoverRho2,q->PoverRho2));
				rp *= aFac; /* convert to comoving acceleration */
				rq *= aFac;
				pa[0] -= rq * dx;
				pa[1] -= rq * dy;
				pa[2] -= rq * dz;
				ACCEL_PRES(q,0) += rp * dx;
				ACCEL_PRES(q,1) += rp * dy;
				ACCEL_PRES(q,2) += rp * dz;
              		        }
			else {
				/* q not active */
			        pPdV += rq*PRES_PDV(pPoverRho2,q->PoverRho2)*dvdotdr;
#ifdef PDVDEBUG
			        p->PdVpres += rq*PRES_PDV(pPoverRho2,q->PoverRho2)*dvdotdr;
#endif
				rq *= (PRES_ACC(pPoverRho2,q->PoverRho2));
				rq *= aFac; /* convert to comoving acceleration */

				pa[0] -= rq*dx;
				pa[1] -= rq*dy;
				pa[2] -= rq*dz;
              		        }
             		}
		p->PdV += pPdV;
		p->mumax = pmumax;
		ACCEL_PRES(p,0) += pa[0];
		ACCEL_PRES(p,1) += pa[1];
		ACCEL_PRES(p,2) += pa[2];
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
			dvdotdr = vFac*(dvx*dx + dvy*dy + dvz*dz) +
				nnList[i].fDist2*smf->H;
			q->PdV += rp*PRES_PDV(q->PoverRho2,pPoverRho2)*dvdotdr;
#ifdef PDVDEBUG
			q->PdVpres += rp*PRES_PDV(q->PoverRho2,pPoverRho2)*dvdotdr;
#endif
			rp *= (PRES_ACC(pPoverRho2,q->PoverRho2));
			rp *= aFac; /* convert to comoving acceleration */

		        ACCEL_PRES(q,0) += rp*dx;
		        ACCEL_PRES(q,1) += rp*dy;
		        ACCEL_PRES(q,2) += rp*dz;
	                }
		} 
	}

/* Original Particle */
void initSphViscosityParticle(void *p)
{
#ifdef PDVDEBUG
	if (TYPEQueryACTIVE((PARTICLE *) p)) {
		 ((PARTICLE *)p)->PdVvisc = 0.0;
	         }
#endif
}

/* Cached copies of particle */
void initSphViscosity(void *p)
{
	if (TYPEQueryACTIVE((PARTICLE *) p)) {
		((PARTICLE *)p)->mumax = 0.0;
		((PARTICLE *)p)->PdV = 0.0;
#ifdef PDVDEBUG
		((PARTICLE *)p)->PdVvisc = 0.0;
#endif
		ACCEL(p,0) = 0.0;
		ACCEL(p,1) = 0.0;
		ACCEL(p,2) = 0.0;
		}
	}

void combSphViscosity(void *p1,void *p2)
{
	if (TYPEQueryACTIVE((PARTICLE *) p1)) {
		((PARTICLE *)p1)->PdV += ((PARTICLE *)p2)->PdV;
#ifdef PDVDEBUG
		((PARTICLE *)p1)->PdVvisc += ((PARTICLE *)p2)->PdVvisc;
#endif
		if (((PARTICLE *)p2)->mumax > ((PARTICLE *)p1)->mumax)
			((PARTICLE *)p1)->mumax = ((PARTICLE *)p2)->mumax;
		ACCEL(p1,0) += ACCEL(p2,0);
		ACCEL(p1,1) += ACCEL(p2,1);
		ACCEL(p1,2) += ACCEL(p2,2);
		}
	}

/* Gather only */
void SphViscosity(PARTICLE *p,int nSmooth,NN *nnList,SMF *smf)
{
        assert(0);
	}

/* Symmetric Gather/Scatter version */
void SphViscositySym(PARTICLE *p,int nSmooth,NN *nnList,SMF *smf)
{
	PARTICLE *q;
	FLOAT ih2,r2,rs1,rq,rp;
	FLOAT dx,dy,dz,dvx,dvy,dvz,dvdotdr;
	FLOAT pPoverRho2,pPdV,pa[3],pMass,pmumax;
	FLOAT ph,pc,pDensity,visc,hav,absmu;
	FLOAT fNorm,fNorm1,aFac,vFac;
	int i;

	pc = p->c;
	pDensity = p->fDensity;
	pMass = p->fMass;
	ph = sqrt(0.25*BALL2(p));
	ih2 = 4.0/BALL2(p);
	fNorm = 0.5*M_1_PI*ih2/ph;
	fNorm1 = fNorm*ih2;	/* converts to physical u */
	aFac = (smf->a);    /* comoving acceleration factor */
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
			dvdotdr = vFac*(dvx*dx + dvy*dy + dvz*dz)
				+ nnList[i].fDist2*smf->H;
			if (dvdotdr > 0.0) continue;

			if (TYPEQueryACTIVE(q)) {
				/* q active */
				rp = rs1 * pMass;
				/* h mean - using just hp probably ok */
				hav=0.5*(ph+sqrt(0.25*BALL2(q)));
				/* mu multiply by a to be consistent with physical c */
				absmu = -hav*dvdotdr*smf->a 
				  /(nnList[i].fDist2+0.01*hav*hav);
				/* mu terms for gas time step */
				if (absmu>pmumax) pmumax=absmu;
				if (absmu>q->mumax) q->mumax=absmu;
				/* viscosity term */

				visc = 
				  (SWITCHCOMBINEA(p,q)*smf->alpha*(pc + q->c) 
				   + SWITCHCOMBINEB(p,q)*smf->beta*2*absmu) 
				  *absmu/(pDensity + q->fDensity);

				pPdV += rq*0.5*visc*dvdotdr;
				q->PdV += rp*0.5*visc*dvdotdr;
#ifdef PDVDEBUG
				p->PdVvisc += rq*0.5*visc*dvdotdr;
				q->PdVvisc += rp*0.5*visc*dvdotdr;
#endif
				rq *= visc;
				rp *= visc;
				rp *= aFac; /* convert to comoving acceleration */
				rq *= aFac;
				
				pa[0] -= rq*dx;
				pa[1] -= rq*dy;
				pa[2] -= rq*dz;
				ACCEL(q,0) += rp*dx;
				ACCEL(q,1) += rp*dy;
				ACCEL(q,2) += rp*dz;
				}
			else {
				/* q not active */
			        /* h mean */
			        hav = 0.5*(ph+sqrt(0.25*BALL2(q)));
				/* mu multiply by a to be consistent with physical c */
				absmu = -hav*dvdotdr*smf->a 
				  /(nnList[i].fDist2+0.01*hav*hav);
				/* mu terms for gas time step */
				if (absmu>pmumax) pmumax=absmu;
				/* viscosity term */

				visc = 
				  (SWITCHCOMBINEA(p,q)*smf->alpha*(pc + q->c) 
				   + SWITCHCOMBINEB(p,q)*smf->beta*2*absmu) 
				  *absmu/(pDensity + q->fDensity);
				
				pPdV += rq*0.5*visc*dvdotdr;
#ifdef PDVDEBUG
				p->PdVvisc += rq*0.5*visc*dvdotdr;
#endif
				rq *= visc;
				rq *= aFac; /* convert to comoving acceleration */
				
				pa[0] -= rq*dx;
				pa[1] -= rq*dy;
				pa[2] -= rq*dz; 
				}
	                }
		p->PdV += pPdV;
		p->mumax = pmumax;
		ACCEL(p,0) += pa[0];
		ACCEL(p,1) += pa[1];
		ACCEL(p,2) += pa[2];
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
			dvdotdr = vFac*(dvx*dx + dvy*dy + dvz*dz) +
				nnList[i].fDist2*smf->H;
			if (dvdotdr > 0.0) continue;

			/* h mean */
		        hav = 0.5*(ph+sqrt(0.25*BALL2(q)));
			/* mu multiply by a to be consistent with physical c */
		        absmu = -hav*dvdotdr*smf->a 
			  /(nnList[i].fDist2+0.01*hav*hav);
				/* mu terms for gas time step */
			if (absmu>q->mumax) q->mumax=absmu;
				/* viscosity */

			visc = 
			  (SWITCHCOMBINEA(p,q)*smf->alpha*(pc + q->c) 
			   + SWITCHCOMBINEB(p,q)*smf->beta*2*absmu) 
			  *absmu/(pDensity + q->fDensity);

			q->PdV += rp*0.5*visc*dvdotdr;
#ifdef PDVDEBUG
			q->PdVvisc += rp*0.5*visc*dvdotdr;
#endif
			rp *= visc;
			rp *= aFac; /* convert to comoving acceleration */

			ACCEL(q,0) += rp*dx;
			ACCEL(q,1) += rp*dy;
			ACCEL(q,2) += rp*dz;
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
#ifdef SHOCKTRACK
		((PARTICLE *)p)->divrhov = 0.0;
		((PARTICLE *)p)->gradrho[0] = 0.0;
		((PARTICLE *)p)->gradrho[1] = 0.0;
		((PARTICLE *)p)->gradrho[2] = 0.0;
#endif
		}
	}

void combDivVort(void *p1,void *p2)
{
	if (TYPEQueryACTIVE((PARTICLE *) p1 )) {
		((PARTICLE *)p1)->divv += ((PARTICLE *)p2)->divv;
		((PARTICLE *)p1)->curlv[0] += ((PARTICLE *)p2)->curlv[0];
		((PARTICLE *)p1)->curlv[1] += ((PARTICLE *)p2)->curlv[1];
		((PARTICLE *)p1)->curlv[2] += ((PARTICLE *)p2)->curlv[2];
#ifdef SHOCKTRACK
		((PARTICLE *)p1)->divrhov += ((PARTICLE *)p2)->divrhov;
		((PARTICLE *)p1)->gradrho[0] += ((PARTICLE *)p2)->gradrho[0];
		((PARTICLE *)p1)->gradrho[1] += ((PARTICLE *)p2)->gradrho[1];
		((PARTICLE *)p1)->gradrho[2] += ((PARTICLE *)p2)->gradrho[2];
#endif
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

	if (!TYPEQueryACTIVE(p)) return;

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
	int i;
 
	mdlassert(smf->pkd->mdl, TYPETest(p,TYPE_GAS));
	
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
		mdlassert(smf->pkd->mdl, TYPETest(q,TYPE_GAS));
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
			dvdotdr = vFac*(dvx*dx + dvy*dy + dvz*dz) +
				nnList[i].fDist2*smf->H;

			if (TYPEQueryACTIVE(q)) {
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

#ifdef SHOCKTRACK
				p->divrhov -= rs1*dvdotdr*q->fMass;
				q->divrhov -= rs1*dvdotdr*pMass;
				p->gradrho[0] += rs1*q->fMass*dx;
				q->gradrho[0] -= rs1*pMass*dx;
				p->gradrho[1] += rs1*q->fMass*dy;
				q->gradrho[1] -= rs1*pMass*dy;
				p->gradrho[2] += rs1*q->fMass*dz;
				q->gradrho[2] -= rs1*pMass*dz;
#endif
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

#ifdef SHOCKTRACK
				p->divrhov -= rs1*dvdotdr*q->fMass;
				p->gradrho[0] += rs1*q->fMass*dx;
				p->gradrho[1] += rs1*q->fMass*dy;
				p->gradrho[2] += rs1*q->fMass*dz;
#endif
		        }
	        }
		} 
	else {
		/* p not active */
		for (i=0;i<nSmooth;++i) {
	        q = nnList[i].pPart;
		mdlassert(smf->pkd->mdl, TYPETest(q,TYPE_GAS));
			if (!TYPEQueryACTIVE(q)) continue; /* neither active */

	        r2 = nnList[i].fDist2*ih2;
			DKERNEL(rs1,r2);
			rs1 *=fNorm;
			rp = rs1*pMass/pDensity;

			dx = nnList[i].dx;
			dy = nnList[i].dy;
			dz = nnList[i].dz;
			dvx = p->vPred[0] - q->vPred[0];
			dvy = p->vPred[1] - q->vPred[1];
			dvz = p->vPred[2] - q->vPred[2];
			dvdotdr = vFac*(dvx*dx + dvy*dy + dvz*dz)
				+ nnList[i].fDist2*smf->H;
			/* q active */
			q->divv -= rp*dvdotdr;
			dv=vFac*(dvz*dy - dvy*dz);
			q->curlv[0] += rp*dv;
			dv=vFac*(dvx*dz - dvz*dx);
			q->curlv[1] += rp*dv;
			dv=vFac*(dvy*dx - dvx*dy);
			q->curlv[2] += rp*dv;

#ifdef SHOCKTRACK
			q->divrhov -= rs1*dvdotdr*pMass;
			q->gradrho[0] -= rs1*pMass*dx;
			q->gradrho[1] -= rs1*pMass*dy;
			q->gradrho[2] -= rs1*pMass*dz;
#endif
	        }
		} 
	}

void initShockTrack(void *p)
{
#ifdef SHOCKTRACK
	if (TYPEQueryACTIVE((PARTICLE *) p )) {
		((PARTICLE *)p)->divrhov = 0.0;
		}
#endif
	}

void combShockTrack(void *p1,void *p2)
{
#ifdef SHOCKTRACK
	if (TYPEQueryACTIVE((PARTICLE *) p1 )) {
	        if (((PARTICLE *)p2)->divrhov > ((PARTICLE *)p2)->divrhov) 
		  ((PARTICLE *)p1)->divrhov = ((PARTICLE *)p2)->divrhov;
		}
#endif
	}

/* Gather only version -- untested */
void ShockTrack(PARTICLE *p,int nSmooth,NN *nnList,SMF *smf)
{
	assert(0);
	}

/* Output is physical divv and curlv -- thus a*h_co*divv is physical */
void ShockTrackSym(PARTICLE *p,int nSmooth,NN *nnList,SMF *smf)
{
	PARTICLE *q;
	double Mach,dv;
	int i,j;

#ifdef SHOCKTRACK
	if (TYPEQueryACTIVE( p )) {
		/* p active */
		for (i=0;i<nSmooth;++i) {
	                q = nnList[i].pPart;
			Mach = 0;
			for (j=0;j<3;j++) {
			  dv = p->vPred[j] - q->gradrho[j];
			  Mach += dv*dv;
			}
			Mach = sqrt(Mach)/p->c;
			if (Mach > p->divrhov) p->divrhov = Mach;
			if (TYPEQueryACTIVE(q)) {
			  Mach = 0;
			  for (j=0;j<3;j++) {
			    dv = q->vPred[j] - p->gradrho[j];
			    Mach += dv*dv;
			  }
			  Mach = sqrt(Mach)/q->c;
			  if (Mach > q->divrhov) q->divrhov = Mach;
		        }
	        }
		} 
	else {
		/* p not active */
		for (i=0;i<nSmooth;++i) {
 	                q = nnList[i].pPart;
			if (!TYPEQueryACTIVE(q)) continue; /* neither active */
			Mach = 0;
			for (j=0;j<3;j++) {
			  dv = q->vPred[j] - p->gradrho[j];
			  Mach += dv*dv;
			}
			Mach = sqrt(Mach)/q->c;
			if (Mach > q->divrhov) q->divrhov = Mach;
	        }
		} 
#endif
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
		ACCEL(p,0) = 0.0;
		ACCEL(p,1) = 0.0;
		ACCEL(p,2) = 0.0;
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
		ACCEL(p1,0) += ACCEL(p2,0);
		ACCEL(p1,1) += ACCEL(p2,1);
		ACCEL(p1,2) += ACCEL(p2,2);
		}
	}

/* Gather only version -- (untested)  */
void HKPressureTerms(PARTICLE *p,int nSmooth,NN *nnList,SMF *smf)
{
	PARTICLE *q;
	FLOAT ih2,r2,rs1;
	FLOAT dx,dy,dz,dvx,dvy,dvz,dvdotdr;
	FLOAT pPoverRho2,pQonRho2,qQonRho2,qhdivv;
	FLOAT ph,pc,pDensity,visc,absmu,qh,pMass,hav;
	FLOAT fNorm,fNorm1,aFac,vFac;
	int i;

	if (!TYPEQueryACTIVE(p)) return;

	pc = p->c;
	pDensity = p->fDensity;
	pMass = p->fMass;
	pPoverRho2 = p->PoverRho2;
	ph = sqrt(0.25*BALL2(p));
	/* QonRho2 given same scaling with a as PonRho2 */
	pQonRho2 = (p->divv>0.0 ? 0.0 : fabs(p->divv)*ph*smf->a
				*(smf->alpha*pc + smf->beta*fabs(p->divv)*ph*smf->a)/pDensity);
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
			p->PdV += rs1*PRES_PDV(pPoverRho2,q->PoverRho2)*dvdotdr;
			rs1 *= (PRES_ACC(pPoverRho2,q->PoverRho2));
			rs1 *= aFac;
			ACCEL(p,0) -= rs1*dx;
			ACCEL(p,1) -= rs1*dy;
			ACCEL(p,2) -= rs1*dz;
			}
		else {
			qh=sqrt(0.25*BALL2(q));
			qhdivv = qh*fabs(q->divv)*smf->a; /* units of physical velocity */
			qQonRho2 = (qhdivv>0.0 ? 0.0 : 
						qhdivv*(smf->alpha*q->c + smf->beta*qhdivv)/q->fDensity);
			visc = pQonRho2 + qQonRho2;
			/* mu -- same timestep criteria as standard sph above (for now) */
			hav=0.5*(qh+ph);
			absmu = -hav*dvdotdr*smf->a
				/(nnList[i].fDist2+0.01*hav*hav);
			if (absmu>p->mumax) p->mumax=absmu;
			p->PdV += rs1 * (PRES_PDV(pPoverRho2,q->PoverRho2) + 0.5*visc) * dvdotdr;
			rs1 *= (PRES_ACC(pPoverRho2,q->PoverRho2) + visc);
			rs1 *= aFac; /* convert to comoving acceleration */
			ACCEL(p,0) -= rs1*dx;
			ACCEL(p,1) -= rs1*dy;
			ACCEL(p,2) -= rs1*dz;
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
	FLOAT fNorm,fNorm1,aFac,vFac;
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
		rq = rs1*q->fMass;
		rp = rs1*pMass;

		dx = nnList[i].dx;
		dy = nnList[i].dy;
		dz = nnList[i].dz;
		dvx = p->vPred[0] - q->vPred[0];
		dvy = p->vPred[1] - q->vPred[1];
		dvz = p->vPred[2] - q->vPred[2];
		dvdotdr = vFac*(dvx*dx + dvy*dy + dvz*dz) + nnList[i].fDist2*smf->H;

		if (dvdotdr>0.0) {
			if (TYPEQueryACTIVE(p)) {
		                p->PdV += rq*PRES_PDV(pPoverRho2,q->PoverRho2)*dvdotdr;
				rq *= (PRES_ACC(pPoverRho2,q->PoverRho2));
				rq *= aFac;
				ACCEL(p,0) -= rq*dx;
				ACCEL(p,1) -= rq*dy;
				ACCEL(p,2) -= rq*dz;
		        }
			if (TYPEQueryACTIVE(q)) {
				q->PdV += rp*PRES_PDV(q->PoverRho2,pPoverRho2)*dvdotdr;
				rp *= (PRES_ACC(pPoverRho2,q->PoverRho2));
				rp *= aFac; /* convert to comoving acceleration */
				ACCEL(q,0) += rp*dx;
				ACCEL(q,1) += rp*dy;
				ACCEL(q,2) += rp*dz;
				}
			}
		else {
			qh=sqrt(0.25*BALL2(q));
			qhdivv = qh*fabs(q->divv)*smf->a; /* units of physical velocity */
			qQonRho2 = (qhdivv>0.0 ? 0.0 : 
						qhdivv*(smf->alpha*q->c + smf->beta*qhdivv)/q->fDensity);
			visc = pQonRho2 + qQonRho2;
			/* mu -- same timestep criteria as standard sph above (for now) */
			hav=0.5*(qh + ph);
			absmu = -hav*dvdotdr*smf->a/(nnList[i].fDist2+0.01*hav*hav);
			if (TYPEQueryACTIVE(p)) {
				if (absmu>p->mumax) p->mumax=absmu;
				p->PdV += rq*(PRES_PDV(pPoverRho2,q->PoverRho2) + 0.5*visc)*dvdotdr;
				rq *= (PRES_ACC(pPoverRho2,q->PoverRho2) + visc);
				rq *= aFac; /* convert to comoving acceleration */
				ACCEL(p,0) -= rq*dx;
				ACCEL(p,1) -= rq*dy;
				ACCEL(p,2) -= rq*dz;
		        }
			if (TYPEQueryACTIVE(q)) {
				if (absmu>q->mumax) q->mumax=absmu;
				q->PdV += rp*(PRES_PDV(q->PoverRho2,pPoverRho2) + 0.5*visc)*dvdotdr;
				rp *= (PRES_ACC(pPoverRho2,q->PoverRho2) + visc);
				rp *= aFac; /* convert to comoving acceleration */
				ACCEL(q,0) += rp*dx;
				ACCEL(q,1) += rp*dy;
				ACCEL(q,2) += rp*dz;
				}
			}
		}
	}

/* Original Particle */
void initHKViscosityParticle(void *p)
{
	ACCEL(p,0) += ACCEL_PRES(p,0);
	ACCEL(p,1) += ACCEL_PRES(p,1);
	ACCEL(p,2) += ACCEL_PRES(p,2);
	}

/* Cached copies of particle */
void initHKViscosity(void *p)
{
	if (TYPEQueryACTIVE((PARTICLE *) p)) {
		((PARTICLE *)p)->mumax = 0.0;
		((PARTICLE *)p)->PdV = 0.0;
		ACCEL(p,0) = 0.0;
		ACCEL(p,1) = 0.0;
		ACCEL(p,2) = 0.0;
		}
	}

void combHKViscosity(void *p1,void *p2)
{
	if (TYPEQueryACTIVE((PARTICLE *) p1)) {
		((PARTICLE *)p1)->PdV += ((PARTICLE *)p2)->PdV;
		if (((PARTICLE *)p2)->mumax > ((PARTICLE *)p1)->mumax)
			((PARTICLE *)p1)->mumax = ((PARTICLE *)p2)->mumax;
		ACCEL(p1,0) += ACCEL(p2,0);
		ACCEL(p1,1) += ACCEL(p2,1);
		ACCEL(p1,2) += ACCEL(p2,2);
		}
	}

/* Gather only version */
void HKViscosity(PARTICLE *p,int nSmooth,NN *nnList,SMF *smf)
{
        assert(0);
	}

/* Bulk viscosity */
void HKViscositySym(PARTICLE *p,int nSmooth,NN *nnList,SMF *smf)
{
	PARTICLE *q;
	FLOAT ih2,r2,rs1,rq,rp;
	FLOAT dx,dy,dz,dvx,dvy,dvz,dvdotdr;
	FLOAT pPoverRho2,pQonRho2,qQonRho2,qhdivv;
	FLOAT ph,pc,pDensity,visc,absmu,qh,pMass,hav;
	FLOAT fNorm,fNorm1,aFac,vFac;
	int i;

	pc = p->c;
	pDensity = p->fDensity;
	pMass = p->fMass;
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
		rq = rs1*q->fMass;
		rp = rs1*pMass;

		dx = nnList[i].dx;
		dy = nnList[i].dy;
		dz = nnList[i].dz;
		dvx = p->vPred[0] - q->vPred[0];
		dvy = p->vPred[1] - q->vPred[1];
		dvz = p->vPred[2] - q->vPred[2];
		dvdotdr = vFac*(dvx*dx + dvy*dy + dvz*dz) + nnList[i].fDist2*smf->H;

		if (dvdotdr<0.0) {
			qh=sqrt(0.25*BALL2(q));
			qhdivv = qh*fabs(q->divv)*smf->a; /* units of physical velocity */
			qQonRho2 = (qhdivv>0.0 ? 0.0 : 
						qhdivv*(smf->alpha*q->c + smf->beta*qhdivv)/q->fDensity);

			visc = SWITCHCOMBINE(p,q)*(pQonRho2 + qQonRho2);
			/* mu -- same timestep criteria as standard sph above (for now) */
			hav=0.5*(qh + ph);
			absmu = -hav*dvdotdr*smf->a/(nnList[i].fDist2+0.01*hav*hav);
			if (TYPEQueryACTIVE(p)) {
				if (absmu>p->mumax) p->mumax=absmu;
		        p->PdV += rq*0.5*visc*dvdotdr;
				rq *= visc;
				rq *= aFac; /* convert to comoving acceleration */
		        ACCEL(p,0) -= rq*dx;
		        ACCEL(p,1) -= rq*dy;
		        ACCEL(p,2) -= rq*dz;
		        }
			if (TYPEQueryACTIVE(q)) {
				if (absmu>q->mumax) q->mumax=absmu;
				q->PdV += rp*0.5*visc*dvdotdr;
				rp *= visc;
				rp *= aFac; /* convert to comoving acceleration */
		        ACCEL(q,0) += rp*dx;
		        ACCEL(q,1) += rp*dy;
		        ACCEL(q,2) += rp*dz;
				}
			}
		}
	}

#ifdef STARFORM
void initDistDeletedGas(void *p1)
{
    /*
     * Warning: kludgery.  We need to accumulate mass in the cached
     * particle, but we also need to keep the original mass around.
     * Lets us another (hopefully unused) field to hold the original
     * mass.
     */
    ((PARTICLE *)p1)->PdV = ((PARTICLE *)p1)->fMass;

    /*
     * Zero out accumulated quantities.
     */
    ((PARTICLE *)p1)->v[0] = 0.0;
    ((PARTICLE *)p1)->v[1] = 0.0;
    ((PARTICLE *)p1)->v[2] = 0.0;
    ((PARTICLE *)p1)->u = 0.0;
    ((PARTICLE *)p1)->fMetals = 0.0;
    }

void combDistDeletedGas(void *p1,void *p2)
{
    /*
     * See kudgery notice above.
     */
    FLOAT fAddedMass = ((PARTICLE *)p2)->fMass - ((PARTICLE *)p2)->PdV;
    
    ((PARTICLE *)p1)->fMass += fAddedMass;
    ((PARTICLE *)p1)->v[0] += ((PARTICLE *)p2)->v[0];
    ((PARTICLE *)p1)->v[1] += ((PARTICLE *)p2)->v[1];
    ((PARTICLE *)p1)->v[2] += ((PARTICLE *)p2)->v[2];
    ((PARTICLE *)p1)->u += ((PARTICLE *)p2)->u;
    ((PARTICLE *)p1)->fMetals += ((PARTICLE *)p2)->fMetals;
    }

void DistDeletedGas(PARTICLE *p,int nSmooth,NN *nnList,SMF *smf)
{	PARTICLE *q;
	FLOAT fNorm,ih2,r2,rs,rstot,fNorm_u;
	int i;

	assert(TYPETest(p, TYPE_GAS));
	ih2 = 4.0/BALL2(p);
        rstot = 0;        
        fNorm_u = 0;
	for (i=0;i<nSmooth;++i) {
            q = nnList[i].pPart;
	    if(TYPETest(q, TYPE_DELETED))
		continue;
	    assert(TYPETest(q, TYPE_GAS));
            r2 = nnList[i].fDist2*ih2;            
            KERNEL(rs,r2);
            rstot += rs;
            fNorm_u += q->fMass*rs;
        }
	assert(rstot > 0.0);
        fNorm = 1./rstot;
        fNorm_u = p->fMass/fNorm_u;        
	for (i=0;i<nSmooth;++i) {
            q = nnList[i].pPart;
	    if(TYPETest(q, TYPE_DELETED))
		continue;

            r2 = nnList[i].fDist2*ih2;            
            KERNEL(rs,r2);
	    /*
	     * All these quantities are per unit mass.
	     */
            q->u += rs*fNorm_u*p->u;
            q->v[0] += rs*fNorm_u*p->v[0];            
            q->v[1] += rs*fNorm_u*p->v[1];
            q->v[2] += rs*fNorm_u*p->v[2];
            q->fMetals += rs*fNorm_u*p->fMetals;

            rs *= fNorm;
            q->fMass += rs*p->fMass;
        }
}

void DeleteGas(PARTICLE *p,int nSmooth,NN *nnList,SMF *smf)
{
/* flag low mass gas particles for deletion */
	PARTICLE *q;
	FLOAT fMasstot,fMassavg;
	int i;

	assert(TYPETest(p, TYPE_GAS));
        fMasstot = 0;
	for (i=0;i<nSmooth;++i) {
            q = nnList[i].pPart;
	    assert(TYPETest(q, TYPE_GAS));
            fMasstot += q->fMass;
        }
        fMassavg = fMasstot/(FLOAT) nSmooth;
        if (p->fMass < smf->dMinMassFrac*fMassavg) {
            pkdDeleteParticle(smf->pkd, p);
        }
        
}

void initDistSNEnergy(void *p1)
{
    /*
     * Warning: kludgery.  We need to accumulate mass in the cached
     * particle, but we also need to keep the original mass around.
     * Lets us another (hopefully unused) field to hold the original
     * mass.
     */
    ((PARTICLE *)p1)->PdV = ((PARTICLE *)p1)->fMass;

    /*
     * Zero out accumulated quantities.
     */
    ((PARTICLE *)p1)->fESNrate = 0.0;
    ((PARTICLE *)p1)->fMetals = 0.0;
    }

void combDistSNEnergy(void *p1,void *p2)
{
    /*
     * See kudgery notice above.
     */
    FLOAT fAddedMass = ((PARTICLE *)p2)->fMass - ((PARTICLE *)p2)->PdV;
    
    ((PARTICLE *)p1)->fMass += fAddedMass;
    ((PARTICLE *)p1)->fESNrate += ((PARTICLE *)p2)->fESNrate;
    ((PARTICLE *)p1)->fMetals += ((PARTICLE *)p2)->fMetals;
    }

void DistSNEnergy(PARTICLE *p,int nSmooth,NN *nnList,SMF *smf)
{
	PARTICLE *q;
	FLOAT fNorm,ih2,r2,rs,rstot,fNorm_u;
	int i;

	assert(TYPETest(p, TYPE_STAR));
	ih2 = 4.0/BALL2(p);
        rstot = 0;        
        fNorm_u = 0;
	
	for (i=0;i<nSmooth;++i) {
            r2 = nnList[i].fDist2*ih2;            
            KERNEL(rs,r2);
            rstot += rs;
            q = nnList[i].pPart;
	    assert(TYPETest(q, TYPE_GAS));
            fNorm_u += q->fMass*rs;
        }
        fNorm = 1./rstot;
        fNorm_u = p->fMSN/fNorm_u;
	for (i=0;i<nSmooth;++i) {
            q = nnList[i].pPart;
            r2 = nnList[i].fDist2*ih2;            
            KERNEL(rs,r2);
            q->fESNrate += rs*fNorm_u*p->fESNrate;
            q->fMetals += rs*fNorm_u*p->fSNMetals;
	    
            rs *= fNorm;            
            q->fMass += rs*p->fMSN;
        }
}
#endif /* STARFORM */
#endif /* GASOLINE */

#ifdef COLLISIONS

void
initFindRejects(void *p)
{
	((PARTICLE *)p)->dtCol = 0;
	}

void
combFindRejects(void *p1,void *p2)
{
	if (((PARTICLE *)p2)->dtCol < 0) ((PARTICLE *)p1)->dtCol = -1;
	}

void
FindRejects(PARTICLE *p,int nSmooth,NN *nnList,SMF *smf)
{
	/*
	 ** Checks "nSmooth" neighbours of "p" for overlap (physical, Hill
	 ** radius, or a combination). When the particles in question are
	 ** of different "size", the one with the smallest size is rejected
	 ** first, otherwise the one with the higher iOrder is rejected.
	 ** Note that only planetesimal neighbours not already rejected
	 ** are considered. This procedure uses a combiner cache so that
	 ** the neighbours of "p" can be flagged with maximum efficiency.
	 */

	PARTICLE *pn;
	double a=0,r,r2,v2,an,rh,rn,sr;
	int i;

	if (p->iColor != PLANETESIMAL || p->dtCol < 0) return;

	r = 2*p->fSoft; /* radius = 2 * softening */

	if (smf->dCentMass > 0) {
		r2 = p->r[0]*p->r[0] + p->r[1]*p->r[1] + p->r[2]*p->r[2];
		v2 = p->v[0]*p->v[0] + p->v[1]*p->v[1] + p->v[2]*p->v[2];
		assert(r2 > 0); /* particle must not be at origin */
		a = 2/sqrt(r2) - v2/(smf->dCentMass + p->fMass);
		assert(a != 0); /* can't handle parabolic orbits */
		a = 1/a;
		}

	for (i=0;i<nSmooth;i++) {
		pn = nnList[i].pPart;
		if (pn->iOrder == p->iOrder || pn->iColor != PLANETESIMAL ||
			pn->dtCol < 0) continue;
		rn = 2*pn->fSoft;
		if (smf->dCentMass > 0) {
			r2 = pn->r[0]*pn->r[0] + pn->r[1]*pn->r[1] + pn->r[2]*pn->r[2];
			v2 = pn->v[0]*pn->v[0] + pn->v[1]*pn->v[1] + pn->v[2]*pn->v[2];
			assert(r2 > 0);
			an = 2/sqrt(r2) - v2/(smf->dCentMass + pn->fMass);
			assert(an != 0);
			an = 1/an;
			rh = pow((p->fMass + pn->fMass)/(3*smf->dCentMass),1.0/3)*(a+an)/2;
			if (rh > r) r = rh;
			if (rh > rn) rn = rh;
			}
		if (rn > r || (rn == r && pn->iOrder < p->iOrder)) continue;
		sr = r + rn;
		if (nnList[i].fDist2 <= sr*sr) pn->dtCol = -1; /* cf REJECT macro */
		}
	}

/*#define VERBOSE_COLLAPSE*/

void
_CheckForCollapse(PARTICLE *p,double dt,double rdotv,double r2,SMF *smf)
{
	/*
	 ** Sets bTinyStep flag of particle "p" to 1 if collision time "dt"
	 ** represents a fractional motion of less than "smf->dCollapseLimit" X
	 ** the current separation distance ("rdotv" is the dot product of the
	 ** relative position and relative velocity; "r2" is the square of the
	 ** distance). This routine should only be called by CheckForCollision().
	 */

	double dRatio;

	dRatio = rdotv*(p->dtPrevCol - dt)/r2;
	assert(dRatio > 0);
	if (dRatio < smf->dCollapseLimit) {
#ifdef VERBOSE_COLLAPSE
		char ach[256];
		(void) sprintf(ach,"WARNING [T=%e]: Tiny step %i & %i "
					   "(dt=%.16e, dRatio=%.16e)\n",smf->dTime,
					   p->iOrder,p->iOrderCol,dt,dRatio);
#ifdef MDL_DIAG
		mdlDiag(smf->pkd->mdl,ach);
#else
		(void) printf("%i: %s",smf->pkd->idSelf,ach);
#endif
#endif
		p->bTinyStep = 1;
		}
	}

#undef VERBOSE_COLLAPSE

void
CheckForCollision(PARTICLE *p,int nSmooth,NN *nnList,SMF *smf)
{
	/*
	 ** Checks whether particle "p" will collide with any of its "nSmooth"
	 ** nearest neighbours in "nnList" during drift interval smf->dStart to
	 ** smf->dEnd, relative to current time. If any collisions are found,
	 ** the relative time to the one that will occur first is noted in
	 ** p->dtCol along with the iOrder of the collider in p->iOrderCol.
	 ** Note that it can happen that only one particle of a colliding pair
	 ** will "know" about the collision, since the other may be inactive.
	 */

	PARTICLE *pn;
	FLOAT vx,vy,vz,rdotv,v2,sr,D,dt;
	int i;

    assert(TYPEQueryACTIVE(p)); /* just to be sure */

	p->dtCol = DBL_MAX; /* initialize */
	p->iOrderCol = -1;
	p->bTinyStep = 0;
	if (smf->dStart == 0) { /* these are set in PutColliderInfo() */
		p->dtPrevCol = 0;
		p->iPrevCol = INT_MAX;
		}

	for (i=0;i<nSmooth;i++) {
		pn = nnList[i].pPart;
		/*
		 ** Consider all valid particles, even if inactive.  We can skip
		 ** this neighbour if it's the last particle we collided with in
		 ** this search interval, eliminating the potential problem of
		 ** "ghost" particles colliding due to roundoff error. Note that
		 ** iPrevCol must essentially be reset after each KICK.
		 */
		if (pn == p || pn->iOrder < 0 || pn->iOrder == p->iPrevCol) continue;
		vx = p->v[0] - pn->v[0];
		vy = p->v[1] - pn->v[1];
		vz = p->v[2] - pn->v[2];
#ifdef SLIDING_PATCH
		if (p->r[0] > pn->r[0] && nnList[i].dx < 0)
			vy += 1.5*smf->dOrbFreq*smf->fLx;
		else if (p->r[0] < pn->r[0] && nnList[i].dx > 0)
			vy -= 1.5*smf->dOrbFreq*smf->fLx;
#endif
		rdotv = nnList[i].dx*vx + nnList[i].dy*vy + nnList[i].dz*vz;
		if (rdotv >= 0)	continue; /* skip if particle not approaching */
		v2 = vx*vx + vy*vy + vz*vz;
		sr = 2*(p->fSoft + pn->fSoft); /* softening = 0.5 * particle radius */
		D = 1 - v2*(nnList[i].fDist2 - sr*sr)/(rdotv*rdotv);
		if (D < 0) continue; /* no real solutions ==> no collision */
		D = sqrt(D);
		dt = rdotv*(D - 1)/v2; /* minimum time to surface contact */
		/*
		 ** There should be no touching or overlapping particles
		 ** at the start of the step...
		 */
		if (smf->dStart == 0 && dt <= 0) {
			char ach[512];
#ifdef FIX_COLLAPSE
			if (dt < p->dtCol) { /* take most negative */
				FLOAT fOverlap = sqrt(nnList[i].fDist2)/sr - 1;
				(void) sprintf(ach,"WARNING [T=%e]: "
							   "POSITION FIX %i & %i D=%e dt=%e\n",
							   smf->dTime,p->iOrder,pn->iOrder,D,dt);
#ifdef MDL_DIAG
				mdlDiag(smf->pkd->mdl,ach);
#else
				(void) printf("%i: %s",smf->pkd->idSelf,ach);
#endif
				if (fOverlap > 0.01) {
					(void) sprintf(ach,"WARNING [T=%e]: LARGE OVERLAP (%g%%)",
								   smf->dTime,100*fOverlap);
#ifdef MDL_DIAG
					mdlDiag(smf->pkd->mdl,ach);
#else
					(void) printf("%i: %s",smf->pkd->idSelf,ach);
#endif
					}
				p->dtCol = dt;
				p->iOrderCol = pn->iOrder;
				continue;
				}
#else /* FIX_COLLAPSE */
			(void) sprintf(ach,"OVERLAP [T=%e]:\n"
						   "%i (r=%g,%g,%g,iRung=%i) &\n"
						   "%i (rn=%g,%g,%g,iRung=%i)\n"
						   "fDist=%g v=%g,%g,%g v_mag=%g\n"
						   "rv=%g sr=%g sep'n=%g D=%g dt=%g\n",
						   smf->dTime,
						   p->iOrder,p->r[0],p->r[1],p->r[2],p->iRung,
						   pn->iOrder,pn->r[0],pn->r[1],pn->r[2],pn->iRung,
						   sqrt(nnList[i].fDist2),vx,vy,vz,sqrt(vx*vx+vy*vy+vz*vz),
						   rdotv,sr,sqrt(nnList[i].fDist2) - sr,D,dt);
#ifdef MDL_DIAG
			mdlDiag(smf->pkd->mdl,ach);
#else
			(void) printf("%i: %s",smf->pkd->idSelf,ach);
#endif
			assert(0); /* particles must not touch or overlap initially */
#endif
			}
		if (dt > smf->dStart && dt <= smf->dEnd) {
			if (dt > p->dtCol) continue; /* skip if this one happens later */
			assert(dt < p->dtCol); /* can't handle simultaneous collisions */
			p->dtCol = dt;
			p->iOrderCol = pn->iOrder;
			if (smf->dCollapseLimit)
				_CheckForCollapse(p,dt,rdotv,nnList[i].fDist2,smf);
			}
		}

#ifdef SAND_PILE
#ifdef TUMBLER
	{
	WALLS *w = &smf->walls;
	double R,ndotr,ndotv,target,dt,r2;
	int j;
	R = 2*p->fSoft; /* this is the particle radius (twice the softening) */

	rdotv = r2 = 0; /* to keep compiler happy */
	for (i=0;i<w->nWalls;i++) {
		/* skip check if previous collision was with this wall, *unless*
		   this is a cylindrical wall. In that case we have to check for
		   collision with a different part of the wall */
		if ( (p->iPrevCol == -i - 1) && (w->wall[i].type != 1) ) continue;
		ndotr = ndotv = 0;
		for (j=0;j<3;j++) {
			ndotr += w->wall[i].n[j] * p->r[j];
			ndotv += w->wall[i].n[j] * p->v[j];
			}
		if (w->wall[i].type == 1) {     /* cylinder wall */
			double rprime[3],vprime[3],rr=0,rv=0,vv=0,dsc;
			for (j=0;j<3;j++) {
				rprime[j] = p->r[j] - ndotr * w->wall[i].n[j];
				vprime[j] = p->v[j] - ndotv * w->wall[i].n[j];
				rr += rprime[j]*rprime[j];
				rv += rprime[j]*vprime[j];
				vv += vprime[j]*vprime[j];
				}
			if (vv == 0) continue;
			target = w->wall[i].radius - R;
			dsc = rv*rv - rr*vv + target*target*vv;
			if (dsc < 0) continue;	/* no intersection with cylinder */
			/* if dsc > 0, there are two values of dt that satisfy the
			   equations. Whether we start inside or outside, we always
			   want the larger so we're colliding from the inside. */
			dt = ( sqrt(dsc) - rv ) / vv;
			if (dt <= 0) continue; /* should do an overlap check here */
			if (smf->dCollapseLimit) {
				if (rv > 0) {
					rdotv = (1 - target/sqrt(rr))*rv;
					r2 = (target - sqrt(rr));
					}
				else {
					rdotv = (1 + target/sqrt(rr))*rv;
					r2 = (target + sqrt(rr));
					}
				r2 *= r2;
				}
			}
		else { /* infinite flat wall */
			double distance; 
			if (ndotv == 0) continue; 
			target = w->wall[i].ndotp; 
			/* first check for overlap */
			if ( (smf->dStart == 0) && (fabs(target-ndotr) <= R) ) {
				(void) fprintf(stderr,"OVERLAP [T=%g]: %i & wall %i dt %g\n",
							   smf->dTime,p->iOrder,i,dt); 
				(void) fprintf(stderr,"x=%f y=%f z=%f vx=%f vy=%f vz=%f\n",
							   p->r[0],p->r[1],p->r[2],p->v[0],p->v[1],p->v[2]); 
				}
			/* now check for collision */
			target += (ndotr < w->wall[i].ndotp) ? -R : R; 
			distance = target - ndotr; 
			dt = distance / ndotv; 
			if (dt <= 0) continue; /* should do an overlap check here */
			if (smf->dCollapseLimit) {
				rdotv = - distance * ndotv; 
				r2 = distance*distance; 
				}
			}
		assert(smf->dStart > 0 || dt > 0);
		if (dt > smf->dStart && dt <= smf->dEnd && dt < p->dtCol) {
			p->dtCol = dt;
			p->iOrderCol = -i -1;
			if (smf->dCollapseLimit)
				_CheckForCollapse(p,dt,rdotv,r2,smf);
			}
		}
	}
#else /* TUMBLER */
	{
	WALLS *w = &smf->walls;
	double R,lx,lz,r2=0,x0,z0,d,m,b,dp,ldotv,l2,st,nx,nz,l;
	int approaching;

	R = 2*p->fSoft;
	v2 = p->v[0]*p->v[0] + p->v[2]*p->v[2]; /* velocity relative to frame */
	for (i=0;i<w->nWalls;i++) {
		/* check endpoints first */
		approaching = 0;
		/* first endpoint */
		lx = p->r[0] - w->wall[i].x1;
		lz = p->r[2] - w->wall[i].z1;
		rdotv = lx*p->v[0] + lz*p->v[2];
		if (p->iPrevCol != -w->nWalls - i*2 - 1 && rdotv < 0) {
			approaching = 1;
			r2 = lx*lx + lz*lz;
			D = 1 - v2*(r2 - R*R)/(rdotv*rdotv);
			if (D >= 0) {
				dt = rdotv*(sqrt(D) - 1)/v2;
				assert(smf->dStart > 0 || dt > 0);
				if (dt > smf->dStart && dt <= smf->dEnd && dt < p->dtCol) {
					p->dtCol = dt;
					p->iOrderCol = -w->nWalls - i*2 - 1; /* endpt 1 encoding */
					if (smf->dCollapseLimit)
						_CheckForCollapse(p,dt,rdotv,r2,smf);
					}
				}
			}
		/* second endpoint */
		lx = p->r[0] - w->wall[i].x2;
		lz = p->r[2] - w->wall[i].z2;
		rdotv = lx*p->v[0] + lz*p->v[2];
		if (p->iPrevCol != -w->nWalls - i*2 - 2 && rdotv < 0) {
			approaching = 1;
			r2 = lx*lx + lz*lz;
			D = 1 - v2*(r2 - R*R)/(rdotv*rdotv);
			if (D >= 0) {
				dt = rdotv*(sqrt(D) - 1)/v2;
				assert(smf->dStart > 0 || dt > 0);
				if (dt > smf->dStart && dt <= smf->dEnd && dt < p->dtCol) {
					p->dtCol = dt;
					p->iOrderCol = -w->nWalls - i*2 - 2; /* endpt 2 encoding */
					if (smf->dCollapseLimit)
						_CheckForCollapse(p,dt,rdotv,r2,smf);
					}
				}
			}
		if (!approaching) continue; /* must be approaching at least 1 endpt */
		/* now check wall */
		if (p->iPrevCol == -i - 1) continue;
		lx = w->wall[i].x2 - w->wall[i].x1;
		lz = w->wall[i].z2 - w->wall[i].z1;
		if (lx == 0) { /* vertical wall */
			if (w->wall[i].x1 < p->r[0] && p->v[0] < 0) {
				d = p->r[0] - w->wall[i].x1 - R;
				dt = -d/p->v[0];
				if (smf->dCollapseLimit) rdotv = d*p->v[0];
				}
			else if (w->wall[i].x1 > p->r[0] && p->v[0] > 0) {
				d = w->wall[i].x1 - p->r[0] - R;
				dt = d/p->v[0];
				if (smf->dCollapseLimit) rdotv = -d*p->v[0];
				}
			else continue;
			x0 = 0; /* to satisfy compiler */
			z0 = p->r[2] + p->v[2]*dt;
			if (smf->dCollapseLimit) r2 = d*d;
			}
		else if (lz == 0) { /* horizontal wall */
			if (w->wall[i].z1 < p->r[2] && p->v[2] < 0) {
				d = p->r[2] - w->wall[i].z1 - R;
				dt = -d/p->v[2];
				if (smf->dCollapseLimit) rdotv = d*p->v[2];
				}
			else if (w->wall[i].z1 > p->r[2] && p->v[2] > 0) {
				d = w->wall[i].z1 - p->r[2] - R;
				dt = d/p->v[2];
				if (smf->dCollapseLimit) rdotv = -d*p->v[2];
				}
			else continue;
			x0 = p->r[0] + p->v[0]*dt;
			z0 = 0;
			if (smf->dCollapseLimit) r2 = d*d;
			}
		else { /* oblique wall */
			m = lz/lx;
			b = w->wall[i].z1 - m*w->wall[i].x1;
			dp = (m*p->r[0] - p->r[2] + b)/sqrt(1 + m*m); /* perp. distance */
			if (dp < 0) dp = -dp;
			ldotv = lx*p->v[0] + lz*p->v[2];
			l2 = lx*lx + lz*lz;
			st = sqrt(1 - ldotv*ldotv/(l2*v2)); /* sin theta */
			d = (dp - R)/st; /* travel distance until contact */
			dt = d/sqrt(v2); /* travel time */
			if (dt <= smf->dStart) continue; /* to save time */
			x0 = p->r[0] + p->v[0]*dt;
			z0 = p->r[2] + p->v[2]*dt;
			nx = lz;
			nz = -lx;
			if ((w->wall[i].x1 - x0)*nx + (w->wall[i].z1 - z0)*nz < 0) {
				nx = -nx;
				nz = -nz;
				}
			rdotv = -(nx*p->v[0] + nz*p->v[2]); /* wrong magnitude... */
			if (rdotv >= 0) continue; /* moving away */
			l = R/sqrt(l2);
			x0 += nx*l;
			z0 += nz*l;
			if (smf->dCollapseLimit) {
				rdotv = (dp - R)*rdotv*l/R; /* roundabout... */
				r2 = (dp - R)*(dp - R);
				}
			}
		/* check perpendicular contact point lies on or between endpoints */
		if ((lx < 0 && (x0 > w->wall[i].x1 || x0 < w->wall[i].x2)) ||
			(lx > 0 && (x0 < w->wall[i].x1 || x0 > w->wall[i].x2)) ||
			(lz < 0 && (z0 > w->wall[i].z1 || z0 < w->wall[i].z2)) ||
			(lz > 0 && (z0 < w->wall[i].z1 || z0 > w->wall[i].z2)))
			continue;
		if (smf->dStart == 0 && dt <= 0) {
			(void) fprintf(stderr,"OVERLAP [T=%g]: %i & wall %i dt %g\n",
					smf->dTime,p->iOrder,i,dt);
			assert(0); /* no backsteps allowed */
			}
		if (dt >= smf->dStart && dt <= smf->dEnd && dt <= p->dtCol) {
			assert(dt < p->dtCol); /* no simultaneous collisions allowed */
			p->dtCol = dt;
			p->iOrderCol = -1 - i; /* wall index encoding */
			if (smf->dCollapseLimit) _CheckForCollapse(p,dt,rdotv,r2,smf);
			}
		}
	}
#endif /* !TUMBLER */
#endif /* SAND_PILE */

	}

#endif /* COLLISIONS */
