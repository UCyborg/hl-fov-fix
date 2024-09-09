#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN

#include <math.h>
#include <stdlib.h>
#include <Windows.h>
#include "detours.h"

#pragma comment(lib, "Version")

#define SCREEN_WIDTH	640.0f
#define SCREEN_HEIGHT	480.0f
#define STANDARD_ASPECT_RATIO	(SCREEN_WIDTH/SCREEN_HEIGHT)

#define M_PI		3.14159265358979323846

#define DEG2RAD(a) (a * M_PI) / 180.0F
#define RAD2DEG(a) (((a) * 180.0f) / M_PI)

void *memmem(const void *l, size_t l_len, const void *s, size_t s_len)
{
	register char *cur, *last;
	const char *cl = (const char *)l;
	const char *cs = (const char *)s;

	/* we need something to compare */
	if (!l_len || !s_len)
		return NULL;

	/* "s" must be smaller or equal to "l" */
	if (l_len < s_len)
		return NULL;

	/* special case where s_len == 1 */
	if (s_len == 1)
		return memchr(l, (int)*cs, l_len);

	/* the last position where it's possible to find "s" in "l" */
	last = (char *)cl + l_len - s_len;

	for (cur = (char *)cl; cur <= last; cur++)
		if (cur[0] == cs[0] && !memcmp(cur, cs, s_len))
			return cur;

		return NULL;
}

float *fov;
void (*O_CalcFov)(void);
void H_CalcFov(void)
{
	float aspectRatio = (float)*(int *)0x3CE3AC8 / *(int *)0x3CE3ACC;

	if (aspectRatio > STANDARD_ASPECT_RATIO)
	{
		*fov = RAD2DEG(2 * atan((aspectRatio / STANDARD_ASPECT_RATIO) * tan(DEG2RAD(*fov) * 0.5f)));
	}

	O_CalcFov();
}

void InstallFovHook(void)
{
	DWORD_PTR dwPatchBase;
	static const BYTE wantedBytes[] = { 0x85, 0xC0, 0x74, 0x0D, 0x8D, 0x44, 0x24, 0x24, 0x50 };

	dwPatchBase = (DWORD_PTR)memmem((void *)0x1D01000, 0x2000, wantedBytes, sizeof(wantedBytes));
	if (dwPatchBase)
	{
		(DWORD_PTR)fov = *(DWORD_PTR *)(dwPatchBase - 9);
		(PBYTE)O_CalcFov = DetourFunction((PBYTE)(dwPatchBase - 5), (PBYTE)H_CalcFov);
	}
}

void (*O_UnpackCodeNotifyHook)(char *, char **, void *, int);
void H_UnpackCodeNotifyHook(char *dll, char **pDll, void *arg3, int arg4)
{
	if (O_CalcFov)
	{
		DetourRemove((PBYTE)O_CalcFov, (PBYTE)H_CalcFov);
		O_CalcFov = NULL;
	}

	O_UnpackCodeNotifyHook(dll, pDll, arg3, arg4);
	InstallFovHook();
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	if (fdwReason == DLL_PROCESS_ATTACH)
	{
		char szPath[MAX_PATH];
		DWORD dwSize;
		DWORD dwHandle;
		void *pVersionInfo;
		char *pStringFileInfoValue;
		UINT fileInfoLen;

		DisableThreadLibraryCalls(hinstDLL);

		GetModuleFileName(NULL, szPath, sizeof(szPath));

		if (!(dwSize = GetFileVersionInfoSize(szPath, &dwHandle)))
		{
			goto fail;
		}

		if (!(pVersionInfo = malloc(dwSize)))
		{
			goto fail;
		}

		if (!GetFileVersionInfo(szPath, dwHandle, dwSize, pVersionInfo)
			|| !VerQueryValue(pVersionInfo, "\\StringFileInfo\\040904b0\\FileDescription", (LPVOID *)&pStringFileInfoValue, &fileInfoLen)
			|| strcmp(pStringFileInfoValue, "Half-Life Launcher")
			|| !VerQueryValue(pVersionInfo, "\\StringFileInfo\\040904b0\\FileVersion", (LPVOID *)&pStringFileInfoValue, &fileInfoLen)
			|| strcmp(pStringFileInfoValue, "1, 1, 1, 0"))
		{
			free(pVersionInfo);
fail:		return FALSE;
		}

		free(pVersionInfo);

		if (!((PBYTE)O_UnpackCodeNotifyHook = DetourFunction((PBYTE)0x40E8F0, (PBYTE)H_UnpackCodeNotifyHook))) goto fail;
		InstallFovHook();
	}
	else if (fdwReason == DLL_PROCESS_DETACH)
	{
		if (O_CalcFov) DetourRemove((PBYTE)O_CalcFov, (PBYTE)H_CalcFov);
		if (O_UnpackCodeNotifyHook) DetourRemove((PBYTE)O_UnpackCodeNotifyHook, (PBYTE)H_UnpackCodeNotifyHook);
	}
	return TRUE;
}
