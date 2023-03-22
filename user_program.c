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
   int numChildren;
   if (argc < 2)
   {
        numChildren = 1;
   } 
   else 
   {
        numChildren = atoi(argv[1]);
   }

   printf(0,"Parent PID: %d\n", getpid());
   printf(0, "Child PIDs will be from %d to %d.\n", getpid()+1, getpid()+numChildren);
   
   int y;
   int count = 0;
   int printOutPriority = 0;
   for(y = getpid() + 1; y < getpid() + numChildren + 1; y++)
   {
	if(count % 2 == 0)
	{
	    printOutPriority = 0;
	}
	else
	{
	    printOutPriority = 2;
	}
	printf(0, "Child PID %d has priority %d.\n", y, printOutPriority);
	count++;
   }
   
   int i;
   int child_ids[numChildren];
   int priorityValue = 0;
   for (i = 0; i < numChildren; i++)
   {
	if(i % 2 == 0)
	{
	   priorityValue = 0;
	}
	else
	{
	   priorityValue = 2;
	}
	child_ids[i] = fork();
        if (child_ids[i] < 0)
        {
            printf(1, "fork error\n");
        } 
        else if (child_ids[i]  == 0)
        {
            //printf(1, "Child Pid: %d\n", getpid());
	    //printf(1, "Priority value: %d\n", priorityValue);
	    // Sleep immediately after the child is created
	    // to allow for other children to get created
            sleep(10);
	    int counter = 0;
	    int x;
	    for(x = 0; x < 1000000000; x++)
	    {
		counter++;
		if(x % 10000 == 0)
		{
		    renice(priorityValue, getpid());
		    //sleep(100);
		}
	    }
	    // Use a really long for loop
            exit();
        } 
   }

   //ps();

   // Only run by parent, waits for all children to exit.
   int pidFromWait;
   int j;
   for(j = 0; j < numChildren; j++)
   {
	pidFromWait = wait();
	printf(0, "Child exited %d\n", pidFromWait);
   }

   exit();
}
