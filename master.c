#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <malloc.h>
#include <assert.h>
#include <time.h>
#include <math.h>
#include "master.h"
#include "tipsydefs.h"
#include "opentype.h"
#include "checkdefs.h"


void _msrLeader(void)
{
    puts("USAGE: main [SETTINGS | FLAGS] [SIM_FILE]");
    puts("SIM_FILE: Configuration file of a particular simulation, which");
    puts("          includes desired settings and relevant input and");
    puts("          output files. Settings specified in this file override");
    puts("          the default settings.");
    puts("SETTINGS");
    puts("or FLAGS: Command line settings or flags for a simulation which");
    puts("          will override any defaults and any settings or flags");
    puts("          specified in the SIM_FILE.");
	}


void _msrTrailer(void)
{
	puts("(see man page for more information)");
	}


void msrInitialize(MSR *pmsr,MDL mdl,int argc,char **argv,char *pszDefaultName)
{
	MSR msr;
	int j,ret;
	int id,iDum;
	char in[SIZE(inSetAdd)];
	char inLvl[SIZE(inLevelize)];
	char *outShow;
	
	msr = (MSR)malloc(sizeof(struct msrContext));
	assert(msr != NULL);
	msr->mdl = mdl;
	*pmsr = msr;
	/*
	 ** Now setup for the input parameters.
	 */
	prmInitialize(&msr->prm,_msrLeader,_msrTrailer);
	msr->param.nThreads = 1;
	prmAddParam(msr->prm,"nThreads",1,&msr->param.nThreads,"sz",
				"<nThreads>");
	msr->param.bDiag = 0;
	prmAddParam(msr->prm,"bDiag",0,&msr->param.bDiag,"d",
				"enable/disable per thread diagnostic output");
	msr->param.bVerbose = 0;
	prmAddParam(msr->prm,"bVerbose",0,&msr->param.bVerbose,"v",
				"enable/disable verbose output");
	msr->param.bPeriodic = 0;
	prmAddParam(msr->prm,"bPeriodic",0,&msr->param.bPeriodic,"p",
				"periodic/non-periodic = -p");
	msr->param.bRestart = 0;
	prmAddParam(msr->prm,"bRestart",0,&msr->param.bRestart,"restart",
				"restart from checkpoint");
	msr->param.nBucket = 8;
	prmAddParam(msr->prm,"nBucket",1,&msr->param.nBucket,"b",
				"<max number of particles in a bucket> = 8");
	msr->param.nSteps = 0;
	prmAddParam(msr->prm,"nSteps",1,&msr->param.nSteps,"n",
				"<number of timesteps> = 0");
	msr->param.iOutInterval = 20;
	prmAddParam(msr->prm,"iOutInterval",1,&msr->param.iOutInterval,
				"oi","<number of timesteps between snapshots> = 20");
	msr->param.iLogInterval = 10;
	prmAddParam(msr->prm,"iLogInterval",1,&msr->param.iLogInterval,
				"ol","<number of timesteps between logfile outputs> = 10");
	msr->param.iCheckInterval = 10;
	prmAddParam(msr->prm,"iCheckInterval",1,&msr->param.iCheckInterval,
				"oc","<number of timesteps between checkpoints> = 10");
	msr->param.iOrder = 2;
	prmAddParam(msr->prm,"iOrder",1,&msr->param.iOrder,"or",
				"<multipole expansion order: 1, 2, or 3> = 2");
	msr->param.nReplicas = 0;
	prmAddParam(msr->prm,"nReplicas",1,&msr->param.nReplicas,"nrep",
				"<nReplicas> = 0 for -p, or 1 for +p");
	msr->param.dSoft = 0.0;
	prmAddParam(msr->prm,"dSoft",2,&msr->param.dSoft,"e",
				"<gravitational softening length> = 0.0");
	msr->param.dDelta = 0.0;
	prmAddParam(msr->prm,"dDelta",2,&msr->param.dDelta,"dt",
				"<time step>");
	msr->param.dEwCut = 1.8;
	prmAddParam(msr->prm,"dEwCut",2,&msr->param.dEwCut,"ew",
				"<dEwCut> = 1.8");
	msr->param.dEwhCut = 2.0;
	prmAddParam(msr->prm,"dEwhCut",2,&msr->param.dEwhCut,"ewh",
				"<dEwhCut> = 2.0");
	msr->param.dTheta = 0.8;
	prmAddParam(msr->prm,"dTheta",2,&msr->param.dTheta,"theta",
				"<Barnes opening criterion> = 0.8");
	msr->param.dAbsPartial = 0.0;
	prmAddParam(msr->prm,"dAbsPartial",2,&msr->param.dAbsPartial,"ap",
				"<absolute partial error opening criterion>");
	msr->param.dRelPartial = 0.0;
	prmAddParam(msr->prm,"dRelPartial",2,&msr->param.dRelPartial,"rp",
				"<relative partial error opening criterion>");
	msr->param.dAbsTotal = 0.0;
	prmAddParam(msr->prm,"dAbsTotal",2,&msr->param.dAbsTotal,"at",
				"<absolute total error opening criterion>");
	msr->param.dRelTotal = 0.0;
	prmAddParam(msr->prm,"dRelTotal",2,&msr->param.dRelTotal,"rt",
				"<relative total error opening criterion>");
	msr->param.dPeriod = 1.0;
	prmAddParam(msr->prm,"dPeriod",2,&msr->param.dPeriod,"L",
				"<periodic box length> = 1.0");
	msr->param.achInFile[0] = 0;
	prmAddParam(msr->prm,"achInFile",3,msr->param.achInFile,"I",
				"<input file name> (file in TIPSY binary format)");
	strcpy(msr->param.achOutName,pszDefaultName); 
	prmAddParam(msr->prm,"achOutName",3,&msr->param.achOutName,"o",
				"<output name for snapshots and logfile> = \"pkdgrav\"");
	msr->param.bComove = 0;
	prmAddParam(msr->prm,"bComove",0,&msr->param.bComove,"cm",
				"enable/disable comoving coordinates = -cm");
	msr->param.dHubble0 = 0.0;
	prmAddParam(msr->prm,"dHubble0",2,&msr->param.dHubble0,"Hub",
				"<dHubble0> = 0.0");
	msr->param.dOmega0 = 1.0;
	prmAddParam(msr->prm,"dOmega0",2,&msr->param.dOmega0,"Om",
				"<dOmega0> = 1.0");
	strcpy(msr->param.achDataSubPath,".");
	prmAddParam(msr->prm,"achDataSubPath",3,&msr->param.achDataSubPath,
				NULL,NULL);
	msr->param.dExtraStore = 1.0;
	prmAddParam(msr->prm,"dExtraStore",2,&msr->param.dExtraStore,NULL,NULL);
	msr->param.nSmooth = 64;
	prmAddParam(msr->prm,"nSmooth",1,&msr->param.nSmooth,"s",
				"<number of particles to smooth over> = 64");
	msr->param.bGatherScatter = 1;
	prmAddParam(msr->prm,"bGatherScatter",0,&msr->param.bGatherScatter,"gs",
				"gather-scatter/gather-only smoothing kernel = +gs");
	/*
	 ** Set the box center to (0,0,0) for now!
	 */
	for (j=0;j<3;++j) msr->fCenter[j] = 0.0;
	/*
	 ** Define any "LOCAL" parameters (LCL)
	 */
	msr->lcl.pszDataPath = getenv("PTOOLS_DATA_PATH");
	/*
	 ** Process command line arguments.
	 */
	ret = prmArgProc(msr->prm,argc,argv);
	if (!ret) {
		msrFinish(msr);
		mdlFinish(mdl);
		exit(1);
		}
	if (msr->param.bPeriodic && !prmSpecified(msr->prm,"nReplicas")) {
		msr->param.nReplicas = 1;
		}
	if (!msr->param.achInFile[0] && !msr->param.bRestart) {
		printf("pkdgrav ERROR: no input file specified\n");
		msrFinish(msr);
		mdlFinish(mdl);
		exit(1);
		}
	/*
	 ** Should we have restarted, maybe?
	 */
	if (!msr->param.bRestart) {
		}
	msr->nThreads = mdlThreads(mdl);
	/*
	 ** Determine opening type.
	 */
	msr->iOpenType = OPEN_JOSH;
	if (prmFileSpecified(msr->prm,"dAbsPartial")) msr->iOpenType = OPEN_ABSPAR;
	if (prmFileSpecified(msr->prm,"dRelPartial")) msr->iOpenType = OPEN_RELPAR;
	if (prmFileSpecified(msr->prm,"dAbsTotal")) msr->iOpenType = OPEN_ABSTOT;
	if (prmFileSpecified(msr->prm,"dRelTotal")) msr->iOpenType = OPEN_RELTOT;
	if (prmArgSpecified(msr->prm,"dTheta")) msr->iOpenType = OPEN_JOSH;
	if (prmArgSpecified(msr->prm,"dAbsPartial")) msr->iOpenType = OPEN_ABSPAR;
	if (prmArgSpecified(msr->prm,"dRelPartial")) msr->iOpenType = OPEN_RELPAR;
	if (prmArgSpecified(msr->prm,"dAbsTotal")) msr->iOpenType = OPEN_ABSTOT;
	if (prmArgSpecified(msr->prm,"dRelTotal")) msr->iOpenType = OPEN_RELTOT;
	switch (msr->iOpenType) {
	case OPEN_JOSH:
		msr->dCrit = msr->param.dTheta;
		break;
	case OPEN_ABSPAR:
		msr->dCrit = msr->param.dAbsPartial;
		break;
	case OPEN_RELPAR:
		msr->dCrit = msr->param.dRelPartial;
		break;
	case OPEN_ABSTOT:
		msr->dCrit = msr->param.dAbsTotal;
		break;
	case OPEN_RELTOT:
		msr->dCrit = msr->param.dRelTotal;
		break;
	default:
		msr->dCrit = msr->param.dTheta;
		}
	/*
	 ** Initialize comove variables.
	 */
	msr->dRedshift = 0.0;
	msr->dCosmoFac = 1.0;
	msr->dHubble = 0.0;
	msr->nRed = 0;
        if(msrComove(msr)) msrReadRed(msr);
        
	/*
	 ** Create the processor subset tree.
	 */
	pstInitialize(&msr->pst,msr->mdl,&msr->lcl);
	for (id=1;id<msr->nThreads;++id) {
		if (msr->param.bVerbose) printf("Adding %d to the pst\n",id);
		DATA(in,inSetAdd)->id = id;
		pstSetAdd(msr->pst,in,SIZE(inSetAdd),NULL,NULL);
		}
	if (msr->param.bVerbose) printf("\n");
	/*
	 ** Levelize the PST.
	 */
	DATA(inLvl,inLevelize)->iLvl = 0;
	pstLevelize(msr->pst,inLvl,SIZE(inLevelize),NULL,NULL);
	if (msr->param.bVerbose) {
		printf("PROCESSOR SET TREE - PST\n");
		outShow = (char *)malloc(MDL_MAX_SERVICE_BYTES);
		assert(outShow != NULL);
		pstShowPst(msr->pst,NULL,0,outShow,&iDum);
		printf("%s\n",outShow);
		}
	}

