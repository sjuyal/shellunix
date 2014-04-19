#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <iostream>
#include <fcntl.h>
#include <signal.h>
#include <vector>

using namespace std;

char cwd[256];       // Variable to store current directory
char histpath[256];   // Var to store history file path
char configpath[256];  // Var to store config file path
bool sig=false;      // Variable to handle signal interrupt
int hisstart=0;    // Var handling start of history file

//Structure to store background process info
struct processlist{
	char procname[100];
	int procid;
};

vector<struct processlist *> procidlist;  // To store background processes

// Handler to kill all the zombie processes
void handleSIGCHLD(int signum){
	int stat;
	while(waitpid(-1, &stat, WNOHANG) > 0);
}	

// Function to kill all the process terminated already
void disproclist(){
	for(unsigned i=0;i<procidlist.size();i++){
		int rt=kill(procidlist[i]->procid,0);	
		if(rt!=0){
			procidlist.erase(procidlist.begin()+i);
		}
	}
}

// Function to hadle signal
void sjhandler(int val){
	sig=true;
}

// Function to remove space from the input string
void removeSpaces(char *src){
	char *dst=src;
	while (*src!='\0') {
		if (*src!=' ')
			*dst++ = *src;
		src++;
    	}
	*dst='\0';
}

// Function to display history
void dispHistory(){
	FILE *fp;
	int i=0;
	int limit=hisstart-1;
        fp=fopen(histpath, "r");
        if(fp==NULL){
		printf("Error in opening History File!!\n");
		return;
	}
	else{
		char line[500];
		while(fgets(line,500,fp)!= NULL){
			if(i<limit){   // Only values limited by shellrc file will be shown
				i++;
				continue;
			}
			printf(" %d %s",i+1,line);
			i++;
    		}
	}
	fclose(fp);
	return;
}

// Function to set the environment variables in both shellrc and bashrc
void setenviron(char * name,char * value){
	int destfd=open(configpath,O_WRONLY|O_CREAT|O_APPEND,0664);
	int input=strlen(name);
	write(destfd,name,input);
	write(destfd," ",1);
	input=strlen(value);
	write(destfd,value,input);
	write(destfd,"\n",1);
	close(destfd);
	int ret=setenv(name,value,1);
	if(ret!=0)
		printf("SetEnv Failed!\n");
}


//Function to add every command executed into bash_history file in current directory
void appendHistory(char * element){
	FILE *fp;
	char *genv,*hst;
	
	genv=getenv("HISTSIZE");
	hst=getenv("HISSTART");
	int permit;
	hisstart=atoi(hst);
	if(genv!=NULL)
		permit=atoi(genv);
	else
		printf("Unable to fetch required HISTSIZE\n");
	fp=fopen(histpath,"r");
	if(fp!=NULL){
		char line[500];
		int linecount=0;
		while(fgets(line,500,fp)!= NULL){
			linecount++;
	    	}
		if(linecount+1>permit)  // Check required if HISSTART variable in shellrc has to be changed if limit is crossed
		{
			hisstart+=1;
			char tmp[256];
			sprintf(tmp, "%d", hisstart);	
			setenviron((char *)"HISSTART",tmp);
		}	
	}
	// Write command to bash_history file
	int destfd=open(histpath,O_WRONLY|O_CREAT|O_APPEND,0664);
	int input=strlen(element);
	write(destfd,element,input);
	write(destfd,"\n",1);
	close(destfd);
}

// Function to prepare environment by setting all the bashrc parameters by shellrc parameters
void prepenv(){
	FILE *fp;
	char name[100];
	char value[100];
	char *tok;
        fp=fopen(configpath, "r");
        if(fp==NULL){
		printf("Error in opening Configuration File!!\n");
		return;
	}
	else{
		char line[500];
		while(fgets(line,500,fp)!= NULL){
			tok=strtok(line," ");	
			if(tok!=NULL) 
				strcpy(name,tok);
			tok=strtok(NULL," ");
			if(tok!=NULL) 
				strcpy(value,tok);
			value[strlen(value)-1]='\0';			
			int ret=setenv(name,value,1);
			if(ret!=0)
				printf("SetEnv Failed!\n");
    		}
	}
	fclose(fp);
	return;
}


