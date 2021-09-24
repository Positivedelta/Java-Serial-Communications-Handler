CC=g++
CXX_COMPILE_FLAGS=-std=c++17 -Wall -O3 -I $(JAVA_HOME)/include -I $(JAVA_HOME)/include/linux

# note, update the mv to reflect underlying architecture, i.e. a 32 / 64 bit so
#
all: libserial_comms_handler_linux_arm.so
	mv libserial_comms_handler_linux_arm.so libserial_comms_handler_linux_arm64.so
#	mv libserial_comms_handler_linux_arm.so libserial_comms_handler_linux_arm32.so

clean:
	rm -rf *.o bitparallel_communication_SerialCommsHandler.h build

libserial_comms_handler_linux_arm.o: SerialCommsHandler.h libserial_comms_handler_linux_arm.cpp
libserial_comms_handler_linux_arm.so: libserial_comms_handler_linux_arm.o

SerialCommsHandler.h:
	mkdir -p build
	javac -cp lib/log4j-api-2.14.1.jar:lib/log4j-core-2.14.1.jar -d build src/bitparallel/communication/*.java -h .

%.o: %.cpp
	$(CC) $(CXX_COMPILE_FLAGS) -fPIC -c $<

%.so: %.o
	$(CC) $(CXX_COMPILE_FLAGS) -shared -o $@ $<
