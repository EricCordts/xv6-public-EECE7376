#include "types.h"
#include "user.h"
#include "stat.h"
#include "fcntl.h"

int
main(int argc, char *argv[])
{   
   int numChildren;
   if (argc < 2)
   {
        printf(0, "Error in usage, need a number of children processes to create (which must be a multiple of 3)\n");
	exit();
   } 
   else if(atoi(argv[1]) % 3 != 0)
   {
	printf(0, "Input needs to be a multiple of 3\n");
	exit();
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
	if(count % 3 == 0)
	{
	    printOutPriority = 0;
	}
	else if(count % 3 == 1)
	{
	    printOutPriority = 1;
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
	if(i % 3 == 0)
	{
	   priorityValue = 0;
	}
	else if(i % 3 == 1)
	{
	   priorityValue = 1;
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
        else if (child_ids[i] == 0)
        {
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
		}
	    }
	    // Use a long for loop
            exit();
        } 
   }

   // Only run by parent, waits for all children to exit.
   int pidFromWait;
   int j;
   for(j = 0; j < numChildren; j++)
   {
	pidFromWait = wait();
	printf(0, "Child PID %d exited\n", pidFromWait);
   }

   exit();
}
