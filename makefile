all: client server

client: client.cpp HandleClient.o 
	g++ -o client client.cpp HandleClient.o -lpthread

server: server.cpp HandleServer.o global.cpp
	g++ -o server server.cpp  HandleServer.o global.cpp -lmysqlclient -lpthread

HandleServer.o:HandleServer.cpp
	g++ -c HandleServer.cpp -lmysqlclient -lpthread 

HandleClient.o:HandleClient.cpp
	g++ -c HandleClient.cpp

clean:
	rm server
	rm client
	rm *.o
