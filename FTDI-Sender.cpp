#include <iostream>
#include <iomanip>
#include <chrono>   // for std::chrono::milliseconds
#include <thread>   // for std::this_thread::sleep_for
#include <windows.h>
#include "FTD2XX.h" // must be in project include path
#include <stdlib.h>   // For _MAX_PATH definition
#include <stdio.h>
#include <malloc.h>

#define OneSector 1024 
#define SectorNum 2000 

/*** HELPER FUNCTION DECLARATIONS ***/

// Formats and writes FT_DEVICE_LIST_INFO_NODE structure to output
// stream object.
std::ostream& operator<<(std::ostream& os, const FT_DEVICE_LIST_INFO_NODE& device);

std::string byteToHex(unsigned char byte) {
	const char hexDigits[] = "0123456789abcdef";
	std::string hexStr;
	hexStr += hexDigits[(byte >> 4) & 0x0F];
	hexStr += hexDigits[byte & 0x0F];
	return hexStr;
}

// Prints a formatted error message and terminates the program with
// status code EXIT_FAILURE.
void error(const char* message);

// Prints FT_STATUS and a formatted error message and terminates
// the program with status code EXIT_FAILURE.
void ft_error(FT_STATUS status, const char* message);

// Prints FT_STATUS and a formatted error message, closes `handle`,
// and terminates the program with status code EXIT_FAILURE.
// (Delegates to ft_error(FT_STATUS, const char*))
void ft_error(FT_STATUS status, const char* message, FT_HANDLE handle);


/*** MAIN PROGRAM ***/

