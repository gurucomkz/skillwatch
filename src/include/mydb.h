/*
		mydb.h


*/
#include <mysql.h>

typedef  MYSQL_RES* DBRES;

DBRES db_query(MYSQL* , const char* , long*affect = NULL, long*last_id = NULL);

