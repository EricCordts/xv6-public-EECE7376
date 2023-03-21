#include "types.h"
#include "user.h"
#include "stat.h"
#include "fcntl.h"

/*
https://www.geeksforgeeks.org/create-n-child-process-parent-process-using-fork-c/
need to have a ps method to print out priority as well
https://www.youtube.com/watch?v=hIXRrv-cBA4
https://www.youtube.com/watch?v=DZ0-GMtOtEc
allproc was modified to add default priority of 1 
*/
int
main(int argc, char *argv[])
{

    /*
    //base test program
    */
    printf(1,"Pid: %d\n", getpid());
    while(1);
    exit();
   /*
   int num;
   if (argc< 2)
   {
        num = 1;
   } 
   else 
   {
        num = atoi(argv[1]);
   }


   int i;
   int child_id;
   for (i=0; i <num; i++)
   {
        child_id = fork();
        if (child_id <0)
        {
            printf(1, "fork error\n");
        } 
        else if (child_id  ==0)
        {
            printf(1, "Child Pid: %d\n", getpid());
            if(child_id %2 == 0){
                renice(0, getpid());
            }
            sleep(50);
            printf(1, "Finished Child Pid: %d\n", getpid());
            exit();
        } 
        else 
        {
            printf(1, "parent pid: %d, child pid: %d\n", getpid(), child_id);
            wait();
        }
   }

   exit();
   */
}
