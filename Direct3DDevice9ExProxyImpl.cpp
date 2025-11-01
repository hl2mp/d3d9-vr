
#include "Direct3DDevice9ExProxyImpl.h"
#include "vr_system.h"

#include "Main.h"

#pragma comment(lib, "d3dx9.lib")

Direct3DDevice9ExProxyImpl::Direct3DDevice9ExProxyImpl(IDirect3DDevice9Ex* pDevice, Direct3D9ExWrapper* pCreatedBy):
	Direct3DDevice9ExWrapper(pDevice, pCreatedBy)
{
}

Direct3DDevice9ExProxyImpl::~Direct3DDevice9ExProxyImpl()
{
}

HRESULT WINAPI Direct3DDevice9ExProxyImpl::Present(CONST RECT* pSourceRect,CONST RECT* pDestRect,HWND hDestWindowOverride,CONST RGNDATA* pDirtyRegion)
{
	HRESULT hr = S_OK;

	hr =  Direct3DDevice9ExWrapper::PresentEx( pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion, 0 );

	GetVRSystem()->PostPresent( this );	

	return hr;
}

HRESULT WINAPI Direct3DDevice9ExProxyImpl::PresentEx(CONST RECT* pSourceRect,CONST RECT* pDestRect,HWND hDestWindowOverride,CONST RGNDATA* pDirtyRegion, DWORD flags)
{
	HRESULT hr = S_OK;
	
	hr = Direct3DDevice9ExWrapper::PresentEx( pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion, flags );
	
	GetVRSystem()->PostPresent( this );
	
	return hr;
}

HRESULT WINAPI Direct3DDevice9ExProxyImpl::CreateTexture(UINT Width,UINT Height,UINT Levels,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DTexture9** ppTexture,HANDLE* pSharedHandle)
{
	HANDLE shared_handle = NULL;

	int index = GetVRSystem()->GetNextCreateTextureIs();
	if( index != -1 )
	{
		char buffer[100];
		sprintf_s(buffer, "CreateTexture: %d %d\n", Width, Height);
		OutputDebugStringA(buffer);

		pSharedHandle = &shared_handle;
	}

	HRESULT creationResult = Direct3DDevice9ExWrapper::CreateTexture(Width, Height, Levels, Usage, Format, Pool, ppTexture, pSharedHandle);

	if( index != -1 )
	{
		GetVRSystem()->StoreSharedTexture(index, *ppTexture, pSharedHandle);
	}

	return creationResult;
}