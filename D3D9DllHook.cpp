#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <fstream>
#include "Main.h"
#include "Direct3D9ExProxyImpl.h"
#include "jsoncpp-0.y.z/json/json.h"
#include "hooks.h"
#include "icommandline.h"
#include "steam/steam_api.h"
#include "vr_sourcevr.h"

DWORD WINAPI InitHL2MP(HMODULE hModule)
{
	m_Hooks = new Hooks();
	m_VR->Activate();
    return 0;
}


void RenameRegistryKey(const char* oldKeyName, const char* newKeyName)
{
    HKEY hOldKey, hNewKey;
    LONG result;

    result = RegOpenKeyEx(HKEY_CURRENT_USER, oldKeyName, 0, KEY_READ, &hOldKey);
	if (result != ERROR_SUCCESS)
		return;

    result = RegCreateKeyEx(HKEY_CURRENT_USER, newKeyName, 0, NULL, 0, KEY_SET_VALUE | KEY_CREATE_SUB_KEY, NULL, &hNewKey, NULL);

    DWORD valueCount, maxValueNameLen;
    result = RegQueryInfoKey(hOldKey, NULL, NULL, NULL, NULL, NULL, NULL, &valueCount, &maxValueNameLen, NULL, NULL, NULL);
    for( DWORD i = 0; i < valueCount; i++ ) {
        char* valueName = (char*)malloc(maxValueNameLen + 1);
        DWORD valueNameSize = maxValueNameLen + 1;
        DWORD valueType;
        BYTE value[1024];
        DWORD valueSize = sizeof(value);

        result = RegEnumValue(hOldKey, i, valueName, &valueNameSize, NULL, &valueType, value, &valueSize);
        if( result == ERROR_SUCCESS ) {
            RegSetValueEx(hNewKey, valueName, 0, valueType, value, valueSize);
        }
        free(valueName);
    }

    RegCloseKey(hOldKey);
    RegCloseKey(hNewKey);

    RegDeleteKey(HKEY_CURRENT_USER, oldKeyName);
}


bool DoesRegistryKeyExist( const char *subKey )
{
	HKEY hKey;
	LONG result = RegOpenKeyEx( HKEY_CURRENT_USER, subKey, 0, KEY_READ, &hKey );
    
	if( result == ERROR_SUCCESS ) {
		RegCloseKey( hKey ); // Закрываем ключ, если он был успешно открыт
		return true; // Ключ существует
	}
    
	return false; // Ключ не существует
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
		{
			if( !SteamAPI_Init() )
				return TRUE;

			ISteamUtils *pSteamUtils = SteamClient()->GetISteamUtils( SteamAPI_GetHSteamPipe(), "SteamUtils010");

			if( pSteamUtils->IsSteamRunningInVR() )
			{
				if( !DoesRegistryKeyExist( "Software\\Valve\\Source\\hl2mp\\Settings_old" ) )
					RenameRegistryKey( "Software\\Valve\\Source\\hl2mp\\Settings", "Software\\Valve\\Source\\hl2mp\\Settings_old" );

				rename("hl2mp/custom/vr.vpk_","hl2mp/custom/vr.vpk");
				CommandLine()->AppendParm( "-dxlevel 95", NULL );
				CommandLine()->AppendParm( "-sw -w 1280 -h 1024", NULL );
				CommandLine()->AppendParm( "-insecure", NULL );
				CommandLine()->AppendParm( "-novid", NULL );
				CommandLine()->AppendParm( "+mat_queue_mode 0", NULL );
				CommandLine()->AppendParm( "+mat_vrmode_adapter -1", NULL );
				CommandLine()->AppendParm( "+r_flashlightdepthtexture 0", NULL );
				//CommandLine()->AppendParm( "+r_shadowrendertotexture 0", NULL );
				CommandLine()->AppendParm( "+r_teeth 0", NULL );
				CommandLine()->AppendParm( "+mat_antialias 0", NULL );
				CommandLine()->AppendParm( "+building_cubemaps 1", NULL );
				CommandLine()->AppendParm( "+con_enable 1", NULL );
				CommandLine()->AppendParm( "-gamepadui", NULL ); //Делает шрифт больше в VGUI

				CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)InitHL2MP, hModule, 0, NULL);
			}
			else
			{
				RenameRegistryKey("Software\\Valve\\Source\\hl2mp\\Settings_old", "Software\\Valve\\Source\\hl2mp\\Settings");

				rename("hl2mp/custom/vr.vpk","hl2mp/custom/vr.vpk_");
				remove("hl2mp/custom/vr.vpk.sound.cache");
				//CommandLine()->AppendParm( "-dxlevel 95", NULL );
				CommandLine()->AppendParm( "+mat_queue_mode 2", NULL );
				CommandLine()->AppendParm( "+mat_vrmode_adapter -1", NULL );
				CommandLine()->AppendParm( "+r_flashlightdepthtexture 1", NULL );
				CommandLine()->AppendParm( "+r_teeth 0", NULL );
				//CommandLine()->AppendParm( "+mat_antialias 4", NULL );
			}
		}
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

