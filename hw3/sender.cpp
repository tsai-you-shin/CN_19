#include "send_and_recv.h"
#include "opencv2/opencv.hpp"
#include <sys/socket.h>
#include <vector>  
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

using namespace std; 
using namespace cv;

int main(int argc, char* argv[]){
	struct sockaddr_in sender, agent;
	char ip[2][64];
	char videoname[64];
	int port[2]; 
	int sendersocket;
	char local[6] = "local";
 	if(argc != 5){
		fprintf(stderr, "Usage: %s <agent ip> <server port> <agent port> <video file>\n", argv[0]);
		fprintf(stderr, "Example: ./sender local 8887 8888 video.mp4\n");
		exit(1);
	} else{
		setIP(ip[0], "local");
		setIP(ip[1], argv[1]);	
		sscanf(argv[2], "%d", &port[0]);
		sscanf(argv[3], "%d", &port[1]);
		sscanf(argv[4], "%s", videoname);	
	}
	
	/*Create UDP socket*/
	sendersocket = socket(PF_INET, SOCK_DGRAM, 0);
	
	/*setting of receive timeout*/
	struct timeval  tv; 
    tv.tv_sec = 0;
    tv.tv_usec = 90000;
    setsockopt(sendersocket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

	/*Configure settings in sender struct*/
	sender.sin_family = AF_INET;
    sender.sin_port = htons(port[0]);
    sender.sin_addr.s_addr = inet_addr(ip[0]);
    memset(sender.sin_zero, '\0', sizeof(sender.sin_zero));  
    /*bind socket*/
    bind(sendersocket,(struct sockaddr *)&sender,sizeof(sender));
    
    /*Configure settings in agent struct*/
    agent.sin_family = AF_INET;
    agent.sin_port = htons(port[1]);
    agent.sin_addr.s_addr = inet_addr(ip[1]);
    memset(agent.sin_zero, '\0', sizeof(agent.sin_zero));    
	socklen_t agent_size = sizeof(agent);
	
   
    
    printf("start transfering\n");
    printf("sender info: ip = %s port = %d and agent info: ip = %s port = %d\n",ip[0], port[0], ip[1], port[1]);
    
    /*structure to temporary restore receive/send data*/
    segment s_tmp;
    struct sockaddr_in tmp_addr;
    socklen_t tmp_size = sizeof(tmp_addr);
    char ipfrom[1024];
    int portfrom;
    
    int winsize = 1, threshold = 16;
	int segmentsize = sizeof(segment);
   	int sendIndex = 1;
   	int ackIndex = 0;
    
    Mat imgServer;
	VideoCapture cap(videoname);
	int width = cap.get(CV_CAP_PROP_FRAME_WIDTH);
    int height = cap.get(CV_CAP_PROP_FRAME_HEIGHT);
	
	memset(&s_tmp, 0, sizeof(s_tmp));
    
    sprintf(s_tmp.data,"%d %d",height, width);
    s_tmp.head.seqNumber = sendIndex;
	while(1){
		printf("send	data	#%d", sendIndex);
		printf(",	winSize = %d\n", winsize);	
		sendto(sendersocket, &s_tmp, segmentsize, 0, (struct sockaddr *)&agent,agent_size);
		segment tmp;		
		int len = recvfrom(sendersocket, &tmp, sizeof(tmp), 0, (struct sockaddr *)&tmp_addr, &tmp_size);
    	if(len > 0){
    		inet_ntop(AF_INET, &tmp_addr.sin_addr.s_addr, ipfrom, sizeof(ipfrom));
        	portfrom = ntohs(tmp_addr.sin_port);
        	if(strcmp(ipfrom, ip[1]) == 0 && portfrom == port[1]){
        		if(tmp.head.ack){
        			ackIndex = tmp.head.ackNumber;
        			printf("recv	ack		#%d\n", ackIndex);
        			winsize++;sendIndex++;
        			break;
        		}
        	}
    	}	
	}
	imgServer = Mat::zeros(height,width, CV_8UC3);   
    if(!imgServer.isContinuous()){
         imgServer = imgServer.clone();
    }
    int imgsize = imgServer.total() * imgServer.elemSize();
	uchar *dataptr = (uchar*)malloc(imgsize);
    vector <segment > segmentbuffer;
    vector <segment > waittosend;
    while(1){
    	while(waittosend.size() <= winsize){
    		cap >> imgServer;
    		if(imgServer.empty())break;
			int leftdata = imgsize;
    		segment tmpseg;
    		
    		memcpy(dataptr,imgServer.data,imgsize);

			uchar *tmpitr = dataptr;
    		while(leftdata > 0){
    			int sendsize = (DATASIZE < leftdata)?DATASIZE:leftdata;
				memcpy(tmpseg.data, tmpitr, sendsize);
    			setHeader(&tmpseg, sendsize, 0, 0, 0, 0, 0);
    			waittosend.insert(waittosend.begin(), tmpseg);
    			leftdata-=sendsize; tmpitr+=sendsize;
    		}
    	}
    	while(sendIndex <= ackIndex + winsize && winsize >= 0){	
			if(waittosend.size() == 0){
				if(ackIndex+1 != sendIndex)break;
    			printf("send	fin\n");
    			setHeader(&s_tmp, 0, sendIndex, 0, 1, 0, 0);
    			sendto(sendersocket, &s_tmp, segmentsize, 0, (struct sockaddr *)&agent,agent_size);
    			segmentbuffer.insert(segmentbuffer.begin(), s_tmp);
				winsize = -1;
				sendIndex++;
				break;
    		}else{
    			printf("send	data	#%d", sendIndex);
				printf(",	winSize = %d\n", winsize);			
				s_tmp = waittosend.back();
				waittosend.pop_back();
				setHeader(&s_tmp, -1, sendIndex, 0, 0, 0, 0);
				sendto(sendersocket, &s_tmp, segmentsize, 0, (struct sockaddr *)&agent,agent_size);
				segmentbuffer.insert(segmentbuffer.begin(), s_tmp);
				sendIndex++;
			}
			
    	}
    	
		while(ackIndex+1 < sendIndex){
			int len = recvfrom(sendersocket, &s_tmp, segmentsize, 0, (struct sockaddr *)&tmp_addr, &tmp_size);
			if(len < 0){
				if(errno == EWOULDBLOCK){
					threshold = (winsize/2 > 1)? winsize/2:1;
					winsize = 1;
					printf("time	out,	threshold = %d\n", threshold);
					
					
					for ( int i = segmentbuffer.size()-1; i>=0;i--){
						if(segmentbuffer[i].head.fin == 1 )
							printf("resnd	fin");
						else
							printf("resnd	data	#%d", segmentbuffer[i].head.seqNumber);
						printf(",	winSize = %d\n", winsize);
						sendto(sendersocket, &segmentbuffer[i], segmentsize, 0, (struct sockaddr *)&agent,agent_size);	
					}	
				}
				else{/*recv error*/;}
			}else{
				inet_ntop(AF_INET, &tmp_addr.sin_addr.s_addr, ipfrom, sizeof(ipfrom));
				portfrom = ntohs(tmp_addr.sin_port);
				if(strcmp(ipfrom, ip[1]) == 0 && portfrom == port[1]){
					if(s_tmp.head.ack){
						if(s_tmp.head.fin){
							printf("recv	finack\n");
							break;
						}
						int tmpIndex = s_tmp.head.ackNumber;
						printf("recv	ack	#%d\n", tmpIndex);
						if(tmpIndex >= ackIndex){
							ackIndex = tmpIndex;
			   			}
			   			/*pop all ack segment*/
			   			while(segmentbuffer.back().head.seqNumber <= ackIndex && !segmentbuffer.empty()){
			   				segmentbuffer.pop_back();
			   			}
						if(winsize < threshold){
							winsize++;
						}
					}else{/*ack error*/;}
				}else{/*ip-port error*/;}
			}
		}
		if(s_tmp.head.ack && s_tmp.head.fin)break;
		if(winsize >= threshold)winsize++;
   	}
}
