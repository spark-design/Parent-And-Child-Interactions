#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <signal.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#define DEBUG
// declared globals
// these store the counts for SIGUSR1 and SIGUSR2
// they are not shared between parent and child, though
// both do have a copy
// I should default them to 0, but Unix-like OS's already
// default all unintialized GLOBALS to 0s
long lCnt, hCnt;
// we will also store a copy of the child's PID
pid_t cPid;
// here a few convenience functions to make the code easier
int checkCall(int val, const char *msg)
{
  if (val == -1)
    {
      if (errno == EINTR) return val; // we were interrupted, so
                                      // no reason to die here
      perror(msg);
      exit(EXIT_FAILURE);
    }
  return val;
}
int randInt(int low, int high)
{
  int rng = high - low;
  double scl = (((double) rand()) / ((double) RAND_MAX + 1));
  int offst = scl * rng;
  return low + offst;
}
// I have chosen to write the random number processing code here
// this makes it easier to do either the itimer or nanosleep
void processVals(void)
{
  pid_t pPid = getppid();
  int val = randInt(0,100);
  char out[1024];
#ifdef DEBUG
  sprintf(out, "Child: Value = %d\n",val);
  checkCall(write(STDOUT_FILENO, out,strlen(out)), "write");
#endif
  
  if (val > 74)
    {
      kill(pPid, SIGUSR2);
    }
  if (val < 26)
    {
      kill(pPid, SIGUSR1);
    }
  if (val > 48 && val < 52)
    {
      sprintf(out,"Child: OK, I am out!\n");
      checkCall(write(STDOUT_FILENO, out,strlen(out)), "write");
      exit(EXIT_SUCCESS);
    }
        
      /*time_t start, end;
      time(&start);  
      time(&end);
      double elapsed = difftime(end,start);
      printf("Elapsed time is %.2lf seconds", elapsed);*/
}
// now let's do our handlers
void pExit(void)
{
  char *out = "Parent is exiting ...\n";
  checkCall(write(STDOUT_FILENO, out, strlen(out)), "write");
}
void cExit(void)
{
  char *out = "Child is exiting ...\n";
  checkCall(write(STDOUT_FILENO, out, strlen(out)), "write");
}
void pSigHnd(int sig)
{
  time_t start, end;
  char buf[1024];
  pid_t p;
  // signals to handle in parent:
  // SIGCHLD
  // SIGINT
  // SIGUSR1
  // SIGUSR2
  if (sig == SIGCHLD)
    {
      while ((p = waitpid(-1, NULL, WNOHANG)) > 0);
      if (p == -1)
{
  if (errno == ECHILD)
    {
      sprintf(buf,"Parent: No more children are running\n");
      checkCall(write(STDOUT_FILENO, buf, strlen(buf)), "write");
      exit(EXIT_SUCCESS);
    } else
    {
      checkCall(-1, "waitpid");
    }
}
    }
  if (sig == SIGINT)
    {
      // We could pause the child here if we wanted to ...
      // that would look like this
      kill (cPid, SIGSTOP); // pause the child .. this will send SIGCHLD
                            // so block handling it using sa_mask in sigaction
      sprintf(buf,"\nDo you want to exit(Y/n)?");
      checkCall(write(STDOUT_FILENO, buf, strlen(buf)), "write");
      checkCall(read(STDIN_FILENO, buf, 1024), "read");
      kill(cPid, SIGCONT); // resume the child 
      if (buf[0] == 'Y' || buf[0] == 'y')
{
  kill(cPid, SIGTERM);
  exit(EXIT_SUCCESS);
}
      
      return;
      
    }
  if (sig == SIGUSR1)
    {
      lCnt++;
      // sprintf(buf, "The child has generated %ld values smaller than 26\n", lCnt);
      time(&end);
      double diff = difftime(end,start);
      if(diff < 100000000){
      sprintf(buf, "Warning! Line pressure is dangerously low! It has been %.2lf seconds since the previous warning\n", diff);
      }
      time(&start);
      checkCall(write(STDOUT_FILENO, buf, strlen(buf)), "write");
    }
  if (sig == SIGUSR2)
    {
      hCnt++;
      // sprintf(buf, "The child has generated %ld values greater than 74\n", hCnt);
      time(&end);
      double diff = difftime(end,start);
      if(diff < 100000000){
      sprintf(buf, "Warning! Line pressure is dangerously high! It has been %.2lf seconds since the previous warning\n", diff);
      }
      time(&start);
      checkCall(write(STDOUT_FILENO, buf, strlen(buf)), "write");
    }
}
void cSigHnd(int sig)
{
  // signals to handle in the child:
  // SIGTERM
  // SIGALRM
  if (sig == SIGTERM)
    {
      exit(EXIT_SUCCESS);
    }
  if (sig == SIGALRM)
    {
      processVals();
    }
}
// function main
int main(int argc, char *argv[])
{
  // declare my variables
  struct sigaction pSA;
  struct sigaction cSA;
  struct itimerval itmr; // this would be a struct timespec
                         // had I opted to use nanosleep
  sigset_t mask;
  // seed the random number generator --
  // Note: you seed the generator ONCE and only ONCE
  // not every time you create a random number.
  srand(time(NULL));
  // initialize all the values
  pSA.sa_handler = pSigHnd;
  sigemptyset(&pSA.sa_mask);
  pSA.sa_flags = 0;
  // add SIGINT, SIGCHLD to the parent signal handler's mask
  sigaddset(&pSA.sa_mask, SIGINT);
  sigaddset(&pSA.sa_mask, SIGCHLD);
  
  cSA.sa_handler = cSigHnd;
  sigemptyset(&cSA.sa_mask);
  cSA.sa_flags = 0;
  sigemptyset(&mask);
  sigaddset(&mask, SIGINT);
  // whether this is the itimerval or timespec
  // you must initialize all fields in the struct
  // not just the ones you are changing from 0
  itmr.it_interval.tv_sec = 3;
  itmr.it_interval.tv_usec = 0;
  itmr.it_value.tv_sec = 3;
  itmr.it_value.tv_usec = 0;
  
  // now, we need to change the disposition for
  // SIGCHLD before we create a child. This is a precaution.
  checkCall(sigaction(SIGCHLD, &pSA, NULL), "sigaction");
  // all of our setup is complete
  cPid = checkCall(fork(), "fork");
  switch (cPid)
    {
    case 0:
      // in the child
      // add the exit handler
      atexit(cExit);
      // mask SIGINT
      checkCall(sigprocmask(SIG_BLOCK,&mask,NULL), "sigprocmask");
      // setup up the signal handlers
      checkCall(sigaction(SIGTERM, &cSA, NULL), "sigaction");
      checkCall(sigaction(SIGALRM, &cSA, NULL), "sigaction");
      // start the itimer
      checkCall(setitimer(ITIMER_REAL, &itmr, NULL), "setitimer");
      while (1) pause(); // start the pause loop in the child
      break;
    default:
      // in the parent
      // add the exit handler
      atexit(pExit);
      // setup the signal handlers
      checkCall(sigaction(SIGINT, &pSA, NULL), "sigaction");
      checkCall(sigaction(SIGUSR1, &pSA, NULL), "sigaction");
      checkCall(sigaction(SIGUSR2, &pSA, NULL), "sigaction");
      while (1)
{
  char buf[1024];
  int numRead, numWrite;
  numRead = checkCall(read(STDIN_FILENO, buf, 1024), "read");
  if (numRead == -1) continue;
  while (1)
    {
      numWrite = checkCall(write(STDOUT_FILENO, buf, numRead), "write");
      if (numWrite == -1) continue; else break;
    }
}
    }
  
  exit (EXIT_SUCCESS);
}
