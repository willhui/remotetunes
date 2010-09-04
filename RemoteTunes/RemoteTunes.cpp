/*
 * RemoteTunes
 * Copyright (C) 2010 Will Hui
 *
 * Distributed under the terms of the MIT license.
 * See LICENSE file for details.
 *
 * TODO:
 * - Install a global hook to detect when iTunes launches
 *   and terminates. This allows us to refresh the COM
 *   handle automatically when necessary. It could also let us
 *   avoid the iTunes complaint on exit that other apps
 *   are still using the iTunes API.
 * - Consolidate the 64-bit UI. Basically strip the 32-bit
 *   version of its GUI and forward messages from the 64-bit
 *   "master" instance to the 32-bit slave. All hook DLLs
 *   (both 32- and 64-bit) should send media commands directly
 *   to the 64-bit master app.
 */

#include "stdafx.h"
#include "RemoteTunes.h"
#include <atlbase.h>
#include <atlcom.h>
#include "objbase.h"
#include "iTunesCOMInterface.h"

#include <Shellapi.h>
#include <commctrl.h>
#include <winbase.h>
#include <string>

#define MAX_LOADSTRING 100

#define MY_MSG		(WM_APP+0)
#define MY_ENABLE	(WM_APP+1)
#define MY_DISABLE	(WM_APP+2)
#define MY_ABOUT	(WM_APP+3)
#define MY_QUIT		(WM_APP+4)


// Global Variables:
static HINSTANCE hInst;                        // Current instance
static TCHAR szTitle[MAX_LOADSTRING];          // The title bar text
static TCHAR szWindowClass[MAX_LOADSTRING];    // the main window class name

// Forward declarations of functions included in this code module:
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK	About(HWND, UINT, WPARAM, LPARAM);

static const TCHAR * const APP_NAME = TEXT("RemoteTunes");
static const TCHAR * const APP_VERSION = TEXT("1.0");
static const TCHAR * const DLL_FILE = TEXT("RemoteTunesHook.dll");

#ifdef _M_IX86

// x86
static const TCHAR * APP_ARCH = TEXT("32-bit");

#else

// x64
static const TCHAR * const APP_ARCH = TEXT("64-bit");

#endif

typedef bool (WINAPI *InstallHookFn)(HWND);
typedef void (WINAPI *RemoveHookFn)(void);

static HWND appWindow = NULL;
static HINSTANCE dllInstance = NULL;
static bool isHookInstalled = false;
static InstallHookFn InstallHook = NULL;
static RemoveHookFn RemoveHook = NULL;
static NOTIFYICONDATA niData;
static IiTunes *iTunes = NULL;

static void EnableHook()
{
	// Load our DLL.
	if (!dllInstance)
	{
		dllInstance = LoadLibrary(DLL_FILE);
		if (!dllInstance)
		{
			MessageBox(NULL, TEXT("Unable to load hook DLL."), TEXT("Error"), MB_OK);
			return;
		}
	}

	// Locate the exported install/remove functions.
	if (!InstallHook || !RemoveHook)
	{
		InstallHook = (InstallHookFn) GetProcAddress(dllInstance, (LPCSTR) MAKEINTRESOURCE(1));
		RemoveHook = (RemoveHookFn) GetProcAddress(dllInstance, (LPCSTR) MAKEINTRESOURCE(2));
		if (!InstallHook || !RemoveHook)
		{
			MessageBox(NULL,
				TEXT("Unable to retrieve install/remove procedures from hook DLL."),
				TEXT("Error"),
				MB_OK);
			return;
		}
	}

	// Inject the DLL into the rest of the system.
	if (!isHookInstalled)
	{
		if (InstallHook(appWindow))
		{
			isHookInstalled = true;
		}
	}
}

static void DisableHook()
{
	if (isHookInstalled)
	{
		RemoveHook();
		isHookInstalled = false;
	}
}