int main()
{
	FT_STATUS status;
	UCHAR LatencyTimer = 255;
	DWORD EventDWord;
	DWORD RxBytes;
	DWORD TxBytes;
	DWORD BytesReceived = 0, BytesWritten = 0;
	char TxBuffer[OneSector];


	// --- Get number of devices ---

	unsigned long deviceCount = 0;

	status = FT_CreateDeviceInfoList(&deviceCount);

	if (status != FT_OK) {
		ft_error(status, "FT_CreateDeviceInfoList");
	}

	if (deviceCount == 0) {
		error("No FTDI devices found.");
	}

	// --- Enumerate devices ---

	FT_DEVICE_LIST_INFO_NODE* deviceInfos = new FT_DEVICE_LIST_INFO_NODE[deviceCount];

	status = FT_GetDeviceInfoList(deviceInfos, &deviceCount);
	// ... populates deviceInfos array

	if (status != FT_OK) {
		ft_error(status, "FT_GetDeviceInfoList");
	}

	// --- Find device of interest ---

	const unsigned long myID = 0x04036014;
	// ... from "Device Manager": Vendor ID = 0403, Product ID = 6014

	FT_DEVICE_LIST_INFO_NODE myDevice{};

	// Find first device that matches `myID`
	for (unsigned int i = 0; i < deviceCount; i++)
	{
		if (deviceInfos[i].ID == myID)
		{
			// Copy device info into `myDevice`
			myDevice = deviceInfos[i];

			// Open device
			status = FT_Open(i, &myDevice.ftHandle);

			if (status != FT_OK) {
				ft_error(status, "FT_Open");
			}

			break;
		}
	}

	// Handle device not found...
	if (myDevice.ID != myID)
	{
		std::cerr << "0 of " << deviceCount << " devices with ID " << std::hex << std::showbase << myID << " found:\n";

		for (unsigned int i = 0; i < deviceCount; i++)
		{
			std::cerr << "Device " << i << "\n";
			std::cerr << deviceInfos[i] << std::endl;
		}

		error("Device not found.");
	}

	std::cout << "Device found with ID " << std::hex << std::showbase << myID << ":\n";
	std::cout << myDevice << std::endl;

	// Clean up device info list

	delete[] deviceInfos;

	// --- Set up command buffers ---

	uint8_t* recvBuffer = (uint8_t*)std::malloc(2000 * 1024);// [2000][1024];
	unsigned long bytesWritten = 0;
	unsigned long bytesRead = 0;


	status = FT_SetTimeouts(myDevice.ftHandle, 500, 500);

	if (status != FT_OK) {
		ft_error(status, "FT_SetTimeouts", myDevice.ftHandle);
	}

	
	std::chrono::milliseconds(10);

	
	int i = 0,j=0;
	for (;i <= 920;) {
		TxBuffer[i++] = 0xFF;// Start Pattern
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0xFF;
		/*TxBuffer[i++] = 0x00;// Close Pattern
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0xFF; */
		TxBuffer[i++] = 0x00;// H 01001000
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0x00;// e 01100101
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0x00;// l 01101100
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0x00;// l 01101100
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0x00;// o 01101111
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0x00;// <space> 00100000
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0x00;// w 01110111
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0x00;//o 01101111
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0x00;//r 01110010
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0x00;//l 01101100
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0x00;//d 01100100
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0x00;
		switch (j) {
		case 0:TxBuffer[i++] = 0x00;//0 00000000
			TxBuffer[i++] = 0x00;
			TxBuffer[i++] = 0xFF;
			TxBuffer[i++] = 0xFF;
			TxBuffer[i++] = 0x00;
			TxBuffer[i++] = 0x00;
			TxBuffer[i++] = 0x00;
			TxBuffer[i++] = 0x00;j++;break;
		case 1:TxBuffer[i++] = 0x00;//1 00110000
			TxBuffer[i++] = 0x00;
			TxBuffer[i++] = 0xFF;
			TxBuffer[i++] = 0xFF;
			TxBuffer[i++] = 0x00;
			TxBuffer[i++] = 0x00;
			TxBuffer[i++] = 0x00;
			TxBuffer[i++] = 0xFF;j++;break;
		case 2:TxBuffer[i++] = 0x00;//1 00110000
			TxBuffer[i++] = 0x00;
			TxBuffer[i++] = 0xFF;
			TxBuffer[i++] = 0xFF;
			TxBuffer[i++] = 0x00;
			TxBuffer[i++] = 0x00;
			TxBuffer[i++] = 0xFF;
			TxBuffer[i++] = 0x00;j++;break;
		case 3:TxBuffer[i++] = 0x00;//1 00110000
			TxBuffer[i++] = 0x00;
			TxBuffer[i++] = 0xFF;
			TxBuffer[i++] = 0xFF;
			TxBuffer[i++] = 0x00;
			TxBuffer[i++] = 0x00;
			TxBuffer[i++] = 0xFF;
			TxBuffer[i++] = 0xFF;j++;break;
		case 4:TxBuffer[i++] = 0x00;//1 00110000
			TxBuffer[i++] = 0x00;
			TxBuffer[i++] = 0xFF;
			TxBuffer[i++] = 0xFF;
			TxBuffer[i++] = 0x00;
			TxBuffer[i++] = 0xFF;
			TxBuffer[i++] = 0x00;
			TxBuffer[i++] = 0x00;j++;break;
		case 5:TxBuffer[i++] = 0x00;//1 00110000
			TxBuffer[i++] = 0x00;
			TxBuffer[i++] = 0xFF;
			TxBuffer[i++] = 0xFF;
			TxBuffer[i++] = 0x00;
			TxBuffer[i++] = 0xFF;
			TxBuffer[i++] = 0x00;
			TxBuffer[i++] = 0xFF;j++;break;
		case 6:TxBuffer[i++] = 0x00;//1 00110000
			TxBuffer[i++] = 0x00;
			TxBuffer[i++] = 0xFF;
			TxBuffer[i++] = 0xFF;
			TxBuffer[i++] = 0x00;
			TxBuffer[i++] = 0xFF;
			TxBuffer[i++] = 0xFF;
			TxBuffer[i++] = 0x00;j++;break;
		case 7:TxBuffer[i++] = 0x00;//1 00110000
			TxBuffer[i++] = 0x00;
			TxBuffer[i++] = 0xFF;
			TxBuffer[i++] = 0xFF;
			TxBuffer[i++] = 0x00;
			TxBuffer[i++] = 0xFF;
			TxBuffer[i++] = 0xFF;
			TxBuffer[i++] = 0xFF;j++;break;
		case 8:TxBuffer[i++] = 0x00;//1 00110000
			TxBuffer[i++] = 0x00;
			TxBuffer[i++] = 0xFF;
			TxBuffer[i++] = 0xFF;
			TxBuffer[i++] = 0xFF;
			TxBuffer[i++] = 0x00;
			TxBuffer[i++] = 0x00;
			TxBuffer[i++] = 0x00;j++;break;
		case 9:TxBuffer[i++] = 0x00;//1 00110000
			TxBuffer[i++] = 0x00;
			TxBuffer[i++] = 0xFF;
			TxBuffer[i++] = 0xFF;
			TxBuffer[i++] = 0xFF;
			TxBuffer[i++] = 0x00;
			TxBuffer[i++] = 0x00;
			TxBuffer[i++] = 0xFF;j++;j = j % 10;break;
		}
	}
	std::cout << "buf size:" << i;
	// Infinite loop
	if (0) { // read from pico
		status = FT_SetTimeouts(myDevice.ftHandle, 500, 500);
		UCHAR MaskA = 0x00; // Set data bus to inputs 
		UCHAR modeA = 0x00;  // Configure FT2232H into 0x40 Sync FIFO Mode 

		status = FT_SetBitMode(myDevice.ftHandle, MaskA, modeA);
		if (status != FT_OK)
			printf("timeout device status not ok %d\n", status);
		Sleep(500);

		MaskA = 0x00; // Set data bus to inputs 
		modeA = 0x40;  // Configure FT2232H into 0x40 Sync FIFO Mode 

		status = FT_SetBitMode(myDevice.ftHandle, MaskA, modeA);

		if (status != FT_OK)
			printf("mode A status not ok %d\n", status);

		Sleep(500);

		DWORD RxBytes = 256;
		DWORD TxBytes;
		DWORD EventDword;

		status = FT_GetStatus(myDevice.ftHandle, &RxBytes, &TxBytes, &EventDword);

		printf("bytes in RX queue %d\n", RxBytes);

		printf("\n");
		while (true)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			//std::cout << "\rWriting: " << std::noshowbase << std::hex << std::setfill('0') << std::setw(2) << int(value);
			status = FT_GetStatus(myDevice.ftHandle, &RxBytes, &TxBytes, &EventDWord);
			UCHAR data_in[65536]; // declare a large buffer for incoming data 
			DWORD r_data_len = RxBytes;
			DWORD data_read;
			memset(data_in, 0, 1028);
			if (status == FT_OK && (TxBytes == 0)) {
				status = FT_Purge(myDevice.ftHandle, FT_PURGE_RX);
				status = FT_Read(myDevice.ftHandle, data_in, r_data_len, &data_read);

				if (status == FT_OK)
				{
					printf("bytes read %d\n", data_read);
					printf("data read %x\n", data_in[0]);
					printf("data read %x\n", data_in[1]);
					printf("data read %x\n", data_in[2]);
					printf("data read %x\n", data_in[3]);
					status = FT_Purge(myDevice.ftHandle, FT_PURGE_RX);
				}
				else
				{
					// FT_Write Failed  
				}
			}
			else {
				//ft_error(status, "FT_Write", myDevice.ftHandle);
			}

			//status = FT_Close(myDevice.ftHandle);
		}
	}
	else { // write to pico
		//set interface into FT245 Synchronous FIFO mode 
		// Initialize MPSSE controller

		status = FT_SetBitMode(myDevice.ftHandle, 0xFF, 0x40); // All pins outputs, MPSSE

		if (status != FT_OK) {
			ft_error(status, "FT_SetBitMode", myDevice.ftHandle);
		}
		while (1)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			status = FT_GetStatus(myDevice.ftHandle, &RxBytes, &TxBytes, &EventDWord);
			if ((status == FT_OK) && (TxBytes == 0))
			{
				status = FT_Write(myDevice.ftHandle, TxBuffer, 128, &BytesWritten);
				if (status == FT_OK)
				{
					// FT_Write OK  
					std::cout << "\n Bytes Write " << BytesWritten << " Byte 0x" << byteToHex(TxBuffer[0]) << " 0x" << byteToHex(TxBuffer[1]) << " 0x" << byteToHex(TxBuffer[2]) << " 0x" << byteToHex(TxBuffer[3]) << " last byte 0x" << byteToHex(TxBuffer[255]);
					status = FT_Purge(myDevice.ftHandle, FT_PURGE_RX);
					status = FT_Purge(myDevice.ftHandle, FT_PURGE_TX);
				}
				else
				{
					std::cout << "write failed";
					// FT_Write Failed  
				}
			}
		}
	}

	return 0;
}


