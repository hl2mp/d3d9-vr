#include "hooks.h"
#include "vr_sourcevr.h"
#include "sdk.h"

uintptr_t GetModuleBase(const char* dllName, uintptr_t offset)
{
	HMODULE hModule = nullptr;
	while( !( hModule = GetModuleHandleA( dllName ) ) )
		Sleep(50);

	MODULEINFO moduleInfo;
	if( !GetModuleInformation( GetCurrentProcess(), hModule, &moduleInfo, sizeof( moduleInfo ) ) )
		return 1;

	uintptr_t baseAddress = reinterpret_cast<uintptr_t>( moduleInfo.lpBaseOfDll );
	uintptr_t g_p_address = baseAddress + offset;
	return g_p_address;
}
//
//typedef bool(__fastcall* CreateMove_t)(void*, float, CUserCmd*);
//
//CreateMove_t oCreateMove = nullptr;
//
//bool __stdcall hkCreateMove(void *self, float flInputSampleTime, CUserCmd* cmd)
//{
//	bool result = oCreateMove(self, flInputSampleTime, cmd);
//
//	if( cmd && cmd->command_number != 0 )
//	{
//		if (GetAsyncKeyState(VK_MBUTTON) & 0x8000)
//		{
//			cmd->buttons |= IN_JUMP;
//		}
//		Msg("Call CMD %d\n",cmd->buttons);
//	}
//
//	return result;
//}
//
//bool HookCreateMove()
//{
//    uintptr_t moduleBase = GetModuleBase("client.dll", 0x632bd0);
//    void** clientMode = *(void***)(moduleBase);
//    
//    if (!clientMode)
//		return false;
//
//    void** vtable = *(void***)clientMode;
//    
//    const int CREATE_MOVE_INDEX = 21;
//    
//    return MH_CreateHook(vtable[CREATE_MOVE_INDEX], &hkCreateMove, 
//                        reinterpret_cast<void**>(&oCreateMove)) == MH_OK &&
//           MH_EnableHook(vtable[CREATE_MOVE_INDEX]) == MH_OK;
//}

Hooks *m_Hooks = NULL;

vgui::IInputInternal *g_InputInternal = NULL;
extern vgui::ISurface *g_pVGuiSurface;
IVEngineClient *engine = NULL;
IEngineTrace *enginetrace = NULL;
IClientEntityList *entitylist = NULL;
extern vgui::IPanel *g_pVGuiPanel;
IEngineVGui *enginevgui = NULL;
IFileSystem *filesystem = NULL;
IVModelInfoClient *modelinfo = NULL;
extern vgui::IVGui *g_pVGui;
extern vgui::ILocalize *g_pVGuiLocalize;
extern vgui::ISchemeManager *g_pVGuiSchemeManager;
extern vgui::IInput *g_pVGuiInput;
extern vgui::ISystem *g_pVGuiSystem;
IVRenderView *render = NULL;
IVEfx *effects = NULL;

Hook<tCreateEntityByName> hkCreateEntityByName;
Hook<tSetModelPointer> hkSetModelPointer;
Hook<tFollowEntity> hkFollowEntity;
Hook<tSendViewModelMatchingSequence> hkSendViewModelMatchingSequence;
Hook<tBuildTransformations> hkBuildTransformations;
Hook<tGetBonePosition> hkGetBonePosition;
Hook<tSetAbsOrigin> hkSetAbsOrigin;
Hook<tCreateMove> hkCreateMove;
Hook<tRenderView> hkRenderView;
Hook<tOverrideView> hkOverrideView;


typedef float(__fastcall* GetViewModelFOV_t)(void*);

GetViewModelFOV_t oGetViewModelFOV = nullptr;

float __stdcall hkGetViewModelFOV( void *self )
{
	float fov = oGetViewModelFOV( self );
	fov = m_VR->m_View[vr::Eye_Left].fov;
	return fov;
}

//typedef bool(__fastcall* CreateMove_t)(void*, float, CUserCmd*);
//
//CreateMove_t oCreateMove = nullptr;
//
//bool __stdcall hkCreateMove(void *self, float flInputSampleTime, CUserCmd* cmd)
//{
//	bool result = oCreateMove(self, flInputSampleTime, cmd);
//
//	if( cmd && cmd->command_number != 0 )
//	{
//		if (GetAsyncKeyState(VK_MBUTTON) & 0x8000)
//		{
//			cmd->buttons |= IN_JUMP;
//		}
//		Msg("Call CMD %d\n",cmd->buttons);
//	}
//
//	return result;
//}