// Function to implement Pipe
void doPipe(int count_pipe, char ** cmd,char proc_state,char *arr){
	int i,temp=0,k=-1;
	char *tok,*ftok;
	pid_t child_id;
	int totalfds=2*count_pipe;
	int fd[totalfds];
	int filefd[2];
    	int flag;
	int hisflag=0,redirflag=0,doubredirflag=0;
	char redir[2];
	char doubredir[3];
	char fname[100];
	char tmpcmd[200];
	
	//Creation of pipes
	for(i=0;i<totalfds;i+=2){
		int retval=pipe(fd+i);
		if(retval<0){
		   	printf("Error in creating Pipe No:%d",i);
		}
    	}
	
	pipe(filefd);
	strcpy(redir,">");
	strcpy(doubredir,">>");
	for(i=0;i<count_pipe+1;i++){
		char **subcomm=(char **)malloc(500*sizeof(char));
		
		strcpy(tmpcmd,cmd[i]);
		// Handling '>' and '>>'	
		if(i==count_pipe && (strstr(cmd[i],redir)!=NULL || strstr(cmd[i],doubredir)!=NULL)){
			if(strstr(cmd[i],doubredir)!=NULL){
				strcpy(redir,">>");	
				doubredirflag=1;			
			}		
			char tempvar[100];			
			redirflag=1;
			ftok=strtok(cmd[i],redir);
			if(ftok!=NULL){
				strcpy(tempvar,ftok);
			}	
			ftok=strtok(NULL,redir);
			removeSpaces(ftok);
			if(ftok!=NULL)
				strcpy(fname,ftok);
			if(strcmp(tempvar,"")!=0 && strcmp(fname,"")!=0){
				strcpy(tmpcmd,tempvar);
			}
		}
		tok=strtok(tmpcmd," ");	
		
		// Handling History
		if(strcmp(tok,"history")==0 && i==0)
			hisflag=1;
		//Handling Export
		if(strcmp(tok,"export")==0 && i==0){
			char *ntok;
			int tflag=0;
			char name[256],value[256];
			tok=strtok(NULL,"");
			if(tok!=NULL){
				ntok=strtok(tok,"=");
				removeSpaces(ntok);
				strcpy(name,ntok);
				if(ntok!=NULL){
					ntok=strtok(NULL,"=");
					if(ntok!=NULL){
						removeSpaces(ntok);
						strcpy(value,ntok);
						setenviron(name,value);					
					}
					else
						tflag=1;		
				}
				else
					tflag=1;
			}
			else
				tflag=1;
			if(tflag==1)
				printf("Invalid Export Command Syntax: Use[export <name>=<value>]\n");
			return;
		}

		//Handling Echo
		if(strcmp(tok,"echo")==0 && i==0){
			char *ntok;
			char name[256];
			tok=strtok(NULL,"");
			if(tok!=NULL){
				if(tok[0]=='$'){
					ntok=strtok(tok,"$");
					if(ntok!=NULL){
						removeSpaces(ntok);
						strcpy(name,ntok);
						//printf("name:<%s>",name);
						ntok=strtok(NULL,"$");
						if(ntok==NULL){
							char *genv;
							genv=getenv(name);
							if(genv!=NULL)
								printf("%s\n",genv);
							else
								printf("\n");
							return;
						}
					}
				}
			}
			printf("Invalid echo Command Syntax: Use[echo $<NAME>]\n");
			return;				
		}
		// Handling Exit
		if(strcmp(tok,"exit")==0){
			printf("Bye!!\n");
			exit(0);
		}
		k=-1;
		while(tok!=NULL) 
		{
			k++;
			subcomm[k]=(char *)malloc(50*sizeof(char));
			strcpy(subcomm[k],tok);
			tok=strtok(NULL," ");
		}
		if(k==0)
			subcomm[++k]=NULL;
		// Handling 'cd'
		if(strcmp(subcomm[0],"cd")==0){
			int r=chdir(subcomm[1]);
			if(r<0)
			{
				printf("Invalid Directory!!\n");
			}	
			return;
		}

		// Forking child process to execute a command		
		child_id=fork();
		
		// Logic to store background processes into a list of vector
		if(proc_state=='&' && child_id!=0){
			struct processlist *plist;
			plist=(struct processlist *)malloc(sizeof(struct processlist *));
			strcpy(plist->procname,arr);
			plist->procid=child_id;
			procidlist.push_back(plist);
		}
		// Logic to implement single and multiple pipes
		if(child_id==0){
			if(proc_state=='&'){
				setsid();			
			}
			if(temp!=0){
				if(dup2(fd[temp-2],0)<0){
		 	        	printf("Error in Dup!!");
	       			}
	    		}
			if(i<count_pipe){
			        if(dup2(fd[temp+1],1)<0){
			        	printf("Error in Dup!!");
		        	}
		    	}
			if(redirflag==1 && i==count_pipe){
				dup2(filefd[1],1);			
			}
			if(hisflag==1 && i==0){
				hisflag=0;
				for(k=0;k<totalfds;k++){
					close(fd[k]);
		    		}
				close(filefd[0]);
				close(filefd[1]);
				dispHistory();
				exit(0);
			}
			else{
				for(k=0;k<totalfds;k++){
					close(fd[k]);
		    		}
				close(filefd[0]);
				close(filefd[1]);
		    		int ret=execvp(subcomm[0],subcomm);
				if(ret<0){
					printf("%s: Unknown command\n",subcomm[0]);
    					exit(0);
				}
			}
		}
		else if(child_id<0){
		    printf("Error in creating Child Process!!");
		}
		temp+=2;
		free(subcomm);
	}
	int cnt=--i;
	for(i=0;i<totalfds;i++){
		close(fd[i]);
	}
	if(redirflag==1)
		close(filefd[1]);
	// Waiting for child process if command is not background
	if(proc_state!='&'){
		for(i=0;i<count_pipe+1;i++){			
			int rt=waitpid(0,&flag,0);
			for(unsigned j=0;j<procidlist.size();j++){
				if(procidlist[j]->procid==rt){
					procidlist.erase(procidlist.begin()+j);
					break;				
				}		
			}
	    	}
		signal(SIGCHLD,SIG_IGN);
		
	}
	// Outpur redirected to file incase redirection is used
	if(redirflag==1 && cnt==count_pipe){
		redirflag=0;
		char readbuffer[10000];
		char path[100];
		strcpy(path,"./");
		strcat(path,fname);
		int destfd;		
		if(doubredirflag==1)
			destfd=open(path,O_WRONLY|O_CREAT|O_APPEND,0664);
		else
			destfd=open(path,O_WRONLY|O_CREAT|O_TRUNC,0664);
		int input;
		while((input=read(filefd[0], readbuffer, sizeof(readbuffer)))>0)
		{	
			write(destfd,readbuffer,input);
		}
		close(destfd);			
	}
	close(filefd[0]);
	fflush(stdout);	
	
}

