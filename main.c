#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include "mdl.h"
#include "pst.h"
#include "master.h"
#include "outtype.h"
#include "smoothfcn.h"

void main_ch(MDL mdl)
{
	PST pst;
	LCL lcl;

	lcl.pszDataPath = (char *)getenv("PTOOLS_DATA_PATH");
	lcl.pkd = NULL;
	pstInitialize(&pst,mdl,&lcl);

	pstAddServices(pst,mdl);

	mdlHandler(mdl);

	pstFinish(pst);
	}

/*DEBUG Can we get rid of some of these msrMassChecks()? -- DCR*/

/*DEBUG Should opaque nature of "msr" be enforced in main()? -- DCR*/ 

int main(int argc,char **argv)
{
	MDL mdl;
	MSR msr;
	FILE *fpLog = NULL;
	char achFile[256]; /*DEBUG use MAXPATHLEN here? -- DCR*/
	double dTime,E,T,U,Eth,dWMax,dIMax,dEMax,dMass,dMultiEff;
	long lSec,lStart;
	int i,iStep,iSec,nActive,iStop = 0;

#ifdef TINY_PTHREAD_STACK
	static int first = 1;
	static char **save_argv;

	/*
	 * Hackery to get around SGI's tiny pthread stack.
	 * Main will be called twice.  The second time, argc and argv
	 * will be garbage, so we have to save them from the first.
	 * Another way to do this would involve changing the interface
	 * to mdlInitialize(), so that this hackery could be hidden
	 * down there.
	 */
	if(first) {
	    save_argv = malloc((argc+1)*sizeof(*save_argv));
	    for(i = 0; i < argc; i++)
	        save_argv[i] = strdup(argv[i]);
	    save_argv[argc] = NULL;
	    }
	else {
	    argv = save_argv;
	    }
	first = 0;
#endif /* TINY_PTHREAD_STACK */

	setbuf(stdout,(char *) NULL); /* no stdout buffering */

	lStart=time(0);
	mdlInitialize(&mdl,argv,main_ch);
	for(argc = 0; argv[argc]; argc++); /* some MDLs can trash argv */
	msrInitialize(&msr,mdl,argc,argv);
	/*
	 ** Check if a restart has been requested.
	 ** Or if it might be required.
	 */
	if (msrRestart(msr)) {
		dTime = msrReadCheck(msr,&iStep);
#ifdef COLLISIONS
		msrCalcHill(msr);
#endif /* COLLISIONS */
		msrInitStep(msr);
		dMass = msrMassCheck(msr,-1.0,"Initial");
		if (msr->param.bVStart) printf("Restart Step:%d\n",iStep);
		if (msrLogInterval(msr)) {
			sprintf(achFile,"%s.log",msrOutName(msr));
			fpLog = fopen(achFile,"a");
			assert(fpLog != NULL);
			setbuf(fpLog,(char *) NULL); /* no buffering */
			fprintf(fpLog,"# RESTART (dTime = %g)\n# ",dTime);
			for (i=0;i<argc;++i) fprintf(fpLog,"%s ",argv[i]);
			fprintf(fpLog,"\n");
			msrLogParams(msr,fpLog);
#ifdef COLLISIONS
			if (msr->param.bDoCollLog) {
				FILE *fp = fopen(msr->param.achCollLog,"a");
				assert(fp);
				fprintf(fp,"RESTART:T=%e\n",dTime);
				fclose(fp);
				}
#endif /* COLLISIONS */
			}
		if(msrKDK(msr) || msr->param.bEpsVel) {
			msrBuildTree(msr,0,dMass,0);
			msrMassCheck(msr,dMass,"After msrBuildTree");
			msrInitAccel(msr);
			if (msrDoGravity(msr)) {
				msrGravity(msr,iStep,msrDoSun(msr),&iSec,&dWMax,&dIMax,&dEMax,&nActive);
				}
			}
		goto Restart;
		}
	/*
	 ** Establish safety lock.
	 */
	if (!msrGetLock(msr)) {
	    msrFinish(msr);
	    mdlFinish(mdl);
	    return 1;
	    }
	/*
	 ** Read in the binary file, this may set the number of timesteps or
	 ** the size of the timestep when the zto parameter is used.
	 */
#ifdef COLLISIONS /* must use "Solar System" (SS) I/O format... */
	dTime = msrReadSS(msr);
	assert(msr->N >= msr->param.nSmooth);
	msrCalcHill(msr);
#else /* COLLISIONS */
	dTime = msrReadTipsy(msr);
#endif /* !COLLISIONS */
	msrInitStep(msr);
#ifdef GLASS
	msrInitGlass(msr);
#endif
	dMass = msrMassCheck(msr,-1.0,"Initial");
	if (prmSpecified(msr->prm,"dSoft")) msrSetSoft(msr,msrSoft(msr));
	msrMassCheck(msr,dMass,"After msrSetSoft");
#ifdef COLLISIONS
	if (msr->param.bFindRejects) msrFindRejects(msr);
#endif /* COLLISIONS */
	/*
	 ** If the simulation is periodic make sure to wrap all particles into
	 ** the "unit" cell. Doing a drift of 0.0 will always take care of this.
	 */
#ifdef COLLISIONS
	/* msrDrift() also finds initial particle overlaps! */
#endif /* COLLISIONS */
	msrDrift(msr,dTime,0.0);
	msrMassCheck(msr,dMass,"After initial msrDrift");

	if (msrSteps(msr) > 0) {
		/*
		 ** Now we have all the parameters for the simulation we can make a 
		 ** log file entry.
		 */
		if (msrComove(msr)) {
			msrSwitchTheta(msr,dTime);
			}
		if (msrLogInterval(msr)) {
			sprintf(achFile,"%s.log",msrOutName(msr));
			fpLog = fopen(achFile,"w");
			assert(fpLog != NULL);
			setbuf(fpLog,(char *) NULL); /* no buffering */
			/*
			 ** Include a comment at the start of the log file showing the
			 ** command line options.
			 */
			fprintf(fpLog,"# ");
			for (i=0;i<argc;++i) fprintf(fpLog,"%s ",argv[i]);
			fprintf(fpLog,"\n");
			msrLogParams(msr,fpLog);
			}
		/*
		 ** Build tree, activating all particles first (just in case).
		 */
		msrInitAccel(msr);
#ifdef GASOLINE
		msrInitSph(msr,dTime);
#endif
		msrActiveRung(msr,0,1);
		msrBuildTree(msr,0,dMass,0);
		msrMassCheck(msr,dMass,"After msrBuildTree");
		if (msrDoGravity(msr)) {
			msrGravity(msr,0.0,msrDoSun(msr),&iSec,&dWMax,&dIMax,&dEMax,&nActive);
			msrMassCheck(msr,dMass,"After msrGravity");
			msrCalcE(msr,MSR_INIT_ECOSMO,dTime,&E,&T,&U,&Eth);
			msrMassCheck(msr,dMass,"After msrCalcE");
			dMultiEff = 1.0;
			if (msrLogInterval(msr)) {
				(void) fprintf(fpLog,"%e %e %e %e %e %e %i %e %e %e %e\n",dTime,
					       1.0/csmTime2Exp(msr->param.csm,dTime)-1.0,
					       E,T,U,Eth,iSec,dWMax,dIMax,
						dEMax,dMultiEff);
				}
			}
		
		fprintf(stderr,"WallRunTime: %d\n",msr->param.iWallRunTime);

		for (iStep=1;iStep<=msrSteps(msr);++iStep) {
			if (msrComove(msr)) {
				msrSwitchTheta(msr,dTime);
				}
			if (msrKDK(msr)) {
				dMultiEff = 0.0;
				lSec = time(0);
#ifdef COLLISIONS
				if (msr->param.bFandG)
					msrPlanetsKDK(msr,iStep - 1,dTime,msrDelta(msr),
								  &dWMax,&dIMax,&dEMax,&iSec);
				else
#endif /* COLLISIONS */
				msrTopStepKDK(msr, iStep-1, dTime,
					      msrDelta(msr), 0, 0, 1,
					      &dMultiEff, &dWMax,
					      &dIMax, &dEMax, &iSec);
				msrRungStats(msr);
				msrCoolVelocity(msr,dTime,dMass);	/* Supercooling if specified */
				msrMassCheck(msr,dMass,"After CoolVelocity in KDK");
				msrGrowMass(msr,dTime,msrDelta(msr)); /* Grow Masses if specified */
				dTime += msrDelta(msr);
				/*
				 ** Output a log file line at each step.
				 ** Note: no extra gravity calculation required.
				 */
				msrCalcE(msr,MSR_STEP_ECOSMO,dTime,&E,&T,&U,&Eth);
				msrMassCheck(msr,dMass,"After msrCalcE in KDK");
				lSec = time(0) - lSec;
				if (msrLogInterval(msr) && iStep%msrLogInterval(msr) == 0) {
					(void) fprintf(fpLog,"%e %e %e %e %e %e %li %e %e %e %e\n",
						       dTime,1.0/csmTime2Exp(msr->param.csm,dTime)-1.0,E,T,U,
								   Eth,lSec,dWMax,dIMax,dEMax,dMultiEff);
					}
				}
			else {
				lSec = time(0);
				msrTopStepDKD(msr, iStep-1, dTime,
					      msrDelta(msr), &dMultiEff);
				msrRungStats(msr);
				msrCoolVelocity(msr,dTime,dMass);	/* Supercooling if specified */
				msrMassCheck(msr,dMass,"After CoolVelocity in DKD");
				msrGrowMass(msr,dTime,msrDelta(msr)); /* Grow Masses if specified */
				dTime += msrDelta(msr);
			        if (msrLogInterval(msr) && iStep%msrLogInterval(msr) == 0) {
				        if (msrDoGravity(msr)) {
					        /*
					        ** Output a log file line.
					        ** Reactivate all particles.
					        */
					        msrActiveRung(msr,0,1);
						msrBuildTree(msr,0,dMass,0);
						msrMassCheck(msr,dMass,"After msrBuildTree in DKD-log");
						msrInitAccel(msr);
						msrGravity(msr,iStep,msrDoSun(msr),&iSec,&dWMax,&dIMax,&dEMax,&nActive);
						msrMassCheck(msr,dMass,"After msrGravity in DKD-log");
					        }
					
                                        msrCalcE(msr,MSR_STEP_ECOSMO,dTime,&E,&T,&U,&Eth);
					msrMassCheck(msr,dMass,"After msrCalcE in DKD-log");
					(void) fprintf(fpLog,"%e %e %e %e %e %e %li %e %e %e %e\n",dTime,
						       1.0/csmTime2Exp(msr->param.csm,dTime)-1.0,E,T,U,Eth,
							   time(0)-lSec,dWMax,dIMax,dEMax,dMultiEff);
					}
				lSec = time(0) - lSec;
				}
			/*
			 ** Check for user interrupt.
			 */
			iStop = msrCheckForStop(msr);
			/*DEBUG a lot of the following seems awfully redundant... -- DCR*/
			if (msrOutTime(msr,dTime)) {
				msrReorder(msr);
				msrMassCheck(msr,dMass,"After msrReorder in OutTime");
				sprintf(achFile,"%s.%05d",msrOutName(msr),iStep);
#ifdef COLLISIONS
				msrWriteSS(msr,achFile,dTime);
#else /* COLLISIONS */
				msrWriteTipsy(msr,achFile,dTime);
				msrMassCheck(msr,dMass,"After msrWriteTipsy in OutTime");
#endif /* !COLLISIONS */
				if (msrDoDensity(msr)) {
					msrActiveRung(msr,0,1);
					msrBuildTree(msr,0,dMass,1);
					msrMassCheck(msr,dMass,"After msrBuildTree in OutTime");
					msrSmooth(msr,dTime,SMX_DENSITY,1);
					msrMassCheck(msr,dMass,"After msrSmooth in OutTime");
					sprintf(achFile,"%s.%05d.den",msrOutName(msr),iStep);
					msrOutArray(msr,achFile,OUT_DENSITY_ARRAY);
					msrMassCheck(msr,dMass,"After msrOutArray in OutTime");
					}
				/*
				 ** Don't allow duplicate outputs.
				 */
				while (msrOutTime(msr,dTime));
				}
			else if (iStep == msrSteps(msr) || iStop) {
				/*
				 ** Final output always produced.
				 */
				msrReorder(msr);
				msrMassCheck(msr,dMass,"After msrReorder in OutFinal");
				sprintf(achFile,"%s.%05d",msrOutName(msr),iStep);
#ifdef COLLISIONS
				msrWriteSS(msr,achFile,dTime);
#else /* COLLISIONS */
				msrWriteTipsy(msr,achFile,dTime);
				msrMassCheck(msr,dMass,"After msrWriteTipsy in OutFinal");
#endif /* !COLLISIONS */
				if (msrDoDensity(msr)) {
					msrActiveRung(msr,0,1);
					msrBuildTree(msr,0,dMass,1);
					msrMassCheck(msr,dMass,"After msrBuildTree in OutFinal");
					msrSmooth(msr,dTime,SMX_DENSITY,1);
					msrMassCheck(msr,dMass,"After msrSmooth in OutFinal");
					sprintf(achFile,"%s.%05d.den",msrOutName(msr),iStep);
					msrOutArray(msr,achFile,OUT_DENSITY_ARRAY);
					msrMassCheck(msr,dMass,"After msrOutArray in OutFinal");
					}
				msrWriteCheck(msr,dTime,iStep);
				msrMassCheck(msr,dMass,"After msrWriteCheck");
				}
			else if (msrOutInterval(msr) > 0) {
				if (iStep%msrOutInterval(msr) == 0) {
					msrReorder(msr);
					msrMassCheck(msr,dMass,"After msrReorder in OutInt");
					sprintf(achFile,"%s.%05d",msrOutName(msr),iStep);
#ifdef COLLISIONS
					msrWriteSS(msr,achFile,dTime);
#else /* COLLISIONS */
					msrWriteTipsy(msr,achFile,dTime);
					msrMassCheck(msr,dMass,"After msrWriteTipsy in OutInt");
#endif /* !COLLISIONS */
					if (msrDoDensity(msr)) {
						msrActiveRung(msr,0,1);
						msrBuildTree(msr,0,dMass,1);
						msrMassCheck(msr,dMass,"After msrBuildTree in OutInt");
						msrSmooth(msr,dTime,SMX_DENSITY,1);
						msrMassCheck(msr,dMass,"After msrSmooth in OutInt");
						sprintf(achFile,"%s.%05d.den",msrOutName(msr),iStep);
						msrOutArray(msr,achFile,OUT_DENSITY_ARRAY);
						msrMassCheck(msr,dMass,"After msrOutArray in OutInt");
						}
					}
				}
			if (msr->param.iWallRunTime > 0) {
			    if (msr->param.iWallRunTime*60 - (time(0)-lStart) < ((int) (lSec*1.5)) ) {
					printf("RunTime limit exceeded.  Writing checkpoint and exiting.\n");
					printf("    iWallRunTime(sec): %d   Time running: %ld   Last step: %ld\n",
						   msr->param.iWallRunTime*60,time(0)-lStart,lSec);
					iStop = 1;
					}
				}
			/*DEBUG isn't this how the outputs above should be done? -- DCR*/
			if (iStop || iStep == msrSteps(msr) ||
				(msrCheckInterval(msr) && iStep%msrCheckInterval(msr) == 0)) {
				/*
				 ** Write a checkpoint.
				 */
				msrWriteCheck(msr,dTime,iStep);
				msrMassCheck(msr,dMass,"After msrWriteCheck");
			Restart:
				;
				}
			if (iStop) break;
			}
		if (msrLogInterval(msr)) (void) fclose(fpLog);
		}
	else {
		/*
		 ** Build tree, activating all particles first (just in case).
		 */
		if (msrDoDensity(msr)) {
			msrActiveRung(msr,0,1);
			msrBuildTree(msr,0,dMass,1);
			msrMassCheck(msr,dMass,"After msrBuildTree in OutSingle Density");
			msrSmooth(msr,dTime,SMX_DENSITY,1);
			msrMassCheck(msr,dMass,"After msrSmooth in OutSingle Density");
			msrReorder(msr);
			msrMassCheck(msr,dMass,"After msrReorder in OutSingle Density");
			sprintf(achFile,"%s.den",msrOutName(msr));
			msrOutArray(msr,achFile,OUT_DENSITY_ARRAY);
			msrMassCheck(msr,dMass,"After msrOutArray in OutSingle Density");
#if 0/*DEBUG*/
			{
			    struct inSmooth smooth;
				int sec,dsec;

				sec = time(0);
				(void) printf("Building QQ tree...\n");
			    msrBuildQQTree(msr, 0, dMass);
				dsec = time(0) - sec;
				(void) printf("QQ tree built, time = %i sec\n",dsec);
			    smooth.nSmooth = 32;
			    smooth.bPeriodic = 0;
			    smooth.bSymmetric = 0;
			    smooth.iSmoothType = SMX_ENCOUNTER;
			    smooth.smf.pkd = NULL;
				smooth.smf.dTime = dTime;
				smooth.smf.dDelta = msrDelta(msr);
				smooth.smf.dCentMass = msr->param.dCentMass;
				sec = time(0);
				(void) printf("Beginning encounter search...\n");
			    pstQQSmooth(msr->pst,&smooth,sizeof(smooth),NULL,NULL);
				dsec = time(0) - sec;
				(void) printf("Encounter search completed, time = %i sec\n",dsec);
			    msrReorder(msr);
			    sprintf(achFile,"%s.enc",msrOutName(msr));
			    msrOutArray(msr,achFile,OUT_DENSITY_ARRAY);
			    }
#endif
			}
		else if (msrDoGravity(msr)) {
			msrActiveRung(msr,0,1);
			msrBuildTree(msr,0,dMass,0);
			msrMassCheck(msr,dMass,"After msrBuildTree in OutSingle Gravity");
			msrInitAccel(msr);
			msrGravity(msr,0.0,msrDoSun(msr),&iSec,&dWMax,&dIMax,&dEMax,&nActive);
			msrMassCheck(msr,dMass,"After msrGravity in OutSingle Gravity");
			msrReorder(msr);
			msrMassCheck(msr,dMass,"After msrReorder in OutSingle Gravity");
			sprintf(achFile,"%s.acc",msrOutName(msr));
			msrOutVector(msr,achFile,OUT_ACCEL_VECTOR);
			msrMassCheck(msr,dMass,"After msrOutVector in OutSingle Gravity");
			sprintf(achFile,"%s.pot",msrOutName(msr));
			msrOutArray(msr,achFile,OUT_POT_ARRAY);
			msrMassCheck(msr,dMass,"After msrOutArray in OutSingle Gravity");
			msrInitDt(msr);
#ifdef HILL_STEP
			msrHillStep(msr);
#else
			msrAccelStep(msr, dTime);
#endif
			msrDtToRung(msr, 0, msrDelta(msr), 1);
			sprintf(achFile,"%s.dt",msrOutName(msr));
			msrOutArray(msr,achFile,OUT_DT_ARRAY);
			}
		}
	msrFinish(msr);
	mdlFinish(mdl);
	return 0;
	}