void msrLogParams(MSR msr, FILE *fp)
{
        int i;
  
	fprintf(fp, "# nThreads: %d", msr->param.nThreads);
	fprintf(fp, " bDiag: %d", msr->param.bDiag);
	fprintf(fp, " bVerbose: %d", msr->param.bVerbose);
	fprintf(fp, " bPeriodic: %d", msr->param.bPeriodic);
	fprintf(fp, " bRestart: %d", msr->param.bRestart);
	fprintf(fp, " nBucket: %d", msr->param.nBucket);
	fprintf(fp, "\n# nSteps: %d", msr->param.nSteps);
	fprintf(fp, " iOutInterval: %d", msr->param.iOutInterval);
	fprintf(fp, " iLogInterval: %d", msr->param.iLogInterval);
	fprintf(fp, " iCheckInterval: %d", msr->param.iCheckInterval);
	fprintf(fp, " iOrder: %d", msr->param.iOrder);
	fprintf(fp, "\n# nReplicas: %d", msr->param.nReplicas);
        if(prmArgSpecified(msr->prm, "dSoft"))
          fprintf(fp, " dSoft: %g", msr->param.dSoft);
        else
          fprintf(fp, " dSoft: input");
	fprintf(fp, " dDelta: %g", msr->param.dDelta);
	fprintf(fp, " dEwCut: %f", msr->param.dEwCut);
	fprintf(fp, " dEwhCut: %f", msr->param.dEwhCut);
        fprintf(fp,"\n#");
	switch (msr->iOpenType) {
	case OPEN_JOSH:
		fprintf(fp, " iOpenType: JOSH");
		break;
	case OPEN_ABSPAR:
		fprintf(fp, " iOpenType: ABSPAR");
		break;
	case OPEN_RELPAR:
		fprintf(fp, " iOpenType: RELPAR");
		break;
	case OPEN_ABSTOT:
		fprintf(fp, " iOpenType: ABSTOT");
		break;
	case OPEN_RELTOT:
		fprintf(fp, " iOpenType: RELTOT");
		break;
	default:
		fprintf(fp, " iOpenType: NONE?");
		}
	fprintf(fp, " dTheta: %f", msr->param.dTheta);
	fprintf(fp, " dAbsPartial: %g", msr->param.dAbsPartial);
	fprintf(fp, " dRealPartial: %g", msr->param.dRelPartial);
	fprintf(fp, " dAbsTotal: %g", msr->param.dAbsTotal);
	fprintf(fp, " dRelTotal: %g", msr->param.dRelTotal);
	fprintf(fp, "\n# dPeriod: %g", msr->param.dPeriod);
	fprintf(fp, " achInFile: %s", msr->param.achInFile);
	fprintf(fp, " achOutName: %s", msr->param.achOutName); 
	fprintf(fp, " bComove: %d", msr->param.bComove);
	fprintf(fp, " dHubble0: %g", msr->param.dHubble0);
	fprintf(fp, " dOmega0: %g", msr->param.dOmega0);
	fprintf(fp, "\n# achDataSubPath: %s", msr->param.achDataSubPath);
	fprintf(fp, " dExtraStore: %f", msr->param.dExtraStore);
	fprintf(fp, " nSmooth: %d", msr->param.nSmooth);
	fprintf(fp, " bGatherScatter: %d\n", msr->param.bGatherScatter);
        fprintf(fp, "# RedOut:");
        for(i = 0; i < msr->nRed; i++)
            fprintf(fp, " %f", msrRedOut(msr, i));
        fprintf(fp, "\n");
}

