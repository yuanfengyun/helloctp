CFLAGS=-g

API=./api/v6.6.1_P1_20210406_api_tradeapi_se_linux64
#API=./api/v6.6.1_p1cp_20210406_api_tradeapi_se_linux64
INCLUDE=-I$(API)
LDIR=-L$(API)
LDFLAGS=-lthostmduserapi_se -lthosttraderapi_se

util.o:util.h util.cpp
	g++ -c util.cpp $(INCLUDE) $(CFLAGS)

position.o:position.h position.cpp
	g++ -c position.cpp $(INCLUDE) $(CFLAGS)

console.o:msg.h console.h console.cpp
	g++ -c console.cpp $(INCLUDE) $(CFLAGS)

mdspi.o:mdspi.h mdspi.cpp
	g++ -c mdspi.cpp $(INCLUDE) $(CFLAGS)

md_op.o:md_op.h md_op.cpp
	g++ -c md_op.cpp $(INCLUDE) $(CFLAGS)

tdspi.o:tdspi.h tdspi.cpp
	g++ -c tdspi.cpp $(INCLUDE) $(CFLAGS)

td_op.o:td_op.h td_op.cpp
	g++ -c td_op.cpp $(INCLUDE) $(CFLAGS)

main.o:main.cpp config.h msg.h util.h
	g++ -c main.cpp $(INCLUDE) $(CFLAGS)

main:mdspi.o tdspi.o main.o md_op.o td_op.o util.o console.o position.o
	g++ -o main main.o mdspi.o tdspi.o md_op.o td_op.o util.o console.o position.o $(LDFLAGS) -lpthread -lreadline $(LDIR) $(CFLAGS)

run:
	LD_LIBRARY_PATH=$(API) ./main

debug:
	LD_LIBRARY_PATH=$(API) gdb main

profill:
	LD_LIBRARY_PATH=$(API) valgrind ./main

clean:
	rm -rf *.o main

simnow:
	LD_LIBRARY_PATH=$(API) ./main tcp://180.168.146.187:10212 tcp://180.168.146.187:10202 9999 simnow_client_test 0000000000000000 123616 nanase4ever

simnow24:
	LD_LIBRARY_PATH=$(API) ./main tcp://180.168.146.187:10131 tcp://180.168.146.187:10130 9999 simnow_client_test 0000000000000000 123616 nanase4ever

xf_test:
	LD_LIBRARY_PATH=$(API) ./main tcp://183.2.185.160:41313 tcp://183.2.185.160:41305 6050 client_yungege_1.0 XFQHXFQHXFQHXFQH 10000066 a####1236543

xf:
	LD_LIBRARY_PATH=$(API) ./main tcp://180.168.146.187:10212 tcp://180.168.146.187:10202 9999 simnow_client_test 0000000000000000 123616 nanase4ever


