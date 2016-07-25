#include <stdio.h>
#include <string>
#include <map>
#include <stdlib.h>

#ifndef __CONFIG_DEF
#define __CONFIG_DEF
typedef std::map<std::string,std::string> confmap;

class ConfigClass{
	confmap data;
public:
	
	int ci(const char *);
	long cl(const char *);
	float cf(const char *);
	double cd(const char *);
	const char* cc(const char * );
	std::string cs(const char * );

	ConfigClass();
	void dump();
	int LoadFromFile(const char* );

};


int ConfI(const char *) ;
long ConfL(const char *);
float ConfF(const char *);
double ConfD(const char *);
const char* ConfC(const char * );
std::string ConfS(const char * );
int ConfLoad(const char *);



#endif
//
