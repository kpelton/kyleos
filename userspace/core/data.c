#include "house.h"
#include <mysql.h>

static void newuser(person * me) {

   MYSQL *conn;
   MYSQL_RES *res;
   MYSQL_ROW row;
   
   char *server = "127.0.0.1";
   char *user = "root";
   char *password = "chmod01";
   char *database = "house";
   

   conn = mysql_init(NULL);

      
   if (!mysql_real_connect(conn, server,user, password, database, 0, NULL, 0)) {
      fprintf(stderr, "%s\n", mysql_error(conn));
      exit(0);
   }



 mysql_close(conn);
}
