
util.o:util.h util.cpp
	g++ -c util.cpp -I./include/ctp

mdspi.o:mdspi.h mdspi.cpp
	g++ -c mdspi.cpp -I./include/ctp

md_op.o:md_op.h md_op.cpp
	g++ -c md_op.cpp -I./include/ctp

tdspi.o:tdspi.h tdspi.cpp
	g++ -c tdspi.cpp -I./include/ctp

td_op.o:td_op.h td_op.cpp
	g++ -c td_op.cpp -I./include/ctp

main.o:main.cpp
	g++ -c main.cpp -I./include/ctp

main:mdspi.o tdspi.o main.o md_op.o td_op.o util.o
	g++ -o main main.o mdspi.o tdspi.o md_op.o td_op.o util.o -lthostmduserapi_se -lthosttraderapi_se -lpthread -L./lib

run:
	LD_LIBRARY_PATH=./lib ./main

