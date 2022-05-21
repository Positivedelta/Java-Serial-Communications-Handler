//
// (c) Bit Parallel Ltd, December 2020
//

#include "SerialCommsHandler.h"

#include <string>
#include <sstream>
#include <stdexcept>
#include <cstdint>

#include <windows.h>
#include <stdio.h>

//
// notes 1, int64_t values are used to hold the Win32 handles, this was dones as the ultimate goal is to use this code in a Java JNI wrapper
//       2, the tx and rx buffers are declared as int8_t, again this is to match the signed byte type when used in Java JNI wrapper
//       3, the IO events are initailiased in nativeStart() and then only reset ate the end of nativeRxRead() and nativeTransmit(), seems sensible...
//       4, items like device and handle are passed into most of these functions, the DLL needs to be stateless as it could have multiple instantiations
//          these items are managed by specific Java instances as the DLL is loaded in a static context
//       5, may need to add locks to allows different Java threads to call nativeRxRead() and nativeTransmit(), not sure, will test and investigate...
//
// inspiration taken from...
//
// https://docs.microsoft.com/en-us/previous-versions/ff802693(v=msdn.10)?redirectedfrom=MSDN#reading-and-writing
// https://stackoverflow.com/questions/25364525/win32-api-how-to-read-the-serial-or-exit-within-a-timeout-if-wasnt-a-data
// https://mssqlwiki.com/2012/02/15/asynchronous-io-example/
//