// Set the current working directory to the same one the application is in.
void ChangeToAppPath()
{
	WCHAR appPath[MAX_PATH] = TEXT("");
	GetModuleFileName(0, appPath, sizeof(appPath) - 1);
	std::wstring appDir = appPath;
	appDir = appDir.substr(0, appDir.rfind(TEXT("\\")));
	SetCurrentDirectory(appDir.c_str());
}

static void InstallTrayIcon(HWND hWnd, HINSTANCE hInstance)
{
	ZeroMemory(&niData, sizeof(NOTIFYICONDATA));
	niData.uVersion = NOTIFYICON_VERSION_4;
	niData.cbSize = sizeof(NOTIFYICONDATA);
	niData.hWnd = hWnd;
	niData.uID = 1;
	niData.uFlags = NIF_ICON | NIF_MESSAGE;
	niData.hIcon = LoadIcon(hInstance, (LPCTSTR)IDI_REMOTETUNES);
	niData.uCallbackMessage = MY_MSG;
	Shell_NotifyIcon(NIM_ADD, &niData);
}

static void SetNormalMenuItem(MENUITEMINFO *item, INT itemID, TCHAR *text)
{
	ZeroMemory(item, sizeof(MENUITEMINFO));
	item->cbSize = sizeof(MENUITEMINFO);
	item->fMask = MIIM_ID | MIIM_FTYPE | MIIM_STRING;
	item->wID = itemID;
	item->fType = MFT_STRING;
	item->dwTypeData = text;
	item->cch = (UINT)_tcslen(text) + 1;
}

static void SetCheckedMenuItem(MENUITEMINFO *item, INT itemID, TCHAR *text, bool isChecked)
{
	SetNormalMenuItem(item, itemID, text);
	item->fMask |= MIIM_CHECKMARKS | MIIM_STATE;
	item->fType |= MFT_RADIOCHECK;
	item->fState = (isChecked ? MFS_CHECKED : 0);
	item->hbmpChecked = NULL;
	item->hbmpUnchecked = NULL;
}

static void iTunesConnect()
{
	HRESULT result = ::CoCreateInstance(CLSID_iTunesApp, NULL, CLSCTX_LOCAL_SERVER,
		IID_IiTunes, (PVOID *) &iTunes);
	if (result != S_OK)
	{
		MessageBox(NULL, TEXT("Could not connect to iTunes COM server."),
			TEXT("Error"), MB_OK);
		iTunes = NULL;
	}
}

static void ShowContextMenu(HWND hWnd)
{
	POINT pt;
	GetCursorPos(&pt);
	HMENU hMenu = CreatePopupMenu();
	if (hMenu)
	{
		MENUITEMINFO item;
		SetCheckedMenuItem(&item, MY_ENABLE, TEXT("Enable"), isHookInstalled);
		InsertMenuItem(hMenu, 1, TRUE, &item);
		SetCheckedMenuItem(&item, MY_DISABLE, TEXT("Disable"), !isHookInstalled);
		InsertMenuItem(hMenu, 2, TRUE, &item);
		SetNormalMenuItem(&item, MY_ABOUT, TEXT("About"));
		InsertMenuItem(hMenu, 3, TRUE, &item);
		SetNormalMenuItem(&item, MY_QUIT, TEXT("Quit"));
		InsertMenuItem(hMenu, 4, TRUE, &item);

		// We must set our window to the foreground or the menu won't
		// disappear when it should.
		SetForegroundWindow(hWnd);
		TrackPopupMenu(hMenu, TPM_BOTTOMALIGN, pt.x, pt.y, 0, hWnd, NULL);
		DestroyMenu(hMenu);
	}
}

static void SetVersionString(HWND aboutBox)
{
	static const int BUFSIZE = 50;
	TCHAR buf[BUFSIZE];
	_stprintf_s(buf, BUFSIZE, TEXT("%s, Version %s (%s)"), APP_NAME, APP_VERSION, APP_ARCH);

	const HWND label = GetDlgItem(aboutBox, IDC_VERSION);
	SetWindowText(label, buf);
}

