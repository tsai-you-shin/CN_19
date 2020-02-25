CC = g++
OPENCV =  `pkg-config --cflags --libs opencv`

RECEIVER = receiver.cpp
SENDER = sender.cpp
AGENT = agent.cpp
RECV = receiver
SEND = sender
AGNT = agent

all: sender receiver agent

sender: $(SENDER)
	$(CC) $(SENDER) -o $(SEND)  $(OPENCV)
receiver: $(RECEIVER)
	$(CC) $(RECEIVER) -o $(RECV)  $(OPENCV)

agent: $(AGENT)
	$(CC) $(AGENT) -o $(AGNT)  $(OPENCV)

.PHONY: clean

clean:
	rm $(SEND) $(RECV) $(AGNT)
