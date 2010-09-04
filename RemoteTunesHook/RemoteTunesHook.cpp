/*
 * RemoteTunes
 * Copyright (C) 2010 Will Hui
 *
 * Distributed under the terms of the MIT license.
 * See LICENSE file for details.
 */

#include "stdafx.h"
#include "RemoteTunesHook.h"

#include <cstdio>
#include <cstdlib>
#include <tchar.h>

static const int ERROR_STRING_SIZE = 1024;

static bool isHookInstalled = false;
static HHOOK hook = NULL;

#pragma data_seg(".shared")
HWND mediaCmdRelayWindowHandle = NULL;
#pragma data_seg()
#pragma comment(linker, "/section:.shared,rws")

static void Complain(const TCHAR *s)
{
	const DWORD err = GetLastError();
	TCHAR buf[ERROR_STRING_SIZE];
	_stprintf_s(buf, ERROR_STRING_SIZE, TEXT("%s (error %i)"), s, err);
	MessageBoxW(NULL, buf, TEXT("Error"), MB_OK);
}

static LRESULT CALLBACK CallWndProc(int nCode, WPARAM wParam, LPARAM lParam) {
	int ret = 0;

	if (nCode == HSHELL_APPCOMMAND)
	{
		int cmd = GET_APPCOMMAND_LPARAM(lParam);
		switch (cmd)
		{
		case APPCOMMAND_MEDIA_PLAY:
		case APPCOMMAND_MEDIA_PAUSE:
		case APPCOMMAND_MEDIA_STOP:
		case APPCOMMAND_MEDIA_PREVIOUSTRACK:
		case APPCOMMAND_MEDIA_NEXTTRACK:
			PostMessage(mediaCmdRelayWindowHandle, WM_USER, cmd, NULL);
			ret = 1;
			break;
		}
	}

	// Return non-zero to block the message from being passed on.
	// Otherwise, invoke CallNextHookEx().
	if (ret > 0)
		return ret;
	else
		return CallNextHookEx(hook, nCode, wParam, lParam);
}

REMOTETUNESHOOK_API bool WINAPI InstallHook(HWND relayWindow)
{
	if (!isHookInstalled)
	{
		hook = SetWindowsHookEx(WH_SHELL, CallWndProc, (HINSTANCE)dllHandle, 0);
		if (hook)
		{
			isHookInstalled = true;
			mediaCmdRelayWindowHandle = relayWindow;
		}
		else
		{
			Complain(TEXT("Could not install the global message receiver hook."));
		}
	}
	return isHookInstalled;
}

REMOTETUNESHOOK_API void WINAPI RemoveHook(void)
{
	if (isHookInstalled)
	{
		UnhookWindowsHookEx(hook);
		isHookInstalled = false;
	}
}