extern "C"
{
	JNIEXPORT jlong JNICALL Java_bitparallel_communication_SerialCommsHandler_nativeStart(JNIEnv* env, jobject self, jstring device, jint baudRate)
	{
		// used when raising exceptions
		//
		const jclass jEx = env->FindClass("java/io/IOException");
		const char* rawDevice = env->GetStringUTFChars(device, NULL);
		const std::string cppDevice = std::string(rawDevice);
		env->ReleaseStringUTFChars(device, rawDevice);

		const HANDLE hComm = CreateFileA(cppDevice.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
		if (hComm == INVALID_HANDLE_VALUE)
		{
			std::stringstream ss;
			ss << "Unable to open device: " << cppDevice << ", failed with ERRNO =  " << GetLastError();
			env->ThrowNew(jEx, ss.str().c_str());
			return NULL;
		}

		// setup the serial port
		//
		DCB dcbSerialParams = DCB();
		dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
		int32_t status = GetCommState(hComm, &dcbSerialParams);
		if (status == 0)
		{
			std::stringstream ss;
			ss << "Unable to read device state for " << cppDevice << ", failed with ERRNO =  " << GetLastError();
			env->ThrowNew(jEx, ss.str().c_str());
			return NULL;
		}

		// using the predefined magic numbers, only the commonly used speeds are defined below, others exist
		//
		int32_t wBaudRate = 0;
		switch (static_cast<int32_t>(baudRate))
		{
			// note, there are lower values that can be added in if needed
			//
			case 1200:
				wBaudRate = CBR_1200;
				break;

			case 2400:
				wBaudRate = CBR_2400;
				break;

			case 4800:
				wBaudRate = CBR_4800;
				break;

			case 9600:
				wBaudRate = CBR_9600;
				break;

			case 57600:
				wBaudRate = CBR_57600;
				break;

			case 115200:
				wBaudRate = CBR_115200;
				break;

			//
			// note, for historic reasons the following baud rates are not defined as part of the DCB struct as old UARTS didn't support
			//       rates this quick! however, on modern hardware they work just fine
			//

			case 230400:
				wBaudRate = 230400;
				break;

			case 460800:
				wBaudRate = 460800;
				break;

			case 921600:
				wBaudRate = 921600;
				break;

			default:
			{
				std::stringstream ss;
				ss << "Invalid baud rate for " << cppDevice << ", supported values are 1200, 2400, 4800, 9600, 57600, 115200, 230400, 460800 and 921600";
				env->ThrowNew(jEx, ss.str().c_str());
				return NULL;
			}
		}

		dcbSerialParams.BaudRate = wBaudRate;
		dcbSerialParams.ByteSize = 8;
		dcbSerialParams.StopBits = ONESTOPBIT;
		dcbSerialParams.Parity = NOPARITY;
		SetCommState(hComm, &dcbSerialParams);

		COMMTIMEOUTS timeouts = COMMTIMEOUTS();
		timeouts.ReadIntervalTimeout = MAXDWORD;
		timeouts.ReadTotalTimeoutMultiplier = 0;
		timeouts.ReadTotalTimeoutConstant = 0;
		timeouts.WriteTotalTimeoutConstant = 0;
		timeouts.WriteTotalTimeoutMultiplier = 0;
		SetCommTimeouts(hComm, &timeouts);

		// prepare to start receiving RX and TX events...
		//
		SetCommMask(hComm, EV_RXCHAR | EV_TXEMPTY);

		return reinterpret_cast<int64_t>(hComm);
	}

	JNIEXPORT void JNICALL Java_bitparallel_communication_SerialCommsHandler_nativeTransmit(JNIEnv* env, jobject self, jbyteArray txData, jstring device, jlong handle)
	{
		int32_t length = static_cast<int32_t>(env->GetArrayLength(txData));
		if (length == 0) return;

		// FIXME! is this slow? perhaps only do this if and when an exception is actually thrown
		//
		// used when raising exceptions
		//
		const jclass jEx = env->FindClass("java/io/IOException");
		const char* rawDevice = env->GetStringUTFChars(device, NULL);
		const std::string cppDevice = std::string(rawDevice);
		env->ReleaseStringUTFChars(device, rawDevice);

		OVERLAPPED overlappedWrite;
		ZeroMemory(&overlappedWrite, sizeof(overlappedWrite));
		overlappedWrite.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

		jboolean isCopy;
		int8_t* bytes = static_cast<int8_t*>(env->GetByteArrayElements(txData, &isCopy));

		int32_t bytesWritten = 0;
//		SetCommMask(reinterpret_cast<HANDLE>(handle), EV_TXEMPTY);
		if (!WriteFile(reinterpret_cast<HANDLE>(handle), bytes, length, (LPDWORD)&bytesWritten, &overlappedWrite))
		{
			if (GetLastError() != ERROR_IO_PENDING)
			{
				// reset the next TX event in case this is recoverable, doubt it...
				//
				SetCommMask(reinterpret_cast<HANDLE>(handle), EV_TXEMPTY);

				std::stringstream ss;
				ss << "Write to " << cppDevice << ", failed with ERRNO =  " << GetLastError();
				env->ThrowNew(jEx, ss.str().c_str());
				return;
			}
			else
			{
				// write is pending...
				// as bWait is true, GetOverlappedResult() will block until the write is complete or an error occurs
				//
				if (GetOverlappedResult(reinterpret_cast<HANDLE>(handle), &overlappedWrite, (LPDWORD)&bytesWritten, TRUE) == 0)
				{
					// reset the next TX event in case this is recoverable, doubt it...
					//
					SetCommMask(reinterpret_cast<HANDLE>(handle), EV_TXEMPTY);

					std::stringstream ss;
					ss << "Overlapped write to " << cppDevice << ", failed with ERRNO =  " << GetLastError();
					env->ThrowNew(jEx, ss.str().c_str());
					return;
				}
			}
		}

		if (CloseHandle(overlappedWrite.hEvent) == 0)
		{
			// note, will fall through and set up the next TX event in case this is recoverable
			//
			std::stringstream ss;
			ss << "Unable to close the write handle for " << cppDevice << ", failed with ERRNO =  " << GetLastError();
			env->ThrowNew(jEx, ss.str().c_str());
		}

		// set up the next TX event
		//
		SetCommMask(reinterpret_cast<HANDLE>(handle), EV_TXEMPTY);
	}

	JNIEXPORT void JNICALL Java_bitparallel_communication_SerialCommsHandler_nativeRxRead(JNIEnv* env, jobject self, jobject byteBuffer, jstring device, jlong handle)
	{
		// FIXME! is this slow? perhaps only do this if and when an exception is actually thrown
		//
		// used when raising exceptions
		//
		const jclass jEx = env->FindClass("java/io/IOException");
		const char* rawDevice = env->GetStringUTFChars(device, NULL);
		const std::string cppDevice = std::string(rawDevice);
		env->ReleaseStringUTFChars(device, rawDevice);

		OVERLAPPED overlappedRead;
		ZeroMemory(&overlappedRead, sizeof(overlappedRead));
		overlappedRead.hEvent = CreateEvent(NULL, true, false, NULL);
//		SetCommMask(reinterpret_cast<HANDLE>(handle), EV_RXCHAR);

		// wait for a read event
		//
		const int32_t msTimeout = bitparallel_communication_SerialCommsHandler_RX_SELECT_TIMEOUT_US / 1000;
		int32_t waitEventType = 0;
		int32_t status = -1;
		if (!WaitCommEvent(reinterpret_cast<HANDLE>(handle), (LPDWORD)&waitEventType, &overlappedRead))
		{
			if (GetLastError() != ERROR_IO_PENDING)
			{
				// setup the next RX event in case this is recoverable
				//
				SetCommMask(reinterpret_cast<HANDLE>(handle), EV_RXCHAR);

				std::stringstream ss;
				ss << "Waiting for read event on " << cppDevice << ", failed with ERRNO =  " << GetLastError();
				env->ThrowNew(jEx, ss.str().c_str());
				return;
			}
		}

		switch (WaitForSingleObject(overlappedRead.hEvent, msTimeout))
		{
			// success, the event signal has been received!
			//
			case WAIT_OBJECT_0:
				status = 0;
				break;

			case WAIT_TIMEOUT:
				status = 1;
				break;

			default:
			{
				// setup the next RX event in case this is recoverable
				//
				SetCommMask(reinterpret_cast<HANDLE>(handle), EV_RXCHAR);

				std::stringstream ss;
				ss << "Waiting for single read object on " << cppDevice << ", failed with ERRNO =  " << GetLastError();
				env->ThrowNew(jEx, ss.str().c_str());
				return;
			}
		}

		// now read the RXed data
		//
		const int64_t bufferSize = env->GetDirectBufferCapacity(byteBuffer);
		int8_t* rxBuffer = static_cast<int8_t*>(env->GetDirectBufferAddress(byteBuffer));
		DWORD bytesRead = 0;
		if (status == 0)
		{
			auto rf = ReadFile(reinterpret_cast<HANDLE>(handle), rxBuffer, static_cast<DWORD>(bufferSize), NULL, &overlappedRead);
			if ((rf == 0) && GetLastError() == ERROR_IO_PENDING)
			{
				while (!GetOverlappedResult(reinterpret_cast<HANDLE>(handle), &overlappedRead, &bytesRead, FALSE))
				{
					// note, if the last error was ERROR_IO_PENDING then this could be harnessed to do 'overlapped' work, i.e. call a useful method
					// otherwise loop and wait for the next event, or an error has occured, other return values include ERROR_HANDLE_EOF (file reads)
					//
					if (GetLastError() != ERROR_IO_PENDING)
					{
						// setup the next RX event in case this is recoverable
						//
						SetCommMask(reinterpret_cast<HANDLE>(handle), EV_RXCHAR);

						std::stringstream ss;
						ss << "Unable to read overlapped data on " << cppDevice << ", failed with ERRNO =  " << GetLastError();
						env->ThrowNew(jEx, ss.str().c_str());
						return;
					}
				}
			}

			bytesRead = static_cast<DWORD>(overlappedRead.InternalHigh);
		}
		else if (status == 1)
		{
			// the read has timed out
			//
			bytesRead = 0;
		}

		status = CloseHandle(_Notnull_ overlappedRead.hEvent);
		if (status == 0)
		{
			// setup the next RX event in case this is recoverable
			//
			SetCommMask(reinterpret_cast<HANDLE>(handle), EV_RXCHAR);

			std::stringstream ss;
			ss << "Unable to close event handle for " << cppDevice << ", failed with ERRNO =  " << GetLastError();
			env->ThrowNew(jEx, ss.str().c_str());
			return;
		}

		// set the buffer limit
		//
		const jclass byteBufferClass = env->GetObjectClass(byteBuffer);
		const jmethodID limitId = env->GetMethodID(byteBufferClass, "limit", "(I)Ljava/nio/Buffer;");
		env->CallObjectMethod(byteBuffer, limitId, bytesRead);

		// setup the next RX event
		//
		SetCommMask(reinterpret_cast<HANDLE>(handle), EV_RXCHAR);
	}

	JNIEXPORT void JNICALL Java_bitparallel_communication_SerialCommsHandler_nativeStop(JNIEnv* env, jobject self, jstring device, jlong handle)
	{
		if (CloseHandle(reinterpret_cast<HANDLE>(handle)) == 0)
		{
			const char* rawDevice = env->GetStringUTFChars(device, NULL);
			const std::string cppDevice = std::string(rawDevice);
			env->ReleaseStringUTFChars(device, rawDevice);

			const jclass jEx = env->FindClass("java/io/IOException");
			std::stringstream ss;
			ss << "Unable to close " << cppDevice << ", failed with ERRNO =  " << GetLastError();
			env->ThrowNew(jEx, ss.str().c_str());
		}
	}
}
