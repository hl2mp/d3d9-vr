#include "vr_sourcevr.h"
#include "sdk.h"
#include <vgui_controls/Frame.h>
#include <vgui_controls/Panel.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/html.h>

using namespace vgui;

class CLoadingDialog : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE(CLoadingDialog, vgui::Frame); 

	CLoadingDialog(vgui::VPANEL parent, const char* msg); 	// Constructor
	~CLoadingDialog(){};				// Destructor

public:
	virtual void Paint( void );
	virtual void OnCommand(const char* pcCommand);

	vgui::Label			*m_pInfoLabel;
	vgui::Button		*m_pCancelButton;
	vgui::Panel			*m_pLoadingBackground;
};

void CreateLoadingDialog()
{
	vgui::VPANEL pRoot = enginevgui->GetPanel(PANEL_ROOT);
	vgui::VPANEL loadingDialog = VFindChildByName(pRoot, "LoadingDialog");

	if(loadingDialog && !engine->IsInGame())
	{
		vgui::VPANEL vInfoPanel = VFindChildByName(loadingDialog, "InfoLabel");

		if( vInfoPanel )
		{
			vgui::Panel* pInfoPanel = g_pVGuiPanel->GetPanel(vInfoPanel, "GameUI");
			if(pInfoPanel)
			{
				vgui::Label* m_pInfoLabel = dynamic_cast<vgui::Label*>(pInfoPanel);
				if(m_pInfoLabel)
				{
					char textBuffer[1024];
					m_pInfoLabel->GetText(textBuffer, sizeof(textBuffer));
					new CLoadingDialog(pRoot, textBuffer);
				}
			}
		}
		g_pVGuiPanel->DeletePanel( loadingDialog );
	}

	//CLoadingDialog *MyPanel = new CLoadingDialog(pRoot, "TEST");
}

CLoadingDialog::CLoadingDialog(VPANEL parent, const char* msg)
: BaseClass(NULL, "LoadingDialog2")
{
	SetSize( m_VR->m_Width, m_VR->m_Height );
	SetPos( 0, 0 );
	
	SetParent( parent );

	SetMinimizeButtonVisible( false );
	SetMaximizeButtonVisible( false );
	SetCloseButtonVisible( false );
	SetSizeable( false );
	SetMoveable( false );
	SetTitleBarVisible( false );

	SetVisible( true );

	SetScheme("ClientScheme");

	CUtlString formatted;
        
    for (const char* p = msg; *p != '\0'; p++) {
        formatted += *p;
        if (*p == '.' && *(p + 1) == ' ') {
            formatted += "\n";
        }
    }

	m_pInfoLabel = new Label( this, "InfoLabel", formatted.String() );
	m_pInfoLabel->SetSize( m_VR->m_Width, m_VR->m_Height );
	m_pInfoLabel->SetCenterWrap( true );

	m_pLoadingBackground = new Panel( this, "LoadingDialogBG2" );
	m_pLoadingBackground->SetSize( 130, 40 );
	m_pLoadingBackground->SetPos( (m_VR->m_Width - 100)/2, m_VR->m_Height/2 + 50 );
	
	m_pCancelButton = new Button( this, "CancelButton", "#GameUI_Close" );
	m_pCancelButton->SetSize( 130, 40 );
	m_pCancelButton->SetPos( (m_VR->m_Width - 100)/2, m_VR->m_Height/2 + 50 );
	m_pCancelButton->SetCenterWrap( true );
	m_pCancelButton->SetCommand( "DeletePanel" );
}


void CLoadingDialog::Paint()
{
	vgui::ipanel()->MoveToFront( GetVPanel() );
}

void CLoadingDialog::OnCommand(const char *command)
{
	if ( !stricmp(command, "DeletePanel") )
	{
		DeletePanel();
	}
	else
	{
		BaseClass::OnCommand(command);
	}
}