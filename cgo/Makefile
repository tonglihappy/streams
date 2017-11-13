CC = g++
AR = ar

CFLAGS = -O3 -DNDEBUG -Wall -c

SRCS = $(wildcard ./src/*.cpp)
OBJS = $(patsubst %.cpp, %.o, $(SRCS))
CSRCS = $(wildcard ./src/*.c)

HEADER_PATH = -I./include/
LIB_PATH = -L./lib/

LIBS = -lavformat -lavcodec -lswscale -lavutil -lavfilter -lswresample -lavdevice -lpostproc -lx264 -ldl -lz -lm -lrt -lpthread

$(OBJS):
	$(CC) $(CFLAGS) $(SRCS) $(HEADER_PATH)
	$(CC) $(CFLAGS) $(CSRCS) $(HEADER_PATH)
	$(AR) cr libffmpeg.a *.o
	mv libffmpeg.a ./lib
	rm -rf *.o
#$(TARGET) : $(OBJS)
#	$(AR) cr libffmpeg.a *.o
#	rm -rf *.o

clean:
	rm -rf ./*.a
	echo "hello"
