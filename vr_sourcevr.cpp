#include "vr_sourcevr.h"
#include "sdk.h"
#include "model_types.h"
#include "dlight.h"

CSourceVR instance;
CSourceVR *m_VR = &instance;

#define MAX_STR_LEN 256

class CVRHands
{
public:
	CVRHands()
	{
		m_hHandRight = NULL;
		m_hHandLeft = NULL;
		m_vrhands = NULL;
		m_kvBones = NULL;
		m_bBipod = false;
		m_vecOffsetLeft = vec3_origin;
		m_vecOffsetRight = vec3_origin;
	};

	~CVRHands(){};

	void Init()
	{
		m_kvBones = new KeyValues("VRBones");
		m_kvBones->LoadFromFile(filesystem, "vrbones.txt", "GAME");

		//Это руки игрока в ВР, взято из hl2vr
		m_vrhands = engine->LoadModel("models/player.mdl");
		
		//Spawn right hand 
		//m_hHandRight = CreateEntityByName( "viewmodel" );
		//m_hHandRight->SetModelPointer( m_vrhands );

		//Spawn left hand 
		m_hHandLeft = CreateEntityByName( "viewmodel" );
		m_hHandLeft->SetModelPointer( m_vrhands );

		m_vecOffsetLeft = Vector( -5, 1, -3 );
		m_vecOffsetRight = Vector( -5, -1, -3 );
	}

	void Update()
	{
		if (!engine->IsInGame()) {
			m_hHandRight = NULL;
			m_hHandLeft = NULL;
			m_vrhands = NULL;

			if( m_kvBones )
			{
				m_kvBones->deleteThis();
				m_kvBones = NULL;
			}

			m_vecOffsetLeft = vec3_origin;
			m_vecOffsetRight = vec3_origin;
			return;
		}

		if (engine->IsInGame() && GetLocalPlayer() && !m_hHandLeft)
			Init();

		if (m_hHandLeft)
		{
			m_hHandLeft->SetAbsOrigin( m_VR->m_HeadPosAbs );
		}

		C_BaseEntity* pViewModel = GetViewModel();
		if (pViewModel && m_hHandRight)
		{
			const model_t* pModel = pViewModel->GetModel();
			if( pModel )
				m_ViewModelName = modelinfo->GetModelName(pViewModel->GetModel() );

			m_hHandRight->FollowEntity(pViewModel);
		}
	};

	const model_t *m_vrhands;

	C_BaseEntity* m_hHandLeft;
	C_BaseEntity* m_hHandRight;

	const char *m_ViewModelName;

	KeyValues *m_kvBones;

	Vector m_vecOffsetLeft;
	Vector m_vecOffsetRight;

	bool m_bBipod;
};

CVRHands *g_pVRHands;

const char *g_pEVRFinger[] =
{
	"VRFinger_Thumb",
	"VRFinger_Index",
	"VRFinger_Middle",
	"VRFinger_Ring",
	"VRFinger_Pinky",
};

const char *g_pEVRHands[] =
{
	"LeftHand",
	"RightHand",
};

Vector stringToVector( const char *str ) {
    Vector v;
    // Используем sscanf для разбора строки
    sscanf( str, "%f %f %f", &v.x, &v.y, &v.z );
    return v;
}

Quaternion stringToQuaternion( const char *str ) {
    Quaternion q;
    // Используем sscanf для разбора строки
    sscanf( str, "%f %f %f %f", &q.x, &q.y, &q.z, &q.w );
    return q;
}

// Функция для интерполяции между двумя кватернионами
Quaternion interpolateQuaternion( float t, const Quaternion* minQuaternion, const Quaternion* maxQuaternion ) {
    Quaternion result;

    // Линейная интерполяция
    result.w = minQuaternion->w + t * (maxQuaternion->w - minQuaternion->w);
    result.x = minQuaternion->x + t * (maxQuaternion->x - minQuaternion->x);
    result.y = minQuaternion->y + t * (maxQuaternion->y - minQuaternion->y);
    result.z = minQuaternion->z + t * (maxQuaternion->z - minQuaternion->z);

    // Нормализация результата
    float length = sqrt(result.w * result.w + result.x * result.x + result.y * result.y + result.z * result.z);
    if (length > 0) {
        result.w /= length;
        result.x /= length;
        result.y /= length;
        result.z /= length;
    }

    return result;
}


Vector interpolateVector( float t, const Vector& v1, const Vector& v2 ) {
	return Vector(
		v1.x + t * (v2.x - v1.x),
		v1.y + t * (v2.y - v1.y),
		v1.z + t * (v2.z - v1.z)
	);
}


void DrawPanelIn3DSpace( vgui::VPANEL pRootPanel, const VMatrix &panelCenterToWorld, int nPixelWidth, int nPixelHeight, float flWorldWidth, float flWorldHeight )
{
	g_pVGuiSurface->SolveTraverse( pRootPanel, false );

	CMatRenderContextPtr pRenderContext( materials );
	//pRenderContext->OverrideDepthEnable( true, false );

	VMatrix pixelToScreen;
	MatrixBuildScale( pixelToScreen, flWorldWidth / nPixelWidth, -flWorldHeight / nPixelHeight, 1.0f );

	VMatrix pixelToWorld;
	MatrixMultiply( panelCenterToWorld, pixelToScreen, pixelToWorld );

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PushMatrix();
	pRenderContext->LoadMatrix( pixelToWorld );

	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PushMatrix();

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PushMatrix();

	g_pVGuiPanel->Repaint( pRootPanel );
	g_pVGuiPanel->PaintTraverse( pRootPanel, true, false  );

	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PopMatrix();

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PopMatrix();

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PopMatrix();
	//pRenderContext->OverrideDepthEnable( false, true );
	
	pRenderContext.SafeRelease();
}


enum HudName
{
	HudHealth,
	HudWeaponSelection,
	HudSuit,
	HudSuitPower,
	HudAmmo,
	HudAmmoSecondary,
	TeamDisplay,
	HudDamageIndicator,
	HudChat,

	HUD_COUNT
};
static vgui::VPANEL m_pHudChild[HUD_COUNT];

#include <vgui_controls/Panel.h>