std::string g_dllPath;
Json::Value g_JSONProperties;
void LoadProperties()
{
	char path[MAX_PATH];
	HMODULE hm = NULL;

	//Get directory this d3d9.dll is in
	if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
		GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		(LPCSTR)&LoadProperties, &hm) == 0)
	{
		OutputDebugString("GetModuleHandle failed\n");
		return;
	}
	if (GetModuleFileName(hm, path, sizeof(path)) == 0)
	{
		OutputDebugString("GetModuleFileName failed\n");
		return;
	}

	g_dllPath = path;
	if (!g_dllPath.empty())
	{
		g_dllPath = g_dllPath.substr(0, g_dllPath.find_last_of('\\') + 1);
	}

	if (g_dllPath.length() > 0)
	{
		std::string propertiesFileName = g_dllPath + "d3d9.json";
		std::ifstream propertiesFile(propertiesFileName, std::ios::in);
		Json::Reader reader;
		if (!reader.parse(propertiesFile, g_JSONProperties, false))
		{
			OutputDebugString("No d3d9.json file found, or invalid content");
		}
	}
}

typedef IDirect3D9* (WINAPI *LPD3D_Direct3DCreate9)(UINT nSDKVersion);
typedef HRESULT (WINAPI *LPD3D_Direct3DCreate9Ex)(UINT nSDKVersion, IDirect3D9Ex**);
typedef int (WINAPI *LPD3D_BeginEvent)( D3DCOLOR col, LPCWSTR wszName );
typedef int (WINAPI *LPD3D_EndEvent)( void );
typedef void (WINAPI *LPD3D_SetMarker)( D3DCOLOR col, LPCWSTR wszName );
typedef void (WINAPI *LPD3D_SetRegion)( D3DCOLOR col, LPCWSTR wszName );
typedef BOOL (WINAPI *LPD3D_QueryRepeatFrame)( void );
typedef void (WINAPI *LPD3D_SetOptions)( DWORD dwOptions );
typedef DWORD (WINAPI *LPD3D_GetStatus)( void );


LPD3D_Direct3DCreate9 g_pfnDirect3DCreate9 = NULL;
LPD3D_Direct3DCreate9Ex g_pfnDirect3DCreate9Ex = NULL;
LPD3D_BeginEvent g_pfnD3DPERF_BeginEvent = NULL;
LPD3D_EndEvent g_pfnD3DPERF_EndEvent = NULL;
LPD3D_SetMarker g_pfnD3DPERF_SetMarker = NULL;
LPD3D_SetRegion g_pfnD3DPERF_SetRegion = NULL;
LPD3D_QueryRepeatFrame g_pfnD3DPERF_QueryRepeatFrame = NULL;
LPD3D_SetOptions g_pfnD3DPERF_SetOptions = NULL;
LPD3D_GetStatus g_pfnD3DPERF_GetStatus = NULL;

std::string GetStringProperty(const std::string& propName, const std::string& defaultValue)
{
	if (g_JSONProperties.isMember(propName))
	{
		Json::Value jValue = g_JSONProperties.get(propName, Json::Value());
		return jValue.asCString();
	}

	return defaultValue;
}

bool GetBoolProperty(const std::string& propName, const bool& defaultValue)
{
	if (g_JSONProperties.isMember(propName))
	{
		Json::Value jValue = g_JSONProperties.get(propName, Json::Value());
		return jValue.asBool();
	}

	return defaultValue;
}

