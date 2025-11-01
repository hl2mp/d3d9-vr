#pragma once
#include "MinHook.h"
#include "offsets.h"
#include "mathlib/mathlib.h"

template <typename T>
struct Hook {
	T fOriginal;
	LPVOID pTarget;
	bool isEnabled;

	int createHook(LPVOID targetFunc, LPVOID detourFunc)
	{
		if (MH_CreateHook(targetFunc, detourFunc, reinterpret_cast<LPVOID *>(&fOriginal)) != MH_OK)
		{
			char errorString[512];
			sprintf_s(errorString, 512, "Failed to create hook with this signature: %s\n", typeid(T).name());
			OutputDebugStringA(errorString);
			return 1;
		}
		pTarget = targetFunc;
		return -1;
	}

	int enableHook()
	{
		if (MH_EnableHook(pTarget) != MH_OK)
		{
			OutputDebugStringA("Failed to enable hook\n");
			return 1;
		}
		isEnabled = true;
		return -1;
	}

	int disableHook()
	{
		if (MH_DisableHook(pTarget) != MH_OK)
		{
			OutputDebugStringA("Failed to disable hook\n");
			return 1;
		}
		isEnabled = false;
		return -1;
	}
};

//client.dll
typedef void *(*tCreateEntityByName)( const char *className );
extern Hook<tCreateEntityByName> hkCreateEntityByName;

struct model_t;
typedef void (*tSetModelPointer)( void *thisptr, const model_t *pModel );
extern Hook<tSetModelPointer> hkSetModelPointer;

class C_BaseEntity;
typedef void (*tFollowEntity)( void *thisptr, C_BaseEntity *pBaseEntity, bool bBoneMerge );
extern Hook<tFollowEntity> hkFollowEntity;

typedef void (*tSendViewModelMatchingSequence)( void *thisptr, int sequence );
extern Hook<tSendViewModelMatchingSequence> hkSendViewModelMatchingSequence;

class CStudioHdr;
class CBoneBitList;
typedef void (*tBuildTransformations)( void *thisptr, CStudioHdr *hdr, Vector *pos, Quaternion *q, matrix3x4_t &cameraTransform, int boneMask, CBoneBitList &boneComputed );
extern Hook<tBuildTransformations> hkBuildTransformations;

typedef void (*tGetBonePosition)( void *thisptr, int iBone, Vector &origin, QAngle &angles );
extern Hook<tGetBonePosition> hkGetBonePosition;

typedef void (*tSetAbsOrigin)( void *thisptr, const Vector& origin );
extern Hook<tSetAbsOrigin> hkSetAbsOrigin;

class CUserCmd;
typedef bool (*tCreateMove)( void *thisptr, float flInputSampleTime, CUserCmd* cmd );
extern Hook<tCreateMove> hkCreateMove;

class CViewSetup;
typedef void (*tOverrideView)( void *thisptr, CViewSetup *pSetup );
extern Hook<tOverrideView> hkOverrideView;

typedef void (*tRenderView)( void *thisptr, CViewSetup &viewRender, int nClearFlags, int whatToDraw );
extern Hook<tRenderView> hkRenderView;

class Hooks
{
public:
	Hooks();
	~Hooks();

	void *GetInterface( const char *dllname, const char *interfacename );
	typedef void *(*tCreateInterface)(const char *name, int *returnCode);

	// Detour functions
	static void* __fastcall dCreateEntityByName( const char *className );
	static void __fastcall dSetModelPointer(void* rcx, const model_t *pModel );
	static void __fastcall dFollowEntity(void* rcx, C_BaseEntity *pBaseEntity, bool bBoneMerge );
	static void __fastcall dSendViewModelMatchingSequence( void* rcx, int sequence );
	static void __fastcall dBuildTransformations( void* rcx, CStudioHdr *hdr, Vector *pos, Quaternion *q, matrix3x4_t &cameraTransform, int boneMask, CBoneBitList &boneComputed );
	static void __fastcall dGetBonePosition( void* rcx, int iBone, Vector &origin, QAngle &angles );
	static void __fastcall dSetAbsOrigin( void* rcx, const Vector& origin );
	static bool __fastcall dCreateMove( void* rcx, float flInputSampleTime, CUserCmd* cmd );
	static void __fastcall dOverrideView( void* rcx, CViewSetup *pSetup );
	static void __fastcall dRenderView( void* rcx, CViewSetup &viewRender, int nClearFlags, int whatToDraw );

	Offsets *m_Offsets;
};

extern Hooks *m_Hooks;