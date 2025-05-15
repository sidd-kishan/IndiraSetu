#include <iostream>
#include <iomanip>
#include <chrono>   // for std::chrono::milliseconds
#include <thread>   // for std::this_thread::sleep_for
#include <windows.h>
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

float g_pan = 0.0f;
float g_tilt = 0.0f;
float g_zoom = 1.0f;

POINT g_lastMousePos;
bool g_mouseDown = false;

float tx = 100.0f;
float ty = 180.0f;


struct Triangle {
	int v0, v1, v2;
};
std::vector<Triangle> triangles;

struct Vector3 {
	float x, y, z;
	Vector3() : x(0), y(0), z(0) {}
	Vector3(float x, float y, float z) : x(x), y(y), z(z) {}
	//int color_r, color_g, color_b;
};
static std::vector<Vector3> vertices;

int color = 0;
// Bresenham's Line Algorithm to draw a line
void drawLine(HDC hdc, int x0, int y0, int x1, int y1) {
	int dx = abs(x1 - x0);
	int dy = abs(y1 - y0);
	int sx = (x0 < x1) ? 1 : -1;
	int sy = (y0 < y1) ? 1 : -1;
	int err = dx - dy;
	while (true) {
		SetPixel(hdc, x0, y0, RGB((color / 10) % 255, (color / 100) % 255, (color / 50) % 255));  // Set pixel color to white
		if (x0 == x1 && y0 == y1) break;
		int e2 = err * 2;
		if (e2 > -dy) { err -= dy; x0 += sx; }
		if (e2 < dx) { err += dx; y0 += sy; }
	}
	color += 1;
}

bool parseOBJ(const std::string& filename, std::vector<Vector3>& vertices, std::vector<Triangle>& triangles) {
	std::ifstream file(filename);
	if (!file.is_open()) {
		std::cerr << "Failed to open OBJ file!" << std::endl;
		return false;
	}

	std::string line;
	while (std::getline(file, line)) {
		std::stringstream ss(line);
		std::string prefix;
		ss >> prefix;

		if (prefix == "v") {
			float x, y, z;
			ss >> x >> y >> z;
			vertices.emplace_back(x, y, z);
		}
		else if (prefix == "f") {
			std::vector<int> faceIndices;
			std::string vertexRef;
			while (ss >> vertexRef) {
				std::stringstream vss(vertexRef);
				std::string indexStr;
				std::getline(vss, indexStr, '/');  // Only vertex index
				int index = std::stoi(indexStr);
				faceIndices.push_back(index - 1); // OBJ is 1-based
			}
			// Triangulate if face has more than 3 vertices
			if (faceIndices.size() >= 3) {
				for (size_t i = 1; i < faceIndices.size() - 1; ++i) {
					triangles.push_back({ faceIndices[0], faceIndices[i], faceIndices[i + 1] });
				}
			}
		}
	}

	return true;
}


Vector3 transformVertex(const Vector3& vertex, float pan, float tilt, float zoom, float tx, float ty) {
	float cosPan = cos(pan), sinPan = sin(pan);
	float cosTilt = cos(tilt), sinTilt = sin(tilt);

	float x = vertex.x * cosTilt + vertex.z * sinTilt;
	float y = vertex.y * cosPan - vertex.z * sinPan;
	float z = -vertex.x * sinTilt + vertex.z * cosTilt;

	x *= zoom;
	y *= zoom;

	x = x + tx;
	y = -y + ty;  // Flip Y here

	return Vector3(x, y, z);
}


// Function to create edges by connecting consecutive vertices
std::vector<std::pair<int, int>> createEdges(const std::vector<Vector3>& vertices, bool closeLoop = true) {
	std::vector<std::pair<int, int>> edges;
	int numVertices = vertices.size();

	if (numVertices < 2) return edges;

	for (int i = 0; i < numVertices - 1; ++i) {
		edges.push_back({ i, i + 1 });  // Connect consecutive vertices
	}

	if (closeLoop && numVertices > 2) {
		edges.push_back({ numVertices - 1, 0 });  // Optionally connect the last vertex to the first (for a closed loop)
	}

	return edges;
}

Vector3 computeNormal(const Vector3& a, const Vector3& b, const Vector3& c) {
	Vector3 u(b.x - a.x, b.y - a.y, b.z - a.z);
	Vector3 v(c.x - a.x, c.y - a.y, c.z - a.z);
	return Vector3(
		u.y * v.z - u.z * v.y,
		u.z * v.x - u.x * v.z,
		u.x * v.y - u.y * v.x
	);
}

bool isVisibleFromOverhead(const Vector3& a, const Vector3& b, const Vector3& c) {
	Vector3 normal = computeNormal(a, b, c);
	return normal.z < 0;  // Z up, right-hand system: front face has negative Z normal
}


