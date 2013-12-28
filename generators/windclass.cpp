/** $Id: windclass.cpp,v 1.2 2008/02/12 00:28:08 d3g637 Exp $
	Copyright (C) 2008 Battelle Memorial Institute
	@file windclass.cpp
	@defgroup windclass
	@ingroup generators

 @{
 **/

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>

#include "generators.h"

#include "windclass.h"
#include "gridlabd.h"



#define HOUR 3600 * TS_SECOND

CLASS *windclass::oclass = NULL;
windclass *windclass::defaults = NULL;

static PASSCONFIG passconfig = PC_BOTTOMUP|PC_POSTTOPDOWN;
static PASSCONFIG clockpass = PC_BOTTOMUP;


windclass::windclass(){};


/* Class registration is only called once to register the class with the core */
windclass::windclass(MODULE *module)
{
	if (oclass==NULL)
	{
		oclass = gl_register_class(module,"windclass",sizeof(windclass),PC_PRETOPDOWN|PC_BOTTOMUP|PC_POSTTOPDOWN);
		if (oclass==NULL)
			GL_THROW("unable to register object class implemented by %s", __FILE__); 

		if (gl_publish_variable(oclass,
			PT_enumeration,"Model_type",PADDR(Model_type),
				PT_KEYWORD,"LINEAR",(enumeration)LINEAR,
				PT_KEYWORD,"LOGISTIC",(enumeration)LOGISTIC,
				PT_KEYWORD,"AGGLINEAR",(enumeration)AGGLINEAR,
				PT_KEYWORD,"AGGLOGISTIC",(enumeration)AGGLOGISTIC,

			PT_double, "power_out", PADDR(power_out),
			PT_double, "cut_in", PADDR(cut_in),
			PT_double, "cut_out", PADDR(cut_out),
			PT_double, "pA", PADDR(pA),
			PT_double, "wA", PADDR(wA),
			PT_double, "pB", PADDR(pB),
			PT_double, "wB", PADDR(wB),
			PT_double, "pC", PADDR(pC),
			PT_double, "wC", PADDR(wC),
			PT_double, "pD", PADDR(pD),
			PT_double, "wD", PADDR(wD),
			PT_double, "pE", PADDR(pE),
			PT_double, "wE", PADDR(wE),
			PT_double, "dMeters",	PADDR(dMeters),
			PT_double, "scale",	PADDR(scale),
			PT_double, "shape",	PADDR(shape),
			PT_double, "spatialStd",PADDR(spatialStd),
			PT_int32,    "dt",	PADDR(dt),
			PT_int32,    "turbines",	PADDR(turbines),
			NULL)<1) GL_THROW("unable to publish properties in %s",__FILE__);
		defaults = this;

		memset(this,0,sizeof(windclass));
		/* TODO: set the default values of all properties here */
	}
}


/* Object creation is called once for each object that is created by the core */
int windclass::create(void) 
{
	memcpy(this,defaults,sizeof(*this));

	// These are all the defaults.
	
	power_out = 1.0;
	Model_type = 1; //LInear

	// Params for all models
	 cut_in = 5.0;
	 cut_out = 30.0;
	mv_avg_wind_speed = 0.0;
	turbines = 1;
	
	// Linear Model Parameters
	 wA=6.0; 	 pA=20.0;
	 wB=10.0;	 pB=75.0;
	 wC=14.0;	 pC=140.0;
	 wD=15.0;	 pD=135.0;
	 wE=30.0;	 pE=130.0;	


	// Aggregate Model Bonus Params
	dMeters = 1000;
	scale = 8;
	shape = 2;
	spatialStd = 1;
	dt = 60;

	return 1; /* return 1 on success, 0 on failure */
}

