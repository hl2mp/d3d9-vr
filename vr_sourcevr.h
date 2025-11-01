#include "vr_system.h"
#include "studio.h"
#include "bone_setup.h"
#include "view_shared.h"
#include "tier3/tier3.h"
#include <vgui/IVGui.h>
#include "vgui/ISurface.h"
#include "vgui/IPanel.h"
#include "vgui/iLocalize.h"
#include "vgui/ISystem.h"
#include "vgui/IScheme.h"
#include "cdll_int.h"
#include "KeyValues.h"
#include "vgui/IInputInternal.h"
#include "engine/IEngineTrace.h"
#include "icliententitylist.h"
#include "filesystem.h"
#include "ienginevgui.h"
#include "ivrenderview.h"
#include "in_buttons.h"
#include "iefx.h"
#include "vguimatsurface\iMatSystemSurface.h"

extern IVEngineClient *engine;
extern vgui::IInputInternal *g_InputInternal;
extern IEngineTrace *enginetrace;
extern IFileSystem *filesystem;
extern IVModelInfoClient *modelinfo;
extern IVRenderView *render;
extern IVEfx *effects;
extern IMatSystemSurface *g_pMatSystemSurface;

class CUserCmd;

inline void CalcFovFromProjection(float *pFov, const VMatrix &proj, bool bUseConservative = true)
{
    float xscale = proj.m[0][0];
    float xoffset = proj.m[0][2];
    float yscale = proj.m[1][1]; 
    float yoffset = proj.m[1][2];

    if (bUseConservative) {
        // Conservative approach: use maximum possible FOV
        // Important for VR to avoid clipping
        float ratio1 = fabsf((-1.0f - xoffset) / xscale);
        float ratio2 = fabsf((1.0f - xoffset) / xscale);
        float ratio3 = fabsf((-1.0f - yoffset) / yscale);
        float ratio4 = fabsf((1.0f - yoffset) / yscale);
        
        float maxRatio = ratio1;
        if (ratio2 > maxRatio) maxRatio = ratio2;
        if (ratio3 > maxRatio) maxRatio = ratio3;
        if (ratio4 > maxRatio) maxRatio = ratio4;
        
        *pFov = 2.0f * RAD2DEG(atanf(maxRatio));
    } else {
        // Balanced approach: average of left/right and top/bottom
        float fovX = RAD2DEG(atanf((1.0f - xoffset) / xscale) + atanf((-1.0f - xoffset) / -xscale));
        float fovY = RAD2DEG(atanf((1.0f - yoffset) / yscale) + atanf((-1.0f - yoffset) / -yscale));
        *pFov = (fovX > fovY) ? fovX : fovY;
    }

	//*pFov = 110;
}

inline void ComposeProjectionTransform(float fLeft, float fRight, 
    float fTop, float fBottom, float zNear, float zFar, 
    float fovScale, VMatrix *pmProj)
{
    // ”прощенное FOV масштабирование
    if (fovScale != 1.0f && fovScale > 0.f)
    {
        float scale = 1.0f / fovScale;
        fRight *= scale;
        fLeft *= scale;
        fTop *= scale;
        fBottom *= scale;
    }

    float idx = 1.0f / (fRight - fLeft);
    float idy = 1.0f / (fBottom - fTop);
    float idz = 1.0f / (zFar - zNear);
    float sx = fRight + fLeft;
    float sy = fBottom + fTop;

    float (*p)[4] = pmProj->m;
    p[0][0] = 2*idx; p[0][1] = 0;     p[0][2] = sx*idx;     p[0][3] = 0;
    p[1][0] = 0;     p[1][1] = 2*idy; p[1][2] = sy*idy;     p[1][3] = 0;
    p[2][0] = 0;     p[2][1] = 0;     p[2][2] = -(zFar + zNear)*idz; p[2][3] = -2.0f*zFar*zNear*idz;
    p[3][0] = 0;     p[3][1] = 0;     p[3][2] = -1.0f;      p[3][3] = 0;
}

static VMatrix OpenVRToSourceCoordinateSystem(const VMatrix& vortex)
{
	const float inchesPerMeter = (float)(39.3700787);

	const vec_t (*v)[4] = vortex.m;
	VMatrix result(
		v[2][2],  v[2][0], -v[2][1], -v[2][3] * inchesPerMeter,
		v[0][2],  v[0][0], -v[0][1], -v[0][3] * inchesPerMeter,
		-v[1][2], -v[1][0],  v[1][1],  v[1][3] * inchesPerMeter,
		-v[3][2], -v[3][0],  v[3][1],  v[3][3]);

	return result;
}


static VMatrix VMatrixFrom34(const float v[3][4])
{
	return VMatrix(
		v[0][0], v[0][1], v[0][2], v[0][3],
		v[1][0], v[1][1], v[1][2], v[1][3],
		v[2][0], v[2][1], v[2][2], v[2][3],
		0,       0,       0,       1       );
}


class SmoothController {
private:
    Vector smoothedPosition;
    QAngle currentAngle;

    float NormalizeAngle(float angle) {
        while (angle > 180.0f) angle -= 360.0f;
        while (angle < -180.0f) angle += 360.0f;
        return angle;
    }

public:
    SmoothController() : smoothedPosition(0.0f, 0.0f, 0.0f), currentAngle(0.0f, 0.0f, 0.0f) {}

    Vector SmoothMovement(const Vector& currentPosition, float smoothingFactor) {
        smoothedPosition.x += (currentPosition.x - smoothedPosition.x) * smoothingFactor;
        smoothedPosition.y += (currentPosition.y - smoothedPosition.y) * smoothingFactor;
        smoothedPosition.z += (currentPosition.z - smoothedPosition.z) * smoothingFactor;

        return smoothedPosition;
    }

