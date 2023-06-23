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

#define BUFF_SZ sizeof ( int ) * 10

using namespace std;
const int SHMKEY1 = 4201069;
const int SHMKEY2 = 4201070;
const int PERMS = 0644;

//message buffer
struct msgbuffer {
   long mtype; //needed
   int pid; //pid of child
   int resource; //which resource to request
};

int main(int argc, char *argv[]) {

  struct msgbuffer buf;
  buf.mtype = 1;
  int msqid = 0;
  key_t key;
  
  if ((key = ftok("oss.cpp", 'B')) == -1) {
    perror("ftok");
    exit(1);
   }
   
  if ((msqid = msgget(key, PERMS)) == -1) {
    perror("msgget in child");
    exit(1);
  }
  
  //shared memory attaching
  int shmid1 = shmget ( SHMKEY1, BUFF_SZ, 0777);
  if ( shmid1 == -1 ) {
      fprintf(stderr,"Error in shmget ...\n");
      exit (1);
  }
  int shmid2 = shmget ( SHMKEY2, BUFF_SZ, 0777);
  if ( shmid2 == -1 ) {
      fprintf(stderr,"Error in shmget ...\n");
      exit (1);
  }
  int * clockSec = ( int * )( shmat ( shmid1, 0, 0 ) );
  int * clockNano = ( int * )( shmat ( shmid2, 0, 0 ) );
  int startTime = *clockSec;
  int startTimeNano = *clockNano;

  //Create random value between 1 and 10 to decide what the worker is going to do
  int randOpt = 0;
  int resCol, resAmt;
  msgbuffer rcvbuf;
  msgbuffer sndbuf;
  srand((unsigned) time(NULL) * getpid());
  
  while(true) {
  //cout << "worker test" << endl;
    if (startTimeNano < *clockNano + rand() % 5000) {
      startTimeNano = *clockNano + 50000;
      //cout << "worker inside timer" << endl;
      randOpt = 0 + rand() % 100;
   // cout << "RandOpt: " << randOpt << endl;
  //if randOpt is 1,2,3,4,5 the worker will request use of a random resource
      if (randOpt < 80) {
      //cout << "Opt1" << endl;
        resCol = 0 + rand() % 10;
    }
    //If randOpt is 6,7,8 the worker will send -1 back to oss telling it to release processes
      if (randOpt >= 81 && randOpt < 95) {
      //cout << "Opt2" << endl;
        if (*clockSec >= startTime + 1) {
          resCol = -1;
        }
      }
    //if randOpt is 9 or 10 the worker will send -2 back to oss telling it to release processes and terminate worker
      if (randOpt >= 95) {
      //cout << "Opt3" << endl;
        if (*clockSec >= startTime + 1) {
          resCol = -2;
          break;
        }
      }
      // now send a message back to our parent
           
      sndbuf.mtype = getppid();
      sndbuf.pid = getpid();
      sndbuf.resource = resCol;
      if (msgsnd(msqid, &sndbuf, 20, 0) == -1) {
        perror("msgsnd to parent failed\n");
        exit(1);
      }
      
      if ( msgrcv(msqid, &rcvbuf, 20, getpid(), 0) == -1) {
        perror("failed to receive message from parent\n");
        exit(1);
      }

    }
  }
  //Second msg send to catch the break option from the while loop
  sndbuf.mtype = getppid();
  sndbuf.pid = getpid();
  sndbuf.resource = resCol;
  if (msgsnd(msqid, &sndbuf, 20, 0) == -1) {
    perror("msgsnd to parent failed\n");
    exit(1);
  }
  if ( msgrcv(msqid, &rcvbuf, 20, getpid(), 0) == -1) {
    perror("failed to receive message from parent\n");
    exit(1);
  }

  shmdt(clockSec);
  shmdt(clockNano);
  
  return 0;
}