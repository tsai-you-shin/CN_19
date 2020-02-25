#include "opencv2/opencv.hpp"
#include <iostream>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h> 
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <signal.h>

#define BUFF_SIZE 1024
#define PACKET_SIZE 1024

using namespace std;
using namespace cv;

int handle_cmd(char *cmd, char *res, int remoteSocket);
void *display(void *ptr);
void *video_ctl(void *ptr);
void *download_file(void *ptr);
void *upload_file(void *ptr);

struct args{
	int socket;
	char path[512];
};

bool blk[1024] = {false};
bool blk_curr[1024] = {false};
fd_set master;
fd_set read_fd;

int main(int argc, char** argv){
	//ignore SIGPIPE
	signal(SIGPIPE,SIG_IGN);
	
   	int localSocket, remoteSocket, port = 4097;    
	
	struct sockaddr_in localAddr,remoteAddr;
          
    int addrLen = sizeof(struct sockaddr_in);  
	
	
	//Arguments - setup port number
	if ( argc != 2 ){
		fprintf(stderr,"the number of argument is wrong !");
		return 0;
	}else{
		port = atoi(argv[1]);
	}		
	
	//make directory
	struct stat dir;
	if (stat("./server_dir", &dir) == -1){
		mkdir("./server_dir", 0700);
	}
	
    //server initialization - socket(), bind(), listen()                 
    localSocket = socket(AF_INET , SOCK_STREAM , 0);
    
    if (localSocket == -1){
        fprintf(stderr,"socket() call failed!!");
        return 0;
    }

    localAddr.sin_family = AF_INET;
    localAddr.sin_addr.s_addr = INADDR_ANY;
    localAddr.sin_port = htons(port);
        
    if( bind(localSocket,(struct sockaddr *)&localAddr , sizeof(localAddr)) < 0) {
        fprintf(stderr,"Can't bind() socket");
        return 0;
    }    
    listen(localSocket , 3);
    
	
	//Use select() to deal with multiple connection
	
	FD_ZERO(&master);
	FD_ZERO(&read_fd);
	FD_SET(localSocket,&master);
	int num_fd = localSocket;
	fprintf(stderr,"Waiting for connections...\n");
	fprintf(stderr,"Server Port: %d \n",port);

	//multiserving
    while(1){    
        for(int i=0; i<=num_fd+1; i++){
        	blk_curr[i] = blk[i];
        }
		read_fd = master;
		if(select(num_fd+1,&read_fd,NULL,NULL,NULL) == -1){
			fprintf(stderr,"select failed\n");
			return 0;
		}
		
		for(int i = 0 ;i < num_fd+1 ; i++){
			if(FD_ISSET(i,&read_fd)){
				if(i == localSocket){
					//Handle new Connection
					remoteSocket = accept(localSocket, (struct sockaddr *)&remoteAddr, (socklen_t*)&addrLen); 
					if (remoteSocket < 0) {
            			fprintf(stderr,"accept failed!\n");
            			return 0;
        			}
					FD_SET(remoteSocket,&master);
					if(remoteSocket > num_fd)num_fd = remoteSocket;
					fprintf(stderr,"New connection accepted: %d\n",remoteSocket);
					
					int sent;
					char send_msg[BUFF_SIZE] = {};
        			strcpy(send_msg,"Welcome !!Server is online!!\n");
        			if( (sent = send(remoteSocket,send_msg,strlen(send_msg),0)) == -1) {
        				fprintf(stderr, "client leaving!\n");
						close(remoteSocket);
						FD_CLR(remoteSocket,&master);
						continue;
					} 
        			
        			FD_SET(remoteSocket,&master);
        			if(remoteSocket > num_fd) num_fd = remoteSocket;
        			
				}else{
					//Deal with data and command 
					if (blk[i] == true || blk_curr[i] == true ) continue;
					int recved, responed;
					char recv_msg[BUFF_SIZE] = {};
					char resp_msg[BUFF_SIZE] = {};
					bzero(recv_msg,sizeof(char)*BUFF_SIZE);
					if ((recved = recv(i, recv_msg, sizeof(char)*BUFF_SIZE, 0)) < 0){
						fprintf(stderr,"recv failed, with received bytes = %d\n",recved);
						close(i);
						FD_CLR(i,&master);
						break;
					} else if(recved == 0){
						fprintf(stderr, "client leaving!\n");
						close(i);
						FD_CLR(i,&master);
					} else{
						pthread_t job;						
						struct args argus;
						
						blk[i] = true;
						recv_msg[recved-1] = '\0';
						
						
						int work_case = handle_cmd(recv_msg,resp_msg,i);
						fprintf(stderr,"client %d with case %d\n", i, work_case);
						switch (work_case){
							//client leaving or wrong detect
							case -1:
								fprintf(stderr, "client leaving!\n");
								close(i);
								FD_CLR(i,&master);
								blk[i] = false;
								break;
							//wrong request command	
							case 0:
								if((responed = send(i,resp_msg,strlen(resp_msg),0)) == -1){
									fprintf(stderr, "client leaving!\n");
									close(i);
									FD_CLR(i,&master);
								}
								blk[i] = false;
								break;		
							//ls					
							case 1:
								if((responed = send(i,resp_msg,strlen(resp_msg),0)) == -1){
									fprintf(stderr, "client leaving!\n");
									close(i);
									FD_CLR(i,&master);
								}
								blk[i] = false;
								break;
							//put file
							case 2:
								argus.socket = i;
								strcpy(argus.path,resp_msg);
								pthread_create(&job, NULL, upload_file, (void*)&argus);
								break;
							//get file
							case 3:
								argus.socket = i;
								strcpy(argus.path,resp_msg);
								pthread_create(&job, NULL, download_file, (void*)&argus);
								break;
							//play mpg video	
							case 4:			
								argus.socket = i;
								strcpy(argus.path,resp_msg);
								pthread_create(&job, NULL, display, (void*)&argus);
			
								break;
						}
					}
					
				}
			}
		}		
    }
    return 0;
}

