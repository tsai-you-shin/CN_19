#include <string.h>
#include <stdio.h>
#include <stdio.h>
#include "opencv2/opencv.hpp"

#define DATASIZE 20000

using namespace cv;
typedef struct {
	int length;
	int seqNumber;
	int ackNumber;
	int fin;
	int syn;
	int ack;
} header;

typedef struct{
	header head;
	char data[DATASIZE];
} segment;

void setIP(char *dst, char *src) {
    if(strcmp(src, "0.0.0.0") == 0 || strcmp(src, "local") == 0
            || strcmp(src, "localhost")) {
        sscanf("127.0.0.1", "%s", dst);
    } else {
        sscanf(src, "%s", dst);
    }
}
void setHeader(segment *seg, int  len, int seqNumber, int ackNumber, int fin, int syn, int ack){
	seg->head.length = (len >=0)?len:seg->head.length;
	seg->head.seqNumber = (seqNumber >=0)?seqNumber:seg->head.seqNumber;
	seg->head.ackNumber = (ackNumber >=0)?ackNumber:seg->head.ackNumber;
	seg->head.fin = (fin >=0)?fin:seg->head.fin;
	seg->head.syn = (syn >=0)?syn:seg->head.syn;
	seg->head.ack = (ack >=0)?ack:seg->head.ack;
}
