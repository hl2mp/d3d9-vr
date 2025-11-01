#pragma once
#include "openvr.h"
#include "d3d9.h"
#include "d3d11.h"

enum TextureID
{
	Texture_None = -1,
	Texture_LeftEye,
	Texture_RightEye,
	Texture_HUD,
	Texture_Blank
};

class ITexture;

struct SharedTextureHolder
{
	SharedTextureHolder() :
		m_d3d11Texture(NULL),
		m_d3d9Texture(NULL) {}

	void Release() {
		if (m_d3d11Texture != NULL)
		{
			m_d3d11Texture->Release();
			m_d3d11Texture = NULL;
			m_d3d9Texture->Release();
			m_d3d9Texture = NULL;
		}
	}

	ID3D11Texture2D*        m_d3d11Texture;
	IDirect3DTexture9*      m_d3d9Texture;
	vr::Texture_t			m_VRTexture;
	ITexture* pTexture;
};


class BaseVRSystem
{
public:
	BaseVRSystem();
	~BaseVRSystem() {};

	void NextCreateTextureIs( int index );
	int	GetNextCreateTextureIs() { return m_nextRTIs; };

	void PostPresent( IDirect3DDevice9Ex *pD3D9 );
	void StoreSharedTexture(int index, IDirect3DTexture9* pIDirect3DTexture9, HANDLE* pShared);

	SharedTextureHolder m_SharedTextureHolder[5];

	int m_nextRTIs;

	ID3D11Device* m_pID3D11Device;
};

BaseVRSystem* GetVRSystem();