void msrFinish(MSR msr)
{
	int id;

	for (id=1;id<msr->nThreads;++id) {
		if (msr->param.bVerbose) printf("Stopping thread %d\n",id);		
		mdlReqService(msr->mdl,id,SRV_STOP,NULL,0);
		mdlGetReply(msr->mdl,id,NULL,NULL);
		}
	pstFinish(msr->pst);
	/*
	 ** finish with parameter stuff, dealocate and exit.
	 */
	prmFinish(msr->prm);
	free(msr);
	}


double Expand2Time(double dExpansion,double dHubble0)
{
	return(2/(3*dHubble0)*pow(dExpansion,1.5));
	}


double Time2Expand(double dTime,double dHubble0)
{
	return(pow(3*dHubble0*dTime/2,2.0/3.0));
	}


double msrReadTipsy(MSR msr)
{
	FILE *fp;
	struct dump h;
	char in[SIZE(inReadTipsy)];
	char achInFile[PST_FILENAME_SIZE];
	int j;
	LCL *plcl = msr->pst->plcl;
	double dTime;
	
	if (msr->param.achInFile[0]) {
		/*
		 ** Add Data Subpath for local and non-local names.
		 */
		achInFile[0] = 0;
		strcat(achInFile,msr->param.achDataSubPath);
		strcat(achInFile,"/");
		strcat(achInFile,msr->param.achInFile);
		strcpy(DATA(in,inReadTipsy)->achInFile,achInFile);
		/*
		 ** Add local Data Path.
		 */
		achInFile[0] = 0;
		if (plcl->pszDataPath) {
			strcat(achInFile,plcl->pszDataPath);
			strcat(achInFile,"/");
			}
		strcat(achInFile,DATA(in,inReadTipsy)->achInFile);

		fp = fopen(achInFile,"r");
		if (!fp) {
			printf("Could not open InFile:%s\n",achInFile);
			msrFinish(msr);
			mdlFinish(msr->mdl);
			exit(1);
			}
		}
	else {
		printf("No input file specified\n");
		msrFinish(msr);
		mdlFinish(msr->mdl);
		exit(1);
		}
	/*
	 ** Assume tipsy format for now, and dark matter only.
	 */
	fread(&h,sizeof(struct dump),1,fp);
	fclose(fp);
	msr->N = h.nbodies;
	assert(msr->N == h.ndark);
	if (msr->param.bComove) {
                if(msr->param.dHubble0 == 0.0) {
                    printf("No hubble constant specified\n");
                    msrFinish(msr);
                    mdlFinish(msr->mdl);
                    exit(1);
                    }
		dTime = Expand2Time(h.time,msr->param.dHubble0);
		msr->dRedshift = 1/h.time - 1;
		}
	else {
		dTime = h.time;
		msr->dRedshift = 0.0;
		}
	if (msr->param.bVerbose) {
		if (msr->param.bComove) {
			printf("Reading file...\nN:%d Time:%g Redshift:%g\n",msr->N,
				   dTime,msr->dRedshift);
			}
		else {
			printf("Reading file...\nN:%d Time:%g\n",msr->N,dTime);
			}
		}
	DATA(in,inReadTipsy)->nStart = 0;
	DATA(in,inReadTipsy)->nEnd = msr->N - 1;
	DATA(in,inReadTipsy)->iOrder = msr->param.iOrder;
	/*
	 ** Since pstReadTipsy causes the allocation of the local particle
	 ** store, we need to tell it the percentage of extra storage it
	 ** should allocate for load balancing differences in the number of
	 ** particles.
	 */
	DATA(in,inReadTipsy)->fExtraStore = msr->param.dExtraStore;
	/*
	 ** Provide the period.
	 */
	for (j=0;j<3;++j) {
		DATA(in,inReadTipsy)->fPeriod[j] = msr->param.dPeriod;
		}
	/*
	 ** Do a parallel Read of the input file, may be benificial for
	 ** some systems, or if there are multiple identical physical files. 
	 ** Start child threads reading!
	 */
	pstReadTipsy(msr->pst,in,SIZE(inReadTipsy),NULL,NULL);
	if (msr->param.bVerbose) puts("Input file has been successfully read.");
	return(dTime);
	}


