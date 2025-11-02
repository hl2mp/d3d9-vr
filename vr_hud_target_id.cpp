#include "vr_sourcevr.h"
#include "sdk.h"

#include <vgui_controls/Panel.h>

// basic team colors
#define COLOR_RED		Color(255, 64, 64, 255)
#define COLOR_BLUE		Color(153, 204, 255, 255)
#define COLOR_YELLOW	Color(255, 178, 0, 255)

class CTargetID : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE(CTargetID, vgui::Panel); 

	CTargetID(); 	// Constructor
	~CTargetID(){};				// Destructor
 
protected:
	virtual void Paint( void );

private:
	float m_flLastChangeTime;
	int m_iLastEntIndex;
 
};

vgui::VPANEL pTargetID;


class CTargetIDAutoCreate
{
public:
    void Init()
    {
		while( !m_VR->m_bActive )
			Sleep(50);

		CTargetID *MyPanel = new CTargetID();
		pTargetID = MyPanel->GetVPanel();
    }

    // —татический метод дл€ передачи в поток
    static DWORD WINAPI InitHL2MP_Thread(LPVOID lpParam)
    {
        CTargetIDAutoCreate* pThis = (CTargetIDAutoCreate*)lpParam;
        pThis->Init();
        return 0;
    }

    CTargetIDAutoCreate()
    {
        // —оздаем поток с правильными параметрами
        CreateThread(NULL, 0, InitHL2MP_Thread, this, 0, NULL);
    }
} g_CTargetIDAutoCreate;


CTargetID::CTargetID()
: BaseClass(NULL, "TargetID2")
{
	vgui::VPANEL pRoot = enginevgui->GetPanel( PANEL_ROOT );
	vgui::VPANEL pChild = VFindChildByName( pRoot, "TargetID" );
	g_pVGuiPanel->DeletePanel( pChild );

	SetSize( m_VR->m_RenderWidth, m_VR->m_RenderHeight );

	m_flLastChangeTime = 0.0;
	m_iLastEntIndex = 0;
}


int GetIDTarget()
{
	C_BaseEntity *pPlayer = GetLocalPlayer();
	if( !pPlayer || !pPlayer->IsAlive() )
		return 0;

	int index = 0;

	trace_t tr;

	QAngle m_angEyeAngles;
	engine->GetViewAngles( m_angEyeAngles );
	m_angEyeAngles[2] = 0.0f;

	Vector vecMuzzleDir;
	AngleVectors( m_angEyeAngles, &vecMuzzleDir);

	Vector vecEnd = pPlayer->EyePosition() + vecMuzzleDir * 1500.0f;

	UTIL_TraceLine( pPlayer->EyePosition(), vecEnd, MASK_SOLID, &tr );

	if ( !tr.startsolid && tr.DidHitNonWorldEntity() )
	{
		index = tr.GetEntityIndex();
	}

	return index;
}


Color GetColorForTargetTeam( int team )
{
	Color tColor;
	switch( team ) {
		case 0:
			tColor = COLOR_YELLOW;
		break;
		case 2:
			tColor = COLOR_BLUE;
		break;
		case 3:
			tColor = COLOR_RED;
		break;
		default:
			tColor = COLOR_GREY;
	}
	return tColor;
}


bool IsPlayerIndex( int index )
{
	return ( index >= 1 && index <= engine->GetMaxClients() ) ? true : false;
}


int FrustumTransform( const VMatrix &worldToSurface, const Vector& point, Vector& screen )
{
	// UNDONE: Clean this up some, handle off-screen vertices
	float w;

	screen.x = worldToSurface[0][0] * point[0] + worldToSurface[0][1] * point[1] + worldToSurface[0][2] * point[2] + worldToSurface[0][3];
	screen.y = worldToSurface[1][0] * point[0] + worldToSurface[1][1] * point[1] + worldToSurface[1][2] * point[2] + worldToSurface[1][3];
	//	z		 = worldToSurface[2][0] * point[0] + worldToSurface[2][1] * point[1] + worldToSurface[2][2] * point[2] + worldToSurface[2][3];
	w		 = worldToSurface[3][0] * point[0] + worldToSurface[3][1] * point[1] + worldToSurface[3][2] * point[2] + worldToSurface[3][3];

	// Just so we have something valid here
	screen.z = 0.0f;

	bool behind;
	if( w < 0.001f )
	{
		behind = true;
		screen.x *= 100000;
		screen.y *= 100000;
	}
	else
	{
		behind = false;
		float invw = 1.0f / w;
		screen.x *= invw;
		screen.y *= invw;
	}

	return behind;
}


