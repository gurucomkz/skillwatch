#include <stdio.h>
#include <string>
#include <map>
#include <stdlib.h>

#include "config.h"
#include "splog.h"


int ConfigClass::ci(const char *key){	
	int x = 0;
	x = atoi(data[key].c_str());
	return x;
}
long ConfigClass::cl(const char *key){	
	long x = 0;
	x = atol(data[key].c_str());
	return x;
}
float ConfigClass::cf(const char *key){	
	float x = 0;
	x = atof(data[key].c_str());
	return x;
}
double ConfigClass::cd(const char *key){	
	double x = 0;
	x = atof(data[key].c_str());
	return x;
}
const char* ConfigClass::cc(const char * key)
{	return data[key].c_str();	}
std::string ConfigClass::cs(const char * key)
{	return data[key];	}

ConfigClass::ConfigClass(){	//here we define defaults
	
}
void ConfigClass::dump(){
	confmap::const_iterator cIter;
	cIter = this->data.begin();
	while(cIter!=data.end()){
		printf("%s = %s\n",cIter->first.c_str(),cIter->second.c_str());
		cIter++;
	}
}

int ConfigClass::LoadFromFile(const char* fname){
	FILE* f;
	char * buf = (char*)malloc(10240), c;
	long i;
	//declare states
	enum {sLFD,	/*lookForDirective*/
		 sRD,	/*reading directive*/
		 sLFV,	//look for value
		 sRV,	//reading value
		 sRC	//reading comment
	}state;		
	std::string k,v;
	bool inq;

	//snprintf(buf,10240,"Reading config from file: %s",fname);
	//rep(buf);
	f = fopen(fname,"r");
	if(f==NULL) {
		free(buf);
		return 0;
	}
	while(fgets(buf,10239,f)){
		//printf("NL: %s\n",buf);
		k = v = "";
		inq = false;
		state = sLFD;
		for(i=0;buf[i];i++){
			c = buf[i];
			switch(state){
			case sLFD:
				if(c == '#' ) {buf[i+1]=0; break;};
				if(c>32){
					k += c;
					state = sRD;
				}
				break;
			case sRD:
				if(c == '#' ) {buf[i+1]=0; break;} ;
				if(c>32){
					k += c;
				}else
					state = sLFV;
				break;
			case sLFV:
				if(c == '#' ) {buf[i+1]=0; break;} ;
				if(c < 33)
					break;
				else
					state = sRV;					
			case sRV:
				if(c == '#' && !inq) {buf[i+1]=0; break;} ;
				if(c > 32 || inq){
					if(!inq){
						if(c=='"')	
							inq = true;
						else
							v += c;
					}else{
						if(c=='"')	{
							inq = false;
							buf[i+1]=0; break;
						}else
							v += c;
					}
				//	printf("V = %s\n",v.c_str());
				}else{
					buf[i+1]=0; break;
				}
				break;
			default:
				free(buf);
				return -1;
			}
		}
		if(k!=""){
			//snprintf(buf,10240,"Config[%s] = %s;",k.c_str(),v.c_str());
			//rep(buf);
			data[k] = v; 
		}
		
	}
	snprintf(buf,10239,"Configuration loaded: %d keys in configuration",data.size());
	rep(buf);
	free(buf);
	fclose(f);
	return 1;
}

static ConfigClass Config;

int ConfI(const char *key) { return Config.ci(key); }
long ConfL(const char *key) { return Config.cl(key); }
float ConfF(const char *key){ return Config.cf(key); }
double ConfD(const char *key){ return Config.cd(key); }
const char* ConfC(const char * key){ return Config.cc(key); }
std::string ConfS(const char * key){ return Config.cs(key); }
int ConfLoad(const char *fname) { return Config.LoadFromFile(fname); }