void msrWriteTipsy(MSR msr,char *pszFileName,double dTime)
{
	FILE *fp;
	struct dump h;
	char in[SIZE(inWriteTipsy)];
	char achOutFile[PST_FILENAME_SIZE];
	LCL *plcl = msr->pst->plcl;
	
	/*
	 ** Add Data Subpath for local and non-local names.
	 */
	achOutFile[0] = 0;
	strcat(achOutFile,msr->param.achDataSubPath);
	strcat(achOutFile,"/");
	strcat(achOutFile,pszFileName);
	strcpy(DATA(in,inWriteTipsy)->achOutFile,achOutFile);
	/*
	 ** Add local Data Path.
	 */
	achOutFile[0] = 0;
	if (plcl->pszDataPath) {
		strcat(achOutFile,plcl->pszDataPath);
		strcat(achOutFile,"/");
		}
	strcat(achOutFile,DATA(in,inWriteTipsy)->achOutFile);
	
	fp = fopen(achOutFile,"w");
	if (!fp) {
		printf("Could not open OutFile:%s\n",achOutFile);
		msrFinish(msr);
		mdlFinish(msr->mdl);
		exit(1);
		}
	/*
	 ** Assume tipsy format for now, and dark matter only.
	 */
	h.nbodies = msr->N;
	h.ndark = msr->N;
	h.nsph = 0;
	h.nstar = 0;
	if (msr->param.bComove)
	  h.time = Time2Expand(dTime,msr->param.dHubble0);
	else
	  h.time = dTime;
	h.ndim = 3;
	if (msr->param.bVerbose) {
		if (msr->param.bComove) {
			printf("Writing file...\nTime:%g Redshift:%g\n",msr->N,
				   dTime,(1/h.time - 1));
			}
		else {
			printf("Writing file...\nTime:%g\n",msr->N,dTime);
			}
		}
	fwrite(&h,sizeof(struct dump),1,fp);
	fclose(fp);
	/*
	 ** Do a parallel write to the output file.
	 */
	pstWriteTipsy(msr->pst,in,SIZE(inWriteTipsy),NULL,NULL);
	if (msr->param.bVerbose) {
		puts("Output file has been successfully written.");
		}
	}