/* Checks for climate object and maps the climate variables to the windturb object variables.  
If no climate object is linked, then default pressure, temperature, and wind speed will be used. */
int windclass::init_climate()
{
	OBJECT *hdr = OBJECTHDR(this);

	// link to climate data
	static FINDLIST *climates = NULL;
	int not_found = 0;
	
	climates = gl_find_objects(FL_NEW,FT_CLASS,SAME,"climate",FT_END);
	if (climates==NULL)
	{
		not_found = 1;
		gl_warning("windturb_dg (id:%d)::init_climate(): no climate data found",hdr->id);
				
	}
	else if (climates->hit_count>1)
	{
		gl_verbose("windturb_dg: %d climates found, using first one defined", climates->hit_count);
	}
	if (climates!=NULL)	
	{
		if (climates->hit_count==0)	
		{
			//default to mock data
			gl_warning("windturb_dg (id:%d)::init_climate(): no climate data found, using static data",hdr->id);
		}
		else //climate data was found
		{
			// force rank of object w.r.t climate
			OBJECT *obj = gl_find_next(climates,NULL);
			if (obj->rank<=hdr->rank)
				gl_set_dependent(obj,hdr);
			pWS = (double*)GETADDR(obj,gl_get_property(obj,"wind_speed"));

			//Make sure it worked
			if (pWS==NULL)
			{
				GL_THROW("windturb_dg (id:%d): Unable to map wind_speed from climate object",obj->id);
			}
		}
	}
	return 1;
}


/* Object initialization is called once after all object have been created */
int windclass::init(OBJECT *parent)
{
	windMean = scale * stir_gamma(1+1/scale);

	init_climate();
	switch(Model_type) 
	{
			// Define IIR filter moving average estimator
			// a ~ 1/N where N is the number of frames to average
			// N = frames per propagation time 
			// T = D/wm = pro time in seconds 
			// dt = distance between update frames in seconds
			
		case AGGLINEAR:   
		case AGGLOGISTIC: alphaIIR = (1.0 /  (dMeters / windMean) / dt);
				break;
		default: alphaIIR = 1.0;
			break;
	}
	gl_verbose("windclass init: about to exit");
	return 1; /* return 1 on success, 0 on failure */
}


/* Presync is called when the clock needs to advance on the first top-down pass */
TIMESTAMP windclass::presync(TIMESTAMP t0, TIMESTAMP t1)
{

	TIMESTAMP t2 = TS_NEVER;
	/* TODO: implement pre-topdown behavior */
	return t2; /* return t2>t1 on success, t2=t1 for retry, t2<t1 on failure */
}

/* Sync is called when the clock needs to advance on the bottom-up pass */
TIMESTAMP windclass::sync(TIMESTAMP t0, TIMESTAMP t1) 
{
	mv_avg_wind_speed = ( alphaIIR * (*pWS) ) + ( (1 - alphaIIR) * mv_avg_wind_speed );
	switch(Model_type) 
	{
		case LINEAR:	power_out = lin_pow( mv_avg_wind_speed );
				break;

		case LOGISTIC:  power_out = log_pow( mv_avg_wind_speed );
				break;

		case AGGLINEAR: power_out = agg_lin_pow( mv_avg_wind_speed );
				break;
		case AGGLOGISTIC: power_out = agg_log_pow( mv_avg_wind_speed );
				break;
	}

	return t1+dt;
}

/* Postsync is called when the clock needs to advance on the second top-down pass */
TIMESTAMP windclass::postsync(TIMESTAMP t0, TIMESTAMP t1)
{
	TIMESTAMP t2 = TS_NEVER;
	/* TODO: implement post-topdown behavior */
	return t2; /* return t2>t1 on success, t2=t1 for retry, t2<t1 on failure */
}

double windclass::stir_gamma(double arg) {

return sqrt(2.0*3.14159265/arg)*pow(arg/2.7182818, arg);

}

double windclass::lin_pow(double wind){

	double power_return = 0.0;

	if((wind) < cut_out) power_return = pE;
	if((wind) < wE) power_return = pE - ((pE - pD)*(wE-(wind)) / (wE - wD));
	if((wind) < wD) power_return = pD - ((pD - pC)*(wD-(wind)) / (wD - wC));
	if((wind) < wC) power_return = pC - ((pC - pB)*(wC-(wind)) / (wC - wB));
	if((wind) < wB) power_return = pB - ((pB - pA)*(wB-(wind)) / (wB - wA));
	if((wind) < wA) power_return = pA - ((pA     )*(wA-(wind)) / (wA - cut_in));
	if((wind) < cut_in) power_return = 0.0;
	if((wind) > cut_out) power_return = 0.0;

	return power_return * turbines;

}