void CSourceVR::RenderHud()
{
	if( engine->IsInGame() && g_pVGuiSurface->IsCursorVisible() && !engine->IsLevelMainMenuBackground() )
	{
		CMatRenderContextPtr pRenderContext( materials );
		pRenderContext->DrawScreenSpaceRectangle(
			materials->FindMaterial( "vgui/black", TEXTURE_GROUP_VGUI ),
			0, 0,
			m_RenderWidth, m_RenderHeight,
			0, 0,
			m_RenderWidth-1, m_RenderHeight-1,
			m_RenderWidth, m_RenderHeight
		);
		pRenderContext.SafeRelease();
	}

	DrawWorldUi();

	if (!engine->IsInGame())
		return;

	//Draw hand, beam in menu
	if( g_pVGuiSurface->IsCursorVisible() )
	{
		dlight_t *dl;
		dl = effects->CL_AllocDlight ( render->GetViewEntity() );
		dl->origin = m_HeadPosAbs;
		dl->color.r = dl->color.g = dl->color.b = 100;
		dl->radius = 400;
		dl->flags = DLIGHT_NO_WORLD_ILLUMINATION;
		dl->die = GetCurTime() + 0.01;

		Vector pUL, pUR, pLL, pLR;
		GetHUDBounds( &pUL, &pUR, &pLL, &pLR );
		Vector vPoint = GetAimPointOnPanel( pUL, pUR, pLL, m_RightControllerPosAbs, m_RightControllerAngAbs );

		if( vPoint != vec3_origin )
		{
			IMaterial* halo = materials->FindMaterial("blueflare1", TEXTURE_GROUP_CLIENT_EFFECTS);
			halo->AddRef();
			DrawHalo(halo, m_RightControllerAngAbs, vPoint, 2.0 );

			IMaterial* mymat = materials->FindMaterial("laservr", TEXTURE_GROUP_CLIENT_EFFECTS);
			mymat->AddRef();
			DrawBeamQuadraticNew( mymat, m_RightControllerPosAbs, m_RightControllerPosAbs, vPoint, 0.7f, vec3_origin, 0.0f );

			g_pVGuiSurface->SetSoftwareCursor( false );
		}
	}

	if( g_pVRHands->m_hHandLeft )
			g_pVRHands->m_hHandLeft->DrawModel( STUDIO_RENDER );

	C_BaseEntity *pPlayer = GetLocalPlayer();
	if( !pPlayer || !pPlayer->IsAlive() )
		return;

	if( !g_pVGuiSurface->IsCursorVisible() )
	{
		extern vgui::VPANEL pTargetID;
		g_pVGuiSurface->SolveTraverse( pTargetID, false );
		g_pVGuiSurface->PaintTraverse( pTargetID );

		//C_BaseEntity* pViewModel = GetViewModel();
		//if( pViewModel )
		//{
		//	if( pPlayer->GetEntPropEnt( "m_hVehicle" ) == NULL )
		//	{
		//		pViewModel->DrawModel( STUDIO_RENDER );
		//	}
		//}

		vgui::VPANEL pRoot = enginevgui->GetPanel( PANEL_ROOT );

		Vector vecLeft, vecRight;
		QAngle angLeft, angRight;

		if( g_pVRHands->m_hHandLeft )
		{
			int iAttachment = g_pVRHands->m_hHandLeft->LookupAttachment( "hud_l" );
			g_pVRHands->m_hHandLeft->GetAttachment( iAttachment, vecLeft, angLeft);
		}

		if( !m_pHudChild[HudHealth] )
		{
			m_pHudChild[HudHealth] = VFindChildByName( pRoot, "HudHealth" );

			int cx, cy;
			g_pVGuiPanel->GetPos( m_pHudChild[HudHealth], cx, cy );
			g_pVGuiPanel->SetPos( m_pHudChild[HudHealth], cx, 0 );
			g_pVGuiPanel->SetParent( m_pHudChild[HudHealth], NULL );
		}

		if( !m_pHudChild[HudSuit] )
		{
			m_pHudChild[HudSuit] = VFindChildByName( pRoot, "HudSuit" );

			int cx, cy;
			g_pVGuiPanel->GetPos( m_pHudChild[HudSuit], cx, cy );
			g_pVGuiPanel->SetPos( m_pHudChild[HudSuit], cx, 0 );
			g_pVGuiPanel->SetParent( m_pHudChild[HudSuit], NULL );
		}

		if( !m_pHudChild[TeamDisplay] )
		{
			m_pHudChild[TeamDisplay] = VFindChildByName( pRoot, "TeamDisplay" );

			int cx, cy, wide, tall;
			g_pVGuiPanel->GetSize( m_pHudChild[HudHealth], wide, tall );
			g_pVGuiPanel->GetPos( m_pHudChild[TeamDisplay], cx, cy );
			g_pVGuiPanel->SetPos( m_pHudChild[TeamDisplay], cx, tall+10 );
			g_pVGuiPanel->SetParent( m_pHudChild[TeamDisplay], NULL );
		}

		VMatrix m_PanelToWorld;
		m_PanelToWorld.SetupMatrixOrgAngles( vecLeft, angLeft );

		#define worldScale 0.01

		DrawPanelIn3DSpace( m_pHudChild[HudHealth], m_PanelToWorld, m_Width, m_Height, m_Width*worldScale, m_Height*worldScale );
		DrawPanelIn3DSpace( m_pHudChild[HudSuit], m_PanelToWorld, m_Width, m_Height, m_Width*worldScale, m_Height*worldScale );
		DrawPanelIn3DSpace( m_pHudChild[TeamDisplay], m_PanelToWorld, m_Width, m_Height, m_Width*worldScale, m_Height*worldScale );

		//Рисуем индиктора урона
		{
			if( !m_pHudChild[HudDamageIndicator] )
			{
				m_pHudChild[HudDamageIndicator] = VFindChildByName( pRoot, "HudDamageIndicator" );

				vgui::Panel* pInfoPanel = g_pVGuiPanel->GetPanel(m_pHudChild[HudDamageIndicator], "ClientDLL");
				if(pInfoPanel)
				{
					pInfoPanel->SetForceStereoRenderToFrameBuffer( false );
				}

				g_pVGuiPanel->SetParent( m_pHudChild[HudDamageIndicator], NULL );
			}

			static const float fAspectRatio = (float)m_RenderWidth/(float)m_RenderHeight;
			float fHFOV = m_View[vr::Eye_Left].fov;
			float fVFOV = m_View[vr::Eye_Left].fov/fAspectRatio;

			const float fHudForward = 500.0;
			float fHudHalfWidth = tan( DEG2RAD( fHFOV * 0.5f ) ) * fHudForward * 0.9;
			float fHudHalfHeight = tan( DEG2RAD( fVFOV * 0.5f ) ) * fHudForward * 0.9;

			VMatrix m_PanelToWorld;
			m_PanelToWorld.SetupMatrixOrgAngles( m_HeadPosAbs, m_HeadAngAbs );

			Vector vHUDOrigin = m_PanelToWorld.GetTranslation() + m_PanelToWorld.GetForward() * fHudForward;
			m_PanelToWorld.SetupMatrixOrgAngles(vHUDOrigin, m_HeadAngAbs );

			VMatrix matOffset;
			matOffset.SetupMatrixOrgAngles( Vector( 0, fHudHalfWidth, fHudHalfHeight ), QAngle( 0, -90, 90 ) );

			m_PanelToWorld = m_PanelToWorld * matOffset;

			DrawPanelIn3DSpace( m_pHudChild[HudDamageIndicator], m_PanelToWorld, m_Width, m_Height, fHudHalfWidth*2, fHudHalfHeight*2 );
		}

		//Рисуем прицел
		QAngle va;
		engine->GetViewAngles( va );

		Vector vecMuzzleDir;
		AngleVectors( va, &vecMuzzleDir );

		Vector vecEnd = pPlayer->EyePosition() + vecMuzzleDir * 8000.0f;
		trace_t trace;
		UTIL_TraceLine( pPlayer->EyePosition(), vecEnd, MASK_SHOT, &trace );

		IMaterial* halo = materials->FindMaterial( "crosshair", TEXTURE_GROUP_CLIENT_EFFECTS );
		halo->AddRef();

		DrawHalo( halo, va, trace.endpos, ( trace.endpos - trace.startpos ).Length() * 0.01 );


		//Слишком ярко, в HDR не видно буквы в ТАБЕ
		//if( !g_pVGuiSurface->IsCursorVisible() )
		//{
		//	static const float fAspectRatio = (float)m_Width/(float)m_Height;
		//	float fHFOV = 60;
		//	float fVFOV = 60/fAspectRatio;

		//	const float fHudForward = 500.0;
		//	float fHudHalfWidth = tan( DEG2RAD( fHFOV * 0.5f ) ) * fHudForward * 1.0;
		//	float fHudHalfHeight = tan( DEG2RAD( fVFOV * 0.5f ) ) * fHudForward * 1.0;
	
		//	Vector vHUDOrigin = m_WorldFromHud.GetTranslation() + m_WorldFromHud.GetForward() * fHudForward;
		//	m_PanelToWorld.SetupMatrixOrgAngles(vHUDOrigin, m_HudAngles );

		//	VMatrix matOffset;
		//	matOffset.SetupMatrixOrgAngles( Vector( 0, fHudHalfWidth, fHudHalfHeight ), QAngle( 0, -90, 90 ) );

		//	m_PanelToWorld = m_PanelToWorld * matOffset;

		//	if( !m_pHudChild[HudChat] )
		//	{
		//		m_pHudChild[HudChat] = VFindChildByName( pRoot, "HudChat" );
		//	}

		//	DrawPanelIn3DSpace( m_pHudChild[HudChat], m_PanelToWorld, m_Width, m_Height, fHudHalfWidth*2, fHudHalfHeight*2 );

		//	DrawPanelIn3DSpace( pRoot, m_PanelToWorld, m_Width, m_Height, fHudHalfWidth*2, fHudHalfHeight*2 );
		//}
	}
}