    QAngle SmoothAngles(const QAngle& target, float smoothingFactor) {
        QAngle smoothed;

        smoothed.x = NormalizeAngle(currentAngle.x + (NormalizeAngle(target.x - currentAngle.x)) * smoothingFactor);
        smoothed.y = NormalizeAngle(currentAngle.y + (NormalizeAngle(target.y - currentAngle.y)) * smoothingFactor);
        smoothed.z = NormalizeAngle(currentAngle.z + (NormalizeAngle(target.z - currentAngle.z)) * smoothingFactor);

        currentAngle = smoothed;
        return smoothed;
    }
};

static SmoothController m_SmoothController[2];

class CSourceVR
{
public:
	CSourceVR() {};
	~CSourceVR() {};

	void Activate();
	void CreateRenderTargets( IMaterialSystem *pMaterialSystem );
	VMatrix GetMidEyeFromEye( vr::EVREye eEye );

	void RepositionHud( bool bReset = false );
	
	void AcquireNewZeroPose();
	void RenderViewDesktop();
	bool GetControllerPose( vr::ETrackedControllerRole controllerRole, VMatrix& matResult );
	
	void DoDistortionProcessing( vr::EVREye eEye );

	void UpdatePoses();
	bool GetEyeProjectionMatrix ( VMatrix *pResult, vr::EVREye eEye, float zNear, float zFar, float fovScale );
	void OverrideView( CViewSetup *pSetup );
	bool RenderView( void* rcx, CViewSetup &viewRender, int nClearFlags, int whatToDraw );
	void PostPresent();

	int	SetActionManifest(const char *fileName);
	void InstallApplicationManifest(const char *fileName);
	bool PressedDigitalAction(vr::VRActionHandle_t &actionHandle, bool checkIfActionChanged = false);
	bool GetAnalogActionData(vr::VRActionHandle_t &actionHandle, vr::InputAnalogActionData_t &analogDataOut);
	void HandleSkeletalInput();
	void ProcessMenuInput();
	void UpdateMouse( Vector pUpperLeft, Vector pUpperRight, Vector pLowerLeft );
	void GetHUDBounds( Vector *pUL, Vector *pUR, Vector *pLL, Vector *pLR );
	void CreateMove( float flInputSampleTime, CUserCmd* cmd );
	void BuildTransformations( void *ecx, CStudioHdr *hdr, Vector *pos, Quaternion *q, matrix3x4_t &cameraTransform, int boneMask, CBoneBitList &boneComputed );
	void RenderHud();
	void DrawWorldUi();

	CViewSetup m_View[2];

	vr::IVRSystem *m_pHmd;
	vr::IVRInput *m_Input;

	vr::TrackedDevicePose_t m_Poses[vr::k_unMaxTrackedDeviceCount];

	Vector m_PlayerViewOrigin; //Original view origin

	VMatrix m_ZeroFromHeadPose;
	Vector m_HeadPosAbs;
	QAngle m_HeadAngAbs;

	Vector    m_RightControllerPosAbs;
	QAngle    m_RightControllerAngAbs;
	VMatrix   m_RightControllerFromHeadPose;

	Vector    m_LeftControllerPosAbs;
	QAngle    m_LeftControllerAngAbs;
	VMatrix   m_LeftControllerFromHeadPose;

	VMatrix m_WorldFromHud;
	QAngle m_HudAngles;

	bool m_CreatedVRTextures;
	float m_fPause;

	uint32_t m_RenderWidth;
	uint32_t m_RenderHeight;

	int m_Width;
	int m_Height;

	bool m_bActive;

	//Ёта текстура будет показыватьс€ на рабочем столе
	IMaterial *m_ScreenMat;

	// action set
	vr::VRActionSetHandle_t m_ActionSet;
	vr::VRActiveActionSet_t m_ActiveActionSet;

	// actions
	vr::VRActionHandle_t m_ActionJump;
	vr::VRActionHandle_t m_ActionPrimaryAttack;
	vr::VRActionHandle_t m_ActionSecondaryAttack;
	vr::VRActionHandle_t m_ActionReload;
	vr::VRActionHandle_t m_ActionWalk;
	vr::VRActionHandle_t m_ActionTurn;
	vr::VRActionHandle_t m_ActionUse;
	vr::VRActionHandle_t m_ActionNextItem;
	vr::VRActionHandle_t m_ActionPrevItem;
	vr::VRActionHandle_t m_ActionResetPosition;
	vr::VRActionHandle_t m_ActionCrouch;
	vr::VRActionHandle_t m_ActionFlashlight;
	vr::VRActionHandle_t m_Spray; 
	vr::VRActionHandle_t m_Scoreboard;
	vr::VRActionHandle_t m_Pause;

	vr::VRActionHandle_t m_LeftHandSkeleton;
	vr::VRActionHandle_t m_RightHandSkeleton;

	vr::VRSkeletalSummaryData_t m_vrSkeletalSummary[2];

	float m_RotationOffset;
};

extern CSourceVR *m_VR;

static vgui::VPANEL VFindChildByName( vgui::VPANEL root, const char *childName, bool recurseDown = true )
{
	for( int i = 0; i < g_pVGuiPanel->GetChildCount( root ); i++ )
	{
		vgui::VPANEL pChild = g_pVGuiPanel->GetChild( root, i );
		if( !pChild )
			continue;

		if( !V_stricmp( g_pVGuiPanel->GetName( pChild ), childName ) )
			return pChild;
		
		if( recurseDown )
		{
			vgui::VPANEL panel = VFindChildByName( pChild, childName, recurseDown );
			if ( panel )
				return panel;
		}
	}

	return NULL;
}