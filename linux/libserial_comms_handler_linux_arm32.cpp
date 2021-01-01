//
// (c) Bit Parallel Ltd, December 2020
//
#include <string>
#include <sstream>

#include <stdint.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

#include "SerialCommsHandler.h"

extern "C"
{
    JNIEXPORT jlong JNICALL Java_bitparallel_communication_SerialCommsHandler_nativeStart(JNIEnv* env, jobject self, jstring device, jint baudRate)
    {
        // convert the java strings to C++ strings
        // note, first convert jstring to char* and then to std::string
        //
        const char *rawDevice = env->GetStringUTFChars(device, NULL);
        const std::string cppDevice = std::string(rawDevice);
        env->ReleaseStringUTFChars(device, rawDevice);

        // these baud rates are limited to the predefined magic numbers set by the Open Group!
        // only the commonly used speeds are defined below, others exist
        //
        int32_t termiosBaudRate = 0;
        switch (static_cast<int32_t>(baudRate))
        {
            // note, there are lower values that can be added in if needed
            //
            case 1200:
                termiosBaudRate = B1200;
                break;

            case 2400:
                termiosBaudRate = B2400;
                break;

            case 4800:
                termiosBaudRate = B4800;
                break;

            case 9600:
                termiosBaudRate = B9600;
                break;

            case 57600:
                termiosBaudRate = B57600;
                break;

            case 115200:
                termiosBaudRate = B115200;
                break;

            case 230400:
                termiosBaudRate = B230400;
                break;

            case 460800:
                termiosBaudRate = B460800;
                break;

            case 921600:
                termiosBaudRate = B921600;
                break;

            default:
            {
                std::stringstream ss;
                ss << "Invalid baud rate for " << cppDevice << ", supported values are 1200, 2400, 4800, 9600, 57600, 115200, 230400, 460800 and 921600";

                const jclass jEx = env->FindClass("java/io/IOException");
                env->ThrowNew(jEx, ss.str().c_str());
                return -1;
            }
        }

        jlong deviceFd = open(cppDevice.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
        if (deviceFd == -1)
        {
            std::stringstream ss;
            ss << "Unable to open device " << cppDevice;

            const jclass jEx = env->FindClass("java/io/IOException");
            env->ThrowNew(jEx, ss.str().c_str());
            return -1;
        }

        // set the serial options: 8n1, ignore parity and use RAW mode
        //
        struct termios options;
        tcgetattr(static_cast<int32_t>(deviceFd), &options);
        cfsetspeed(&options, termiosBaudRate);
        options.c_iflag = IGNPAR;
        options.c_cflag &= ~CSTOPB;
        options.c_cflag |= (CS8 & CLOCAL & CREAD);
        cfmakeraw(&options);
        tcsetattr(static_cast<int32_t>(deviceFd), TCSANOW, &options);

        return deviceFd;
    }

    JNIEXPORT void JNICALL Java_bitparallel_communication_SerialCommsHandler_nativeTransmit(JNIEnv* env, jobject self, jbyteArray txData, jstring device, jlong deviceFd)
    {
        int32_t size = static_cast<int32_t>(env->GetArrayLength(txData));
        if (size == 0) return;

        jboolean isCopy;
        int8_t* bytes = static_cast<int8_t*>(env->GetByteArrayElements(txData, &isCopy));

        int32_t i = 0;
        int32_t writeRetValue;
        while ((size > 0) && (writeRetValue = write(static_cast<int32_t>(deviceFd), &bytes[i], size)) != size)
        {
            if (writeRetValue < 0)
            {
                // note, EAGAIN and EWOULDBLOCK often have the same value, but it's not guaranteed, so check both
                //
                if ((errno == EINTR) || (errno == EAGAIN) || (errno == EWOULDBLOCK)) continue;

                // release the array to allow garbage collection
                //
                env->ReleaseByteArrayElements(txData, bytes, 0);

                // convert the java device string to a C++ string
                //
                const char *rawDevice = env->GetStringUTFChars(device, NULL);
                const std::string cppDevice = std::string(rawDevice);
                env->ReleaseStringUTFChars(device, rawDevice);

                const jclass jEx = env->FindClass("java/io/IOException");
                std::stringstream ss;
                ss << "Unable to transmit data to " << cppDevice << ", failed with ERRNO =  " << errno;
                env->ThrowNew(jEx, ss.str().c_str());
                return;
            }

            size -= writeRetValue;
            i += writeRetValue;
        }

        // release the array to allow garbage collection
        //
        env->ReleaseByteArrayElements(txData, bytes, 0);
    }

    JNIEXPORT void JNICALL Java_bitparallel_communication_SerialCommsHandler_nativeStop(JNIEnv* env, jobject self, jstring device, jlong deviceFd)
    {
        // close the device
        //
        if (deviceFd != -1)
        {
            if (close(static_cast<int32_t>(deviceFd)) == -1)
            {
                // convert the java device string to a C++ string
                //
                const char *rawDevice = env->GetStringUTFChars(device, NULL);
                const std::string cppDevice = std::string(rawDevice);
                env->ReleaseStringUTFChars(device, rawDevice);

                std::stringstream ss;
                ss << "Unable to close device " << cppDevice;

                const jclass jEx = env->FindClass("java/io/IOException");
                env->ThrowNew(jEx, ss.str().c_str());
                return;
            }
        }
    }

    JNIEXPORT void JNICALL Java_bitparallel_communication_SerialCommsHandler_nativeRxRead(JNIEnv* env, jobject self, jobject byteBuffer, jstring device, jlong deviceFd)
    {
        fd_set readFdSet;
        FD_ZERO(&readFdSet);
        FD_SET(static_cast<int32_t>(deviceFd), &readFdSet);

        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = bitparallel_communication_SerialCommsHandler_RX_SELECT_TIMEOUT_US;

        const int32_t maxFd = static_cast<int32_t>(1 + deviceFd);
        const int64_t bufferSize = env->GetDirectBufferCapacity(byteBuffer);
        int8_t* buffer = static_cast<int8_t*>(env->GetDirectBufferAddress(byteBuffer));
        int32_t bytesRead = 0;
        auto fdCount = select(maxFd, &readFdSet, NULL, NULL, &timeout);
        if (fdCount > 0 && FD_ISSET(static_cast<int32_t>(deviceFd), &readFdSet))
        {
            bytesRead = read(static_cast<int32_t>(deviceFd), buffer, bufferSize);
            if (bytesRead < 0)
            {
                // convert the java device string to a C++ string
                //
                const char *rawDevice = env->GetStringUTFChars(device, NULL);
                const std::string cppDevice = std::string(rawDevice);
                env->ReleaseStringUTFChars(device, rawDevice);

                std::stringstream ss;
                ss << "Unable to read from device " << cppDevice;

                const jclass jEx = env->FindClass("java/io/IOException");
                env->ThrowNew(jEx, ss.str().c_str());
                return;
            }
        }

        // set the buffer limit
        //
        const jclass byteBufferClass = env->GetObjectClass(byteBuffer);
        const jmethodID limitId = env->GetMethodID(byteBufferClass, "limit", "(I)Ljava/nio/Buffer;");
        env->CallObjectMethod(byteBuffer, limitId, bytesRead);

        return;
    }
}