#include "materialsystem/imesh.h"

void CSourceVR::DrawWorldUi()
{
	Vector vUL, vUR, vLL, vLR;
	GetHUDBounds( &vUL, &vUR, &vLL, &vLR );

	CMatRenderContextPtr pRenderContext(materials);

	IMaterial* mymat = materials->FindMaterial("vgui/inworldui", TEXTURE_GROUP_VGUI);

	IMesh* pMesh = pRenderContext->GetDynamicMesh(true, NULL, NULL, mymat);

	CMeshBuilder meshBuilder;
	meshBuilder.Begin(pMesh, MATERIAL_TRIANGLE_STRIP, 2);

	meshBuilder.Position3fv(vLR.Base());
	meshBuilder.TexCoord2f(0, 1, 1);
	meshBuilder.AdvanceVertexF<VTX_HAVEPOS, 1>();

	meshBuilder.Position3fv(vLL.Base());
	meshBuilder.TexCoord2f(0, 0, 1);
	meshBuilder.AdvanceVertexF<VTX_HAVEPOS, 1>();

	meshBuilder.Position3fv(vUR.Base());
	meshBuilder.TexCoord2f(0, 1, 0);
	meshBuilder.AdvanceVertexF<VTX_HAVEPOS, 1>();

	meshBuilder.Position3fv(vUL.Base());
	meshBuilder.TexCoord2f(0, 0, 0);
	meshBuilder.AdvanceVertexF<VTX_HAVEPOS, 1>();

	meshBuilder.End();
	pMesh->Draw();
}


void CSourceVR::BuildTransformations( void *ecx, CStudioHdr *hdr, Vector *pos, Quaternion *q, matrix3x4_t &cameraTransform, int boneMask, CBoneBitList &boneComputed )
{
	C_BaseEntity *pPlayer = GetLocalPlayer();
	if( !pPlayer || !pPlayer->IsAlive() && !g_pVGuiSurface->IsCursorVisible() )
		return;

	/*
	C_BaseEntity *pViewHands = g_pVRHands->m_hHandRight;

	if( GetViewModel() )
	{
		pViewHands = GetViewModel();
		//char boneName[256];
		//V_FileBase( g_pVRHands->m_ViewModelName, boneName, sizeof( boneName ) );
		//Msg("Call %s\n", g_pVRHands->m_ViewModelName);

		//KeyValues *manifest = new KeyValues("AdminsList");
		//manifest->LoadFromFile(filesystem, "weapons/v_pistol.txt", "GAME");
	}
	else if( pViewHands )
	{
		pViewHands->SetAbsOrigin( m_HeadPosAbs );
	}
	*/

	C_BaseEntity* pViewModel = GetViewModel();
	if( pViewModel == ecx && (m_RightControllerPosAbs.Length() != 0) )
	{
		AngleMatrix( m_RightControllerAngAbs, m_RightControllerPosAbs, cameraTransform );
	}

	if( g_pVRHands->m_hHandLeft == ecx )
	{
		int lHand = Studio_BoneIndexByName( hdr, "ValveBiped.Bip01_L_Hand" );
		if( lHand != -1 && !g_pVRHands->m_bBipod )
		{
			pos[lHand] = m_LeftControllerPosAbs - m_HeadPosAbs;
			AngleQuaternion( m_LeftControllerAngAbs, q[lHand] );
		}
		else if( g_pVRHands->m_bBipod )
		{
			//Так как мы держим оружие двумя руками, то ничего не делаем.
			return;
		}

		bool bCanUseRightHand = false;
		if( !pPlayer->GetActiveWepon() || g_pVGuiSurface->IsCursorVisible() )
		{
			bCanUseRightHand = true;
		}

		if( bCanUseRightHand )
		{
			int rHand = Studio_BoneIndexByName( hdr, "ValveBiped.Bip01_R_Hand" );
			if( rHand != -1 )
			{
				pos[rHand] = m_RightControllerPosAbs - m_HeadPosAbs;
				QAngle correctedAngles = m_RightControllerAngAbs;
				correctedAngles[ROLL] += 180.0f;
				AngleQuaternion( correctedAngles, q[rHand] );
			}
		}

		for ( int i = 0; i < ARRAYSIZE( g_pEVRHands ); i++ )
		{
			//Пропускаем пальцы правой руки если есть оружие в руках или не в меню.
			if( !bCanUseRightHand && i == 1 )
				continue;

			KeyValues *kvLeftHand = g_pVRHands->m_kvBones->FindKey( g_pEVRHands[i] );
			for ( int j = 0; j < ARRAYSIZE( g_pEVRFinger ); j++ )
			{
				float fFinger = m_vrSkeletalSummary[i].flFingerCurl[j];

				KeyValues *kvFinger = kvLeftHand->FindKey( g_pEVRFinger[j] );
				if (kvFinger == nullptr)
					continue;

				for ( KeyValues *sub = kvFinger->GetFirstSubKey(); sub != NULL; sub = sub->GetNextKey() )
				{
					int iBone = Studio_BoneIndexByName( hdr, sub->GetName() );

					if( iBone == -1 )
						continue;

					q[iBone] = interpolateQuaternion( fFinger, &stringToQuaternion( sub->GetString( "minAngles" ) ), &stringToQuaternion( sub->GetString( "maxAngles" ) ) );
					pos[iBone] = interpolateVector( fFinger, stringToVector( sub->GetString( "minOrigin" ) ), stringToVector( sub->GetString( "maxOrigin" ) ) );
				}
			}
		}
	}
}


