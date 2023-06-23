#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/msg.h>
#include <fstream>
#include <errno.h>

#define BUFF_SZ sizeof ( int ) * 10
const int SHMKEY1 = 4201069;
const int SHMKEY2 = 4201070;
const int PERMS = 0644;
using namespace std;

int shmid1;
int shmid2;
int msqid;
int *nano;
int *sec;



struct resources{

  //tracks how much of each resource each process is requesting
  int resource1; 
  int resource2;
  int resource3; 
  int resource4; 
  int resource5; 
  int resource6; 
  int resource7; 
  int resource8;
  int resource9; 
  int resource10;   
  pid_t pid; // process id of this child
  
};

//message buffer
struct msgbuffer {
   long mtype; //needed
   int pid; //pid of child
   int resource; //which resource to request
};

void myTimerHandler(int dummy) {
  
  shmctl( shmid1, IPC_RMID, NULL ); // Free shared memory segment shm_id
  shmctl( shmid2, IPC_RMID, NULL ); 
  if (msgctl(msqid, IPC_RMID, NULL) == -1) { //Free memory queue
      perror("msgctl");
      exit(1);
   }
  cout << "Oss has been running for 3 seconds! Freeing shared memory before exiting" << endl;
  cout << "Shared memory detached" << endl;
  kill(0, SIGKILL);
  exit(1);

}

static int setupinterrupt(void) { /* set up myhandler for SIGPROF */
  struct sigaction act;
  act.sa_handler = myTimerHandler;
  act.sa_flags = 0;
  return (sigemptyset(&act.sa_mask) || sigaction(SIGPROF, &act, NULL));
}

static int setupitimer(void) { /* set ITIMER_PROF for 60-second intervals */
  struct itimerval value;
  value.it_interval.tv_sec = 3;
  value.it_interval.tv_usec = 0;
  value.it_value = value.it_interval;
  return (setitimer(ITIMER_PROF, &value, NULL));  
}



void myHandler(int dummy) {
    shmctl( shmid1, IPC_RMID, NULL ); // Free shared memory segment shm_id
    shmctl( shmid2, IPC_RMID, NULL );
    if (msgctl(msqid, IPC_RMID, NULL) == -1) { //Free memory queue
      perror("msgctl");
      exit(1);
   } 
    cout << "Ctrl-C detected! Freeing shared memory before exiting" << endl;
    cout << "Shared memory detached" << endl;
    kill(0, SIGKILL);
    exit(1);
}

void initClock(int check) {

  
  if (check == 1) {
    shmid1 = shmget ( SHMKEY1, BUFF_SZ, 0777 | IPC_CREAT );
    if ( shmid1 == -1 ) {
      fprintf(stderr,"Error in shmget ...\n");
      exit (1);
    }
    shmid2 = shmget ( SHMKEY2, BUFF_SZ, 0777 | IPC_CREAT );
    if ( shmid2 == -1 ) {
      fprintf(stderr,"Error in shmget ...\n");
      exit (1);
    }
    cout << "Shared memory created" << endl;
    // Get the pointer to shared block
    sec = ( int * )( shmat ( shmid1, 0, 0 ) );
    nano = ( int * )( shmat ( shmid2, 0, 0 ) );
    *sec = 0;
    *nano = 0;
    return;
  }
//detaches shared memory
  else {
    shmdt(sec);    // Detach from the shared memory segment
    shmdt(nano);
    shmctl( shmid1, IPC_RMID, NULL ); // Free shared memory segment shm_id
    shmctl( shmid2, IPC_RMID, NULL ); 
    cout << "Shared memory detached" << endl;
    return;
  }
  
}

void incrementClock(int incNano, int incSec) {

  int * clockSec = ( int * )( shmat ( shmid1, 0, 0 ) );
  int * clockNano = ( int * )( shmat ( shmid2, 0, 0 ) );
  
  *clockNano = *clockNano + incNano;
  if (*clockNano >= 1000000000) {
    *clockNano = *clockNano - 1000000000;
    *clockSec = *clockSec + 1;
  }
  *clockSec = *clockSec + incSec;
  shmdt(clockSec);
  shmdt(clockNano);
  return;
  
}


