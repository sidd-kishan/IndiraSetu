#include <iostream>
#include <iomanip>
#include <chrono>   // for std::chrono::milliseconds
#include <thread>   // for std::this_thread::sleep_for
#include <windows.h>
#include "FTD2XX.h" // must be in project include path
#include <stdlib.h>   // For _MAX_PATH definition
#include <stdio.h>
#include <malloc.h>
#include <vector>
#include <string.h>
#include <fstream>
#include <sstream>
#include <cmath>
#include <string>
#include <ctype.h>

constexpr int kMaxRows = 1024;
constexpr int kColsPerRow = 14;
constexpr int WINDOW_WIDTH = 320;
constexpr int WINDOW_HEIGHT = 240;


struct Point2D {
	int x, y;
};

struct Triangle2D {
	Point2D a, b, c;
	unsigned char color;
};

std::vector<Triangle2D> triangleList;


void bit_expand_byte(unsigned char byte, unsigned char* out);

// Function to print the buffer in a readable format (similar to the previous tool)
void print_tx_buffer(const unsigned char* tx, int tx_len,unsigned char * input, int input_len,
					 int prefix_len, int suffix_len, int include_crc) {
	int index = 0;

	printf("TxBuffer[] = {\n");

	while (index < tx_len) {
		// 1. Print prefix
		if (prefix_len > 0) {
			printf(" ");
			for (int i = 0; i < prefix_len && index < tx_len; i++) {
				printf(" 0x%02X", tx[index++]);
				if (i < prefix_len - 1 && index < tx_len) printf(",");
			}
			printf("  // Prefix\n");
		}

		// 2. Print message (bit-expanded)
		for (int c = 0; c < input_len && index + 8 <= tx_len; c++) {
			printf("  ");
			for (int b = 0; b < 8; b++) {
				printf("0x%02X", tx[index++]);
				if (b < 7 || (include_crc || suffix_len)) printf(", ");
			}
			printf("  // '%c'\n", isprint(input[c]) ? input[c] : '.');
		}

		// 3. Print CRC (if included)
		if (include_crc && index + 8 <= tx_len) {
			printf("  ");
			for (int b = 0; b < 8; b++) {
				printf("0x%02X", tx[index++]);
				if (b < 7 || suffix_len) printf(", ");
			}
			printf("  // CRC8\n");
		}

		// 4. Print suffix
		if (suffix_len > 0 && index + suffix_len <= tx_len) {
			printf("  ");
			for (int i = 0; i < suffix_len && index < tx_len; i++) {
				printf("0x%02X", tx[index++]);
				if (i < suffix_len - 1 && index < tx_len) printf(", ");
			}
			printf("  // Suffix\n");
		}

		// Stop if buffer is filled
		if (index >= tx_len) {
			break;
		}
	}

	printf("};\n");
}


#define TXBUILDER_MAX_SIZE 8192

static unsigned char crc8(const unsigned char* data, int len) {
	unsigned char crc = 0x00;
	for (int i = 0; i < len; i++) {
		crc ^= data[i];
		for (int j = 0; j < 8; j++) {
			if (crc & 0x80)
				crc = (crc << 1) ^ 0x07;
			else
				crc <<= 1;
		}
	}
	return crc;
}

static void bit_expand_byte(unsigned char byte, unsigned char* out) {
	for (int i = 0; i < 8; i++) {
		out[i] = (byte & (1 << (7 - i))) ? 0xFF : 0x00;
	}
}

