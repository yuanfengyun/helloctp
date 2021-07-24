CFLAGS=-g

util.o:util.h util.cpp
	g++ -c util.cpp -I./include/ctp $(CFLAGS)

console.o:msg.h console.h console.cpp
	g++ -c console.cpp -I./include/ctp $(CFLAGS)

mdspi.o:mdspi.h mdspi.cpp
	g++ -c mdspi.cpp -I./include/ctp $(CFLAGS)

md_op.o:md_op.h md_op.cpp
	g++ -c md_op.cpp -I./include/ctp $(CFLAGS)

tdspi.o:tdspi.h tdspi.cpp
	g++ -c tdspi.cpp -I./include/ctp $(CFLAGS)

td_op.o:td_op.h td_op.cpp
	g++ -c td_op.cpp -I./include/ctp $(CFLAGS)

main.o:main.cpp config.h msg.h util.h
	g++ -c main.cpp -I./include/ctp $(CFLAGS)

main:mdspi.o tdspi.o main.o md_op.o td_op.o util.o console.o
	g++ -o main main.o mdspi.o tdspi.o md_op.o td_op.o util.o console.o -lthostmduserapi_se -lthosttraderapi_se -lpthread -L./lib $(CFLAGS)

run:
	LD_LIBRARY_PATH=./lib ./main

debug:
	LD_LIBRARY_PATH=./lib gdb main

profill:
	LD_LIBRARY_PATH=./lib valgrind ./main

clean:
	rm -rf *.o main

simnow:
	LD_LIBRARY_PATH=./lib ./main tcp://180.168.146.187:10212 tcp://180.168.146.187:10202 9999 simnow_client_test 0000000000000000 123616 nanase4ever

simnow24:
	LD_LIBRARY_PATH=./lib ./main tcp://180.168.146.187:10131 tcp://180.168.146.187:10130 9999 simnow_client_test 0000000000000000 123616 nanase4ever

xf_test:
	LD_LIBRARY_PATH=./lib ./main tcp://180.168.146.187:10212 tcp://180.168.146.187:10202 9999 simnow_client_test 0000000000000000 123616 nanase4ever

xf:
	LD_LIBRARY_PATH=./lib ./main tcp://180.168.146.187:10212 tcp://180.168.146.187:10202 9999 simnow_client_test 0000000000000000 123616 nanase4ever


