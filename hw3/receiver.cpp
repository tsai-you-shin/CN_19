#include "send_and_recv.h"
#include "opencv2/opencv.hpp"
#include <sys/socket.h>
#include <vector>  
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

using namespace cv;
using namespace std; 

void flushdata(vector <segment > *recvseg, uchar *buffer){
	segment tmp;
	uchar *itr = buffer;
	while(!recvseg->empty()){
		tmp = recvseg->back();
		recvseg->pop_back();
		memcpy(itr, tmp.data, tmp.head.length);
		itr += tmp.head.length;
	}
}

int main(int argc, char* argv[]){
	struct sockaddr_in agent, receiver;
	char ip[2][64];
	int port[2]; 
	int receiversocket;
	char local[6] = "local";
 	if(argc != 4){
		fprintf(stderr, "Usage: %s <agent ip>  <agent port> <receiver port> \n", argv[0]);
		fprintf(stderr, "Example: ./sender local 8888 8889\n");
		exit(1);
	} else{
		setIP(ip[0], argv[1]);
		setIP(ip[1], local);	
		sscanf(argv[2], "%d", &port[0]);
		sscanf(argv[3], "%d", &port[1]);
	}
	
	/*Create UDP socket*/
	receiversocket = socket(PF_INET, SOCK_DGRAM, 0);

    /*Configure settings in agent struct*/
    agent.sin_family = AF_INET;
    agent.sin_port = htons(port[0]);
    agent.sin_addr.s_addr = inet_addr(ip[0]);
    memset(agent.sin_zero, '\0', sizeof(agent.sin_zero));    
	socklen_t agent_size = sizeof(agent);
	
    /*Configure settings in receiver struct*/
    receiver.sin_family = AF_INET;
    receiver.sin_port = htons(port[1]);
    receiver.sin_addr.s_addr = inet_addr(ip[1]);
    memset(receiver.sin_zero, '\0', sizeof(receiver.sin_zero)); 
    /*bind socket*/
    bind(receiversocket,(struct sockaddr *)&receiver,sizeof(receiver));
    
    printf("start transfering\n");
    printf("agent info: ip = %s port = %d and receiver info: ip = %s port = %d\n",ip[0], port[0], ip[1], port[1]);
    
    /*structure to temporary restore receive/send data*/
    segment s_tmp;
    struct sockaddr_in tmp_addr;
    socklen_t tmp_size = sizeof(tmp_addr);
    int segmentsize = sizeof(segment), nextack=1;
    int portfrom;
    char ipfrom[1000];
    /*opencv part*/
    Mat imgClient;
    int width, height, imgSize, buffersize;
	uchar *buff;
	
	vector <segment > recvseg;
	
    while(1){  
    	memset(&s_tmp, 0, sizeof(s_tmp));
        segmentsize = recvfrom(receiversocket, &s_tmp, sizeof(s_tmp), 0, (struct sockaddr *)&tmp_addr, &tmp_size);
        if(segmentsize > 0){
            inet_ntop(AF_INET, &tmp_addr.sin_addr.s_addr, ipfrom, sizeof(ipfrom));
            portfrom = ntohs(tmp_addr.sin_port);
            
            if(strcmp(ipfrom, ip[0]) == 0 && portfrom == port[0]){
            	if(s_tmp.head.seqNumber == nextack){
            		segment tmp;
					memset(&tmp, 0, sizeof(tmp));
            		if(s_tmp.head.fin == 1){
						printf("recv	fin\n");
						printf("send	finack\n");
						setHeader(&tmp, 0, nextack, 0, 1, 0, 1);
						sendto(receiversocket, &tmp, segmentsize, 0, (struct sockaddr *)&agent,agent_size);
						break;
					} else if (nextack == 1){
						printf("recv	data	#%d\n", nextack);
						sscanf(s_tmp.data, "%d %d", &height, &width);
						imgClient = Mat::zeros(height,width, CV_8UC3);   
    
   						if(!imgClient.isContinuous()){
         					imgClient = imgClient.clone();
    					}
    					imgSize = imgClient.total() * imgClient.elemSize();
    					buff = (uchar*)malloc(imgSize);
						buffersize = imgSize/DATASIZE+1;

    					printf("send	ack	#%d\n", nextack);
						setHeader(&tmp, 0, 0,nextack, 0, 0, 1);
						sendto(receiversocket, &tmp, segmentsize, 0, (struct sockaddr *)&agent,agent_size);
    					nextack++;
					} else{	
						printf("recv	data	#%d\n", nextack);
						recvseg.insert(recvseg.begin(), s_tmp);
						printf("send	ack	#%d\n", nextack);
						setHeader(&tmp, 0, 0, nextack, 0, 0, 1);
						sendto(receiversocket, &tmp, segmentsize, 0, (struct sockaddr *)&agent,agent_size);
						nextack++;

						if(recvseg.size() == buffersize){					
							printf("flush\n");
							flushdata(&recvseg, buff);
							memcpy(imgClient.data, buff, imgSize);
							imshow("Video", imgClient);
							char c = (char)waitKey(100);
							if(c >= 0)
								break;
						} 
						
					}

	            }else{
            		segment tmp;
					memset(&tmp, 0, sizeof(tmp));
            		printf("drop	data	#%d\n", s_tmp.head.seqNumber);
            		printf("send	ack	#%d\n", nextack-1);
            		setHeader(&tmp, 0, 0, nextack-1, 0, 0, 1);
            		sendto(receiversocket, &tmp, segmentsize, 0, (struct sockaddr *)&agent,agent_size);
            	}
            }else{/*agent ip port error*/;}
        }
    }
    
}