/*** HELPER FUNCTION DEFINITIONS ***/

void error(const char* message)
{
	std::cerr << "\033[1;31m[ERROR]\033[0m " << message << std::endl;
	exit(EXIT_FAILURE);
}

void ft_error(FT_STATUS status, const char* message)
{
	std::cerr << "\033[1;31m[ERROR]\033[0m " << message << ": " << status << std::endl;
	exit(EXIT_FAILURE);
}

void ft_error(FT_STATUS status, const char* message, FT_HANDLE handle)
{
	FT_Close(handle);
	ft_error(status, message);
}

std::ostream& operator<<(std::ostream& os, const FT_DEVICE_LIST_INFO_NODE& device)
{
	os << "  @" << std::showbase << std::hex << &device << "\n";
	os << "  Flags=" << std::showbase << std::hex << device.Flags << "\n";
	os << "  Type=" << std::showbase << std::hex << device.Type << "\n";
	os << "  ID=" << std::showbase << std::hex << device.ID << "\n";
	os << "  LocId=" << std::showbase << std::hex << device.LocId << "\n";
	os << "  SerialNumber=" << device.SerialNumber << "\n";
	os << "  Description=" << device.Description << "\n";
	os << "  ftHandle=" << std::showbase << std::hex << device.ftHandle << "\n";

	return os;
}
