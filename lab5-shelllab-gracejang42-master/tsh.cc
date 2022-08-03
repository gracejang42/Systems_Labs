// 
// tsh - A tiny shell program with job control
// 
//Grace Horton grho5175 
//

using namespace std;

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <string>

#include "globals.h"
#include "jobs.h"
#include "helper-routines.h"

//
// Needed global variable definitions
//

static char prompt[] = "tsh> ";
int verbose = 0;
extern char **environ; //defined in lib, points to envp[0] (environment pointer)


//
// You need to implement the functions eval, builtin_cmd, do_bgfg,
// waitfg, sigchld_handler, sigstp_handler, sigint_handler
//
// The code below provides the "prototypes" for those functions
// so that earlier code can refer to them. You need to fill in the
// function bodies below.
// 

void eval(char *cmdline);
int builtin_cmd(char **argv);
void do_bgfg(char **argv);
void waitfg(pid_t pid);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);

//
// main - The shell's main routine 
//
int main(int argc, char *argv[]) 
{
  int emit_prompt = 1; // emit prompt (default)

  //
  // Redirect stderr to stdout (so that driver will get all output
  // on the pipe connected to stdout)
  //
  dup2(1, 2);

  /* Parse the command line */
  char c;
  while ((c = getopt(argc, argv, "hvp")) != EOF) {
    switch (c) {
    case 'h':             // print help message
      usage();
      break;
    case 'v':             // emit additional diagnostic info
      verbose = 1;
      break;
    case 'p':             // don't print a prompt
      emit_prompt = 0;  // handy for automatic testing
      break;
    default:
      usage();
    }
  }

  //
  // Install the signal handlers
  //

  //
  // These are the ones you will need to implement
  //
  Signal(SIGINT,  sigint_handler);   // ctrl-c
  Signal(SIGTSTP, sigtstp_handler);  // ctrl-z
  Signal(SIGCHLD, sigchld_handler);  // Terminated or stopped child

  //
  // This one provides a clean way to kill the shell
  //
  Signal(SIGQUIT, sigquit_handler); 

  //
  // Initialize the job list
  //
  initjobs(jobs);

  //
  // Execute the shell's read/eval loop
  //
  for(;;) {
    //
    // Read command line
    //
    if (emit_prompt) {
      printf("%s", prompt);
      fflush(stdout);
    }

    char cmdline[MAXLINE];

    if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin)) {
      app_error("fgets error");
    }
    //
    // End of file? (did user type ctrl-d?)
    //
    if (feof(stdin)) {
      fflush(stdout);
      exit(0);
    }

    //
    // Evaluate command line
    //
    eval(cmdline);
    fflush(stdout);
    fflush(stdout);
  } 

  exit(0); //control never reaches here
}
  
/////////////////////////////////////////////////////////////////////////////
//
// eval - Evaluate the command line that the user has just typed in
// 
// If the user has requested a built-in command (quit, jobs, bg or fg)
// then execute it immediately. Otherwise, fork a child process and
// run the job in the context of the child. If the job is running in
// the foreground, wait for it to terminate and then return.  Note:
// each child process must have a unique process group ID so that our
// background children don't receive SIGINT (SIGTSTP) from the kernel
// when we type ctrl-c (ctrl-z) at the keyboard.
//
void eval(char *cmdline) 
{
    //printf("Im in eval\n");
   char *argv[MAXARGS];
   pid_t pid;
   //sigset_t mask;
   //sigemptyset(&mask);
   //sigaddset(&mask,SIGCHLD); 
    
   int bg = parseline(cmdline, argv); 
    
  if (argv[0] == NULL)//ignore empty lines 
  {
      return; 
  }
  if (!builtin_cmd(argv)) {//if not a built-in command 	
	 if ((pid = fork()) == 0){ //forking a child if not a built in command
	  /* child runs user job */
	  setpgid(0,0); //setting the process group id to 0
	 	if (execve(argv[0], argv, environ) < 0){ //returns -1 on error, loads and runs argv[0] if OK
	 	printf("%s: Command not found\n", argv[0]);
	 	exit(0);
	 	}
	  /* parent waits for foreground job to terminate */
	 }
	 if (!bg) {// user did not type '&', job is foreground
	 addjob(jobs,pid,FG,cmdline);//adding foreground jobs
	 waitfg(pid); //waiting for all foregroung jobs to complete
	 }
	 else// job is background
	 {
	 addjob(jobs,pid,BG,cmdline);//adding background jobs
	 printf("[%d] (%d) %s",pid2jid(pid),pid,cmdline);
	 } 
 }
  return;
}


/////////////////////////////////////////////////////////////////////////////
//
// builtin_cmd - If the user has typed a built-in command then execute
// it immediately. The command name would be in argv[0] and
// is a C string. We've cast this to a C++ string type to simplify
// string comparisons; however, the do_bgfg routine will need 
// to use the argv array as well to look for a job number.
//
int builtin_cmd(char **argv) 
{
  string cmd(argv[0]);
  string s1("jobs");
  string s2("bg");
  string s3("fg");
  string s4("quit");

  if(cmd == s1){
      listjobs(jobs);
      return 1;
  }
  if(cmd == s2){// technically s2 and s3 can be combined but for simplicities sake...
      do_bgfg(argv);
      return 1;
  }
  if(cmd == s3){
      do_bgfg(argv);
      return 1;
  }
  if(cmd == s4){
      exit(0);
  }  

    return 0;//not a built in command
}

