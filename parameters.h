#ifndef PARAMETERS_HINCLUDED
#define PARAMETERS_HINCLUDED


struct parameters {
	/*
	 ** Parameters for PKDGRAV.
	 */
	int nThreads;
	int bDiag;
	int bOverwrite;
	int bVWarnings;
	int bVStart;
	int bVStep;
	int bVRungStat;
	int bVDetails;
	int bPeriodic;
	int bRestart;
	int bComove;
	int bParaRead;
	int bParaWrite;
	int bCannonical;
	int bStandard;
	int bKDK;
	int bBinary;
	int bEpsVel;
	int bSqrtPhi;
	int bISqrtRho;
	int bNonSymp;
	int bDoDensity;
	int bSymCool;
	int bDoGravity;
	int bFandG;
	int bHeliocentric;
	int bLogHalo;
	int bHernquistSpheroid;
	int bMiyamotoDisk;
	int nBucket;
	int iOutInterval;
	int iLogInterval;
	int iCheckInterval;
	int iOrder;
	int iEwOrder;
	int nReplicas;
	int nSteps;
	int nSmooth;
	int iMaxRung;
	int nSuperCool;
        int iWallRunTime;
	double dEta;
	double dExtraStore;
	double dSoft;
	double dDelta;
	double dEwCut;
	double dEwhCut;
	double dTheta;
	double dTheta2;
	double dAbsPartial;
	double dRelPartial;
	double dAbsTotal;
	double dRelTotal;
	double dPeriod;
	double dxPeriod;
	double dyPeriod;
	double dzPeriod;
	double dHubble0;
	double dOmega0;
	double dRedTo;
	double dCentMass;
	char achInFile[256];
	char achOutName[256];
	char achDataSubPath[256];
	double dCoolFac;
	double dCoolDens;
	double dCoolMaxDens;
	/*
	 ** Additional parameters for GASOLINE.
	 */
	int bGeometric;
	double dConstAlpha;
	double dConstBeta;
	double dConstGamma;
	double dMeanMolWeight;
	double dGasConst;
	double dMsolUnit;
	double dKpcUnit;
#ifdef COLLISIONS
	/*
	 ** Additional parameters for collision code...
	 */
	int bFindRejects;
	double dSmallStep;
	int iOutcomes;
	double dEpsN;
	double dEpsT;
	int bDoCollLog;
	char achCollLog[256];
#endif /* COLLISIONS */
	};


#endif




