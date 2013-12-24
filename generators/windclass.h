#ifndef _WINDCLASS_H
#define _WINDCLASS_H
#include "gridlabd.h"
class windclass {
public:
	// TODO: add your published variables here using GridLAB types (see PROPERTY)
	double myDouble; // just an example
private:
	// TODO: add any unpublished variables here (any type is ok)
	double *pDouble; // just an example
public:
	static CLASS *oclass;
	static windclass *defaults;
public:
	windclass(MODULE *module);
	int create(void);
	int init(OBJECT *parent);
	TIMESTAMP sync(TIMESTAMP t0, TIMESTAMP t1);
};
#endif