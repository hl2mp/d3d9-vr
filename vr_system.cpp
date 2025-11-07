#include "vr_system.h"
#include "vr_sourcevr.h"

#pragma comment (lib, "d3d11.lib")

BaseVRSystem* GetVRSystem()
{
    static BaseVRSystem instance;
    return &instance;
}


BaseVRSystem::BaseVRSystem() : 
    m_nextRTIs( -1 ),
    m_pID3D11Device( NULL )
{
	memset(m_SharedTextureHolder, 0, sizeof(m_SharedTextureHolder));

	HRESULT errorCode = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0,
			NULL, NULL, D3D11_SDK_VERSION, &m_pID3D11Device, NULL, NULL);
}


void BaseVRSystem::NextCreateTextureIs( int index )
{
	m_nextRTIs = index;
}


void MsgD3D9(const char* fmt, ...)
{
	char Buffer[1024];

	va_list list;
	va_start(list, fmt);
	vsnprintf(Buffer, sizeof(Buffer), fmt, list);
	va_end(list);

	OutputDebugStringA(Buffer);
}


void BaseVRSystem::PostPresent( IDirect3DDevice9Ex *pD3D9 )
{
	if( !m_VR->m_bActive )
		return;

	//Wait for the work to finish
	IDirect3DQuery9* pEventQuery = nullptr;
	pD3D9->CreateQuery( D3DQUERYTYPE_EVENT, &pEventQuery );
	if( pEventQuery != nullptr )
	{
		pEventQuery->Issue( D3DISSUE_END );

		while( pEventQuery->GetData( nullptr, 0, D3DGETDATA_FLUSH ) != S_OK );
			pEventQuery->Release();
	}

	m_VR->PostPresent();
}


void BaseVRSystem::StoreSharedTexture( int index, IDirect3DTexture9* pIDirect3DTexture9, HANDLE* pShared )
{
	if( *pShared == NULL )
		return;

	m_SharedTextureHolder[index].Release();
	
	ID3D11Resource* pID3D11Resource = NULL;
	if( SUCCEEDED( m_pID3D11Device->OpenSharedResource( *pShared, __uuidof( ID3D11Resource ), ( void** )&pID3D11Resource ) ) )
	{
		ID3D11Texture2D *pID3D11Texture2D = NULL;
		if( SUCCEEDED( pID3D11Resource->QueryInterface( __uuidof( ID3D11Texture2D ), ( void** )&pID3D11Texture2D ) ) )
		{
			m_SharedTextureHolder[index].m_d3d11Texture = pID3D11Texture2D;

			m_SharedTextureHolder[index].m_d3d9Texture = pIDirect3DTexture9;
			pIDirect3DTexture9->AddRef();

			vr::Texture_t vrTexture = { pID3D11Texture2D, vr::TextureType_DirectX, vr::ColorSpace_Auto };
			m_SharedTextureHolder[index].m_VRTexture = vrTexture;
		}

		pID3D11Resource->Release();
	}

	m_nextRTIs = -1;
}