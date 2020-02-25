#include "opencv2/opencv.hpp"
#include <iostream>
#include <sys/socket.h> 
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h> 
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>


#define BUFF_SIZE 1024
#define PACKET_SIZE 1024

using namespace std;
using namespace cv;

int check_cmd(char *cmd, int localSocket);
int dir_control(int localSocket, char *action, char *file);
void display(void *ptr);

struct args{
	int socket;
	char *path;
};


int main(int argc , char *argv[])
{
	int port;
	char ip[16];
	//Arguments - setup port number
	if ( argc != 2 ){
		fprintf(stderr, "the number of argument is wrong !");
	}else{
		int i = 0;
		while(argv[1][i]!=':')i++;
		strncpy(ip,argv[1],i);
		ip[i] = '\0';
		port = atoi(&argv[1][i+1]);
		fprintf(stderr,"connect to %s:%d\n",ip,port);
	}		
	//make directory
	struct stat dir;
	if (stat("./client_dir", &dir) == -1){
		mkdir("./client_dir", 0700);
	}
                    
	//server initialization - socket(), connect()  
    int localSocket, recved;
    localSocket = socket(AF_INET , SOCK_STREAM , 0);

    if (localSocket == -1){
        fprintf(stderr,"Fail to create a socket.\n");
        return 0;
    }

    struct sockaddr_in info;
    bzero(&info,sizeof(info));
    info.sin_family = PF_INET;
    info.sin_addr.s_addr = inet_addr(ip);
    info.sin_port = htons(port);

    int err = connect(localSocket,(struct sockaddr *)&info,sizeof(info));
    if(err==-1){
        fprintf(stderr,"Connection error\n");
        return 0;
    }
    
      
	char recv_msg[BUFF_SIZE] = {};
	char cmd[BUFF_SIZE] = {};
	
	//receved messege
	while(1){
		bzero(recv_msg, sizeof(char)*BUFF_SIZE);
		if ((recved = recv(localSocket, recv_msg, sizeof(char)*BUFF_SIZE, 0)) < 0){
		    fprintf(stderr, "recv failed, with received bytes = %d\n", recved);            
	   
		}
		else if (recved == 0){
		    fprintf(stderr, "server offline\n");
		    break;
		  	
		} else{ 
			fprintf(stderr, "%s\n", recv_msg);
		        
			fgets(cmd,BUFF_SIZE,stdin);		    
			while(check_cmd(cmd,localSocket) == -1) fgets(cmd,BUFF_SIZE,stdin);			
			
		}        
    }
    
    fprintf(stderr,"close Socket\n");
    close(localSocket);
    return 0;
}

int check_cmd(char *cmd, int localSocket){
	char action[8] = {};
	char file[32] = {}; 
	char path[64] = {};
	char trash[4] = {};
	int sent;
	int tokens = sscanf(cmd, "%s %s %s", action, file, trash);
	//fprintf(stderr, "tokens: (%s) (%s) (%s)\n", action, file, trash);
	if( tokens == 1 && strcmp(action,"ls") == 0 ){
		//fprintf(stderr,"CLIENT: correct command format: %s\n", action);
	} else if (tokens == 2 ){
		if (strcmp(action,"put") == 0 ){
			struct stat buf;
			sprintf(path,"./client_dir/%s",file);
			if(stat(path, &buf) == -1){
				fprintf(stderr, "The '%s' doesn't exist.\n\n",file);
				return -1;
			}
			//else fprintf(stderr,"CLIENT: correct command format: %s %s\n", action, file);
			
		} else if ( strcmp(action,"get") == 0){
			//fprintf(stderr,"CLIENT: correct command format: %s %s\n", action, file);
			sprintf(path,"./client_dir/%s",file);
			//check file exist on server side
			
		} else if (strcmp(action,"play") == 0){
			sprintf(path,"./client_dir/%s",file);
			//fprintf(stderr,"CLIENT: correct command format: %s %s\n", action, file);
			
		} else {
			fprintf(stderr, "Command not found \n\n");
			return -1;
		}
	} else {
		fprintf(stderr, "Command not found \n\n");
		return -1;
	}
	sent = send(localSocket,cmd,strlen(cmd),0);
	
	return dir_control(localSocket, action, path);
}