Hooks::Hooks()
{
	if( MH_Initialize() != MH_OK )
		OutputDebugStringA("Failed to init MinHook Hooks\n");

	entitylist = (IClientEntityList*)GetInterface("client.dll", VCLIENTENTITYLIST_INTERFACE_VERSION);

	g_pFullFileSystem = filesystem = (IFileSystem*)GetInterface("FileSystem_Stdio.dll", FILESYSTEM_INTERFACE_VERSION);

	modelinfo = (IVModelInfoClient*)GetInterface("engine.dll", VMODELINFO_CLIENT_INTERFACE_VERSION);

	g_pMaterialSystem = materials = (IMaterialSystem*)GetInterface("MaterialSystem.dll", MATERIAL_SYSTEM_INTERFACE_VERSION);

	g_pVGuiSurface = (vgui::ISurface*)GetInterface("vguimatsurface.dll", VGUI_SURFACE_INTERFACE_VERSION);

	engine = (IVEngineClient*)GetInterface("engine.dll", VENGINE_CLIENT_INTERFACE_VERSION);

	g_InputInternal = (vgui::IInputInternal*)GetInterface("vgui2.dll", VGUI_INPUTINTERNAL_INTERFACE_VERSION);

	enginetrace = (IEngineTrace*)GetInterface("engine.dll", INTERFACEVERSION_ENGINETRACE_CLIENT);

	g_pVGuiPanel = (vgui::IPanel*)GetInterface("vgui2.dll", VGUI_PANEL_INTERFACE_VERSION);

	enginevgui = (IEngineVGui*)GetInterface("engine.dll", VENGINE_VGUI_VERSION);

	g_pVGui = (vgui::IVGui*)GetInterface("vgui2.dll", VGUI_IVGUI_INTERFACE_VERSION);

	g_pVGuiLocalize = (vgui::ILocalize*)GetInterface("vgui2.dll", VGUI_LOCALIZE_INTERFACE_VERSION);

	g_pVGuiSchemeManager = (vgui::ISchemeManager*)GetInterface("vgui2.dll", VGUI_SCHEME_INTERFACE_VERSION);

	g_pVGuiInput = (vgui::IInput*)GetInterface("vgui2.dll", VGUI_INPUT_INTERFACE_VERSION);

	g_pVGuiSystem = (vgui::ISystem*)GetInterface("vgui2.dll", VGUI_SYSTEM_INTERFACE_VERSION);

	render = (IVRenderView*)GetInterface("engine.dll", VENGINE_RENDERVIEW_INTERFACE_VERSION);

	effects = (IVEfx*)GetInterface("engine.dll", VENGINE_EFFECTS_INTERFACE_VERSION);

	g_pMatSystemSurface = (IMatSystemSurface*)GetInterface("vguimatsurface.dll", VGUI_SURFACE_INTERFACE_VERSION);

	m_Offsets = new Offsets();

	LPVOID CreateEntityByNameAddr = reinterpret_cast<LPVOID>(m_Offsets->CreateEntityByName.address);
	hkCreateEntityByName.createHook(CreateEntityByNameAddr, &dCreateEntityByName);

	LPVOID SetModelPointerAddr = reinterpret_cast<LPVOID>(m_Offsets->SetModelPointer.address);
	hkSetModelPointer.createHook(SetModelPointerAddr, &dSetModelPointer);

	LPVOID FollowEntityAddr = reinterpret_cast<LPVOID>(m_Offsets->FollowEntity.address);
	hkFollowEntity.createHook(FollowEntityAddr, &dFollowEntity);
	
	LPVOID SendViewModelMatchingSequenceAddr = reinterpret_cast<LPVOID>(m_Offsets->SendViewModelMatchingSequence.address);
	hkSendViewModelMatchingSequence.createHook(SendViewModelMatchingSequenceAddr, &dSendViewModelMatchingSequence);
	
	LPVOID BuildTransformationsAddr = reinterpret_cast<LPVOID>(m_Offsets->BuildTransformations.address);
	hkBuildTransformations.createHook(BuildTransformationsAddr, &dBuildTransformations);
	
	LPVOID GetBonePositionAddr = reinterpret_cast<LPVOID>(m_Offsets->GetBonePosition.address);
	hkGetBonePosition.createHook(GetBonePositionAddr, &dGetBonePosition);
	
	LPVOID SetAbsOriginAddr = reinterpret_cast<LPVOID>(m_Offsets->SetAbsOrigin.address);
	hkSetAbsOrigin.createHook(SetAbsOriginAddr, &dSetAbsOrigin);

	PVOID RenderViewAddr = reinterpret_cast<LPVOID>(m_Offsets->RenderView.address);
	hkRenderView.createHook(RenderViewAddr, &dRenderView);

	hkCreateEntityByName.enableHook();
	hkSetModelPointer.enableHook();
	hkFollowEntity.enableHook();
	hkSendViewModelMatchingSequence.enableHook();
	hkBuildTransformations.enableHook();
	hkGetBonePosition.enableHook();
	hkSetAbsOrigin.enableHook();
	hkRenderView.enableHook();

	uintptr_t moduleBase = GetModuleBase( "client.dll", m_Offsets->CreateMove );
    void** clientMode = *(void***)(moduleBase);
    
	while (!clientMode)
	{
		Sleep(10);
		clientMode = *(void***)(moduleBase);
	}

    void** vtable = *(void***)clientMode;

	hkCreateMove.createHook(vtable[21], &dCreateMove);
	hkCreateMove.enableHook();

	hkOverrideView.createHook(vtable[16], &dOverrideView);
	hkOverrideView.enableHook();

	MH_CreateHook(vtable[32], &hkGetViewModelFOV, reinterpret_cast<void**>(&oGetViewModelFOV));
	MH_EnableHook(vtable[32]);
}


