CC=g++-9.1
CXX_COMPILE_FLAGS=-std=c++17 -Wall -O3 -I $(JAVA_HOME)/include -I $(JAVA_HOME)/include/linux

all: libserial_comms_handler_linux_arm32.so

clean:
	rm -rf *.o *.so SerialCommsHandler.h build

libserial_comms_handler_linux_arm32.o: SerialCommsHandler.h libserial_comms_handler_linux_arm32.cpp
libserial_comms_handler_linux_arm32.so: libserial_comms_handler_linux_arm32.o

SerialCommsHandler.h:
	mkdir -p build
	javac -cp lib/log4j-api-2.8.2.jar:lib/log4j-core-2.8.2.jar -d build src/bitparallel/communication/*.java
	javah -o SerialCommsHandler.h -cp build bitparallel.communication.SerialCommsHandler

%.o: %.cpp
	$(CC) $(CXX_COMPILE_FLAGS) -fPIC -c $<

%.so: %.o
	$(CC) $(CXX_COMPILE_FLAGS) -shared -o $@ $<