int APIENTRY _tWinMain(HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPTSTR    lpCmdLine,
	int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	MSG msg;
	HACCEL hAccelTable;

	ChangeToAppPath();

	// Initialize global strings
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_REMOTETUNES, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// Perform application initialization:
	if (!InitInstance (hInstance, nCmdShow))
	{
		return FALSE;
	}
	
	CoInitialize(NULL);
	iTunesConnect();
	EnableHook();

	hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_REMOTETUNES));

	// Main message loop:
	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	DisableHook();
	CoUninitialize();
	return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
//  COMMENTS:
//
//    This function and its usage are only necessary if you want this code
//    to be compatible with Win32 systems prior to the 'RegisterClassEx'
//    function that was added to Windows 95. It is important to call this function
//    so that the application will get 'well formed' small icons associated
//    with it.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_REMOTETUNES));
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= MAKEINTRESOURCE(IDC_REMOTETUNES);
	wcex.lpszClassName	= szWindowClass;
	wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassEx(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	HWND hWnd;

	hInst = hInstance; // Store instance handle in our global variable

	hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);

	if (!hWnd)
	{
		return FALSE;
	}
	
	InstallTrayIcon(hWnd, hInstance);
	//ShowWindow(hWnd, nCmdShow);
	//UpdateWindow(hWnd);

	appWindow = hWnd;
	return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND	- process the application menu
//  WM_PAINT	- Paint the main window
//  WM_DESTROY	- post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	PAINTSTRUCT ps;
	HDC hdc;

	switch (message)
	{
	case MY_MSG:
		// Handle tray icon events.
		switch (lParam)
		{
		case WM_LBUTTONDBLCLK:
			break;
		case WM_RBUTTONDOWN:
		case WM_CONTEXTMENU:
			ShowContextMenu(hWnd);
			break;
		}
		break;

	case WM_COMMAND:
		wmId    = LOWORD(wParam);
		wmEvent = HIWORD(wParam);
		// Parse the menu selections:
		switch (wmId)
		{
		case MY_ENABLE:
			EnableHook();
			break;
		case MY_DISABLE:
			DisableHook();
			break;
		case MY_ABOUT:
		case IDM_ABOUT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), NULL, About);
			break;
		case MY_QUIT:
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		break;

	case WM_USER:
		switch (wParam)
		{
		case APPCOMMAND_MEDIA_PLAY:
			if (iTunes->Play() != S_OK)
			{
				iTunesConnect();
				if (iTunes != NULL)
					iTunes->Play();
			}
			break;
		case APPCOMMAND_MEDIA_PAUSE:
			if (iTunes->Pause() != S_OK)
			{
				iTunesConnect();
				if (iTunes != NULL)
					iTunes->Pause();
			}
			break;
		case APPCOMMAND_MEDIA_STOP:
			if (iTunes->Stop() != S_OK)
			{
				iTunesConnect();
				if (iTunes != NULL)
					iTunes->Stop();
			}
			break;
		case APPCOMMAND_MEDIA_PREVIOUSTRACK:
			if (iTunes->PreviousTrack() != S_OK)
			{
				iTunesConnect();
				if (iTunes != NULL)
					iTunes->PreviousTrack();
			}
			break;
		case APPCOMMAND_MEDIA_NEXTTRACK:
			if (iTunes->NextTrack() != S_OK)
			{
				iTunesConnect();
				if (iTunes != NULL)
					iTunes->NextTrack();
			}
			break;
		default:
			break;
		}
		break;

	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		// TODO: Add any drawing code here...
		EndPaint(hWnd, &ps);
		break;

	case WM_DESTROY:
		niData.uFlags = 0;
		Shell_NotifyIcon(NIM_DELETE, &niData);
		PostQuitMessage(0);
		break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		SetVersionString(hDlg);
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}
