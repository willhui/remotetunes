/*
 * RemoteTunes
 * Copyright (C) 2010 Will Hui
 *
 * Distributed under the terms of the MIT license.
 * See LICENSE file for details.
 */

#include "stdafx.h"

HMODULE dllHandle = NULL;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	dllHandle = hModule;
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
		case DLL_THREAD_ATTACH:
		case DLL_THREAD_DETACH:
		case DLL_PROCESS_DETACH:
			break;
	}
	return TRUE;
}
