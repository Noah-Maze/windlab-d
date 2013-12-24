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
			PT_enumeration,"Gen_mode",PADDR(Gen_mode),
				PT_KEYWORD,"MY_ZERO",(enumeration)MY_ZERO,
				PT_KEYWORD,"MY_CONSTANT",(enumeration)MY_CONSTANT,
				PT_KEYWORD,"MY_RANDOM",(enumeration)MY_RANDOM,

			PT_double, "a_new_double", PADDR(a_new_double),
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
	/* TODO: set the context-free initial value of properties */
	a_new_double = 1.0;
	Gen_mode = 1;
	return 1; /* return 1 on success, 0 on failure */
}

/* Object initialization is called once after all object have been created */
int windclass::init(OBJECT *parent)
{

	gl_verbose("windclass init: about to exit");
	//init_climate();
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
	switch(Gen_mode) 
	{
		case MY_ZERO:	a_new_double += 0;
				break;
		case MY_CONSTANT: a_new_double += 0.01;
				break;
		case MY_RANDOM: a_new_double += rand();
				break;
	}

	return t1+60;
}

/* Postsync is called when the clock needs to advance on the second top-down pass */
TIMESTAMP windclass::postsync(TIMESTAMP t0, TIMESTAMP t1)
{
	TIMESTAMP t2 = TS_NEVER;
	/* TODO: implement post-topdown behavior */
	return t2; /* return t2>t1 on success, t2=t1 for retry, t2<t1 on failure */
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