void CTargetID::Paint()
{
	wchar_t sIDString[512];
	sIDString[0] = 0;

	Color c;
	
	// Get our target's ent index
	int iEntIndex = GetIDTarget();
	// Didn't find one?
	if ( !iEntIndex )
	{
		// Check to see if we should clear our ID
		if ( m_flLastChangeTime && (engine->Time() > (m_flLastChangeTime + 0.5)) )
		{
			m_flLastChangeTime = 0;
			sIDString[0] = 0;
			m_iLastEntIndex = 0;
		}
		else
		{
			// Keep re-using the old one
			iEntIndex = m_iLastEntIndex;
		}
	}
	else
	{
		m_flLastChangeTime = engine->Time();
	}

	// Is this an entindex sent by the server?
	if ( iEntIndex )
	{
		C_BaseEntity *pPlayer = static_cast<C_BaseEntity*>( entitylist->GetClientEntity( iEntIndex ) );
		C_BaseEntity *pLocalPlayer = GetLocalPlayer();

		const char *printFormatString = NULL;
		wchar_t wszPlayerName[ MAX_PLAYER_NAME_LENGTH ];
		wchar_t wszHealthText[ 10 ];
		bool bShowHealth = false;
		bool bShowPlayerName = false;
		
		if ( IsPlayerIndex( iEntIndex ) )
		{
			player_info_t pinfo;
			engine->GetPlayerInfo( iEntIndex, &pinfo );

			c = GetColorForTargetTeam( pPlayer->GetEntProp<int>("m_iTeamNum") );

			bShowPlayerName = true;
			g_pVGuiLocalize->ConvertANSIToUnicode( pinfo.name,  wszPlayerName, sizeof(wszPlayerName) );
			
			if( ( pPlayer->GetEntProp<int>("m_iTeamNum") != 0 ) && ( pPlayer->GetEntProp<int>("m_iTeamNum") == pLocalPlayer->GetEntProp<int>("m_iTeamNum") ) )
			{
				printFormatString = "#Playerid_sameteam";
				bShowHealth = true;
			}
			else
			{
				printFormatString = "#Playerid_diffteam";
			}
		
			if ( bShowHealth )
			{
				_snwprintf( wszHealthText, ARRAYSIZE(wszHealthText) - 1, L"%.0f%%",  (float)pPlayer->GetEntProp<int>("m_iHealth") );
				wszHealthText[ ARRAYSIZE(wszHealthText)-1 ] = '\0';
			}
		}

		if ( printFormatString )
		{
			if ( bShowPlayerName && bShowHealth )
			{
				g_pVGuiLocalize->ConstructString( sIDString, sizeof(sIDString), g_pVGuiLocalize->Find(printFormatString), 2, wszPlayerName, wszHealthText );
			}
			else if ( bShowPlayerName )
			{
				g_pVGuiLocalize->ConstructString( sIDString, sizeof(sIDString), g_pVGuiLocalize->Find(printFormatString), 1, wszPlayerName );
			}
			else if ( bShowHealth )
			{
				g_pVGuiLocalize->ConstructString( sIDString, sizeof(sIDString), g_pVGuiLocalize->Find(printFormatString), 1, wszHealthText );
			}
			else
			{
				g_pVGuiLocalize->ConstructString( sIDString, sizeof(sIDString), g_pVGuiLocalize->Find(printFormatString), 0 );
			}
		}

		if ( sIDString[0] )
		{
			vgui::HScheme scheme = g_pVGuiSchemeManager->GetScheme( "ClientScheme" );
			vgui::HFont hFont = g_pVGuiSchemeManager->GetIScheme(scheme)->GetFont( "TargetID", true );

			int wide, tall;
			g_pVGuiSurface->GetTextSize( hFont, sIDString, wide, tall );
			g_pVGuiSurface->DrawSetTextFont( hFont );

			Vector pHudTarget = pPlayer->EyePosition();

			int iAttachment = pPlayer->LookupAttachment( "eyes" );
			if( iAttachment != 1 )
			{
				QAngle pNotUse;
				pPlayer->GetAttachment( iAttachment, pHudTarget, pNotUse );
			}

			Vector screen;
			screen.Init();
			FrustumTransform ( engine->WorldToScreenMatrix(), pHudTarget, screen );

			int xpos = 0.5f * ( 1.0f + screen[0] ) * m_VR->m_RenderWidth + 0.5f;
			int ypos = 0.5f * ( 1.0f - screen[1] ) * m_VR->m_RenderHeight + 0.5f;
			xpos -= wide / 2;
			ypos -= 50;

			g_pVGuiSurface->DrawSetTextPos( xpos, ypos );
			g_pVGuiSurface->DrawSetTextColor( c );
			g_pVGuiSurface->DrawPrintText( sIDString, static_cast<int>(wcslen(sIDString)) );
		}
	}
}