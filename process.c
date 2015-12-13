/*
	Developer: Stylianos Rousoglou
	Name of file: process.c

	Completes the implementations of the shell Bsh;
	It executes built-in commands, handles redirection, piping, 
	backgrounding, local variables, and errors 

*/

#define _GNU_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "/c/cs323/Hwk5/getLine.h"
#include "/c/cs323/Hwk5/parse.h"
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

//Sets ? variable and prints pipe error
#define WARN(status, cause) set(status), perror(cause);
//Exits with status mandated by the spec 
#define exitStatus(s) WIFEXITED(s) ? WEXITSTATUS(s) : 128+WTERMSIG(s)

#define OK (0) 		//Status for success
#define FAIL (1)	//Status for failure

//Function definitions 
int	process (CMD *cmdList); 		//Starting point for root of tree
void	specialCases(CMD *cmdList);		//Handles special & and ; cases																
int 	simple(CMD *cmdList); 			//Executes a simple command
int 	Error(CMD* cmdList);			//Sets environment variable to errno
void 	extractLocals(CMD *cmdList);		//Extracts local variables
int 	toPipe(CMD *cmdList);			//Pipes from left to right child
void 	redirectIn(CMD *cmdList);		//Redirects stdin of anything
int 	redirectOut(CMD *cmdList);		//Redirects stdout of anything
void 	backG(CMD *cmdList);			//Runs cmdList in a subshell

//Sets environment variable ? to status
void set(int status){
	char num[10];
	sprintf(num, "%d", status);
	setenv("?", num, 1);
}

//Reaps all zombie processes, printing their pid and exit status
void reap(){
	int pid, status;
	while((pid = waitpid(-1, &status, WNOHANG))>0)
		fprintf(stderr, "Completed: %d (%d)\n", pid, exitStatus(status)); 
}

//Runs cmdList in a subshell
void backG(CMD *cmdList){
	int pid;
	if((pid = fork())<0){			//Handle fork error
		WARN(errno, "Backgrounded process");
		return;
	} if(pid == 0){
		exit(process(cmdList));		//Background processes = 0 status
	} else fprintf(stderr, "Backgrounded: %d\n", pid);
}

//Function called from mainBsh.c, handles different types of tree cmdList by 
//calling different subroutines, or recursively calling itself
int process (CMD *cmdList){
	reap();								//Reap zombies
	int pid, status = 0;
	if(cmdList->type==SIMPLE){					//Simple 
		status = simple(cmdList);
	} else if(cmdList->type==SUBCMD){				//Stage
		if((pid = fork())<0){
			set((status=errno));
			perror("pipe2"); 
			return status;
		} if(pid == 0){
			redirectIn(cmdList);
			redirectOut(cmdList);
			status = process(cmdList->left);
			exit(status);
		} else{
			signal(SIGINT, SIG_IGN);
			waitpid(pid, &status, 0);
			signal(SIGINT, SIG_DFL);
			status = exitStatus(status);
		}
	} else if(cmdList->type==PIPE){					//Pipeline
		status = toPipe(cmdList);
	} else if(cmdList->type==SEP_AND){				// && case								
		if(!(status = process(cmdList->left))) 		
		status = process(cmdList->right);
	} else if(cmdList->type==SEP_OR){ 				// || case
		if((status = process(cmdList->left)))
		status = process(cmdList->right);
	} else if(cmdList->type==SEP_END){				// ; case
		status = process(cmdList->left);		
		if(cmdList->right) status = process(cmdList->right);                    
	} else if(cmdList->type==SEP_BG){				// & case
		specialCases(cmdList);
		if (cmdList->right) status = process(cmdList->right);
	} set(status); 
	return status;
}

//Treats the special case of A; B & and A& B&
void specialCases(CMD *cmdList){
	if((cmdList->left->type == SEP_END || cmdList->left->type == SEP_BG)
						&& cmdList->left->right){ 
		CMD *new = cmdList->left->right; 
		cmdList->left->right = NULL;
		process(cmdList->left);
		backG(new);
	} else backG(cmdList->left);
}

//Redirects stdin to whatever cmdList->fromFile mandates
void redirectIn(CMD *cmdList){
	int status;
	if(cmdList->fromFile){
		int newfd = open(cmdList->fromFile, O_RDONLY);
		if(newfd==-1){ 
			status = errno;
			perror(cmdList->fromFile);
			exit(status);
		} else {
			dup2(newfd, 0);
			close(newfd);
		}
	}
}

///Redirects stdout to whatever cmdList->toFile mandates
int redirectOut(CMD *cmdList){
	int saved = 0, status;
	if(cmdList->toFile){
		int newfd;
		if(cmdList->toType==RED_OUT) 
			newfd = creat(cmdList->toFile, S_IRWXU);
		else newfd = open(cmdList->toFile, O_CREAT|O_WRONLY|O_APPEND, S_IRWXU);
		if(newfd==-1){ 
			status = errno;
			perror(cmdList->toFile);
			exit(status);
		} else {
			saved = dup(1);
			dup2(newfd, 1);
			close(newfd);
		}
	} return saved;
}