void CSourceVR::Activate()
{
	vr::HmdError error = vr::VRInitError_None;
	m_pHmd = vr::VR_Init( &error, vr::VRApplication_Scene );

	if( error != vr::VRInitError_None )
	{
		Msg( "Unable to initialize HMD tracker. Error code %d\n", error );
		return;
	}

	g_pVGuiSurface->GetScreenSize( m_Width, m_Height );

	m_pHmd->GetRecommendedRenderTargetSize( &m_RenderWidth, &m_RenderHeight );

	m_Input = vr::VRInput();

	InstallApplicationManifest("manifest.vrmanifest");
    SetActionManifest("action_manifest.json");

	vr::VRCompositor()->SetTrackingSpace( vr::TrackingUniverseSeated );

	g_pVRHands = new CVRHands();

	m_bActive = true;
}


void CSourceVR::CreateRenderTargets( IMaterialSystem *pMaterialSystem )
{
	pMaterialSystem->BeginRenderTargetAllocation();

	if( !m_ScreenMat )
	{
		pMaterialSystem->FindMaterial( "vgui/inworldui", TEXTURE_GROUP_VGUI )->AddRef();
		pMaterialSystem->FindMaterial( "vgui/black", TEXTURE_GROUP_VGUI )->AddRef();

		KeyValues *screenMatKV = new KeyValues( "UnlitGeneric" );
		screenMatKV->SetString( "$basetexture", "leftEye0" );
		m_ScreenMat = pMaterialSystem->CreateMaterial( "vr_screen_rt", screenMatKV );
		m_ScreenMat->AddRef();

		//engine->ClientCmd_Unrestricted( "hud_reloadscheme\n" );
		//engine->ClientCmd_Unrestricted( "exec autoexec\n" );
	}
	
	GetVRSystem()->NextCreateTextureIs( Texture_HUD );
	GetVRSystem()->m_SharedTextureHolder[Texture_HUD].pTexture = pMaterialSystem->CreateNamedRenderTargetTextureEx("_rt_gui", m_Width, m_Height, RT_SIZE_NO_CHANGE, pMaterialSystem->GetBackBufferFormat(), MATERIAL_RT_DEPTH_SHARED, TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT, CREATERENDERTARGETFLAGS_HDR );

	GetVRSystem()->NextCreateTextureIs( Texture_LeftEye );
	GetVRSystem()->m_SharedTextureHolder[Texture_LeftEye].pTexture = pMaterialSystem->CreateNamedRenderTargetTextureEx("leftEye0", m_RenderWidth, m_RenderHeight, RT_SIZE_NO_CHANGE, IMAGE_FORMAT_RGBA8888, MATERIAL_RT_DEPTH_SEPARATE, TEXTUREFLAGS_RENDERTARGET | TEXTUREFLAGS_NOMIP );

	GetVRSystem()->NextCreateTextureIs( Texture_RightEye );
	GetVRSystem()->m_SharedTextureHolder[Texture_RightEye].pTexture = pMaterialSystem->CreateNamedRenderTargetTextureEx("rightEye0", m_RenderWidth, m_RenderHeight, RT_SIZE_NO_CHANGE, IMAGE_FORMAT_RGBA8888, MATERIAL_RT_DEPTH_SEPARATE, TEXTUREFLAGS_RENDERTARGET | TEXTUREFLAGS_NOMIP );

	pMaterialSystem->EndRenderTargetAllocation();

	AcquireNewZeroPose();

	m_CreatedVRTextures = true;
}


VMatrix CSourceVR::GetMidEyeFromEye( vr::EVREye eEye )
{
	if( m_pHmd )
	{
		vr::HmdMatrix34_t matMidEyeFromEye = m_pHmd->GetEyeToHeadTransform( eEye );
		return OpenVRToSourceCoordinateSystem( VMatrixFrom34( matMidEyeFromEye.m ) );
	}
	else
	{
		VMatrix mat;
		mat.Identity();
		return mat;
	}
}


void CSourceVR::AcquireNewZeroPose()
{
	//Пока нет идей как сделать правильно!
	vr::VRChaperone()->ResetZeroPose( vr::TrackingUniverseSeated );
	m_RotationOffset = 0.0f;
	RepositionHud( true );
}


void CSourceVR::RenderViewDesktop()
{
	if( !m_ScreenMat || !m_CreatedVRTextures )
		return;

	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->SetRenderTarget( NULL );

	int eyeWidth = m_RenderWidth;
	int eyeHeight = m_RenderHeight;

	float outWidth = (float)eyeWidth;
	float outHeight = outWidth * ( (float)m_Height / (float)m_Width );
	float heightOffset = ( eyeHeight - outHeight ) / 2;

	pRenderContext->ClearBuffers( true, true );
	
	// crop one of the eyes onto the screen
	pRenderContext->DrawScreenSpaceRectangle(
		m_ScreenMat,
		0, 0,
		m_Width, m_Height,
		0, heightOffset,
		outWidth-1, heightOffset + outHeight-1,
		eyeWidth, eyeHeight
	);

	pRenderContext.SafeRelease();

	//Рендрем HUD и меню на рабочий стол если видно курсор (Оно надо?)
	if(!g_pVGuiSurface->IsCursorVisible())
		return;

	CViewSetup view2d;
	view2d.x				= 0;
	view2d.y				= 0;
	view2d.width			= m_Width;
	view2d.height			= m_Height;
    
	render->Push2DView(view2d, 0, NULL, NULL);
	render->VGui_Paint(PAINT_UIPANELS | PAINT_CURSOR | PAINT_INGAMEPANELS);
	render->PopView(NULL);
}


bool CSourceVR::GetControllerPose( vr::ETrackedControllerRole controllerRole, VMatrix& matResult ) {
	Vector vmorigin;
	QAngle vmangles;

	// Получаем индекс контроллера по его роли
	vr::TrackedDeviceIndex_t controllerIndex = m_pHmd->GetTrackedDeviceIndexForControllerRole( controllerRole );
	if (controllerIndex == -1)
		return false;

	vr::TrackedDevicePose_t controllerPose = m_Poses[controllerIndex];
	if (!controllerPose.bPoseIsValid)
		return false;
	
	VMatrix matOffsetSnapTurn;
	matOffsetSnapTurn.SetupMatrixOrgAngles( Vector( 0, 0, 5 ), QAngle( 0, m_RotationOffset, 0 ) );

	VMatrix matOffsetPose = matOffsetSnapTurn * OpenVRToSourceCoordinateSystem( VMatrixFrom34( controllerPose.mDeviceToAbsoluteTracking.m ) );
	MatrixAngles(matOffsetPose.As3x4(), vmangles, vmorigin);

	if( vmorigin.IsValid() ) {
		// Компенсация дрожания
		vmangles = m_SmoothController[controllerRole].SmoothAngles( vmangles, 0.3 );
		vmorigin = m_SmoothController[controllerRole].SmoothMovement( vmorigin, 0.3 );
	}

	vmorigin = vmorigin - m_ZeroFromHeadPose.GetTranslation();

	// Настраиваем матрицы с учетом позиции и углов
	VMatrix matOffset;
	matOffset.SetupMatrixOrgAngles( vec3_origin, QAngle( 45,0,0 ) );

	VMatrix controllerMatrix;
	controllerMatrix.SetupMatrixOrgAngles( vmorigin, vmangles );

	matResult = controllerMatrix * matOffset;
	
	return true;
}