int handle_cmd(char *cmd, char *res, int remoteSocket){
	char action[8] = {};
	char file[256] = {}; 
	char path[512] = {};
	int n_read; 
	int tokens = sscanf(cmd, "%s %s", action, file);
	if (tokens >=0 && tokens <= 2  ){
		if(strcmp(action,"ls") == 0 ){
			DIR *d;
			struct dirent *dir;
			char tmp[255] = {};
			int cnt=0;
			d = opendir("./server_dir");
			if(d){
				while((dir = readdir(d)) != NULL){
					sprintf(tmp, "%s\n", dir->d_name);
					strcat(res,tmp);
					cnt++;
				}
				closedir(d);	
			}
			sprintf(tmp, "total %d files \n", cnt);
			strcat(res,tmp);
			return 1;
			
		} else if (strcmp(action,"put") == 0 ){
			struct stat buf;
			
			sprintf(path,"./server_dir/%s",file);
			if (send(remoteSocket,"OK\0",3,0) == -1)return -1;;
			strcpy(res,path);
			return 2;
			
			
			
		} else if ( strcmp(action,"get") == 0){
			struct stat buf;
			sprintf(path,"./server_dir/%s",file);
			if(stat(path, &buf) == -1){
				sprintf(res, "The '%s' doesn't exist.\n",file);
				return 0;
			}else{
			//start to send the file
				if (send(remoteSocket,"OK\n",3,0) == -1)return -1;;
				recv(remoteSocket,res,sizeof(char)*BUFF_SIZE,0);
				if(strncmp(res,"OK",2) == 0) fprintf(stderr, "client %d start to download\n",remoteSocket);
			}
			strcpy(res,path);
			return 3;
			
			
		} else if (strcmp(action,"play") == 0){
			struct stat buf;
			sprintf(path,"./server_dir/%s",file);
			if(stat(path, &buf) == -1){
				sprintf(res, "The '%s' doesn't exist.\n",file);
				return 0;
			}else { 
				int len = strlen(file);

				if (len >= 5 && (strncmp(".mpg",&file[len-4],4) != 0)){
					sprintf(res, "The '%s' is not a mpg file\n",file);
					return 0;
				} else if (len < 5){
					sprintf(res, "The '%s' is not a mpg file\n",file);
					return 0;
				}else {
					if(	send(remoteSocket,"OK\0",3,0) == -1)return -1;
				}
			}
			recv(remoteSocket,res,sizeof(char)*BUFF_SIZE,0);
			strcpy(res,path);
			return 4;
			
		} else {
			fprintf(stderr, "it shound not happened\n");
			return -1;
		}
	} else {
		fprintf(stderr, "it shound not happened\n");
		return -1;
	} 
	return 0;
}

