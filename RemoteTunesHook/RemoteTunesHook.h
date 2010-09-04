#pragma once

// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the REMOTETUNESHOOK_EXPORTS
// symbol defined on the command line. This symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// REMOTETUNESHOOK_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
#ifdef REMOTETUNESHOOK_EXPORTS

#define REMOTETUNESHOOK_API __declspec(dllexport)

extern HMODULE dllHandle;

#else

#define REMOTETUNESHOOK_API __declspec(dllimport)

#endif

// This variable is shared among all actively loaded instances of the DLL.
extern HWND mediaCmdRelayWindowHandle;

REMOTETUNESHOOK_API bool WINAPI InstallHook(HWND relayWindow);
REMOTETUNESHOOK_API void WINAPI RemoveHook(void);
