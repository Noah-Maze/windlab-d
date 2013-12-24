#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include "windclass.h"
CLASS *windclass::oclass = NULL;
windclass *windclass::defaults = NULL;
windclass::windclass(MODULE *module)
{
	if (oclass==NULL)
	{
		oclass = gl_register_class(module,"windclass",PC_BOTTOMUP);
		if (gl_publish_variable(oclass,
			// TODO: publish your variables here
			PT_double, "my_double", PADDR(myDouble), // just an example
			NULL)<1) GL_THROW("unable to publish properties in %s",__FILE__);
		defaults = this;
		// TODO: initialize the default values that apply to all objects of this class
		myDouble = 1.23; // just an example
		pDouble = NULL; // just an example
	}
}
void windclass::create(void)
{
	memcpy(this,defaults,sizeof(*this));
	// TODO: initialize the defaults values that do not depend on relatioships with other objects
}
int windclass::init(OBJECT *parent)
{
	// find the data in the parent object
	windclass *pwindclass = OBJECTDATA(parent,windclass); // just an example

	// TODO: initialize the default values that depend on relationships with other objects
	pDouble = &(pwindclass->double); // just an example

	// return 1 on success, 0 on failure
	return 1;
}
TIMESTAMP windclass::sync(TIMESTAMP t0, TIMESTAMP t1)
{
	// TODO: update the state of the object
	myDouble = myDouble*1.01; // just an example

	// TODO: compute time to next state change
	return (TIMESTAMP)(t1 + myDouble/TS_SECOND); // just an example
}

//////////////////////////////////////////////////////////////////////////
// IMPLEMENTATION OF CORE LINKAGE
//////////////////////////////////////////////////////////////////////////

/// Create a windclass object
EXPORT int create_windclass(OBJECT **obj, ///< a pointer to the OBJECT*
						  OBJECT *parent) ///< a pointer to the parent OBJECT
{
	*obj = gl_create_object(windclass::oclass,sizeof(windclass));
	if (*obj!=NULL)
	{
		windclass *my = OBJECTDATA(*obj,windclass);
		gl_set_parent(*obj,parent);
		my->create();
		return 1;
	}
	return 0;
}

/// Initialize the object
EXPORT int init_windclass(OBJECT *obj, ///< a pointer to the OBJECT
						OBJECT *parent) ///< a pointer to the parent OBJECT
{
	if (obj!=NULL)
	{
		windclass *my = OBJECTDATA(obj,windclass);
		return my->init(parent);
	}
	return 0;
}

/// Synchronize the object
EXPORT TIMESTAMP sync_windclass(OBJECT *obj, ///< a pointer to the OBJECT
			  TIMESTAMP t1) ///< the time to which the OBJECT's clock should advance
{
	TIMESTAMP t2 = OBJECTDATA(obj,windclass)->sync(obj->clock,t1);
	obj->clock = t1;
	return t2;
}