// Function to implement History
bool runfromhist(char * arr){
	int i=hisstart;
	unsigned int j;
	
	// Logic to implement ![0-9]*
	if(arr[1]>='0' && arr[1]<='9'){
		char ser[100];
		ser[0]=arr[1];
		for(j=2;j<strlen(arr);j++){
			if(arr[j]>='0' && arr[j]<='9')
				ser[j-1]=arr[j];
			else
				return false;	
		}
		ser[j-1]='\0';
		int search=atoi(ser);		
		FILE *fp=fopen(histpath,"r");
		char line[500];
		while(fgets(line,500,fp)!= NULL){
			if(i==search){
	      			strncpy(arr,line,strlen(line)-1);
				arr[strlen(line)-1] = '\0';
				return true;
			}
			i++;
    		}	
		fclose(fp);
	}
	else if(arr[1]=='-' && arr[2]>='0' && arr[2]<='9'){  // Logic to implement !-[0-9]*
		char ser[100];
		ser[0]=arr[2];
		for(j=3;j<strlen(arr);j++){
			if(arr[j]>='0' && arr[j]<='9')
				ser[j-2]=arr[j];
			else
				return false;	
		}
		ser[j-2]='\0';
		int search=atoi(ser);	
		FILE *fp=fopen(histpath,"r");
		char line[500];
		int linecount=0;
		while(fgets(line,500,fp)!= NULL){
			linecount++;
    		}
		fclose(fp);
		fp=fopen(histpath,"r");
		search=linecount-search+1;
		if(search<0){
			return false;
		}
		while(fgets(line,500,fp)!= NULL){
			if(i==search){
	      			strncpy(arr,line,strlen(line)-1);
				arr[strlen(line)-1] = '\0';
				return true;
			}
			i++;
    		}	
		fclose(fp);
	}
	else if(arr[1]=='!'){  // Logic to implement '!!'
		int search;	
		FILE *fp=fopen(histpath,"r");
		char line[500];
		int linecount=0;
		while(fgets(line,500,fp)!= NULL){
			linecount++;
    		}
		search=linecount;
		fclose(fp);
		fp=fopen(histpath,"r");
		while(fgets(line,500,fp)!= NULL){
			if(i==search){
	      			strncpy(arr,line,strlen(line)-1);
				arr[strlen(line)-1]='\0';
				return true;
			}
			i++;
    		}	
		fclose(fp);	
	}else if(arr[1]!='-' && (arr[1]<'0' || arr[1]>'9')){  // Logic to implement ![0-9a-zA-Z]*
		char ser[100];
		ser[0]=arr[1];
		for(j=2;j<strlen(arr);j++){
			ser[j-1]=arr[j];
		}
		ser[j-1]='\0';
		char line[500];
		int flag=0;
		FILE *fp=fopen(histpath,"r");
		while(fgets(line,500,fp)!= NULL){
			if(strncmp(line,ser,strlen(ser))==0){
				char *temp=(char *)malloc(100*sizeof(char));
	      			strncpy(temp,line,strlen(line)-1);
				temp[strlen(line)-1]='\0';
				strcpy(arr,temp);				
				flag=1;
			}
    		}	
		fclose(fp);
		if(flag==1)
			return true;
	}
	return false;
}