void msrSetSoft(MSR msr)
{
  char in[SIZE(inSetSoft)];
  
  if (msr->param.bVerbose) printf("Set Softening...\n");
  DATA(in, inSetSoft)->dSoft = msr->param.dSoft;
  pstSetSoft(msr->pst, in, SIZE(inSetSoft), NULL, NULL);
}

void msrBuildTree(MSR msr)
{
	char in[SIZE(inBuildTree)];
	int sec,dsec;
	int i;
	KDN *pkdn;

	if (msr->param.bVerbose) printf("Domain Decomposition...\n");
	sec = time(0);
	pstDomainDecomp(msr->pst,NULL,0,NULL,NULL);
	dsec = time(0) - sec;
	if (msr->param.bVerbose) {
		printf("Domain Decomposition complete, Wallclock: %d secs\n\n",dsec);
		}
	if (msr->param.bVerbose) printf("Building local trees...\n");
	DATA(in,inBuildTree)->iCell	= 1;	/* ROOT */
	DATA(in,inBuildTree)->nBucket = msr->param.nBucket;
	DATA(in,inBuildTree)->iOpenType = msr->iOpenType;
	DATA(in,inBuildTree)->dCrit = msr->dCrit;
	sec = time(0);
	pstBuildTree(msr->pst,in,SIZE(inBuildTree),NULL,NULL);
	dsec = time(0) - sec;
	if (msr->param.bVerbose) {
		printf("Local Trees built, Wallclock: %d secs\n\n",dsec);
		}
	if (msr->param.bVerbose) {
		pkdn = msr->lcl.kdTop;
		for (i=1;i<msr->lcl.nTopNodes;++i) {
			printf("LTT:%2d fSplit:(%1d)%f l:%d u:%d\n",i,pkdn[i].iDim,
				   pkdn[i].fSplit,pkdn[i].pLower,pkdn[i].pUpper);
			printf("       MIN:%15f %15f %15f\n",pkdn[i].bnd.fMin[0],
				   pkdn[i].bnd.fMin[1],pkdn[i].bnd.fMin[2]);
			printf("       MAX:%15f %15f %15f\n",pkdn[i].bnd.fMax[0],
				   pkdn[i].bnd.fMax[1],pkdn[i].bnd.fMax[2]);
			}
		}
	}


void msrDomainColor(MSR msr)
{
	pstDomainColor(msr->pst,NULL,0,NULL,NULL);
	}


void msrReorder(MSR msr)
{
	int sec,dsec;

	if (msr->param.bVerbose) printf("Ordering...\n");
	sec = time(0);
	pstDomainOrder(msr->pst,NULL,0,NULL,NULL);
	pstLocalOrder(msr->pst,NULL,0,NULL,NULL);
	dsec = time(0) - sec;
	if (msr->param.bVerbose) {
		printf("Order established, Wallclock: %d secs\n\n",dsec);
		}
	}


void msrOutArray(MSR msr,char *pszFile,int iType)
{
	char in[SIZE(inOutArray)];
	char achOutFile[PST_FILENAME_SIZE];
	LCL *plcl = msr->pst->plcl;
	FILE *fp;

	if (pszFile) {
		/*
		 ** Add Data Subpath for local and non-local names.
		 */
		achOutFile[0] = 0;
		strcat(achOutFile,msr->param.achDataSubPath);
		strcat(achOutFile,"/");
		strcat(achOutFile,pszFile);
		strcpy(DATA(in,inOutArray)->achOutFile,achOutFile);
		/*
		 ** Add local Data Path.
		 */
		achOutFile[0] = 0;
		if (plcl->pszDataPath) {
			strcat(achOutFile,plcl->pszDataPath);
			strcat(achOutFile,"/");
			}
		strcat(achOutFile,DATA(in,inOutArray)->achOutFile);

		fp = fopen(achOutFile,"w");
		if (!fp) {
			printf("Could not open Array Output File:%s\n",achOutFile);
			msrFinish(msr);
			mdlFinish(msr->mdl);
			exit(1);
			}
		}
	else {
		printf("No Array Output File specified\n");
		msrFinish(msr);
		mdlFinish(msr->mdl);
		exit(1);
		}
	/*
	 ** Write the Header information and close the file again.
	 */
	fprintf(fp,"%d\n",msr->N);
	fclose(fp);
	DATA(in,inOutArray)->iType = iType;
	pstOutArray(msr->pst,in,SIZE(inOutArray),NULL,NULL);
	}


