CC=g++-9.1
CXX_COMPILE_FLAGS=-std=c++17 -Wall -O3 -I $(JAVA_HOME)/include -I $(JAVA_HOME)/include/linux

all: libserial_comms_handler_linux_arm32.so jar

clean:
	rm -rf *.o *.so SerialCommsHandler.h lib build

jar:
	mkdir -p lib
	jar cvf lib/serial-handler-v1.0.jar -C build .
	jar uvf lib/serial-handler-v1.0.jar *.so
	#jar uvf lib/serial-handler-v1.0.jar *.dll

libserial_comms_handler_linux_arm32.o: SerialCommsHandler.h libserial_comms_handler_linux_arm32.cpp
libserial_comms_handler_linux_arm32.so: libserial_comms_handler_linux_arm32.o

SerialCommsHandler.h:
	mkdir -p build
	javac -d build src/bitparallel/communication/*.java
	javah -o SerialCommsHandler.h -cp build bitparallel.communication.SerialCommsHandler

%.o: %.cpp
	$(CC) $(CXX_COMPILE_FLAGS) -fPIC -c $<

%.so: %.o
	$(CC) $(CXX_COMPILE_FLAGS) -shared -o $@ $<
