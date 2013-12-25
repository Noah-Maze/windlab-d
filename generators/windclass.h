/** $Id: windclass.h,v 1.0 2008/07/18
	Copyright (C) 2008 Battelle Memorial Institute
	@file windclass.h
	@addtogroup windclass

 @{  
 **/

#ifndef _windclass_H
#define _windclass_H

#include <stdarg.h>
#include "gridlabd.h"

class windclass
{
private:
	/* TODO: put private variables here */

protected:
	/* TODO: put unpublished but inherited variables */
public:
	/* TODO: put published variables here */
	enum MODEL_TYPE {LINEAR=1, LOGISTIC, AGGLINEAR, AGGLOGISTIC};
	enumeration Model_type;  //operating mode of the generator 
	double power_out;
	double * pWS;
	
	// Params for all models
	double cut_in;
	double cut_out;
	
	// Linear Model Parameters
	double wA;
	double wB;
	double wC;
	double wD;
	double wE;
	double pA;
	double pB;
	double pC;
	double pD;
	double pE;	

public:
	/* required implementations */
	windclass(void);
	windclass(MODULE *module);
	int create(void);
	int init(OBJECT *parent);
	int init_climate(void);
	//double timestamp_to_hours(TIMESTAMP t);
	//TIMESTAMP rfb_event_time(TIMESTAMP t0, complex power, double e);
	TIMESTAMP presync(TIMESTAMP t0, TIMESTAMP t1);
	TIMESTAMP sync(TIMESTAMP t0, TIMESTAMP t1);
	TIMESTAMP postsync(TIMESTAMP t0, TIMESTAMP t1);

public:
	static CLASS *oclass;
	static windclass *defaults;
#ifdef OPTIONAL
	static CLASS *pclass; /**< defines the parent class */
	TIMESTAMPP plc(TIMESTAMP t0, TIMESTAMP t1); /**< defines the default PLC code */
#endif
};

#endif