void msrOutVector(MSR msr,char *pszFile,int iType)
{
	char in[SIZE(inOutVector)];
	char achOutFile[PST_FILENAME_SIZE];
	LCL *plcl = msr->pst->plcl;
	FILE *fp;

	if (pszFile) {
		/*
		 ** Add Data Subpath for local and non-local names.
		 */
		achOutFile[0] = 0;
		strcat(achOutFile,msr->param.achDataSubPath);
		strcat(achOutFile,"/");
		strcat(achOutFile,pszFile);
		strcpy(DATA(in,inOutVector)->achOutFile,achOutFile);
		/*
		 ** Add local Data Path.
		 */
		achOutFile[0] = 0;
		if (plcl->pszDataPath) {
			strcat(achOutFile,plcl->pszDataPath);
			strcat(achOutFile,"/");
			}
		strcat(achOutFile,DATA(in,inOutVector)->achOutFile);

		fp = fopen(achOutFile,"w");
		if (!fp) {
			printf("Could not open Vector Output File:%s\n",achOutFile);
			msrFinish(msr);
			mdlFinish(msr->mdl);
			exit(1);
			}
		}
	else {
		printf("No Vector Output File specified\n");
		msrFinish(msr);
		mdlFinish(msr->mdl);
		exit(1);
		}
	/*
	 ** Write the Header information and close the file again.
	 */
	fprintf(fp,"%d\n",msr->N);
	fclose(fp);
	DATA(in,inOutVector)->iType = iType;
	DATA(in,inOutVector)->iDim = 0;
	pstOutVector(msr->pst,in,SIZE(inOutVector),NULL,NULL);
	DATA(in,inOutVector)->iDim = 1;
	pstOutVector(msr->pst,in,SIZE(inOutVector),NULL,NULL);
	DATA(in,inOutVector)->iDim = 2;
	pstOutVector(msr->pst,in,SIZE(inOutVector),NULL,NULL);
	}


void msrDensity(MSR msr)
{
	char in[SIZE(inDensity)];
	int sec,dsec;

	DATA(in,inDensity)->nSmooth = msr->param.nSmooth;
	DATA(in,inDensity)->bGatherScatter = msr->param.bGatherScatter;
	if (msr->param.bVerbose) printf("Calculating Densities...\n");
	sec = time(0);
	pstDensity(msr->pst,in,SIZE(inDensity),NULL,NULL);
	dsec = time(0) - sec;
	printf("Densities Calculated, Wallclock:%d secs\n\n",dsec);
	}


void msrGravity(MSR msr,int *piSec,double *pdWMax,double *pdIMax,
				double *pdEMax)
{
	char in[SIZE(inGravity)];
	char out[SIZE(outGravity)];
	int iDum;
	int sec,dsec;
	double dPartAvg,dCellAvg;
	double dWAvg,dWMax,dWMin;
	double dIAvg,dIMax,dIMin;
	double dEAvg,dEMax,dEMin;

	if (msr->param.bVerbose) printf("Calculating Gravity...\n");
	sec = time(0);
    DATA(in,inGravity)->nReps = msr->param.nReplicas;
    DATA(in,inGravity)->bPeriodic = msr->param.bPeriodic;
    DATA(in,inGravity)->dEwCut = msr->param.dEwCut;
    DATA(in,inGravity)->dEwhCut = msr->param.dEwhCut;
	pstGravity(msr->pst,in,SIZE(inGravity),out,&iDum);
	dsec = time(0) - sec;
	printf("Gravity Calculated, Wallclock:%d secs\n",dsec);
	*piSec = dsec;
	dPartAvg = DATA(out,outGravity)->dPartSum/msr->N;
	dCellAvg = DATA(out,outGravity)->dCellSum/msr->N;
	dWAvg = DATA(out,outGravity)->dWSum/msr->nThreads;
	dIAvg = DATA(out,outGravity)->dISum/msr->nThreads;
	dEAvg = DATA(out,outGravity)->dESum/msr->nThreads;
	dWMax = DATA(out,outGravity)->dWMax;
	*pdWMax = dWMax;
	dIMax = DATA(out,outGravity)->dIMax;
	*pdIMax = dIMax;
	dEMax = DATA(out,outGravity)->dEMax;
	*pdEMax = dEMax;
	dWMin = DATA(out,outGravity)->dWMin;
	dIMin = DATA(out,outGravity)->dIMin;
	dEMin = DATA(out,outGravity)->dEMin;
	printf("dPartAvg:%f dCellAvg:%f\n",dPartAvg,dCellAvg);
	printf("Walk CPU     Avg:%10f Max:%10f Min:%10f\n",dWAvg,dWMax,dWMin);
	printf("Interact CPU Avg:%10f Max:%10f Min:%10f\n",dIAvg,dIMax,dIMin);
	printf("Ewald CPU    Avg:%10f Max:%10f Min:%10f\n",dEAvg,dEMax,dEMin);	
	printf("\n");
	}


void msrStepCosmo(MSR msr,double dTime)
{
	double a;
	
	a = Time2Expand(dTime,msr->param.dHubble0);
	msr->dRedshift = 1/a - 1;
	msr->dHubble = msr->param.dHubble0*(1+msr->dRedshift)*
		sqrt(1+msr->param.dOmega0*msr->dRedshift);
	if (msr->param.bComove) msr->dCosmoFac = a;	
	}