void renderWireframe(HDC hdc, const std::vector<Vector3>& vertices, const std::vector<std::pair<int, int>>& edges,
	const std::vector<Triangle>& triangles,
	float pan, float tilt, float zoom, float tx, float ty) {
	int edge_count = 0, triangle_count = 0;
	for (const auto& edge : edges) {
		const Vector3& v1 = vertices[edge.first];
		const Vector3& v2 = vertices[edge.second];

		// Transform the vertices into 2D space
		Vector3 p1 = transformVertex(v1, pan, tilt, zoom, tx, ty);
		Vector3 p2 = transformVertex(v2, pan, tilt, zoom, tx, ty);

		// Print the coordinates of the line endpoints
		//std::cout << "Line: (" << static_cast<int>(p1.x) << ", " << static_cast<int>(p1.y) << ") to "
		//    << "(" << static_cast<int>(p2.x) << ", " << static_cast<int>(p2.y) << ")" << std::endl;

		// Draw the line between the transformed vertices
		//drawLine(hdc, static_cast<int>(p1.x), static_cast<int>(p1.y), static_cast<int>(p2.x), static_cast<int>(p2.y));
		edge_count += 1;
	}

	// Print triangle screen-space vertex coordinates (no rendering)

	for (const auto& tri : triangles) {
		// Use original 3D coordinates for back-face test
		const Vector3& a = vertices[tri.v0];
		const Vector3& b = vertices[tri.v1];
		const Vector3& c = vertices[tri.v2];

		//std::cout << "Triangle: ("
		//    << static_cast<int>(a.x) << "," << static_cast<int>(a.y) << ") -> ("
		//    << static_cast<int>(b.x) << "," << static_cast<int>(b.y) << ") -> ("
		//    << static_cast<int>(c.x) << "," << static_cast<int>(c.y) << ")\n";

		if (!isVisibleFromOverhead(a, b, c)) continue; // Skip triangles facing away

		// Transform to screen space for drawing
		Vector3 ta = transformVertex(a, pan, tilt, zoom, tx, ty);
		Vector3 tb = transformVertex(b, pan, tilt, zoom, tx, ty);
		Vector3 tc = transformVertex(c, pan, tilt, zoom, tx, ty);

		drawLine(hdc, static_cast<int>(ta.x), static_cast<int>(ta.y), static_cast<int>(tb.x), static_cast<int>(tb.y));
		drawLine(hdc, static_cast<int>(tb.x), static_cast<int>(tb.y), static_cast<int>(tc.x), static_cast<int>(tc.y));
		drawLine(hdc, static_cast<int>(tc.x), static_cast<int>(tc.y), static_cast<int>(ta.x), static_cast<int>(ta.y));

		triangle_count += 1;
	}

	std::cout << edge_count << " trigs: " << triangle_count << "\n";
}

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

void writeTrianglesToCSV(const std::string& filename, const std::vector<Triangle>& triangles,
	const std::vector<Vector3>& vertices, float pan, float tilt, float zoom, float tx, float ty) {
	std::ofstream file(filename);
	if (!file.is_open()) {
		std::cerr << "Failed to open output CSV file!" << std::endl;
		return;
	}

	int triCounter = 0;
	std::vector<float> lineBuffer;
	srand(42); // Seed for reproducible color

	for (const auto& tri : triangles) {
		Vector3 a = transformVertex(vertices[tri.v0], pan, tilt, zoom, tx, ty);
		Vector3 b = transformVertex(vertices[tri.v1], pan, tilt, zoom, tx, ty);
		Vector3 c = transformVertex(vertices[tri.v2], pan, tilt, zoom, tx, ty);

		// Check all x/y < 255
		if (a.x >= 255 || a.y >= 255 || b.x >= 255 || b.y >= 255 || c.x >= 255 || c.y >= 255) continue;
		if (a.x < 0 || a.y < 0 || b.x < 0 || b.y < 0 || c.x < 0 || c.y < 0) continue;

		// Append triangle coords to line
		lineBuffer.push_back(a.x); lineBuffer.push_back(a.y);
		lineBuffer.push_back(b.x); lineBuffer.push_back(b.y);
		lineBuffer.push_back(c.x); lineBuffer.push_back(c.y);

		++triCounter;

		if (triCounter % 2 == 0) {
			// Add fake color (random or based on triangle index)
			std::vector<unsigned char> data = {
			static_cast<unsigned char>(lineBuffer[0]),
			static_cast<unsigned char>(lineBuffer[1]),
			static_cast<unsigned char>(lineBuffer[2]),
			static_cast<unsigned char>(lineBuffer[3]),
			static_cast<unsigned char>(lineBuffer[4]),
			static_cast<unsigned char>(lineBuffer[5])
			};
			int color = crc8(data.data(), static_cast<int>(data.size())) % 256;
			lineBuffer.push_back(color);
			data = {
			static_cast<unsigned char>(lineBuffer[6]),
			static_cast<unsigned char>(lineBuffer[7]),
			static_cast<unsigned char>(lineBuffer[8]),
			static_cast<unsigned char>(lineBuffer[9]),
			static_cast<unsigned char>(lineBuffer[10]),
			static_cast<unsigned char>(lineBuffer[11])
			};
			color = crc8(data.data(), static_cast<int>(data.size())) % 256; 
			lineBuffer.push_back(color);
			// Write 2 triangles per line = 14 values
			for (size_t i = 0; i < lineBuffer.size(); ++i) {
				file << std::setw(3) << std::setfill('0') << static_cast<int>(lineBuffer[i]);
				if (i != lineBuffer.size() - 1) file << ",";
			}
			file << "\n";
			lineBuffer.clear();
		}
	}

	file.close();
	std::cout << "Triangles exported to: " << filename << std::endl;
}


LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	static std::vector<std::pair<int, int>> edges;
	//static std::vector<Triangle> triangles;
	switch (uMsg) {
	case WM_CREATE:
		// Load model once
		if (!parseOBJ("C:\\Users\\Pentest1\\Downloads\\obj\\WOLF.obj", vertices, triangles)) {
			MessageBox(hwnd, L"Failed to load OBJ", L"Error", MB_OK);
		}
		edges = createEdges(vertices);  // Create edges once
		break;

	case WM_LBUTTONDOWN:
		g_mouseDown = true;
		GetCursorPos(&g_lastMousePos);
		ScreenToClient(hwnd, &g_lastMousePos);
		break;

	case WM_LBUTTONUP:
		g_mouseDown = false;
		break;

	case WM_MOUSEMOVE:
		if (g_mouseDown) {
			POINT currentPos;
			GetCursorPos(&currentPos);
			ScreenToClient(hwnd, &currentPos);

			int dx = currentPos.x - g_lastMousePos.x;
			int dy = currentPos.y - g_lastMousePos.y;

			g_pan += dx * 0.01f;   // adjust sensitivity as needed
			g_tilt += dy * 0.01f;

			g_lastMousePos = currentPos;

			InvalidateRect(hwnd, NULL, TRUE);  // Force redraw
		}
		break;

	case WM_MOUSEWHEEL:
		g_zoom += GET_WHEEL_DELTA_WPARAM(wParam) / 420.0f;  // 120 is one notch
		if (g_zoom < 0.1f) g_zoom = 0.1f;
		InvalidateRect(hwnd, NULL, TRUE);
		break;

	case WM_PAINT: {
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hwnd, &ps);

		// Clear screen (fill black background)
		RECT clientRect;
		GetClientRect(hwnd, &clientRect);
		HBRUSH bgBrush = CreateSolidBrush(RGB(0, 0, 0)); // Black background
		FillRect(hdc, &clientRect, bgBrush);
		DeleteObject(bgBrush);

		

		renderWireframe(hdc, vertices, edges, triangles, g_pan, g_tilt, g_zoom, tx, ty);

		EndPaint(hwnd, &ps);
		break;
	}

	case WM_DESTROY:
		writeTrianglesToCSV("C:\\Users\\Pentest1\\Desktop\\output.csv", triangles, vertices, g_pan, g_tilt, g_zoom, tx, ty);
		PostQuitMessage(0);
		break;

	case WM_KEYDOWN:
		switch (wParam) {
		case VK_LEFT:
			g_pan -= 0.1f;
			InvalidateRect(hwnd, NULL, TRUE);
			break;
		case VK_RIGHT:
			g_pan += 0.1f;
			InvalidateRect(hwnd, NULL, TRUE);
			break;
		case VK_UP:
			g_tilt -= 0.1f;
			InvalidateRect(hwnd, NULL, TRUE);
			break;
		case VK_DOWN:
			g_tilt += 0.1f;
			InvalidateRect(hwnd, NULL, TRUE);
			break;
		}
		break;
	default:
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
	return 0;
}



int main()
{
	// Define window class
	const wchar_t CLASS_NAME[] = L"WireframeWindowClass";  // Change to wide string literal

	WNDCLASS wc = {};
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = GetModuleHandle(NULL);
	wc.lpszClassName = CLASS_NAME;

	RegisterClass(&wc);

	// Create window
	HWND hwnd = CreateWindowEx(
		0, CLASS_NAME, L"Wireframe Renderer",  // Change to wide string literal
		WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 320, 240, NULL, NULL, wc.hInstance, NULL);

	if (!hwnd) {
		std::cerr << "Window creation failed!" << std::endl;
		return 1;
	}

	ShowWindow(hwnd, SW_SHOW);

	// Message loop
	MSG msg = {};
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return 0;
}