void CSourceVR::DoDistortionProcessing( vr::EVREye eEye )
{
	if( !m_CreatedVRTextures )
		return;

	if( eEye == vr::Eye_Left )
	{
		CViewSetup view2d;
		view2d.x				= 0;
		view2d.y				= 0;
		view2d.width			= m_Width;
		view2d.height			= m_Height;
    
		render->Push2DView(view2d, 0, GetVRSystem()->m_SharedTextureHolder[Texture_HUD].pTexture, NULL);
		CMatRenderContextPtr pRenderContext(g_pMaterialSystem);
		
		pRenderContext->ClearColor4ub(0, 0, 0, 0);
		pRenderContext->ClearBuffers(true, false);
		pRenderContext->OverrideAlphaWriteEnable(true, true);

		render->VGui_Paint( PAINT_UIPANELS | PAINT_CURSOR | PAINT_INGAMEPANELS );
		pRenderContext->OverrideAlphaWriteEnable(false, true);
		render->PopView(NULL);
	}

	render->Push3DView( m_View[eEye], 0, GetVRSystem()->m_SharedTextureHolder[eEye].pTexture, NULL );
	RenderHud();
	render->PopView( NULL );
}


void CSourceVR::UpdatePoses()
{
	vr::VRCompositor()->WaitGetPoses(m_Poses, vr::k_unMaxTrackedDeviceCount, NULL, 0);

	//Вдруг background не сработает
	if( !engine->IsInGame() )
	{
		g_pVGuiSurface->SetSoftwareCursor( true );
	}

	if( m_Poses[vr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid )
	{
		VMatrix matOffsetSnapTurn;
		matOffsetSnapTurn.SetupMatrixOrgAngles( Vector( 0, 0, 5 ), QAngle( 0, m_RotationOffset, 0 ) );
		m_ZeroFromHeadPose = matOffsetSnapTurn * OpenVRToSourceCoordinateSystem( VMatrixFrom34( m_Poses[vr::k_unTrackedDeviceIndex_Hmd].mDeviceToAbsoluteTracking.m ) );
	}

	Vector viewTranslation = m_ZeroFromHeadPose.GetTranslation();
	if ( viewTranslation.IsLengthGreaterThan ( 15 ) && !engine->IsLevelMainMenuBackground() )
		AcquireNewZeroPose();

	//Позиция контроллеров
	GetControllerPose( vr::TrackedControllerRole_RightHand, m_RightControllerFromHeadPose );
	GetControllerPose( vr::TrackedControllerRole_LeftHand, m_LeftControllerFromHeadPose );
}


bool CSourceVR::GetEyeProjectionMatrix ( VMatrix *pResult, vr::EVREye eEye, float zNear, float zFar, float fovScale )
{
	if( !pResult || !m_pHmd || !m_bActive )
		return false;

	float fLeft, fRight, fTop, fBottom;
	m_pHmd->GetProjectionRaw( eEye, &fLeft, &fRight, &fTop, &fBottom );

	ComposeProjectionTransform( fLeft, fRight, fTop, fBottom, zNear, zFar, fovScale, pResult );
	return true;
}


void CSourceVR::OverrideView( CViewSetup *pSetup )
{
	if( !m_CreatedVRTextures || !g_pVGuiSurface->IsCursorVisible() )
	{
		m_PlayerViewOrigin = pSetup->origin;
	}
	else
	{
		pSetup->origin = m_PlayerViewOrigin;
	}

	pSetup->width = m_RenderWidth;
	pSetup->height = m_RenderHeight;

	m_View[vr::Eye_Left] = *pSetup;
	m_View[vr::Eye_Right] = *pSetup;
	
	static float m_WorldZoomScale = 1.0;
	if (GetAsyncKeyState(VK_MBUTTON) & 0x8000)
	{
		m_WorldZoomScale = 0.1;
    }
	else
	{
		m_WorldZoomScale = 1.0;
	}


	m_View[vr::Eye_Left].m_bViewToProjectionOverride = true;
	GetEyeProjectionMatrix ( &m_View[vr::Eye_Left].m_ViewToProjection, vr::Eye_Left,  pSetup->zNear, pSetup->zFar, 1.0f/m_WorldZoomScale );
	CalcFovFromProjection ( &m_View[vr::Eye_Left].fov, &m_View[vr::Eye_Left].m_flAspectRatio, m_View[vr::Eye_Left].m_ViewToProjection );

	m_View[vr::Eye_Right].m_bViewToProjectionOverride = true;
	GetEyeProjectionMatrix ( &m_View[vr::Eye_Right].m_ViewToProjection, vr::Eye_Right, pSetup->zNear, pSetup->zFar, 1.0f/m_WorldZoomScale );
	CalcFovFromProjection ( &m_View[vr::Eye_Right].fov, &m_View[vr::Eye_Right].m_flAspectRatio, m_View[vr::Eye_Right].m_ViewToProjection );

	C_BaseEntity *pPlayer = GetLocalPlayer();
	if( pPlayer )
	{
		if( !g_pVGuiSurface->IsCursorVisible() )
		{
			C_BaseEntity *pVehicleAnimating = pPlayer->GetEntPropEnt("m_hVehicle");
			if( pVehicleAnimating )
			{
				int eyeAttachmentIndex = pVehicleAnimating->LookupAttachment( "vehicle_driver_eyes" );

				Vector vehicleEyeOrigin;
				QAngle vehicleEyeAngles;
				pVehicleAnimating->GetAttachment( eyeAttachmentIndex, vehicleEyeOrigin, vehicleEyeAngles );

				m_RotationOffset = vehicleEyeAngles.y;
			}
		}


		////pPlayer->SetEntPropSend( "m_iFOV", (int)m_View[vr::Eye_Left].fov );
		//if ( 90.0f == pSetup->fov )
		//{
		//	pSetup->fov = m_View[vr::Eye_Left].fov;
		//	m_WorldZoomScale = 1.0f;
		//}
		//else
		//{
		//	float fGameFOV = pSetup->fov;
		//	m_WorldZoomScale = (fGameFOV == 0.0f) ? 1.0f : 
		//	tanf(DEG2RAD(Min(fGameFOV / 2.0f, 170.0f) * 0.5f)) / 
		//	tanf(DEG2RAD(60 * 0.5f));
		//	Msg("In zoom!\n");
		//}
		//Msg("Call %f\n", m_WorldZoomScale);

		pSetup->fov = m_View[vr::Eye_Left].fov;
		pPlayer->SetEntPropSend( "m_iDefaultFOV", (int)m_View[vr::Eye_Left].fov );
	}

	VMatrix worldFromTorso;
	worldFromTorso.SetupMatrixOrgAngles( pSetup->origin, ( engine->IsLevelMainMenuBackground() || ( m_RightControllerPosAbs.Length() != 0 ) ) ? vec3_angle : pSetup->angles );

	VMatrix m_WorldFromMidEye = worldFromTorso * m_ZeroFromHeadPose;
	MatrixAngles( m_WorldFromMidEye.As3x4(), m_HeadAngAbs, m_HeadPosAbs );

	//FIX Sound origin rotation
	pSetup->origin = m_HeadPosAbs;
	pSetup->angles = m_HeadAngAbs;

	// Move eyes to IPD positions.
	VMatrix worldFromLeftEye  = m_WorldFromMidEye * GetMidEyeFromEye( vr::Eye_Left );
	VMatrix worldFromRightEye = m_WorldFromMidEye * GetMidEyeFromEye( vr::Eye_Right );
	
	// Finally convert back to origin+angles.
	MatrixAngles( worldFromLeftEye.As3x4(),  m_View[vr::Eye_Left].angles, m_View[vr::Eye_Left].origin );
	MatrixAngles( worldFromRightEye.As3x4(),  m_View[vr::Eye_Right].angles, m_View[vr::Eye_Right].origin );

	//Позиция контроллеров от головы в игре
	VMatrix matOffsetWorld;
	matOffsetWorld.SetupMatrixOrgAngles( m_HeadPosAbs, vec3_angle );

	//Позиция правого контроллера в игре
	VMatrix matOffsetRight;
	matOffsetRight.SetupMatrixOrgAngles( g_pVRHands->m_vecOffsetRight, vec3_angle );

	VMatrix matResultRight = matOffsetWorld * m_RightControllerFromHeadPose * matOffsetRight;
	MatrixAngles( matResultRight.As3x4(), m_RightControllerAngAbs, m_RightControllerPosAbs );

	//Позиция левого контроллера в игре
	VMatrix matOffsetLeft;
	matOffsetLeft.SetupMatrixOrgAngles( g_pVRHands->m_vecOffsetLeft, vec3_angle );

	VMatrix matResultLeft = matOffsetWorld * m_LeftControllerFromHeadPose * matOffsetLeft;
	MatrixAngles( matResultLeft.As3x4(), m_LeftControllerAngAbs, m_LeftControllerPosAbs );

	RepositionHud();

	if( g_pVRHands )
		g_pVRHands->Update();

	//Рендерим главное меню в гарнитуру, когда не в игре.
	if( engine->IsInGame() )
		return;

	for( int i = 0; i < 2; i++ )
	{
		DoDistortionProcessing( (vr::EVREye)i );
	}
}


bool CSourceVR::RenderView( void* rcx, CViewSetup &viewRender, int nClearFlags, int whatToDraw )
{
	whatToDraw = 0;

	if( !g_pVGuiSurface->IsCursorVisible() && !engine->IsLevelMainMenuBackground() )
		whatToDraw = RENDERVIEW_DRAWVIEWMODEL;

	CMatRenderContextPtr pRenderContext( materials );

	pRenderContext->SetRenderTarget( GetVRSystem()->m_SharedTextureHolder[vr::Eye_Left].pTexture );
	hkRenderView.fOriginal( rcx, m_View[vr::Eye_Left], nClearFlags, whatToDraw );
	DoDistortionProcessing( vr::Eye_Left );

	whatToDraw |= RENDERVIEW_SUPPRESSMONITORRENDERING;

	pRenderContext->SetRenderTarget( GetVRSystem()->m_SharedTextureHolder[vr::Eye_Right].pTexture );
	hkRenderView.fOriginal( rcx, m_View[vr::Eye_Right], nClearFlags, whatToDraw );
	DoDistortionProcessing( vr::Eye_Right );

	pRenderContext.SafeRelease();

	RenderViewDesktop();

	return false;
}


void CSourceVR::PostPresent()
{
	static bool m_bLoadMainMenu = true;
	
	if( !engine->IsInGame() )
	{
		if( m_bLoadMainMenu || engine->IsDrawingLoadingImage() )
		{
			m_CreatedVRTextures = false;
			m_fPause = engine->Time() + 1.0;
			vr::VRCompositor()->ClearLastSubmittedFrame();
		}
		m_bLoadMainMenu = false;
	}
	else
	{
		m_bLoadMainMenu = true;
	}

	if( !m_CreatedVRTextures && m_fPause <= engine->Time() )
	{
		CreateRenderTargets( materials );

		if( engine->IsLevelMainMenuBackground() )
		{
			m_RotationOffset = 180.0f;
			RepositionHud( true );

			engine->ClientCmd_Unrestricted("hidepanel info\n");
			engine->ClientCmd_Unrestricted("gameui_activate\n");
			engine->ClientCmd_Unrestricted("sv_cheats 1\n");
			engine->ClientCmd_Unrestricted("setpos -3163 2949 72\n");
			engine->ClientCmd_Unrestricted("sv_cheats 0\n");
			m_PlayerViewOrigin = Vector( -3163, 2949, 72 );
		}

		if( !engine->IsInGame() )
		{
			extern void CreateLoadingDialog();
			CreateLoadingDialog();
			m_CreatedVRTextures = false;
			m_fPause = engine->Time() + 1.0;
			vr::VRCompositor()->ClearLastSubmittedFrame();
			engine->ClientCmd_Unrestricted("map_background dm_lockdown\n");
		}
	}

	if( m_CreatedVRTextures )
	{
		vr::VRCompositor()->Submit( vr::Eye_Left, &GetVRSystem()->m_SharedTextureHolder[vr::Eye_Left].m_VRTexture );
		vr::VRCompositor()->Submit( vr::Eye_Right, &GetVRSystem()->m_SharedTextureHolder[vr::Eye_Right].m_VRTexture );
	}

	UpdatePoses();

	if( g_pVGuiSurface->IsCursorVisible() )
		ProcessMenuInput();
	
	HandleSkeletalInput();
}


void CSourceVR::RepositionHud( bool bReset )
{
	if( g_pVGuiSurface->IsCursorVisible() )
	{
		if( bReset )
		{
			m_HudAngles.y = m_RotationOffset;	
		}

		m_WorldFromHud.SetupMatrixOrgAngles( m_PlayerViewOrigin, m_HudAngles );
		return;
	}

	//HuD подгоняем к игроку плавно
	static bool bEnableCorrect;
	float diff = AngleDiff( m_HeadAngAbs.y, m_HudAngles.y );

	if( fabs(diff) > 10.0f )
		bEnableCorrect = true;
	else if( fabs(diff) < 2.0f )
		bEnableCorrect = false;

	if( bEnableCorrect )
	{
		m_HudAngles.y += diff * 0.1;
		AngleNormalize( m_HudAngles.y );
	}

	m_WorldFromHud.SetupMatrixOrgAngles( m_HeadPosAbs, m_HudAngles );
}


int CSourceVR::SetActionManifest(const char *fileName) 
{
    char currentDir[MAX_STR_LEN];
    GetCurrentDirectory(MAX_STR_LEN, currentDir);
    char path[MAX_STR_LEN];
    sprintf_s(path, MAX_STR_LEN, "%s\\SteamVRActionManifest\\%s", currentDir, fileName);

    if (m_Input->SetActionManifestPath(path) != vr::VRInputError_None) 
        Msg("SetActionManifestPath failed\n");

    m_Input->GetActionHandle("/actions/main/in/Jump", &m_ActionJump);
    m_Input->GetActionHandle("/actions/main/in/PrimaryAttack", &m_ActionPrimaryAttack);
    m_Input->GetActionHandle("/actions/main/in/Reload", &m_ActionReload);
    m_Input->GetActionHandle("/actions/main/in/Use", &m_ActionUse);
    m_Input->GetActionHandle("/actions/main/in/Walk", &m_ActionWalk);
    m_Input->GetActionHandle("/actions/main/in/Turn", &m_ActionTurn);
    m_Input->GetActionHandle("/actions/main/in/SecondaryAttack", &m_ActionSecondaryAttack);
    m_Input->GetActionHandle("/actions/main/in/NextItem", &m_ActionNextItem);
    m_Input->GetActionHandle("/actions/main/in/PrevItem", &m_ActionPrevItem);
    m_Input->GetActionHandle("/actions/main/in/ResetPosition", &m_ActionResetPosition);
    m_Input->GetActionHandle("/actions/main/in/Crouch", &m_ActionCrouch);
    m_Input->GetActionHandle("/actions/main/in/Flashlight", &m_ActionFlashlight);
    m_Input->GetActionHandle("/actions/main/in/Spray", &m_Spray);
    m_Input->GetActionHandle("/actions/main/in/Scoreboard", &m_Scoreboard);
    m_Input->GetActionHandle("/actions/main/in/Pause", &m_Pause);

	m_Input->GetActionHandle("/actions/main/in/skeleton_lefthand", &m_LeftHandSkeleton);
	m_Input->GetActionHandle("/actions/main/in/skeleton_righthand", &m_RightHandSkeleton);

    m_Input->GetActionSetHandle("/actions/main", &m_ActionSet);
    
    m_ActiveActionSet.ulActionSet = m_ActionSet;

    return 0;
}


void CSourceVR::InstallApplicationManifest(const char *fileName)
{
    char currentDir[MAX_STR_LEN];
    GetCurrentDirectory(MAX_STR_LEN, currentDir);
    char path[MAX_STR_LEN];
    sprintf_s(path, MAX_STR_LEN, "%s\\SteamVRActionManifest\\%s", currentDir, fileName);

    vr::VRApplications()->AddApplicationManifest(path);
}


bool CSourceVR::PressedDigitalAction( vr::VRActionHandle_t &actionHandle, bool checkIfActionChanged )
{
    vr::InputDigitalActionData_t digitalActionData;
    vr::EVRInputError result = m_Input->GetDigitalActionData( actionHandle, &digitalActionData, sizeof( digitalActionData ), vr::k_ulInvalidInputValueHandle );
    
    if( result == vr::VRInputError_None )
    {
        if( checkIfActionChanged )
            return digitalActionData.bState && digitalActionData.bChanged;
        else
            return digitalActionData.bState;
    }

    return false;
}


bool CSourceVR::GetAnalogActionData( vr::VRActionHandle_t &actionHandle, vr::InputAnalogActionData_t &analogDataOut )
{
    vr::EVRInputError result = m_Input->GetAnalogActionData( actionHandle, &analogDataOut, sizeof( analogDataOut ), vr::k_ulInvalidInputValueHandle );

    if( result == vr::VRInputError_None )
        return true;

    return false;
}


void CSourceVR::HandleSkeletalInput()
{
	vr::InputSkeletalActionData_t data;
	
	vr::VRInput()->GetSkeletalActionData( m_LeftHandSkeleton, &data, sizeof(vr::InputSkeletalActionData_t));
	if (data.bActive)
		vr::VRInput()->GetSkeletalSummaryData( m_LeftHandSkeleton, vr::EVRSummaryType::VRSummaryType_FromAnimation, &m_vrSkeletalSummary[0] );

	vr::VRInput()->GetSkeletalActionData( m_RightHandSkeleton, &data, sizeof(vr::InputSkeletalActionData_t));
	if (data.bActive)
		vr::VRInput()->GetSkeletalSummaryData( m_RightHandSkeleton, vr::EVRSummaryType::VRSummaryType_FromAnimation, &m_vrSkeletalSummary[1] );
}


void CSourceVR::ProcessMenuInput()
{
	m_Input->UpdateActionState(&m_ActiveActionSet, sizeof(vr::VRActiveActionSet_t), 1);

	if (PressedDigitalAction(m_Pause, true))
		engine->ClientCmd_Unrestricted("gameui_hide");

	static bool bPressedButtonLeft;
	if(	PressedDigitalAction( m_ActionPrimaryAttack ) )
    {
		INPUT input;
		input.type = INPUT_MOUSE;
        input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
        SendInput(1, &input, sizeof(INPUT));

		bPressedButtonLeft = true;
    }
	else if( bPressedButtonLeft )
    {
		INPUT input;
		input.type = INPUT_MOUSE;
		input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
		SendInput(1, &input, sizeof(INPUT));

		bPressedButtonLeft = false;
    }
	
	vr::InputAnalogActionData_t analogActionData;

	if (GetAnalogActionData(m_ActionTurn, analogActionData))
	{
		float delta = analogActionData.y;
		float absDelta = fabs(delta);
    
		// Настраиваемый порог с гистерезисом
		const float DEAD_ZONE = 0.1f;
		if(absDelta <= DEAD_ZONE)
			return;

		// Защита от слишком частых вызовов
		static float m_Wait;
		float currentTime = engine->Time();
    
		if(m_Wait > currentTime)
			return;

		// Настраиваемая задержка (мин 0.1с, макс 1.0с)
		float delay = 0.05f + (0.9f * (1.0f - absDelta));
		m_Wait = currentTime + delay;

		// Прокрутка в нужном направлении
		int scrollValue = static_cast<int>((delta > 0) ? WHEEL_DELTA : -WHEEL_DELTA);

		INPUT input = {0};
		input.type = INPUT_MOUSE;
		input.mi.dwFlags = MOUSEEVENTF_WHEEL;
		input.mi.mouseData = scrollValue;
		SendInput(1, &input, sizeof(INPUT));
	}
}


void CSourceVR::UpdateMouse( Vector pUpperLeft, Vector pUpperRight, Vector pLowerLeft )
{
	Vector vecMuzzleDir;
	AngleVectors( m_RightControllerAngAbs, &vecMuzzleDir);

	Vector vecEnd = m_RightControllerPosAbs + vecMuzzleDir * 8000.0f;
	
	Ray_t lookDir;
	lookDir.Init( m_RightControllerPosAbs, vecEnd );

	float u, v;

	if( !ComputeIntersectionBarycentricCoordinates( lookDir, pUpperLeft, pUpperRight, pLowerLeft, u, v, NULL ) )
		return;

	if ( ((u < 0) || (v < 0) || (u > 1) || (v > 1)) )
		return;

	int px = (int)(u * m_Width + 0.5f);
	int py = (int)(v * m_Height + 0.5f);

	static int cursorX;
	static int cursorY;

	if( ( px != cursorX ) || ( cursorY != py ) )
	{
		g_InputInternal->SetCursorPos(px, py);
		cursorX = px;
		cursorY = py;
	}
}


void CSourceVR::GetHUDBounds( Vector *pUL, Vector *pUR, Vector *pLL, Vector *pLR )
{
	static const float fAspectRatio = (float)m_Width/(float)m_Height;
	float fHFOV = 60;
	float fVFOV = 60/fAspectRatio;

	const float fHudForward = 100.0;
	float fHudHalfWidth = tan( DEG2RAD( fHFOV * 0.5f ) ) * fHudForward * 1.0;
	float fHudHalfHeight = tan( DEG2RAD( fVFOV * 0.5f ) ) * fHudForward * 1.0;

	Vector vHalfWidth = m_WorldFromHud.GetLeft() * -fHudHalfWidth;
	Vector vHalfHeight = m_WorldFromHud.GetUp() * fHudHalfHeight;
	Vector vHUDOrigin = m_WorldFromHud.GetTranslation() + m_WorldFromHud.GetForward() * fHudForward;

	*pUL = vHUDOrigin - vHalfWidth + vHalfHeight;
	*pUR = vHUDOrigin + vHalfWidth + vHalfHeight;
	*pLL = vHUDOrigin - vHalfWidth - vHalfHeight;
	*pLR = vHUDOrigin + vHalfWidth - vHalfHeight;

	if( g_pVGuiSurface->IsCursorVisible() )
		UpdateMouse( *pUL, *pUR, *pLL );
}


void CSourceVR::CreateMove( float flInputSampleTime, CUserCmd* cmd )
{
	if( !m_bActive )
		return;

	//if( cmd && cmd->command_number != 0 )
	//{
	//	return;
	//}

	C_BaseEntity *pPlayer = GetLocalPlayer();
	if( !pPlayer || !m_CreatedVRTextures || g_pVGuiSurface->IsCursorVisible() )
		return;

	m_Input->UpdateActionState(&m_ActiveActionSet, sizeof(vr::VRActiveActionSet_t), 1);

	Vector vecMuzzleDir;
	AngleVectors( m_RightControllerAngAbs, &vecMuzzleDir);

	Vector vecEnd = m_RightControllerPosAbs + vecMuzzleDir * 8000.0f;
	
	trace_t trace;
	UTIL_TraceLine( m_RightControllerPosAbs, vecEnd, MASK_SHOT, &trace );

	Vector vecDirToEnemyEyes = ( trace.endpos - pPlayer->EyePosition() ).Normalized();

	if( !vecDirToEnemyEyes.IsValid() )
		return;

	QAngle viewangles;
	VectorAngles( vecDirToEnemyEyes, viewangles );

	//weapon_rpg
	UpdateLaserDot( trace.endpos + ( trace.plane.normal * 4.0f ) );

	if (GetAsyncKeyState(VK_MBUTTON) & 0x8000)
	{
		//C_BaseEntity *pViewModel = GetViewModel();
	//	hkSendViewModelMatchingSequence.fOriginal( pViewModel, 9 );
    }

	if( pPlayer->IsAlive() && ( m_RightControllerPosAbs.Length() != 0 ) )
		cmd->viewangles = viewangles;

	vr::InputAnalogActionData_t analogActionData;

	bool bPushingThumbstick = false;
	
	if (GetAnalogActionData(m_ActionWalk, analogActionData))		
	{
		if( fabs( analogActionData.y ) > 0.1f )
		{
			cmd->forwardmove = 450 * analogActionData.y;
		}

		if( fabs( analogActionData.x ) > 0.1 )
		{
			cmd->sidemove = 450 * analogActionData.x;
		}

		if( cmd->forwardmove > 0 )
		{
			cmd->buttons |= IN_FORWARD;
		}
		else if( cmd->forwardmove < 0 )
		{
			cmd->buttons |= IN_BACK;
		}

		bPushingThumbstick = fabs(analogActionData.y) > 0.1f || fabs(analogActionData.x) > 0.1f;

		if( pPlayer->GetEntPropEnt("m_hVehicle") == NULL )
		{
			//Направление движения от головы
			float yawDelta = cmd->viewangles.y - m_HeadAngAbs.y;
			float yawCos = cos(DEG2RAD(yawDelta));
			float yawSin = sin(DEG2RAD(yawDelta));

			Vector newMotion;
			newMotion.x = cmd->forwardmove * yawCos - cmd->sidemove * yawSin;
			newMotion.y = cmd->forwardmove * yawSin + cmd->sidemove * yawCos;

			cmd->forwardmove = newMotion.x;
			cmd->sidemove = newMotion.y;
		}
	}

	if (bPushingThumbstick && PressedDigitalAction(m_ActionResetPosition))
		cmd->buttons |= IN_SPEED;

	if (!bPushingThumbstick && PressedDigitalAction(m_ActionResetPosition, true))
		AcquireNewZeroPose();

	static bool bPressedButtonAttack = false;
	if (PressedDigitalAction(m_ActionPrimaryAttack))
	{
		engine->ClientCmd_Unrestricted("+attack");
		bPressedButtonAttack = true;
	}
	else if( bPressedButtonAttack )
	{
		engine->ClientCmd_Unrestricted("-attack");
		bPressedButtonAttack = false;
	}

	if (PressedDigitalAction(m_ActionSecondaryAttack))
		cmd->buttons |= IN_ATTACK2;

	if (PressedDigitalAction(m_ActionJump))
		cmd->buttons |= IN_JUMP;

	if (PressedDigitalAction(m_ActionUse))
		cmd->buttons |= IN_USE;

	if (PressedDigitalAction(m_ActionReload))
		cmd->buttons |= IN_RELOAD;

	if (PressedDigitalAction(m_ActionCrouch))
		cmd->buttons |= IN_DUCK;

	if (PressedDigitalAction(m_ActionPrevItem, true))
		engine->ClientCmd_Unrestricted("invprev");
	else if (PressedDigitalAction(m_ActionNextItem, true))
		engine->ClientCmd_Unrestricted("invnext");

	if (PressedDigitalAction(m_ActionFlashlight, true))
		engine->ClientCmd_Unrestricted("impulse 100");

	if (PressedDigitalAction(m_Spray, true))
		engine->ClientCmd_Unrestricted("impulse 201");

	if (PressedDigitalAction(m_Pause, true))
		engine->ClientCmd_Unrestricted("gameui_activate");

	static bool bPressedButtonScore = false;
	bool isControllerVertical = m_RightControllerAngAbs.x > 60 || m_RightControllerAngAbs.x < -45;
	if (PressedDigitalAction(m_Scoreboard) || isControllerVertical )
	{
		engine->ClientCmd_Unrestricted("+score");
		bPressedButtonScore = true;
	}
	else if( bPressedButtonScore )
	{
		engine->ClientCmd_Unrestricted("-score");
		bPressedButtonScore = false;
	}

	static bool bPressedTurn = false;
	if (GetAnalogActionData(m_ActionTurn, analogActionData))
	{
		if (!bPressedTurn && analogActionData.x > 0.5)
		{
			m_RotationOffset -= 45.0;
			bPressedTurn = true;
		}
		else if (!bPressedTurn && analogActionData.x < -0.5)
		{
			m_RotationOffset += 45.0;
			bPressedTurn = true;
		}
		else if (analogActionData.x < 0.3 && analogActionData.x > -0.3)
			bPressedTurn = false;

		if( bPressedTurn )
			m_RotationOffset -= 360 * std::floor(m_RotationOffset / 360);
	}
}