double windclass::log_pow(double wind){

	if((wind) < cut_in)  return 0.0;
	if((wind) > cut_out) return 0.0;

	return turbines* (pD + ((pA-pD) / pow( (1+((wind)/pC)), pE)));
}

double windclass::agg_lin_pow(double wind){
 // Returns a six-trapezoid estimation of the integral of the power output about a normally distributed wind speed
 
 double total = 0;
 total = total = ( ( 0.0214 * ( lin_pow(wind + (-3*spatialStd) ) + lin_pow(wind + (-2*spatialStd) ) ) ) + 
                   ( 0.1359 * ( lin_pow(wind + (-2*spatialStd) ) + lin_pow(wind + (-1*spatialStd) ) ) ) + 
                   ( 0.3413 * ( lin_pow(wind + (-1*spatialStd) ) + lin_pow(wind + (0*spatialStd) ) ) )  + 
                   ( 0.3413 * ( lin_pow(wind + (0*spatialStd) ) + lin_pow(wind + (1*spatialStd) ) ) )   + 
                   ( 0.1359 * ( lin_pow(wind + (1*spatialStd) ) + lin_pow(wind + (2*spatialStd) ) ) )   + 
                   ( 0.0214 * ( lin_pow(wind + (2*spatialStd) ) + lin_pow(wind + (3*spatialStd) ) ) )     ) / 2;

 return total * turbines;
}

double windclass::agg_log_pow(double wind){
 double total = 0;
 total = total = ( ( 0.0214 * ( log_pow(wind + (-3*spatialStd) ) + log_pow(wind + (-2*spatialStd) ) ) ) + 
                   ( 0.1359 * ( log_pow(wind + (-2*spatialStd) ) + log_pow(wind + (-1*spatialStd) ) ) ) + 
                   ( 0.3413 * ( log_pow(wind + (-1*spatialStd) ) + log_pow(wind + (0*spatialStd) ) ) )  + 
                   ( 0.3413 * ( log_pow(wind + (0*spatialStd) ) + log_pow(wind + (1*spatialStd) ) ) )   + 
                   ( 0.1359 * ( log_pow(wind + (1*spatialStd) ) + log_pow(wind + (2*spatialStd) ) ) )   + 
                   ( 0.0214 * ( log_pow(wind + (2*spatialStd) ) + log_pow(wind + (3*spatialStd) ) ) )     ) / 2;

 return total * turbines;
}

//////////////////////////////////////////////////////////////////////////
// IMPLEMENTATION OF CORE LINKAGE
//////////////////////////////////////////////////////////////////////////

EXPORT int create_windclass(OBJECT **obj, OBJECT *parent) 
{
	try 
	{
		*obj = gl_create_object(windclass::oclass);
		if (*obj!=NULL)
		{
			windclass *my = OBJECTDATA(*obj,windclass);
			gl_set_parent(*obj,parent);
			return my->create();
		}
	} 
	catch (char *msg) 
	{
		gl_error("create_windclass: %s", msg);
	}
	return 0;
}

EXPORT int init_windclass(OBJECT *obj, OBJECT *parent) 
{
	try 
	{
		if (obj!=NULL)
			return OBJECTDATA(obj,windclass)->init(parent);
	}
	catch (char *msg)
	{
		gl_error("init_windclass(obj=%d;%s): %s", obj->id, obj->name?obj->name:"unnamed", msg);
	}
	return 0;
}

EXPORT TIMESTAMP sync_windclass(OBJECT *obj, TIMESTAMP t1, PASSCONFIG pass)
{
	TIMESTAMP t2 = TS_NEVER;
	windclass *my = OBJECTDATA(obj,windclass);
	try
	{
		switch (pass) {
		case PC_PRETOPDOWN:
			t2 = my->presync(obj->clock,t1);
			break;
		case PC_BOTTOMUP:
			t2 = my->sync(obj->clock,t1);
			break;
		case PC_POSTTOPDOWN:
			t2 = my->postsync(obj->clock,t1);
			break;
		default:
			GL_THROW("invalid pass request (%d)", pass);
			break;
		}
		if (pass==clockpass)
			obj->clock = t1;		
	}
	catch (char *msg)
	{
		gl_error("sync_windclass(obj=%d;%s): %s", obj->id, obj->name?obj->name:"unnamed", msg);
	}
	return t2;
}


