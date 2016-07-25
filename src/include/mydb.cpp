#include <mysql.h>
#include <pthread.h>
#include "mydb.h"
#include "splog.h"
//#include "stats.h"


pthread_mutex_t	mydb_query_lock = PTHREAD_MUTEX_INITIALIZER;


DBRES db_query(MYSQL* conn, const char* sql, long* affect,long*last_id){
	if(affect!=NULL)
		*affect = 0;
	if(last_id!=NULL)
		*last_id = 0;
	DBRES ret = NULL;
	pthread_mutex_lock(&mydb_query_lock);
	//Stats().Upd(ST_DBQueries,1);
	if(mysql_query(conn, sql)){
		rep(mysql_error(conn));	
		rep(sql);	
	}
	else {//если запрос удался
		
		if(!(ret = mysql_store_result(conn)))
		{//если это запрос типа UPDATE/INSERT или на SELECT произошла ошибка
			
			if (mysql_errno(conn)) // это ошибка на SELECT?
			{
				rep(mysql_error(conn));
				rep(sql);
			}else{
				if(affect != NULL)
					*affect = mysql_affected_rows(conn);
				if(last_id != NULL)
					*last_id = mysql_insert_id(conn);
			}
			
		}
			
	}
	

	pthread_mutex_unlock(&mydb_query_lock);
	return ret;
}