void msrCalcE(MSR msr,int bFirst,double dTime,double *E,double *T,double *U)
{
	char out[SIZE(outCalcE)];
	int iDum;

	pstCalcE(msr->pst,NULL,0,out,&iDum);
	*T = DATA(out,outCalcE)->T;
	*U = DATA(out,outCalcE)->U;
	/*
	 ** Do the comoving coordinates stuff.
	 */
	*U *= msr->dCosmoFac;
	*T *= pow(msr->dCosmoFac,4.0);
	if (msr->param.bComove && !bFirst) {
		msr->dEcosmo += 0.5*(dTime - msr->dTimeOld)*
			(msr->dHubbleOld*msr->dUOld + msr->dHubble*(*U));
		}
	else {
		msr->dEcosmo = 0.0;
		}
	msr->dTimeOld = dTime;
	msr->dUOld = *U;
	msr->dHubbleOld = msr->dHubble;
	*E = (*T) + (*U) - msr->dEcosmo;
	}


void msrDrift(MSR msr,double dDelta)
{
	char in[SIZE(inDrift)];
	int j;

	DATA(in,inDrift)->dDelta = dDelta;
	for (j=0;j<3;++j) {
		DATA(in,inDrift)->fCenter[j] = msr->fCenter[j];
		}
	pstDrift(msr->pst,in,SIZE(inDrift),NULL,NULL);
	}


void msrKick(MSR msr,double dDelta)
{
	char in[SIZE(inKick)];
	
	DATA(in,inKick)->dvFacOne = 
		(1 - msr->dHubble*dDelta)/(1 + msr->dHubble*dDelta);
	DATA(in,inKick)->dvFacTwo = 
		dDelta/pow(msr->dCosmoFac,3)/(1 + msr->dHubble*dDelta);
	pstKick(msr->pst,in,SIZE(inKick),NULL,NULL);
	}


double msrReadCheck(MSR msr,int *piStep)
{
	FILE *fp;
	struct msrCheckPointHeader h;
	char in[SIZE(inReadCheck)];
	char achInFile[PST_FILENAME_SIZE];
	int i,j;
	LCL *plcl = msr->pst->plcl;
	double dTime;
	
	/*
	 ** Add Data Subpath for local and non-local names.
	 */
	achInFile[0] = 0;
	strcat(achInFile,msr->param.achDataSubPath);
	strcat(achInFile,"/");
	sprintf(achInFile,"%s.chk",msr->param.achOutName);
	strcpy(DATA(in,inReadCheck)->achInFile,achInFile);
	/*
	 ** Add local Data Path.
	 */
	achInFile[0] = 0;
	if (plcl->pszDataPath) {
		strcat(achInFile,plcl->pszDataPath);
		strcat(achInFile,"/");
		}
	strcat(achInFile,DATA(in,inReadCheck)->achInFile);
	
	fp = fopen(achInFile,"r");
	if (!fp) {
		printf("Could not open InFile:%s\n",achInFile);
		msrFinish(msr);
		mdlFinish(msr->mdl);
		exit(1);
		}
	fread(&h,sizeof(struct msrCheckPointHeader),1,fp);
	fclose(fp);
	dTime = h.dTime;
	*piStep = h.iStep;
	msr->N = h.N;
	msr->param = h.param;
	msr->iOpenType = h.iOpenType;
	msr->dCrit = h.dCrit;
	msr->dRedshift = h.dRedshift;
	msr->dHubble = h.dHubble;
	msr->dCosmoFac = h.dCosmoFac;
	msr->dEcosmo = h.dEcosmo;
	msr->dHubbleOld = h.dHubbleOld;
	msr->dUOld = h.dUOld;
	msr->dTimeOld = h.dTimeOld;
	msr->nRed = h.nRed;
	for (i=0;i<msr->nRed;++i) {
		msr->dRedOut[i] = h.dRedOut[i];
		}
	if (msr->param.bVerbose) {
		if (msr->param.bComove) {
			printf("Reading checkpoint file...\nN:%d Time:%g Redshift:%g\n",
				   msr->N,dTime,msr->dRedshift);
			}
		else {
			printf("Reading checkpoint file...\nN:%d Time:%g\n",msr->N,dTime);
			}
		}
	DATA(in,inReadCheck)->nStart = 0;
	DATA(in,inReadCheck)->nEnd = msr->N - 1;
	DATA(in,inReadCheck)->iOrder = msr->param.iOrder;
	/*
	 ** Since pstReadCheck causes the allocation of the local particle
	 ** store, we need to tell it the percentage of extra storage it
	 ** should allocate for load balancing differences in the number of
	 ** particles.
	 */
	DATA(in,inReadCheck)->fExtraStore = msr->param.dExtraStore;
	/*
	 ** Provide the period.
	 */
	for (j=0;j<3;++j) {
		DATA(in,inReadCheck)->fPeriod[j] = msr->param.dPeriod;
		}
	/*
	 ** Do a parallel Read of the input file, may be benificial for
	 ** some systems, or if there are multiple identical physical files. 
	 ** Start child threads reading!
	 */
	pstReadCheck(msr->pst,in,SIZE(inReadCheck),NULL,NULL);
	if (msr->param.bVerbose) puts("Checkpoint file has been successfully read.");
	return(dTime);
	}