int parse_hex_pattern(const char* pattern_str, unsigned char* buffer, int max_len) {
	int count = 0;
	const char* p = pattern_str;

	while (*p && count < max_len) {
		// Skip leading whitespace
		while (isspace((unsigned char)*p)) p++;

		// Read one token (up to next comma or end)
		char token[16] = { 0 };
		int i = 0;
		while (*p && *p != ',' && i < (int)(sizeof(token) - 1)) {
			token[i++] = *p++;
		}
		token[i] = '\0';

		// Convert to byte
		if (i > 0) {
			char* endptr;
			int value;

			if (strncmp(token, "0x", 2) == 0 || strncmp(token, "0X", 2) == 0)
				value = (int)strtol(token, &endptr, 16);
			else
				value = (int)strtol(token, &endptr, 10);

			if (*endptr == '\0' && value >= 0 && value <= 255)
				buffer[count++] = (unsigned char)value;
			else
				fprintf(stderr, "Warning: Invalid token \"%s\" ignored\n", token);
		}

		// Skip comma
		if (*p == ',') p++;
	}

	return count;
}
// Function to build the TxBuffer by repeating message and adding CRC, prefix, and suffix
unsigned char* build_tx_buffer(const unsigned char* input, int input_len, unsigned char* prefix, int prefix_len,
							   unsigned char* suffix, int suffix_len, int tx_len,
							   int include_crc, int* actual_len) {
	unsigned char* TxBuffer = (unsigned char*)malloc(tx_len * sizeof(unsigned char));
	if (!TxBuffer) {
		fprintf(stderr, "Memory allocation failed for TxBuffer.\n");
		return NULL;
	}

	int buffer_index = 0;
	int input_pos = 0;

	// Build the buffer, repeating [prefix][msg][crc8][suffix]
	while (buffer_index < tx_len) {
		// 1. Add the prefix
		if (prefix_len > 0) {
			for (int i = 0; i < prefix_len && buffer_index < tx_len; i++) {
				TxBuffer[buffer_index++] = prefix[i];
			}
		}

		// 2. Add the message (bit-expanded)
		unsigned char expanded[8];
		for (int i = 0; i < input_len && buffer_index + 8 <= tx_len; i++) {
			bit_expand_byte(input[i], expanded);
			for (int b = 0; b < 8; b++) {
				TxBuffer[buffer_index++] = expanded[b];
			}
		}

		// 3. Add CRC (if required)
		if (include_crc) {
			unsigned char crc = 0x00;
			for (int i = 0; i < input_len; i++) {
				crc ^= input[i];
				for (int j = 0; j < 8; j++) {
					if (crc & 0x80)
						crc = (crc << 1) ^ 0x07;
					else
						crc <<= 1;
				}
			}
			unsigned char crc_exp[8];
			bit_expand_byte(crc, crc_exp);
			for (int b = 0; b < 8 && buffer_index < tx_len; b++) {
				TxBuffer[buffer_index++] = crc_exp[b];
			}
		}

		// 4. Add the suffix
		if (suffix_len > 0) {
			for (int i = 0; i < suffix_len && buffer_index < tx_len; i++) {
				TxBuffer[buffer_index++] = suffix[i];
			}
		}

		// Stop if the buffer is filled up
		if (buffer_index >= tx_len) {
			break;
		}
	}

	*actual_len = buffer_index;
	return TxBuffer;
}


std::vector<int> searchPatternMultiple(uint8_t* buffer, size_t bufferSize, uint8_t* pattern, size_t patternSize) {
	std::vector<int> matches;

	for (size_t i = 0; i <= bufferSize - patternSize; ++i) {
		bool match = true;
		for (size_t j = 0; j < patternSize; ++j) {
			if (buffer[i + j] != pattern[j]) {
				match = false;
				break;
			}
		}

		if (match) {
			matches.push_back(i);  // store the index of the match
		}
	}

	return matches;  // return a vector of all match positions
}

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

void drawTriangle(HDC hdc, const Triangle2D& tri) {
	HPEN pen = CreatePen(PS_SOLID, 1, RGB(tri.color, tri.color, tri.color));
	HGDIOBJ oldPen = SelectObject(hdc, pen);

	MoveToEx(hdc, tri.a.x, tri.a.y, NULL);
	LineTo(hdc, tri.b.x, tri.b.y);
	LineTo(hdc, tri.c.x, tri.c.y);
	LineTo(hdc, tri.a.x, tri.a.y); // close

	SelectObject(hdc, oldPen);
	DeleteObject(pen);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
	case WM_PAINT: {
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hwnd, &ps);
		RECT rect;
		GetClientRect(hwnd, &rect);
		FillRect(hdc, &rect, (HBRUSH)(COLOR_WINDOW + 1));
		for (const auto& tri : triangleList)
			drawTriangle(hdc, tri);
		EndPaint(hwnd, &ps);
		break;
	}
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return 0;
}



