#ifndef OUTTYPE_HINCLUDED
#define OUTTYPE_HINCLUDED

#include "pkd.h"

#define OUT_COLOR_ARRAY		1
#define OUT_DENSITY_ARRAY	2
#define OUT_POT_ARRAY		3
#define OUT_AMAG_ARRAY		4
#define OUT_IMASS_ARRAY		5
#define OUT_RUNG_ARRAY		6

#ifdef GASOLINE

#define OUT_U_ARRAY			7
#define OUT_UDOT_ARRAY		8
#define OUT_HSMDIVV_ARRAY	9
#define OUT_HI_ARRAY		16
#define OUT_HeI_ARRAY		17
#define OUT_HeII_ARRAY		18
#define OUT_BALSARASWITCH_ARRAY 20
#define OUT_DIVV_ARRAY          21
#define OUT_MUMAX_ARRAY         22
#define OUT_SHOCKTRACKER_ARRAY  23
#define OUT_DIVONCONH_ARRAY     24
#define OUT_DIVONCONX_ARRAY     25
#define OUT_DIVRHOV_ARRAY       26
#define OUT_PDV_ARRAY           27
#define OUT_PDVPRES_ARRAY       28
#define OUT_PDVVISC_ARRAY       29
#define OUT_PDVSN_ARRAY         30
#define OUT_USN_ARRAY           31
#endif
#define OUT_SOFT_ARRAY          32
#define OUT_IORDER_ARRAY	33
#define OUT_IGASORDER_ARRAY	34
#define OUT_TCOOLAGAIN_ARRAY 35
#define OUT_MSTAR_ARRAY      36

#define OUT_H_ARRAY			19

#define OUT_DT_ARRAY		10

#ifdef COLLISIONS
#define OUT_REJECTS_ARRAY	11
#endif

#define OUT_POS_VECTOR		1
#define OUT_VEL_VECTOR		2
#define OUT_ACCEL_VECTOR	3

#ifdef NEED_VPRED
#define OUT_VPRED_VECTOR	4
#define OUT_GRADRHO_VECTOR      5
#define OUT_ACCELPRES_VECTOR	6
#endif
#define OUT_RFORM_VECTOR	7
#define OUT_VFORM_VECTOR	8

void pkdOutArray(PKD,char *,int,int);
void pkdOutVector(PKD,char *,int,int,int);

#endif