/////////////////////////////////////////////////////////////////////////////
//
// do_bgfg - Execute the builtin bg and fg commands
//
void do_bgfg(char **argv) 
{
  //printf("I'm in do_bgfg\n");  
  string cmd(argv[0]);  
  struct job_t *jobp=NULL;
  pid_t pid;
    
  /* Ignore command if no argument */
  if (argv[1] == NULL) {
    printf("%s command requires PID or %%jobid argument\n", argv[0]);
    return;
  }
    
  /* Parse the required PID or %JID arg */
  if (isdigit(argv[1][0])) {
    pid_t pid = atoi(argv[1]);
    if (!(jobp = getjobpid(jobs, pid))) {
      printf("(%d): No such process\n", pid);
      return;
    }
  }
  else if (argv[1][0] == '%') {
    int jid = atoi(&argv[1][1]);
    if (!(jobp = getjobjid(jobs, jid))) {
      printf("%s: No such job\n", argv[1]);
      return;
    }
  }	    
  else {
    printf("%s: argument must be a PID or %%jobid\n", argv[0]);
    return;
  }
   
    if(jobp!=NULL){//if job
        pid = jobp->pid;
        if((jobp->state)== ST){
            kill(-pid,SIGCONT);//if pid<0 kill sends sig to every process in process group |pid|
        }
        if(cmd == "bg"){// if cmd "bg" set state to background
            (jobp->state) = BG;
            printf("[%d] (%d) %s", jobp->jid, pid, jobp->cmdline);
        }
        else{// cmd is "fg" set state to foreground
            (jobp->state) = FG;
            waitfg(pid);//wait for foreground jobs
        }
    }
  return;
}

/////////////////////////////////////////////////////////////////////////////
//
// waitfg - Block until process pid is no longer the foreground process
//
void waitfg(pid_t pid)
{
  struct job_t *jp1;
  jp1 = getjobpid(jobs,pid);
    //while in the foreground 
  while(jp1!=NULL&&(jp1->state==FG))//we need to wait for the FG process to finish
	{
	 sleep(1);//sleep 1 sec, returns 0 if sleep time has elapsed..
	}
 return;
    
 //printf("Im at the end of waitfg\n");
}

/////////////////////////////////////////////////////////////////////////////
//
// Signal handlers
//


/////////////////////////////////////////////////////////////////////////////
//
// sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
//     a child job terminates (becomes a zombie), or stops because it
//     received a SIGSTOP or SIGTSTP signal. The handler reaps all
//     available zombie children, but doesn't wait for any other
//     currently running children to terminate.  
//
void sigchld_handler(int sig) 
{
 struct job_t *jp2;
 int status = -1;//set to pid value at the start:: status:value pointed to by statusp 
		pid_t pid;
				
		
 while((pid=waitpid(-1,&status,WNOHANG|WUNTRACED))>0) //returns pid of child if OK (always +), 0 or -1 on error 
 {
    //printf("Im in the sigchld loop\n");
	jp2 = getjobpid(jobs,pid);
	
	if(WIFEXITED(status)){   
        //printf("Im in WIFEXITED in sigchld while loop\n");
		deletejob(jobs,pid);//deleting jobs if exited normally
	}
	if(WIFSIGNALED(status)){
	   printf("Job [%d] (%d) terminated by signal 2\n",jp2->jid,jp2->pid);
	   deletejob(jobs,pid);	//deleting jobs terminated with signals
	}
	if(WIFSTOPPED(status))//returns true if the child that caused the return is stopped
	{
	printf("Job [%d] (%d) stopped by signal 20\n",jp2->jid,jp2->pid);
	jp2->state = 3; //changing the state of stopped jobs to ST i.e. 3
	}
	//printf("Inside loop end\n");			
 }
	//printf("Im outside the sigchld loop at return\n");	
 return;
}

/////////////////////////////////////////////////////////////////////////////
//
// sigint_handler - The kernel sends a SIGINT to the shell whenver the
//    user types ctrl-c at the keyboard.  Catch it and send it along
//    to the foreground job.  
//
void sigint_handler(int sig) 
{
   pid_t pid = fgpid(jobs);
	if(pid>0)//if parent 
	{
		kill(-pid,SIGINT); //sending SIGINT signal to the foreground process
	}//SIGINT - terminate 
  return;
}

/////////////////////////////////////////////////////////////////////////////
//
// sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
//     the user types ctrl-z at the keyboard. Catch it and suspend the
//     foreground job by sending it a SIGTSTP.  
//
void sigtstp_handler(int sig) 
{
    pid_t pid = fgpid(jobs);
	if(pid==0){//if child..
      return;
    }
	else
    {
	kill(-pid,SIGTSTP); //sending SIGSTP signal to all stopped process
	}//SIGTSTP: Stop until next SIGCONT
}

/*********************
 * End signal handlers
 *********************/