/*** MAIN PROGRAM ***/

int main()
{
	FT_STATUS status;
	UCHAR LatencyTimer = 255;
	DWORD EventDWord;
	DWORD RxBytes;
	DWORD TxBytes;
	DWORD BytesReceived = 0, BytesWritten = 0;


	const wchar_t CLASS_NAME[] = L"MyTriangleWindow";
	WNDCLASS wc = {};
	wc.lpfnWndProc = WndProc;
	wc.hInstance = GetModuleHandle(NULL);
	wc.lpszClassName = CLASS_NAME;
	RegisterClass(&wc);

	HWND hwnd = CreateWindowEx(
		0, CLASS_NAME, L"Triangle Viewer",
		WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME ^ WS_MAXIMIZEBOX,
		CW_USEDEFAULT, CW_USEDEFAULT, 320, 240,
		NULL, NULL, wc.hInstance, NULL);

	if (!hwnd) {
		std::cerr << "Window creation failed.\n";
		return 1;
	}

	
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


	status = FT_SetTimeouts(myDevice.ftHandle, 0, 0);
	status = FT_SetFlowControl(myDevice.ftHandle, FT_FLOW_RTS_CTS, 0, 0);
	status = FT_SetUSBParameters(myDevice.ftHandle, 0x10000, 0x10000);
	if (status != FT_OK) {
		ft_error(status, "FT_SetTimeouts", myDevice.ftHandle);
	}


	std::chrono::milliseconds(10);

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
		int msg_index = 0;
		status = FT_SetBitMode(myDevice.ftHandle, 0xFF, 0x40); // All pins outputs, MPSSE
		if (status != FT_OK) {
			ft_error(status, "FT_SetBitMode", myDevice.ftHandle);
		}
		unsigned char input[1024][15];/* = {
				"\x48\x65\x6C\x6C\x6F\x20\x57\x6F\x72\x6C\x64\x30\x30\x30",
				"\x48\x65\x6C\x6C\x6F\x20\x57\x6F\x72\x6C\x64\x31\x30\x30",
				"\x48\x65\x6C\x6C\x6F\x20\x57\x6F\x72\x6C\x64\x32\x30\x30",
				"\x48\x65\x6C\x6C\x6F\x20\x57\x6F\x72\x6C\x64\x33\x30\x30",
				"\x48\x65\x6C\x6C\x6F\x20\x57\x6F\x72\x6C\x64\x34\x30\x30",
				"\x48\x65\x6C\x6C\x6F\x20\x57\x6F\x72\x6C\x64\x35\x30\x30",
				"\x48\x65\x6C\x6C\x6F\x20\x57\x6F\x72\x6C\x64\x36\x30\x30",
				"\x48\x65\x6C\x6C\x6F\x20\x57\x6F\x72\x6C\x64\x37\x30\x30",
				"\x48\x65\x6C\x6C\x6F\x20\x57\x6F\x72\x6C\x64\x38\x30\x30",
				"\x48\x65\x6C\x6C\x6F\x20\x57\x6F\x72\x6C\x64\x39\x30\x30"
		};*/
		int total_input = 0;

		std::ifstream file("C:\\Users\\Pentest1\\Desktop\\data.csv");
		std::string line;

		while (std::getline(file, line) && total_input < 1024) {
			std::istringstream ss(line);
			std::string token;
			int col = 0;

			while (std::getline(ss, token, ',') && col < 14) {
				int value = std::stoi(token);

				if (value < 0 || value > 255) {
					std::cerr << "Invalid value at line " << total_input << ", col " << col << ": " << value << std::endl;
					return 1;
				}

				input[total_input][col] = static_cast<unsigned char>(value);
				++col;
			}

			if (col != 14) {
				std::cerr << "Line " << total_input << " does not have exactly 14 values.\n";
				return 1;
			}

			++total_input;
		}

		std::cout << "Loaded " << total_input << " rows from CSV.\n";

		// (Optional) Print the loaded data
		for (int i = 0; i < total_input; ++i) {
			for (int j = 0; j < 14; ++j) {
				std::cout << static_cast<int>(input[i][j]) << (j < 13 ? ',' : '\n');
			}
		}

		for (int i = 0; i < total_input; ++i) {
			unsigned char* row = input[i];

			Triangle2D t1 = {
				{ row[0], row[1] }, { row[2], row[3] }, { row[4], row[5] }, row[12]
			};
			Triangle2D t2 = {
				{ row[6], row[7] }, { row[8], row[9] }, { row[10], row[11] }, row[13]
			};

			triangleList.push_back(t1);
			triangleList.push_back(t2);
		}
		ShowWindow(hwnd, SW_SHOW);
		UpdateWindow(hwnd);

		MSG msg = {};
		while (GetMessage(&msg, NULL, 0, 0)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		int lable = 0;
		msg_index = 0;
		//total_input = 10;
		srand(static_cast<unsigned int>(time(0)));
		while (1)
		{
			int tx_len = 1024;
			msg_index = (msg_index + (rand() % total_input)) % total_input;
			lable++;
			// Define prefix, suffix, and pattern from strings
			const char* prefix_str = "0xFF,0X00,0XFF,0x00,0x00,0xFF,0x00,0XFF";
			const char* suffix_str = "";//"0x00,0xFF,0x00,0xFF,0xFF,0x00,0xFF,0x00";
			const char* pattern_str = "0x00,0xFF,0x00,0x00,0xFF,0x00,0x00,0x00";

			unsigned char prefix[64], suffix[64], pattern[64];
			int prefix_len = parse_hex_pattern(prefix_str, prefix, 64);
			int suffix_len = parse_hex_pattern(suffix_str, suffix, 64);
			int pattern_len = parse_hex_pattern(pattern_str, pattern, 64);
			int actual_len = 0;
			unsigned char* TxBuffer = build_tx_buffer(input[msg_index], 14, prefix, prefix_len, suffix, suffix_len, tx_len, 1, &actual_len);
			if (!TxBuffer) {
				fprintf(stderr, "Failed to build TxBuffer.\n");
				return 1;
			}

			//printf("Generated TxBuffer (%d bytes).\n", actual_len);
			//print_tx_buffer(TxBuffer, actual_len, input[msg_index], 15, prefix_len, suffix_len, 1);
			/*
			if (pattern_len > 0) {
				std::vector<int> matches = searchPatternMultiple(TxBuffer, tx_len, pattern, pattern_len);

				std::cout << "Pattern found at indices: ";
				for (int index : matches) {
					std::cout << index << " ";
				}
				std::cout << std::endl;
			}
			*/
			std::this_thread::sleep_for(std::chrono::microseconds(1));
			status = FT_GetStatus(myDevice.ftHandle, &RxBytes, &TxBytes, &EventDWord);
			if ((status == FT_OK) && (TxBytes == 0))
			{
				status = FT_Write(myDevice.ftHandle, TxBuffer, 256, &BytesWritten);
				if (status == FT_OK)
				{
					// FT_Write OK  
					//std::cout << "\n Bytes Write " << BytesWritten << " Byte 0x" << byteToHex(TxBuffer[0]) << " 0x" << byteToHex(TxBuffer[1]) << " 0x" << byteToHex(TxBuffer[2]) << " 0x" << byteToHex(TxBuffer[3]) << " last byte 0x" << byteToHex(TxBuffer[255]);
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