int dir_control(int localSocket, char *action, char *path){
	int sent, recved, n_read; 
	char recv_buff[BUFF_SIZE] = {};
	char data_buff[PACKET_SIZE] = {};
	if ( strcmp(action,"ls") == 0 ){
		return 0;
	} else if ( strcmp(action,"put") == 0 ){
		//upload file to server
		int cnt =0;
		recv(localSocket, recv_buff, sizeof(char)*BUFF_SIZE, 0);
		fprintf(stderr, "put: %s\n",recv_buff);
		if(strncmp(recv_buff, "OK", 2) != 0) return -1;
		
		FILE* fp = fopen( path , "rb");
		while(!feof(fp)){
			n_read = fread(data_buff, sizeof(char), PACKET_SIZE, fp);
			//fprintf(stderr, "fread %d bytes from file\n",n_read);
			if(n_read == 0)break;
			n_read = send(localSocket,data_buff,n_read,0);
			//fprintf(stderr, "upload %d bytes to server\n",n_read);
			
			recv(localSocket, recv_buff, sizeof(char)*BUFF_SIZE, 0);
			if(strncmp(recv_buff,"ACK",3) != 0 ) fprintf(stderr,"upload error\n");
			cnt++;
			if((cnt%100) == 0)fprintf(stderr,"-");
			if((cnt%=2000) == 0)fprintf(stderr,"\n");
			
		}
		send(localSocket,"@@@EOF@@@\0", 10,0);
		fclose(fp);		
		
	} else if ( strcmp(action,"get") == 0 ){
		//downloadload file to server
		int cnt = 0;
		recv(localSocket, recv_buff, sizeof(char)*BUFF_SIZE, 0);
		fprintf(stderr, "get : %s\n",recv_buff);
		if(strncmp(recv_buff, "The", 3)==0) return -1;
		else send(localSocket,"OK\0", 3,0);
		
		FILE* fp = fopen( path , "wb+");
		if (!fp) fprintf(stderr, "fopen error");
		while(1){
			n_read = recv(localSocket,data_buff,sizeof(char)*PACKET_SIZE,0);
			//fprintf(stderr, "download %d bytes from server\n",n_read);
			
			if(strcmp(data_buff,"@@@EOF@@@")==0)break;
			n_read = fwrite(data_buff, sizeof(char), n_read, fp);
			//fprintf(stderr, "fwrite %d bytes to file\n",n_read);
			cnt++;
			if((cnt%100) == 0)fprintf(stderr,"-");
			if((cnt%=2000) == 0)fprintf(stderr,"\n");
			send(localSocket,"ACK\0", 4,0);
			
		}	
		send(localSocket,"OK\0", 3, 0);
		fclose(fp);
		
	} else if ( strcmp(action,"play") == 0 ){;
		recv(localSocket, recv_buff, sizeof(char)*BUFF_SIZE, 0);
		if(strncmp(recv_buff, "The", 3)==0) {
			fprintf(stderr, "%s\n",recv_buff);
			return -1;
		}
		send(localSocket,"ACK\0", 4,0);
			//play opencv		
		display(&localSocket);
		
	} else {fprintf(stderr, "error token ");}
	return 0;
}

void display(void *ptr){
	int socket = *(int *)ptr;
	Mat imgClient;
	char buff[32];
	
	
	int width, height;
	recv(socket, buff, sizeof(buff), 0);
	sscanf(buff, "%d %d", &height, &width);
	fprintf(stderr, "%d %d \n",width,height);
	send(socket, "OK\0", 3, 0);
	
	
    imgClient = Mat::zeros(height,width, CV_8UC3);   
    
    if(!imgClient.isContinuous()){
         imgClient = imgClient.clone();
    }
    int imgSize = imgClient.total() * imgClient.elemSize();
	uchar buffer[imgSize];
    uchar *iptr = imgClient.data;
    int recved = 0;
    int c = 0;
    bool status = true;
  	fprintf(stderr, "-- Start to play the video --\n");
  	fprintf(stderr, "Press any key to stop the video\n");
	
	while(status){	
		if((recved = recv(socket,buffer,imgSize,MSG_WAITALL)) < 0){
			fprintf(stderr, "revc frame error");
			break;
		}
			
		int i;
		for(i=0;i<100;i++){if (buffer[i]!=i)break;}
		if(i>=100)break;
		
		if (waitKey(33.3333) >= 0){	
			send(socket, "QUIT\0", 5, 0);	
			while(1){
				recved = recv(socket,buffer,imgSize,MSG_WAITALL);
				for(i=0;i<100;i++){if (buffer[i]!=i)break;}
				if(i>=100)break;
			}		
			break;	
		}else{
		
			send(socket, "ACK\0", 4, 0);	
		}
		
		memcpy(iptr,buffer, imgSize);
		imshow("Video", imgClient);
	}
	send(socket, "OK\0", 3, 0);

	
	destroyAllWindows();
	fprintf(stderr,"Quit\n");
}