///Redirects stdout of built-in commands in parent shell
int redirectOutBsh(CMD *cmdList, int* status){
	int saved = 0;
	if(cmdList->toFile){
		int newfd;
		if(cmdList->toType==RED_OUT) 
			newfd = creat(cmdList->toFile, S_IRWXU);
		else newfd = open(cmdList->toFile, O_CREAT|O_WRONLY|O_APPEND, S_IRWXU);
		if(newfd==-1){ 
			*status = errno;
			WARN(*status, cmdList->toFile);
			return -1;
		} else {
			saved = dup(1);
			dup2(newfd, 1);
			close(newfd);
		}
	} return saved;
}

//Handles all types of built-ins and executables found in SIMPLE cmdList
int simple(CMD *cmdList){
	int status, pid, saved;
	if(!strcmp(cmdList->argv[0], "dirs")){		//Print CWD
		char buffer[PATH_MAX];
		if(cmdList->argc!=1){ 
			fprintf(stderr, "Usage: dirs\n");
			set(1);
			return FAIL;
		} if(!getcwd(buffer, PATH_MAX)) return Error(cmdList);
		saved = redirectOutBsh(cmdList, &status);
		if(saved==-1) return status;
		printf("%s\n", buffer);
		if(cmdList->toFile){
			dup2(saved, 1);
			close(saved);
		} set(0);
		return 0;
	} else if(!strcmp(cmdList->argv[0], "cd")) {	//Change dir
		saved = redirectOutBsh(cmdList, &status);
		if(saved==-1) return status;
		if(cmdList->argc==1){
			char* home = getenv("HOME");
			for(int i=0; i<cmdList->nLocal; i++){
				if(!strcmp(cmdList->locVar[i], "HOME"))
					home = cmdList->locVal[i];
			} 
			if(chdir(home)==-1) status = Error(cmdList);
		} else if(cmdList->argc==2){
			if(chdir(cmdList->argv[1])==-1){
				status = Error(cmdList);
			}
		} else{
			fprintf(stderr, "Usage: cd [directory]\n");
			set(1);
			status = 1;
		} set(status);
		if(cmdList->toFile){
			dup2(saved, 1);
			close(saved);
		} return status;
	} else if(!strcmp(cmdList->argv[0], "wait")){	//Wait for all children 
		set(0);
		if(cmdList->argc!=1){
			perror("Usage: wait");
			set(1);
			return 1;
		}
		int newfd = 0;
		if(cmdList->toFile){
			if(cmdList->toType==RED_OUT) 
				newfd = creat(cmdList->toFile, S_IRWXU);
			else newfd = open(cmdList->toFile, O_CREAT|O_WRONLY|O_APPEND, S_IRWXU);
			if(newfd==-1){ 
				status = errno;
				perror(cmdList->toFile);
				return status;
			} 
		} signal(SIGINT, SIG_IGN);
		while ((pid = waitpid(-1, &status, 0))>0) {
			fprintf(stderr, "Completed: %d (%d)\n", pid, exitStatus(status));
		} 	signal(SIGINT, SIG_DFL);
		if(cmdList->toFile) close(newfd);
		return 0;
	} else{
		if((pid = fork())<0){
			WARN((status =errno), "simple"); 
			return(status);
		} if(pid==0){
			redirectIn(cmdList);
			redirectOut(cmdList);
			extractLocals(cmdList);
			execvp(cmdList->argv[0], cmdList->argv);
			int status2 =errno;
			perror(cmdList->argv[0]); 
			exit(status2);
		} else{
			signal(SIGINT, SIG_IGN);
			waitpid(pid, &status,0);
			signal(SIGINT, SIG_DFL);
		}
	} return exitStatus(status);
}

//Sets environment variable to errno in case of an error
int Error(CMD* cmdList){
	int status = errno;
	WARN(status, cmdList->argv[1]);
	return status;
}

//Function that establishes a pipe between left and right childs of cmdList
int toPipe(CMD *cmdList){
	int fd[2];			//File descriptors for pipe
	int pid, pid2, status, 		//Process ID for child1, child2, status
		result1=0, result2=0;	//Store statuses of two children

	if(pipe(fd) || (pid = fork()) < 0){ 
		WARN((status=errno), "fork pipe");//Catching system failures
		return status;
	} else if(pid == 0){
		close(fd[0]);		//No reading from pipe
		dup2(fd[1], 1);			
		close(fd[1]);
		result1 = process(cmdList->left); //Return status from process
		exit(result1);
	} else close(fd[1]);		//No writing to pipe

	if((pid2 = fork())<0){
		waitpid(pid, NULL, 0);
		WARN((status=errno), "fork pipe");
		return status;
	} else if(pid2 == 0){
		dup2(fd[0], 0);
		close(fd[0]);
		result2 = process(cmdList->right);
		exit(result2);
	} else close(fd[0]);
	signal(SIGINT, SIG_IGN);
	waitpid(pid, &status, 0);
	int status1 = exitStatus(status);
	waitpid(pid2, &status, 0);
	signal(SIGINT, SIG_DFL);
	int status2 = exitStatus(status);
	if(status2) return status2;
	return status1;
}

//Extracts local variables of cmdList to current environment
void extractLocals(CMD *cmdList){
	for(int i=0; i<cmdList->nLocal; i++)
	    setenv(cmdList->locVar[i], cmdList->locVal[i], 1);
}