// Function to take input command from user and call appropriate functions accordingly
void myshell(int lenoftotarg,char * argument[]){
	string buf;
	int i=-1;
	int countpipe=0,flag=0;
	getcwd(cwd,sizeof(cwd));
	strcpy(histpath,cwd);
	strcat(histpath,"/bash_history");
	strcpy(configpath,cwd);
	strcat(configpath,"/shellrc");
	prepenv();
	printf("Bazinga:-$");
	while(1){
		signal(SIGINT, sjhandler);   // Signal to catch Ctrl+C
		i=-1;
		flag=0;
		countpipe=0;
		
		char *tok=(char *)malloc(500*sizeof(char));
		char *fgtok=(char *)malloc(500*sizeof(char));
		char *fgtoktmp=(char *)malloc(500*sizeof(char));
		char *arr=(char *)malloc(500*sizeof(char));
		char **cmd=(char **)malloc(500*sizeof(char));
		
		getline(cin,buf);	// Taking input from user
		if(sig){
			sig=false;
			for(unsigned j=0;j<procidlist.size();j++){
				kill(procidlist[j]->procid,1);		
			}
		}
		disproclist();
		strcpy(arr,buf.c_str());
		char proc_state=arr[strlen(arr)-1];
		
		// Command starting from ! (History)
		if(arr[0]=='!'){
			bool b=runfromhist(arr);
			if(!b){ 
				printf("Invalid Command !! Doesnot Match with History\n");
				flag=1;
			}
		}
		if(!sig)
			appendHistory(arr);	// Appending command to history file
		if(proc_state=='&'){
			arr[strlen(arr)-1]='\0';
		}
		char newarr[256];
		strcpy(newarr,arr);
		fgtok=strtok(newarr," ");
		// Logic to handle 'fg' command
		if(fgtok!=NULL && flag==0){
			if(strcmp(fgtok,"fg")==0){
				fgtok=strtok(NULL," ");
				if(fgtok==NULL)
					strcpy(fgtoktmp,"1");
				else
					strcpy(fgtoktmp,fgtok);
				bool match=false;
				for(unsigned j=0;j<procidlist.size();j++){
					if((j+1)==(unsigned)atoi(fgtoktmp)){
						strcpy(arr,procidlist[j]->procname);
						kill(procidlist[j]->procid,1);
						procidlist.erase(procidlist.begin()+j);
						match=true;
						break;			
					}		
				}
				if(!match){
					printf("No Such Job to Run!!\n");
					strcpy(arr,"");		
				}		
			}	
		}

		if(flag!=1)
			tok=strtok(arr,"|");

		// Logic to pause all the filter background process from running again

		for(unsigned j=0;j<procidlist.size();j++){
			if(strstr(procidlist[j]->procname,"wc")!=NULL || strstr(procidlist[j]->procname,"head")!=NULL || strstr(procidlist[j]->procname,"tail")!=NULL){
				kill(procidlist[j]->procid,SIGSTOP);
			}		
		}
		if(tok!=NULL && flag==0){
			if(strcmp(tok,"exit")==0){
				printf("Bye!!!\n");
				exit(0);
			}

			while(tok!=NULL)
			{
				i++;
				cmd[i]=(char *)malloc(50*sizeof(char));
				strcpy(cmd[i],tok);
				countpipe++;
				tok=strtok(NULL,"|");
			}
			
			signal(SIGCHLD,handleSIGCHLD);
			
			// Calling pipe function to run the command
			doPipe(countpipe-1,cmd,proc_state,arr);
			disproclist();
		}
		free(arr);
		free(cmd);
		free(tok);
		free(fgtoktmp);
		signal(SIGINT,sjhandler);
		printf("Bazinga:-$");
		// Logic to kill all the process if signal interupt is given
		if(sig){
			sig=false;
			for(unsigned j=0;j<procidlist.size();j++){
				kill(procidlist[j]->procid,1);		
			}
		}
	}
}
