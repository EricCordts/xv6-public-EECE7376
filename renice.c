#include "types.h"
#include "user.h"

int
main(int argc, char *argv[])
{
   if(argc < 3)
   {
       printf(2, "renice usage...\n");
       exit();
   }
   
   int i;
   for(i = 2; i < argc; i++)
   {
       renice(atoi(argv[1]), atoi(argv[i]));
   }	
   exit();
}