void msrWriteCheck(MSR msr,double dTime,int iStep)
{
	FILE *fp;
	struct msrCheckPointHeader h;
	char in[SIZE(inWriteCheck)];
	char oute[SIZE(outSetTotal)];
	char achOutFile[PST_FILENAME_SIZE];
	char achOutTmp[PST_FILENAME_SIZE];
	char achOutBak[PST_FILENAME_SIZE];
	char ach[4*PST_FILENAME_SIZE];
	int iDum,i;
	LCL *plcl = msr->pst->plcl;
	
	/*
	 ** Add Data Subpath for local and non-local names.
	 */
	achOutFile[0] = 0;
	strcat(achOutFile,msr->param.achDataSubPath);
	strcat(achOutFile,"/");
	sprintf(achOutFile,"%s.chk",msr->param.achOutName);
	strcpy(DATA(in,inWriteCheck)->achOutFile,achOutFile);
	/*
	 ** Add local Data Path.
	 */
	achOutFile[0] = 0;
	if (plcl->pszDataPath) {
		strcat(achOutFile,plcl->pszDataPath);
		strcat(achOutFile,"/");
		}
	strcat(achOutFile,DATA(in,inWriteCheck)->achOutFile);
	sprintf(achOutTmp, "%s%s", achOutFile, ".tmp");
	sprintf(achOutBak, "%s%s", achOutFile, ".bak");
	strcat(DATA(in,inWriteCheck)->achOutFile, ".tmp");
	
	fp = fopen(achOutTmp,"w");
	if (!fp) {
		printf("Could not open OutFile:%s\n",achOutTmp);
		msrFinish(msr);
		mdlFinish(msr->mdl);
		exit(1);
		}
	h.dTime = dTime;
	h.iStep = iStep;
	h.N = msr->N;
	h.param = msr->param;
	h.iOpenType = msr->iOpenType;
	h.dCrit = msr->dCrit;
	h.dRedshift = msr->dRedshift;
	h.dHubble = msr->dHubble;
	h.dCosmoFac = msr->dCosmoFac;
	h.dEcosmo = msr->dEcosmo;
	h.dHubbleOld = msr->dHubbleOld;
	h.dUOld = msr->dUOld;
	h.dTimeOld = msr->dTimeOld;
	h.nRed = msr->nRed;
	for (i=0;i<msr->nRed;++i) {
		h.dRedOut[i] = msr->dRedOut[i];
		}
	if (msr->param.bVerbose) {
		if (msr->param.bComove) {
			printf("Writing checkpoint file...\nTime:%g Redshift:%g\n",msr->N,
				   dTime,(1/dTime - 1));
			}
		else {
			printf("Writing checkpoint file...\nTime:%g\n",msr->N,dTime);
			}
		}
	fwrite(&h,sizeof(struct msrCheckPointHeader),1,fp);
	fclose(fp);
	/*
	 ** Do a parallel write to the output file.
	 */
	pstSetTotal(msr->pst,NULL,0,oute,&iDum);
	DATA(in,inWriteCheck)->nStart = 0;
	pstWriteCheck(msr->pst,in,SIZE(inWriteCheck),NULL,NULL);
	if (msr->param.bVerbose) {
		puts("Checkpoint file has been successfully written.");
		}
	sprintf(ach, "mv -f %s %s; mv %s %s", achOutFile, achOutBak,
		achOutTmp, achOutFile);
	system(ach);
	}


double msrRedOut(MSR msr,int iRed)
{
	if (iRed < msr->nRed) {
		return(msr->dRedOut[iRed]);
		}
	else {
		return(-1.0);
		}
	}


void msrReadRed(MSR msr)
{
	char achFile[PST_FILENAME_SIZE];
	char ach[PST_FILENAME_SIZE];
	LCL *plcl = &msr->lcl;
	FILE *fp;
	int iRed,ret;
	
	/*
	 ** Add Data Subpath for local and non-local names.
	 */
	achFile[0] = 0;
	strcat(achFile,msr->param.achDataSubPath);
	strcat(achFile,"/");
	sprintf(achFile,"%s.red",msr->param.achOutName);
	/*
	 ** Add local Data Path.
	 */
	if (plcl->pszDataPath) {
		strcpy(ach,achFile);
		sprintf(achFile,"%s/%s",plcl->pszDataPath,ach);
		}
	fp = fopen(achFile,"r");
	if (!fp) {
		printf("Could not open redshift input file:%s\n",achFile);
		msrFinish(msr);
		mdlFinish(msr->mdl);
		exit(1);
		}
	iRed = 0;
	while (1) {
		ret = fscanf(fp,"%lf",&msr->dRedOut[iRed]);
		if (ret != 1) break;
		++iRed;
		}
	msr->nRed = iRed;
	fclose(fp);
	}


int msrSteps(MSR msr)
{
	return(msr->param.nSteps);
	}


char *msrOutName(MSR msr)
{
	return(msr->param.achOutName);
	}


double msrDelta(MSR msr)
{
	return(msr->param.dDelta);
	}


double msrRedshift(MSR msr)
{
	return(msr->dRedshift);
	}


int msrCheckInterval(MSR msr)
{
	return(msr->param.iCheckInterval);
	}


int msrLogInterval(MSR msr)
{
	return(msr->param.iLogInterval);
	}


int msrOutInterval(MSR msr)
{
	return(msr->param.iOutInterval);
	}


int msrRestart(MSR msr)
{
	return(msr->param.bRestart);
	}


int msrComove(MSR msr)
{
	return(msr->param.bComove);
	}