Hooks::~Hooks()
{
	if (MH_Uninitialize() != MH_OK)
		OutputDebugStringA("Failed to uninitialize MinHook\n");
}


void *Hooks::GetInterface(const char *dllname, const char *interfacename)
{
	while( !GetModuleHandleA( dllname ) )
		Sleep(50);

    tCreateInterface CreateInterface = (tCreateInterface)GetProcAddress(GetModuleHandle(dllname), "CreateInterface");

    int returnCode = 0;
    void *createdInterface = CreateInterface(interfacename, &returnCode);

    return createdInterface;
}


void* __fastcall Hooks::dCreateEntityByName( const char *className )
{
	return hkCreateEntityByName.fOriginal( className );
}


void __fastcall Hooks::dSetModelPointer( void* rcx, const model_t *pModel )
{
	hkSetModelPointer.fOriginal( rcx, pModel );
}


void __fastcall Hooks::dFollowEntity( void* rcx, C_BaseEntity *pBaseEntity, bool bBoneMerge )
{
	hkFollowEntity.fOriginal( rcx, pBaseEntity, bBoneMerge );
}


void __fastcall Hooks::dSendViewModelMatchingSequence( void* rcx, int sequence )
{
	hkSendViewModelMatchingSequence.fOriginal( rcx, sequence );
}


void __fastcall Hooks::dBuildTransformations( void* rcx, CStudioHdr *hdr, Vector *pos, Quaternion *q, matrix3x4_t &cameraTransform, int boneMask, CBoneBitList &boneComputed )
{
	m_VR->BuildTransformations( rcx, hdr, pos, q, cameraTransform, boneMask, boneComputed );
	hkBuildTransformations.fOriginal( rcx, hdr, pos, q, cameraTransform, boneMask, boneComputed );
}


void __fastcall Hooks::dGetBonePosition( void* rcx, int iBone, Vector &origin, QAngle &angles )
{
	hkGetBonePosition.fOriginal( rcx, iBone, origin, angles );
}


void __fastcall Hooks::dSetAbsOrigin( void* rcx, const Vector &origin )
{
	hkSetAbsOrigin.fOriginal( rcx, origin );
}


bool __fastcall Hooks::dCreateMove( void* rcx, float flInputSampleTime, CUserCmd* cmd )
{
	bool result = hkCreateMove.fOriginal( rcx, flInputSampleTime, cmd );
	m_VR->CreateMove( flInputSampleTime, cmd );
	return result;
}


void __fastcall Hooks::dOverrideView( void* rcx, CViewSetup *pSetup )
{
	hkOverrideView.fOriginal( rcx, pSetup );
	if( m_VR->m_bActive )
		m_VR->OverrideView( pSetup );
}


void __fastcall Hooks::dRenderView( void* rcx, CViewSetup &viewRender, int nClearFlags, int whatToDraw )
{
	if( m_VR->RenderView( rcx, viewRender, nClearFlags, whatToDraw ) )
		hkRenderView.fOriginal( rcx, viewRender, nClearFlags, whatToDraw );
}