void *upload_file(void *ptr){
	int socket = ((struct args*)ptr)->socket;
	char *path = ((struct args*)ptr)->path;	
	
	char data_buff[PACKET_SIZE] = {};
	int n_read;
	FILE *fp = fopen( path , "wb+");
	while(1){
		n_read = recv(socket,data_buff,sizeof(char)*PACKET_SIZE,0);
		//fprintf(stderr, "recv %d bytes from client\n",n_read);
		if(strcmp(data_buff,"@@@EOF@@@")==0){
			if(send(socket,"\nfinish uploading the file\n\0",28, 0) == -1){
				close(socket);
				FD_CLR(socket,&master);
			}
			break;
		}
		n_read = fwrite(data_buff, sizeof(char), n_read, fp);
		//fprintf(stderr, "fwrite %d bytes to file\n",n_read);
		if (send(socket,"ACK\0",4,0) == -1){
			close(socket);
			FD_CLR(socket,&master);
			break;
		}
	}		
	fclose(fp);	
	blk[socket] = false;
}


void *download_file(void *ptr){
	int socket = ((struct args*)ptr)->socket;
	char *path = ((struct args*)ptr)->path;	
	
	char data_buff[PACKET_SIZE] = {};
	int n_read;
	FILE *fp = fopen( path , "rb");
	while(!feof(fp)){
		n_read = fread(data_buff, sizeof(char), sizeof(data_buff), fp);
		//fprintf(stderr, "fread %d bytes from file\n",n_read);
		if(n_read == 0)break;
		if ((n_read = send(socket,data_buff,n_read,0)) == -1){
			blk[socket] = false;
			close(socket);
			FD_CLR(socket,&master);
			fclose(fp);
			return 0;
		}
		//fprintf(stderr, "send %d bytes to client\n",n_read);
		
		recv(socket, data_buff, sizeof(char)*BUFF_SIZE, 0);
		if(strncmp(data_buff,"ACK",3) != 0 ) fprintf(stderr,"client %d download error\n",socket);
		
	}
	fclose(fp);
	
	if (send(socket,"@@@EOF@@@\0", 10, 0) == -1){
		blk[socket] = false;
		close(socket);
		FD_CLR(socket,&master);
		return 0;
	}
	
	recv(socket,data_buff,sizeof(char)*BUFF_SIZE,0);
	sprintf(data_buff,"\nfinish downloading the file\n");
	if (send(socket,data_buff,strlen(data_buff),0)== -1){
		blk[socket] = false;
		close(socket);
		FD_CLR(socket,&master);
		return 0;
	}
	
	blk[socket] = false;
}



void *display(void *ptr){
	
	int socket = ((struct args*)ptr)->socket;
	char *path = ((struct args*)ptr)->path;	

	Mat imgServer;
	
	VideoCapture cap(path);
    fprintf(stderr,"client %d start display video \n",socket);
    	
	int width = cap.get(CV_CAP_PROP_FRAME_WIDTH);
    int height = cap.get(CV_CAP_PROP_FRAME_HEIGHT);
    char buff[PACKET_SIZE];
    

    sprintf(buff,"%d %d",height, width);
    
    if (send(socket,buff,strlen(buff),0) ==-1){
    	blk[socket] = false;
		close(socket);
		FD_CLR(socket,&master);
		return 0;
    }
    recv(socket,buff,sizeof(buff),0);

    imgServer = Mat::zeros(height,width, CV_8UC3);   
    
    if(!imgServer.isContinuous()){
         imgServer = imgServer.clone();
    }
    
    int sent = 0;
   
    
    int imgSize = imgServer.total() * imgServer.elemSize();
	int c=0;
	uchar buffer[imgSize];
 
	while(1){
			
		cap >> imgServer;
		if (imgServer.empty()) {
			fprintf(stderr,"client %d video end\n",socket);	
			break;
		}
		
		memcpy(buffer,imgServer.data, imgSize);
		if((sent = send(socket,buffer,imgSize,0)) == -1){
			blk[socket] = false;
			close(socket);
			FD_CLR(socket,&master);
			fprintf(stderr, "client %d leaving!\n",socket);
			return 0;
		}
		
		recv(socket,buff,PACKET_SIZE,0);
		if(strncmp(buff,"QUIT",4) == 0) break;
		else if(strcmp("ACK",buff)!=0)fprintf(stderr,"video ACK failed\n");
		
	}
	for(int i=0;i<imgSize;i++){buffer[i]= i;}
	
	if (send(socket,buffer,imgSize,0) == -1){
		blk[socket] = false;
		close(socket);
		FD_CLR(socket,&master);
		fprintf(stderr, "client %d leaving!\n",socket);
		return 0;
	}
	
	
	recv(socket,buff,PACKET_SIZE,0);
	cap.release();
		

	sprintf(buff,"-- End playing the video --\n");
	if (send(socket,buff,strlen(buff),0) == -1){
		blk[socket] = false;
		close(socket);
		FD_CLR(socket,&master);
		return 0;
	}
	
	
	
	blk[socket] = false;
}