int main(int argc, char *argv[])  {

//Ctrl-C handler
  signal(SIGINT, myHandler);
  if (setupinterrupt() == -1) {
    perror("Failed to set up handler for SIGPROF");
    return 1;  
  }
  if (setupitimer() == -1) {
    perror("Failed to set up the ITIMER_PROF interval timer");
    return 1;
  }


//Had issues with default selection in switch decided to have an argc catch at the beginning to insure that more than one option is given
  if (argc == 1) {
  
    cout << "Error! No parameters given, enter ./oss -h for how to operate this program" << endl;
    exit(1);

  }
  
  int opt, optCounter = 0;
  int status;
  string fValue;

//opt function to collect command line params  
  while ((opt = getopt ( argc, argv, "hr:f:" ) ) != -1) {
    
    optCounter++;
    
    switch(opt) {
    
      case 'h':
        cout << "Usage: ./oss -f logFileName" << endl;
        cout << "-f: The name of the file the program will write to for logging." << endl;
        exit(1);
        
        case 'f':
          fValue = optarg;
          while (fValue == "") {
            cout << "Error! No log file name given! Please provide a log file!" << endl;
            cin >> fValue;
          }
          break;
    } 
  }
    
//setup logfile
  ofstream logFile(fValue.c_str());
    
//Creates seed for random gen
    int randSec = 0, randNano = 0;
    srand((unsigned) time(NULL));
    initClock(1);
    
//create Message queue
  struct msgbuffer buf;
  key_t key;
  
  if ((key = ftok("oss.cpp", 'B')) == -1) {
      perror("ftok");
      exit(1);
   }
   
   if ((msqid = msgget(key, PERMS | IPC_CREAT)) == -1) {
      perror("msgget");
      exit(1);
   }

//creates arracy of structs to hold max possible processes
  struct resources resourceRequest[18];
  struct resources resourceAllocation[18];
  int amtResource [10] = {20, 20, 20, 20, 20, 20, 20, 20, 20, 20};
  //init all resource(request and allocation) values to 0 for better looking output
  for (int i = 0; i < 18; i++) {
    resourceRequest[i].resource1 = 0; 
    resourceRequest[i].resource2 = 0;
    resourceRequest[i].resource3 = 0; 
    resourceRequest[i].resource4 = 0; 
    resourceRequest[i].resource5 = 0; 
    resourceRequest[i].resource6 = 0; 
    resourceRequest[i].resource7 = 0; 
    resourceRequest[i].resource8 = 0;
    resourceRequest[i].resource9 = 0; 
    resourceRequest[i].resource10 = 0;  
    resourceRequest[i].pid = 0;
  }
  for (int i = 0; i < 18; i++) {
    resourceAllocation[i].resource1 = 0; 
    resourceAllocation[i].resource2 = 0;
    resourceAllocation[i].resource3 = 0; 
    resourceAllocation[i].resource4 = 0; 
    resourceAllocation[i].resource5 = 0; 
    resourceAllocation[i].resource6 = 0; 
    resourceAllocation[i].resource7 = 0; 
    resourceAllocation[i].resource8 = 0;
    resourceAllocation[i].resource9 = 0; 
    resourceAllocation[i].resource10 = 0;  
    resourceAllocation[i].pid = 0;
  }
  int * clockSec = ( int * )( shmat ( shmid1, 0, 0 ) );
  int * clockNano = ( int * )( shmat ( shmid2, 0, 0 ) );
  pid_t pid;
  int clockCheck = 0;
  int checkTime = 1;
  int numProcess = 0;
  int numProcessInSystem = 0;
  msgbuffer rcvbuf;
  
  //while loop to generate children until there are 40 total making sure to only have 18 processes running total at a time
  while (numProcess < 40) {
    if(numProcessInSystem < 18) {
      if (clockCheck <= *clockNano + rand() % 500) {
        char *args[]={"./worker", NULL};
        pid = fork();
        if(pid == 0) {
          execvp(args[0],args);
          cout << "Exec failed! Terminating" << endl;
          logFile << "Exec failed! Terminating" << endl;   
          exit(1);   
        }
        else {
          logFile << "Creating worker: " << pid << endl;
          numProcessInSystem++;
          //cout << numProcessInSystem << endl;
          for(int i = 0; i < 18; i++) {
            if(resourceRequest[i].pid == 0) {
              resourceRequest[i].pid = pid;
              resourceAllocation[i].pid = pid;
              break;
            }
          }
          clockCheck = *clockNano + 500;
        }
      }
    }
    
    //message receieve with no wait
    if(msgrcv(msqid, &rcvbuf, 20, 0, IPC_NOWAIT)==-1) {
      if(errno == ENOMSG) {
        //cout << "no message" << endl;
      }
      else {
        printf("Got an error from msgrcv\n");
        perror("msgrcv");
        exit(1); 
      }
    }
    else { 
      //cout << "check" << endl;
    //Request a resource from whatever the worker requested
      buf.mtype = rcvbuf.pid;
      //cout << rcvbuf.resource << endl;
      //if worker sends -2 that means it will be terminating after releasing resources

      //looks for a matching pids one from the resource table and one from the message  
      for( int i = 0; i < 18; i++) {
        //cout << resourceRequest[i].pid << " Worker PID: " << rcvbuf.pid << endl;
        if(resourceRequest[i].pid == rcvbuf.pid) {
          if (rcvbuf.resource == 0) {
            if(amtResource[0] > 0) {
              logFile << "Worker " << rcvbuf.pid << " requesting R" << rcvbuf.resource+1 << " at time " << *clockSec << ":" << *clockNano <<endl;
              resourceRequest[i].resource1++;
              amtResource[0]--;
            }
          }
          if (rcvbuf.resource == 1) {
            if(amtResource[1] > 0) {
              logFile << "Worker " << rcvbuf.pid << " requesting R" << rcvbuf.resource+1 << " at time " << *clockSec << ":" << *clockNano <<endl;
              resourceRequest[i].resource2++;
              amtResource[1]--;
            }
          }
          if (rcvbuf.resource == 2) {
            if(amtResource[2] > 0) {
              logFile << "Worker " << rcvbuf.pid << " requesting R" << rcvbuf.resource+1 << " at time " << *clockSec << ":" << *clockNano <<endl;
              resourceRequest[i].resource3++;
              amtResource[2]--;
            }
          }
          if (rcvbuf.resource == 3) {
            if(amtResource[3] > 0) {
              logFile << "Worker " << rcvbuf.pid << " requesting R" << rcvbuf.resource+1 << " at time " << *clockSec << ":" << *clockNano <<endl;
              resourceRequest[i].resource4++;
              amtResource[3]--;
            }
          }
          if (rcvbuf.resource == 4) {
            if(amtResource[4] > 0) {
              logFile << "Worker " << rcvbuf.pid << " requesting R" << rcvbuf.resource+1 << " at time " << *clockSec << ":" << *clockNano <<endl;
              resourceRequest[i].resource5++;
              amtResource[4]--;
            }
          }
          if (rcvbuf.resource == 5) {
            if(amtResource[5] > 0) {
              logFile << "Worker " << rcvbuf.pid << " requesting R" << rcvbuf.resource+1 << " at time " << *clockSec << ":" << *clockNano <<endl;
              resourceRequest[i].resource6++;
              amtResource[5]--;
            }
          }
          if (rcvbuf.resource == 6) {
            if(amtResource[6] > 0) {
              logFile << "Worker " << rcvbuf.pid << " requesting R" << rcvbuf.resource+1 << " at time " << *clockSec << ":" << *clockNano <<endl;
              resourceRequest[i].resource7++;
              amtResource[6]--;
            }
          }
          if (rcvbuf.resource == 7) {
            if(amtResource[7] > 0) {
              logFile << "Worker " << rcvbuf.pid << " requesting R" << rcvbuf.resource+1 << " at time " << *clockSec << ":" << *clockNano <<endl;
              resourceRequest[i].resource8++;
              amtResource[7]--;
            }
          }
          if (rcvbuf.resource == 8) {
            if(amtResource[8] > 0) {
              logFile << "Worker " << rcvbuf.pid << " requesting R" << rcvbuf.resource+1 << " at time " << *clockSec << ":" << *clockNano <<endl;
              resourceRequest[i].resource9++;
              amtResource[8]--;
            }
          }
          if (rcvbuf.resource == 9) {
            if(amtResource[9] > 0) {
              logFile << "Worker " << rcvbuf.pid << " requesting R" << rcvbuf.resource+1 << " at time " << *clockSec << ":" << *clockNano <<endl;
              resourceRequest[i].resource10++;
              amtResource[9]--;
            }
          }
        }
      }
      //if message from worker was a negative number indicates to release resources
      if (rcvbuf.resource < 0) {
        logFile << "Worker " << rcvbuf.pid << " releasing resources at time " << *clockSec << ":" << *clockNano <<endl;
        for( int i = 0; i < 18; i++) {
          //removes resources from request matrix
          if(resourceRequest[i].pid == buf.mtype) {
            resourceRequest[i].pid = 0;
            while(resourceRequest[i].resource1 != 0) {
              resourceRequest[i].resource1--;
              amtResource[0]++;
            }
            while(resourceRequest[i].resource2 != 0) {
              resourceRequest[i].resource2--;
              amtResource[1]++;
            }
            while(resourceRequest[i].resource3 != 0) {
              resourceRequest[i].resource3--;
              amtResource[2]++;
            }
            while(resourceRequest[i].resource4 != 0) {
              resourceRequest[i].resource4--;
              amtResource[3]++;
            }
            while(resourceRequest[i].resource5 != 0) {
              resourceRequest[i].resource5--;
              amtResource[4]++;
            }
            while(resourceRequest[i].resource6 != 0) {
              resourceRequest[i].resource6--;
              amtResource[5]++;
            }
            while(resourceRequest[i].resource7 != 0) {
              resourceRequest[i].resource7--;
              amtResource[6]++;
            }
            while(resourceRequest[i].resource8 != 0) {
              resourceRequest[i].resource8--;
              amtResource[7]++;
            }
            while(resourceRequest[i].resource9 != 0) {
              resourceRequest[i].resource9--;
              amtResource[8]++;
            }
            while(resourceRequest[i].resource10 != 0) {
              resourceRequest[i].resource10--;
              amtResource[9]++;
            }
          }
            //Removes resources from allocation matrix
          if(resourceAllocation[i].pid == buf.mtype) {
            resourceAllocation[i].pid = 0;
            while(resourceAllocation[i].resource1 != 0) {
              resourceAllocation[i].resource1--;
              amtResource[0]++;
            }
            while(resourceAllocation[i].resource2 != 0) {
              resourceAllocation[i].resource2--;
              amtResource[1]++;
            }
            while(resourceAllocation[i].resource3 != 0) {
              resourceAllocation[i].resource3--;
              amtResource[2]++;
            }
            while(resourceAllocation[i].resource4 != 0) {
              resourceAllocation[i].resource4--;
              amtResource[3]++;
            }
            while(resourceAllocation[i].resource5 != 0) {
              resourceAllocation[i].resource5--;
              amtResource[4]++;
            }
            while(resourceAllocation[i].resource6 != 0) {
              resourceAllocation[i].resource6--;
              amtResource[5]++;
            }
            while(resourceAllocation[i].resource7 != 0) {
              resourceAllocation[i].resource7--;
              amtResource[6]++;
            }
            while(resourceAllocation[i].resource8 != 0) {
              resourceAllocation[i].resource8--;
              amtResource[7]++;
            }
            while(resourceAllocation[i].resource9 != 0) {
              resourceAllocation[i].resource9--;
              amtResource[8]++;
            }
            while(resourceAllocation[i].resource10 != 0) {
              resourceAllocation[i].resource10--;
              amtResource[9]++;
            }
          }
        }
        //if the resource was -2 that means it will release and free up room for more processes
        if(rcvbuf.resource == -2) {
          cout << "and terminating" << endl;
          //cout << "--check" << endl;
          //cout << rcvbuf.pid << endl;
          numProcessInSystem--;
        }
      }
    //set send to any value cause it dont matter 
    //sends msg back the worker allowing to continue or close
    buf.resource = 0;
    if (msgsnd(msqid, &buf, 20, 0) == -1)
        perror("msgsnd");
    }
    
    //increments the clock by 50000ns
    incrementClock(50000, 0);
    
    //deadlock detection will check every simulated second
    if(checkTime == *clockSec) {
      logFile << "One second has passed running deadlock detection" << endl;
      checkTime++;
      bool noDeadlock = false;
      bool processGood = false;
      int badRows[18];
      int goodRows = 0;
      int work[10];
      int deadlockAmtResource[10];
      //copies of main struct just for the deadlock loop so as not mess with other code
      struct resources deadlockResourceRequest[18];
      struct resources deadlockResourceAllocation[18];
      for(int i = 0; i < 18; i++) {
        badRows[i] = -1;
        deadlockResourceRequest[i] = resourceRequest[i];
        deadlockResourceAllocation[i] = resourceAllocation[i]; 
      }
      copy(deadlockAmtResource, deadlockAmtResource + 10, amtResource);
      
      while(noDeadlock == false) {

        for(int i = 0; i < 18; i++) {
          //build work array to compare with availiable resources
          work[0] = deadlockResourceRequest[i].resource1;
          work[1] = deadlockResourceRequest[i].resource2;
          work[2] = deadlockResourceRequest[i].resource3;
          work[3] = deadlockResourceRequest[i].resource4;
          work[4] = deadlockResourceRequest[i].resource5;
          work[5] = deadlockResourceRequest[i].resource6;
          work[6] = deadlockResourceRequest[i].resource7;
          work[7] = deadlockResourceRequest[i].resource8;
          work[8] = deadlockResourceRequest[i].resource9;
          work[9] = deadlockResourceRequest[i].resource10;
          
          //compare work array to available resource array
          for(int j = 0; j < 10; j++) {
            if(deadlockAmtResource[j] >= work[j]) {
              processGood = true;
            }
            else {
              processGood = false;
              break;
            }
          }
//if the amount of resources total is less than or equal to the amount of resources requested by a process add the allocated resources to the work array and make that process 0
          if (processGood == true) {
            deadlockAmtResource[0] += deadlockResourceAllocation[i].resource1;
            deadlockAmtResource[1] += deadlockResourceAllocation[i].resource2;
            deadlockAmtResource[2] += deadlockResourceAllocation[i].resource3;
            deadlockAmtResource[3] += deadlockResourceAllocation[i].resource4;
            deadlockAmtResource[4] += deadlockResourceAllocation[i].resource5;
            deadlockAmtResource[5] += deadlockResourceAllocation[i].resource6;
            deadlockAmtResource[6] += deadlockResourceAllocation[i].resource7;
            deadlockAmtResource[7] += deadlockResourceAllocation[i].resource8;
            deadlockAmtResource[8] += deadlockResourceAllocation[i].resource9;
            deadlockAmtResource[9] += deadlockResourceAllocation[i].resource10;
            for(int i = 0; i < 18; i++) {
              deadlockResourceRequest[i].resource1 = 0; 
              deadlockResourceRequest[i].resource2 = 0;
              deadlockResourceRequest[i].resource3 = 0; 
              deadlockResourceRequest[i].resource4 = 0; 
              deadlockResourceRequest[i].resource5 = 0; 
              deadlockResourceRequest[i].resource6 = 0; 
              deadlockResourceRequest[i].resource7 = 0; 
              deadlockResourceRequest[i].resource8 = 0;
              deadlockResourceRequest[i].resource9 = 0; 
              deadlockResourceRequest[i].resource10 = 0;  
              deadlockResourceAllocation[i].resource1 = 0; 
              deadlockResourceAllocation[i].resource2 = 0;
              deadlockResourceAllocation[i].resource3 = 0; 
              deadlockResourceAllocation[i].resource4 = 0; 
              deadlockResourceAllocation[i].resource5 = 0; 
              deadlockResourceAllocation[i].resource6 = 0; 
              deadlockResourceAllocation[i].resource7 = 0; 
              deadlockResourceAllocation[i].resource8 = 0;
              deadlockResourceAllocation[i].resource9 = 0; 
              deadlockResourceAllocation[i].resource10 = 0;  
              deadlockResourceAllocation[i].pid = 0;
            }
            goodRows++;
          }
//else it will give the process num to an array to later be used for culling processes to fix deadlocking
          else {
            badRows[i] = i;
          }
          
        }
        //all processes pass detection therefor no deadlock
        if (goodRows == 18) {
          logFile << "No deadlock detected" << endl;
          noDeadlock = true;
        }
        //one or more processes did not pass protection so will simulate terminating one and redo the algrithm
        else {
          logFile << "Deadlock detected removing a process and attempting again" << endl;
          for(int i = 0; i < 18; i++) {
            if(badRows[i] >= 0) {
              deadlockResourceRequest[badRows[i]].resource1 = 0; 
              deadlockResourceRequest[badRows[i]].resource2 = 0;
              deadlockResourceRequest[badRows[i]].resource3 = 0; 
              deadlockResourceRequest[badRows[i]].resource4 = 0; 
              deadlockResourceRequest[badRows[i]].resource5 = 0; 
              deadlockResourceRequest[badRows[i]].resource6 = 0; 
              deadlockResourceRequest[badRows[i]].resource7 = 0; 
              deadlockResourceRequest[badRows[i]].resource8 = 0;
              deadlockResourceRequest[badRows[i]].resource9 = 0; 
              deadlockResourceRequest[badRows[i]].resource10 = 0;  
              deadlockResourceAllocation[badRows[i]].resource1 = 0; 
              deadlockResourceAllocation[badRows[i]].resource2 = 0;
              deadlockResourceAllocation[badRows[i]].resource3 = 0; 
              deadlockResourceAllocation[badRows[i]].resource4 = 0; 
              deadlockResourceAllocation[badRows[i]].resource5 = 0; 
              deadlockResourceAllocation[badRows[i]].resource6 = 0; 
              deadlockResourceAllocation[badRows[i]].resource7 = 0; 
              deadlockResourceAllocation[badRows[i]].resource8 = 0;
              deadlockResourceAllocation[badRows[i]].resource9 = 0; 
              deadlockResourceAllocation[badRows[i]].resource10 = 0;  
              deadlockResourceAllocation[badRows[i]].pid = 0;
              logFile << "Process P" << badRows[i] << " has been terminated" << endl;
              break;
            }
          }
        }
      }
      
          
      logFile << "Displaying Resource Allocation Matrix" << endl;  
      logFile << "    R1 R2 R3 R4 R5 R6 R7 R8 R9 R10" << endl;
      for (int i = 0; i < 18; i++) {
        logFile << "P" << i << ": " << resourceAllocation[i].resource1<<  "  " << resourceAllocation[i].resource2 << "  " << resourceAllocation[i].resource3 << "  " << resourceAllocation[i].resource4 << "  " << resourceAllocation[i].resource5 << "  " << resourceAllocation[i].resource6 << "  " << resourceAllocation[i].resource7 << "  " << resourceAllocation[i].resource8 << "  " << resourceAllocation[i].resource9 << "  " << resourceAllocation[i].resource10 << "  " << endl;
      }
    }
    
    //allocate requests
    for(int i = 0; i < 18; i++) {
      if(resourceRequest[i].pid == rcvbuf.pid) {
      resourceAllocation[i] = resourceRequest[i];
      resourceRequest[i].resource1 = 0; 
      resourceRequest[i].resource2 = 0;
      resourceRequest[i].resource3 = 0; 
      resourceRequest[i].resource4 = 0; 
      resourceRequest[i].resource5 = 0; 
      resourceRequest[i].resource6 = 0; 
      resourceRequest[i].resource7 = 0; 
      resourceRequest[i].resource8 = 0;
      resourceRequest[i].resource9 = 0; 
      resourceRequest[i].resource10 = 0;  
      resourceRequest[i].pid = 0;
      }
    }

  }

  
  cout << "Oss finished" << endl;
  logFile << "Oss finished" << endl;
  shmdt(clockSec);
  shmdt(clockNano);
  if (msgctl(msqid, IPC_RMID, NULL) == -1) {
      perror("msgctl");
      exit(1);
   }
  initClock(0);
  return 0;
  
}
