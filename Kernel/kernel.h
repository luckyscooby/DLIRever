/*
	Citrino Kernel DLL
	General Operations
*/

#pragma once

#ifdef QUARTZKERNEL_EXPORTS
#define QUARTZKERNEL_API __declspec(dllexport)
#else
#define QUARTZKERNEL_API __declspec(dllimport)
#endif

#define WIN32_LEAN_AND_MEAN

#include <Windows.h>
#include <windowsx.h>
#include <d3d9.h>
#include <iostream>
#include <cstring>

#pragma comment (lib, "d3d9.lib")

namespace QuartzKernel
{
	// Human Input/Output
	namespace HIO
	{
		HRESULT hPrivateResult;

		LPDIRECT3D9 d3d;			// Pointer to Direct3D interface;
		LPDIRECT3DDEVICE9 d3ddev;	// Pointer to device class;

#define SCREEN_WIDTH	800
#define SCREEN_HEIGHT	600

		QUARTZKERNEL_API void InitD3D(HWND);			// Sets up and initializes Direct3D;
		QUARTZKERNEL_API void RenderFrame(void);		// Renders a single frame;
		QUARTZKERNEL_API void CleanD3D(void);		// Closes Direct3D and releases memory;

		// Console forecolor manipulation via SetConsoleTextAttribute()
		enum
		{
			BLACK = 0,
			DARKBLUE = FOREGROUND_BLUE,
			DARKGREEN = FOREGROUND_GREEN,
			DARKCYAN = FOREGROUND_GREEN | FOREGROUND_BLUE,
			DARKRED = FOREGROUND_RED,
			DARKMAGENTA = FOREGROUND_RED | FOREGROUND_BLUE,
			DARKYELLOW = FOREGROUND_RED | FOREGROUND_GREEN,
			DARKGRAY = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
			GRAY = FOREGROUND_INTENSITY,
			BLUE = FOREGROUND_INTENSITY | FOREGROUND_BLUE,
			GREEN = FOREGROUND_INTENSITY | FOREGROUND_GREEN,
			CYAN = FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_BLUE,
			RED = FOREGROUND_INTENSITY | FOREGROUND_RED,
			MAGENTA = FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_BLUE,
			YELLOW = FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN,
			WHITE = FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
		};

		// Outputs LPSZ program message to standard console, supporting foreground color (optional) and output to file (optional);
		QUARTZKERNEL_API void Output(LPTSTR, WORD wColor = GRAY, HANDLE hLogFile = NULL);

		// Inputs LPSZ user message from standard console;
		QUARTZKERNEL_API void Input(LPTSTR);

		// Halts console flux until user presses any key;
		QUARTZKERNEL_API inline void PauseConsole(void);
	}

	// File System
	namespace FS
	{
		HRESULT hPrivateResult;

		QUARTZKERNEL_API bool CheckFileExistence(LPTSTR);
	};

	// System
	namespace SYS
	{
		HRESULT hPrivateResult;

#define MAX_PLUGIN 1	// Number of maximum allowed simultaneous modules

		struct st_modulealloc {
			LPTSTR lpszDll;
			HANDLE hThread;
			HMODULE hDll;
		};

		// Loads a DLL module (desinged for Citrino only) and initializes it on a new thread;
		QUARTZKERNEL_API void LoadModule(st_modulealloc*);

		// Unload a DLL module terminating it's thread and cleaning up;
		QUARTZKERNEL_API void DeloadModule(st_modulealloc*);

		// Halts current thread preventing CPU overload. Designed for intensive loops;
		QUARTZKERNEL_API void YieldCPU(void);
	};

	// Error/Execption Handling
	namespace EH
	{

	};
}

