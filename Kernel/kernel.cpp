#include "kernel.h"

namespace QuartzKernel
{
	namespace HIO
	{
		void InitD3D(HWND hWnd)
		{
			d3d = Direct3DCreate9(D3D_SDK_VERSION);	// Create Direct3D interface;

			D3DPRESENT_PARAMETERS d3dpp;	// Create a struct to hold various device information;
			ZeroMemory(&d3dpp, sizeof(d3dpp));
			d3dpp.Windowed = false;
			d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;	// Discard old frames;
			d3dpp.hDeviceWindow = hWnd;	// Set the window to be used by Direct3D;
			//d3dpp.BackBufferFormat = D3DFMT_X8B8G8R8;	// Set back buffer format to 32-bit;
			d3dpp.BackBufferWidth = SCREEN_WIDTH;
			d3dpp.BackBufferHeight = SCREEN_HEIGHT;

			hPrivateResult = d3d->CreateDevice(
				D3DADAPTER_DEFAULT,
				D3DDEVTYPE_HAL,
				hWnd,
				D3DCREATE_SOFTWARE_VERTEXPROCESSING,
				&d3dpp,
				&d3ddev);
		}

		void RenderFrame(void)
		{
			// Clear the window to black;
			d3ddev->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);

			d3ddev->BeginScene();

			// Do rendering on the back buffer here //

			d3ddev->EndScene();

			// Displays the created frame;
			d3ddev->Present(NULL, NULL, NULL, NULL);
		}

		void CleanD3D(void)
		{
			// Close and release the 3D device;
			d3ddev->Release();

			// Close and release Direct3D;
			d3d->Release();
		}

		void Output(LPTSTR msg, WORD wColor, HANDLE hLogFile)
		{
			HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

			// Time stamp
			SYSTEMTIME localTime;
			GetLocalTime(&localTime);
			TCHAR time[32];
			sprintf_s(time, "\t\t<%02d:%02d:%02d - %02d/%02d/%d>",
				localTime.wHour,
				localTime.wMinute,
				localTime.wSecond,
				localTime.wMonth,
				localTime.wDay,
				localTime.wYear);

			// Get previous console output attributes
			CONSOLE_SCREEN_BUFFER_INFO csbiInfo;
			GetConsoleScreenBufferInfo(hConsole, &csbiInfo);

			// Set parameter color to output
			SetConsoleTextAttribute(hConsole, wColor);

			// Write message string
			WriteConsole(hConsole, msg, strlen(msg), NULL, NULL);
			if (hLogFile != NULL) {
				WriteFile(hLogFile, msg, strlen(msg), NULL, NULL);
			}

			// Revert to original console attributes
			SetConsoleTextAttribute(hConsole, csbiInfo.wAttributes);

			//CloseHandle(hConsole);
		}

		void Input(LPTSTR msg)
		{
			HANDLE hConsole = GetStdHandle(STD_INPUT_HANDLE);
			DWORD dwRead;

			ReadConsole(hConsole, msg, MAX_PATH * sizeof(TCHAR), &dwRead, NULL);
		}

		void PauseConsole(void)
		{
			std::cin.get();
		}
	}

	namespace FS
	{
		bool CheckFileExistence(LPTSTR path)
		{
			HANDLE			hFind;

			hFind = FindFirstFile(path, NULL);

			if (hFind == INVALID_HANDLE_VALUE) {
				return false;
			}
			else {
				FindClose(hFind);
				return true;
			}
		}
	}
	
	namespace SYS
	{
		void LoadModule(st_modulealloc* moall)
		{
			typedef						LPTHREAD_START_ROUTINE(WINAPI *LPMODULEPROC)(void);
			LPMODULEPROC				lpProc;

			moall->hDll = LoadLibrary(moall->lpszDll);

			if (moall->hDll != NULL) {
				HIO::Output("OK");
				lpProc = (LPMODULEPROC)GetProcAddress(moall->hDll, MAKEINTRESOURCEA(1));

				if (lpProc != NULL) {
					moall->hThread = CreateThread(NULL, 0, lpProc(), NULL, 0, NULL);
					return;
				}
				else {
					HIO::Output("FAILED TO CALL _INITRUNTIME FROM DLL.\n", HIO::RED);
					FreeLibrary(moall->hDll);
					return;
				}
			} else {
				return;
			}
		}

		void DeloadModule(st_modulealloc* moall)
		{
			// First we must send a signal to thread for it to end itself.

			// Now we deload
			FreeLibrary(moall->hDll);
		}
	}
}