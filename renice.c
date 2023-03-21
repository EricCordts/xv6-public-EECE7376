#include "types.h"
#include "user.h"

int
main(int argc, char *argv[])
{
   if(argc != 3)
   {
       printf(2, "renice usage...\n");
       exit();
   }

   renice(atoi(argv[1]), atoi(argv[2]));
	
   exit();
}
