#include "vr_sourcevr.h"

#include <vgui_controls/Frame.h>
#include <vgui_controls/Button.h>
#include "vgui/IScheme.h"
#include "vgui_controls/HTML.h"

 class CPanelKeyBoard : public vgui::Frame
 {
 	DECLARE_CLASS_SIMPLE(CPanelKeyBoard, vgui::Frame); 

 	CPanelKeyBoard(vgui::VPANEL parent); 	// Constructor
 	~CPanelKeyBoard(){};				// Destructor
 
 protected:
 	//VGUI overrides:
 	virtual void OnTick();
 	virtual void OnCommand(const char* pcCommand);

 private:
 	//Other used VGUI control Elements:
 
 };


void CreateKeyBoard()
{
	vgui::VPANEL pRoot = enginevgui->GetPanel( PANEL_ROOT );
	CPanelKeyBoard *MyPanel = new CPanelKeyBoard(pRoot);
}

 // Constuctor: Initializes the Panel
CPanelKeyBoard::CPanelKeyBoard(vgui::VPANEL parent)
: BaseClass(NULL, "PanelKeyBoard")
{
	SetParent( parent );

	SetKeyBoardInputEnabled( false );
	SetMouseInputEnabled( true );
	
	SetProportional( false );
	SetTitleBarVisible( false );
	SetMinimizeButtonVisible( false );
	SetMaximizeButtonVisible( false );
	SetCloseButtonVisible( false );
	SetSizeable( true );
	SetMoveable( true );
	SetVisible( true );
	SetSize( 520, 202 );

	SetScheme("ClientScheme");

	vgui::ivgui()->AddTickSignal( GetVPanel(), 100 );

	const char *str = "1234567890";
	for (int i = 0; str[i] != '\0'; i++) {
		char temp_str[2] = { str[i], '\0' };

		static int curpos = 1;

		vgui::Button *m_pOK = new vgui::Button(this, temp_str, temp_str );
		m_pOK->SetPos( curpos,1 );
		m_pOK->SetSize( 50, 50 );
		m_pOK->SetCommand( temp_str );
		curpos += 51;
	}

	str = "qwertyuiop";

	for (int i = 0; str[i] != '\0'; i++) {
		char temp_str[2] = { str[i], '\0' };

		static int curpos = 26;

		vgui::Button *m_pOK = new vgui::Button(this, "ok", temp_str );
		m_pOK->SetPos( curpos,51 );
		m_pOK->SetSize( 50, 50 );
		m_pOK->SetCommand( temp_str );
		curpos += 51;
	}

	str = "asdfghjkl";

	for (int i = 0; str[i] != '\0'; i++) {
		char temp_str[2] = { str[i], '\0' };

		static int curpos = 1;

		vgui::Button *m_pOK = new vgui::Button(this, "ok", temp_str );
		m_pOK->SetPos( curpos,101 );
		m_pOK->SetSize( 50, 50 );
		m_pOK->SetCommand( temp_str );
		curpos += 51;
	}

	str = "zxcvbnm";

	for (int i = 0; str[i] != '\0'; i++) {
		char temp_str[2] = { str[i], '\0' };

		static int curpos = 26;

		vgui::Button *m_pOK = new vgui::Button(this, "ok", temp_str );
		m_pOK->SetPos( curpos,151 );
		m_pOK->SetSize( 50, 50 );
		m_pOK->SetCommand( temp_str );
		curpos += 51;
	}
}

void CPanelKeyBoard::OnTick()
{
	if( g_pVGuiSurface->IsCursorVisible() )
		SetVisible( true );
	else
		SetVisible( false );

	BaseClass::OnTick();
}

void CPanelKeyBoard::OnCommand(const char* pcCommand)
{
	wchar_t wszLocalized;
	wszLocalized = pcCommand[0];
	g_InputInternal->InternalKeyTyped( wszLocalized );
	Msg("Call %s\n", pcCommand);
	BaseClass::OnCommand(pcCommand);
}