static bool LoadD3D9Dll()
{
	HMODULE d3d9DllHandle = NULL;
	if (d3d9DllHandle)
		return true;

	LoadProperties();

	std::string dllFileName;
	std::string nextDllProperty = GetStringProperty("useNextDll", "");
	char buffer[MAX_PATH + 1];
	if (!nextDllProperty.empty())
	{
		//Load another proxy d3d9.dll in the chain
		dllFileName = g_dllPath + nextDllProperty;
	}
	else
	{
		//Load default system d3d9.dll
		GetSystemDirectory(buffer, sizeof(buffer));
		buffer[MAX_PATH] = 0;
		dllFileName = buffer;
		dllFileName += "\\d3d9.dll";
	}

	d3d9DllHandle = LoadLibrary(dllFileName.c_str());

	if (!d3d9DllHandle)
	{
		OutputDebugString((std::string("Failed to load ") + dllFileName).c_str());
		return false;
	}
	
	// Get function addresses
	g_pfnDirect3DCreate9 = (LPD3D_Direct3DCreate9)GetProcAddress(d3d9DllHandle, "Direct3DCreate9");
	g_pfnDirect3DCreate9Ex = (LPD3D_Direct3DCreate9Ex)GetProcAddress(d3d9DllHandle, "Direct3DCreate9Ex");
	if (!g_pfnDirect3DCreate9 || !g_pfnDirect3DCreate9Ex)
	{
		DEBUG_LOG("Failed to get Direct3DCreate9 or Direct3DCreate9Ex from d3d9.dll");
		FreeLibrary(d3d9DllHandle);
		return false;
	}

	g_pfnD3DPERF_BeginEvent = (LPD3D_BeginEvent)GetProcAddress(d3d9DllHandle, "D3DPERF_BeginEvent");
	g_pfnD3DPERF_EndEvent = (LPD3D_EndEvent)GetProcAddress(d3d9DllHandle, "D3DPERF_EndEvent");
	g_pfnD3DPERF_SetMarker = (LPD3D_SetMarker)GetProcAddress(d3d9DllHandle, "D3DPERF_SetMarker");
	g_pfnD3DPERF_SetRegion = (LPD3D_SetRegion)GetProcAddress(d3d9DllHandle, "D3DPERF_SetRegion");
	g_pfnD3DPERF_QueryRepeatFrame = (LPD3D_QueryRepeatFrame)GetProcAddress(d3d9DllHandle, "D3DPERF_QueryRepeatFrame");
	g_pfnD3DPERF_SetOptions = (LPD3D_SetOptions)GetProcAddress(d3d9DllHandle, "D3DPERF_SetOptions");
	g_pfnD3DPERF_GetStatus = (LPD3D_GetStatus)GetProcAddress(d3d9DllHandle, "D3DPERF_GetStatus");

	// Done
	return true;
}

IDirect3D9* WINAPI Direct3DCreate9(UINT nSDKVersion)
{
	if (!LoadD3D9Dll())
		return NULL;

	IDirect3D9Ex *pD3DEx = NULL;

	{
		DEBUG_LOG("g_pfnDirect3DCreate9Ex\n");
		HRESULT hr = g_pfnDirect3DCreate9Ex(nSDKVersion, &pD3DEx);

		if (FAILED(hr))
		{
			OutputDebugString("Failed: g_pfnDirect3DCreate9Ex");
			return NULL;
		}
	}

	return new Direct3D9ExProxyImpl(pD3DEx);
}

HRESULT WINAPI Direct3DCreate9Ex(UINT nSDKVersion, IDirect3D9Ex **ppDirect3D9Ex)
{
	if (!LoadD3D9Dll())
		return NULL;
	
	HRESULT hr = E_NOTIMPL;

	IDirect3D9Ex *pD3DEx = NULL;

	{
		DEBUG_LOG("g_pfnDirect3DCreate9Ex\n");
		hr = g_pfnDirect3DCreate9Ex(nSDKVersion, &pD3DEx);

		if (FAILED(hr))
		{
			OutputDebugString("Failed: g_pfnDirect3DCreate9Ex");
			return hr;
		}
	}

	*ppDirect3D9Ex = new Direct3D9ExProxyImpl(pD3DEx);

	return S_OK;
}

int WINAPI D3DPERF_BeginEvent( D3DCOLOR col, LPCWSTR wszName )
{
	return g_pfnD3DPERF_BeginEvent(col, wszName);
}

int WINAPI D3DPERF_EndEvent( void )
{
	return g_pfnD3DPERF_EndEvent();
}

void WINAPI D3DPERF_SetMarker( D3DCOLOR col, LPCWSTR wszName )
{
	g_pfnD3DPERF_SetMarker(col, wszName);
}

void WINAPI D3DPERF_SetRegion( D3DCOLOR col, LPCWSTR wszName )
{
	g_pfnD3DPERF_SetRegion(col, wszName);
}

BOOL WINAPI D3DPERF_QueryRepeatFrame( void )
{
	return g_pfnD3DPERF_QueryRepeatFrame();
}

void WINAPI D3DPERF_SetOptions( DWORD dwOptions )
{
	g_pfnD3DPERF_SetOptions(dwOptions);
}

DWORD WINAPI D3DPERF_GetStatus( void )
{
	return g_pfnD3DPERF_GetStatus();
}
