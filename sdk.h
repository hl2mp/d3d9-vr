#include "client_class.h"
#include "icliententity.h"
#include "../game/shared/shareddefs.h"
#include "dt_recv.h"
#include "worldsize.h"
#include "hooks.h"
#include "collisionutils.h"

struct sm_sendprop_info_t
{
	RecvProp *prop;					/**< Property instance. */
	size_t actual_offset;		/**< Actual computed offset. */
};

static typedescription_t *FindFieldByName( const char *fieldname, datamap_t *dmap )
{
	int c = dmap->dataNumFields;
	for ( int i = 0; i < c; i++ )
	{
		typedescription_t *td = &dmap->dataDesc[ i ];
		if ( td->fieldType == FIELD_VOID )
			continue;

		if ( td->fieldType == FIELD_EMBEDDED )
		{
			typedescription_t *ret = FindFieldByName( fieldname, td->td );
			if ( ret )
			{
				return ret;
			}
		}

		if ( !V_stricmp( td->fieldName, fieldname ) )
		{
			return td;
		}
	}

	if ( dmap->baseMap )
	{
		return FindFieldByName( fieldname, dmap->baseMap );
	}

	return NULL;
}

inline bool UTIL_FindInSendTable( RecvTable *pTable, const char *name, sm_sendprop_info_t *info, unsigned int offset )
{
	int props = pTable->GetNumProps();
	for( int i = 0; i < props; i++ )
	{
		RecvProp *prop = pTable->GetProp( i );

		// Skip InsideArray props (SendPropArray / SendPropArray2),
		// we'll find them later by their containing array.
		if( prop->IsInsideArray() ) {
			continue;
		}

		const char *pname = prop->GetName();
		RecvTable *pInnerTable = prop->GetDataTable();

		if( pname && strcmp( name, pname ) == 0 )
		{
			// get true offset of CUtlVector
			if( prop->GetOffset() == 0 && pInnerTable && pInnerTable->GetNumProps() )
			{
				RecvProp *pLengthProxy = pInnerTable->GetProp(0);
				const char *ipname = pLengthProxy->GetName();
				if( ipname && strcmp( ipname, "lengthproxy" ) == 0 && pLengthProxy->GetExtraData() )
				{
					info->prop = prop;
					info->actual_offset = offset + *reinterpret_cast<size_t *>( reinterpret_cast<intptr_t>( pLengthProxy->GetExtraData() ) + 16 );
					return true;
				}
				
			}
			
			info->prop = prop;
			info->actual_offset = offset + info->prop->GetOffset();
			return true;
		}
		
		if( pInnerTable && UTIL_FindInSendTable( pInnerTable, name, info, offset + prop->GetOffset() ) )
			return true;
	}

	return false;
}

inline C_BaseEntity *CreateEntityByName( const char *className ) 
{
	return (C_BaseEntity *)hkCreateEntityByName.fOriginal( className );
}

class C_BaseEntity : public IClientEntity
{
public:
	virtual datamap_t *GetDataDescMap( void ) = 0;
	virtual int YouForgotToImplementOrDeclareClientClass() = 0;
	virtual ClientClass* GetClientClass() = 0; 
	virtual datamap_t *GetPredDescMap( void ) = 0;
	
	virtual ~C_BaseEntity() = 0;
	/*
	C_BaseEntity *GetEntPropEnt( const char *str )
	{
		size_t offset;
		sm_sendprop_info_t info;

		ClientClass *cc = GetClientClass();

		bool found = UTIL_FindInSendTable( cc->m_pRecvTable, str, &info, 0);

		if( !found )
			return NULL;

		offset = info.actual_offset;

		CBaseHandle &hndl = *(CBaseHandle *)((uint8_t *)this + offset);
		if( !hndl.IsValid() )
			return NULL;

		int index = hndl.GetEntryIndex();
		return (C_BaseEntity *)entitylist->GetClientEntity( index );
	}

	bool SetEntPropEnt( const char *str, C_BaseEntity *entity = NULL  )
	{
		size_t offset;
		sm_sendprop_info_t info;

		ClientClass *cc = GetClientClass();

		bool found = UTIL_FindInSendTable( cc->m_pRecvTable, str, &info, 0);

		if( !found )
			return false;

		offset = info.actual_offset;

		CBaseHandle &hndl = *(CBaseHandle *)((uint8_t *)this + offset);
		if( entity != NULL ) {
			IHandleEntity *pHandleEnt = (IHandleEntity *)entity;
			hndl.Set(pHandleEnt);
		}
		else {
			hndl.Set(NULL);
		}

		return true;
	}
	*/
	int GetEntPropSend( const char *str, bool IsBool = false ) 
	{
		size_t offset;
		sm_sendprop_info_t info;

		ClientClass *cc = GetClientClass();

		bool found = UTIL_FindInSendTable( cc->m_pRecvTable, str, &info, 0);

		if( !found )
			return -1;

		offset = info.actual_offset;

		if(IsBool)
			return *(bool *)((uint8_t *)this + offset) ? 1 : 0;

		return *(int *)((uint8_t *)this + offset);
	}
	
	void SetEntPropSend( const char *str, int value ) 
	{
		size_t offset;
		sm_sendprop_info_t info;

		ClientClass *cc = GetClientClass();

		bool found = UTIL_FindInSendTable( cc->m_pRecvTable, str, &info, 0);

		if( !found )
			return;

		offset = info.actual_offset;

		*(int *)((uint8_t *)this + offset) = value;
	}

	
	template<typename T>
	bool SetEntProp(const char* str, const T& value, bool bPredDescMap = true)
	{
		datamap_t* descMap = bPredDescMap ? GetPredDescMap() : GetDataDescMap();
		typedescription_t* td = FindFieldByName(str, descMap);
    
		if (!td) 
		{
			Msg("SetEntProp %s not found\n",str);
			return false;
		}
		size_t offset = td->fieldOffset[TD_OFFSET_NORMAL];
		T* fieldPtr = reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(this) + offset);
    
		*fieldPtr = value;
    
		return true;
	}

	bool SetEntPropEnt(const char* str, C_BaseEntity *pEntity, bool bPredDescMap = true)
	{
		CBaseHandle hndl;
		if (pEntity != NULL)
		{
			IHandleEntity *pHandleEnt = (IHandleEntity *)pEntity;
			hndl.Set(pHandleEnt);
		}
    
		return SetEntProp(str, hndl, bPredDescMap);
	}

	template<typename T>
	T GetEntProp(const char* str, bool bPredDescMap = true)
	{
		datamap_t* descMap = bPredDescMap ? GetPredDescMap() : GetDataDescMap();
		typedescription_t* td = FindFieldByName(str, descMap);
    
		if (!td)
		{
			// Возвращаем значение по умолчанию
			return T(); // Для POD-типов: 0, false, nullptr и т.д.
		}

		size_t offset = td->fieldOffset[TD_OFFSET_NORMAL];
		const T* fieldPtr = reinterpret_cast<const T*>(reinterpret_cast<const uint8_t*>(this) + offset);
    
		return *fieldPtr;
	}

	C_BaseEntity* GetEntPropEnt(const char* str, bool bPredDescMap = true)
	{
		CBaseHandle &hEntity = GetEntProp<CBaseHandle>(str, bPredDescMap);
		if( !hEntity.IsValid() )
			return NULL;

		int index = hEntity.GetEntryIndex();
		return static_cast<C_BaseEntity*>(entitylist->GetClientEntity( index ));
	}

	Vector EyePosition()
	{
		return GetAbsOrigin() + GetEntProp<Vector>( "m_vecViewOffset" );
	}

	char IsAlive()
	{
		char m_lifeState = GetEntProp<char>( "m_lifeState" );
		return m_lifeState == LIFE_ALIVE;
	}

	void SetModelPointer( const model_t *pModel )
	{
		hkSetModelPointer.fOriginal( this, pModel );
	}

	void C_BaseEntity::SetModelIndex( int index_ )
	{
		short m_nModelIndex = index_;
		SetEntProp( "m_nModelIndex", m_nModelIndex );
		const model_t *pModel = modelinfo->GetModel( m_nModelIndex );
		SetModelPointer( pModel );
	}

	void FollowEntity( C_BaseEntity *pBaseEntity, bool bBoneMerge = true )
	{
		hkFollowEntity.fOriginal( this, pBaseEntity, bBoneMerge );
	}

	void SendViewModelMatchingSequence( void *ecx, int sequence )
	{
		hkSendViewModelMatchingSequence.fOriginal( this, sequence );
	}

	void GetBonePosition( int iBone, Vector &origin, QAngle &angles )
	{
		hkGetBonePosition.fOriginal( this, iBone, origin, angles );
	}

	void SetAbsOrigin( const Vector& origin )
	{
		hkSetAbsOrigin.fOriginal( this, origin );
	}
	
	C_BaseEntity *GetActiveWepon()
	{
		C_BaseEntity *m_hActiveWeapon = GetEntPropEnt( "m_hActiveWeapon" );
		return m_hActiveWeapon;
	}
};


class EHANDLE
{
private:
	C_BaseEntity* m_pEntity;
	int m_iSerialNumber;
	int m_index;
    
public:
	EHANDLE() : m_pEntity(NULL), m_iSerialNumber(0), m_index(-1) {}
    
	void operator=(C_BaseEntity* pEntity) {
		if (pEntity) {
			m_pEntity = pEntity;
			m_iSerialNumber = pEntity->GetRefEHandle().GetSerialNumber();
			m_index = pEntity->GetRefEHandle().GetEntryIndex();
		}
		else {
			Reset();
		}
	}
    
	bool IsValid() {
		if( m_index == -1 )
			return false;
        
		m_pEntity = (C_BaseEntity *)entitylist->GetClientEntity( m_index );
		if( !m_pEntity || ( m_pEntity->GetRefEHandle().GetSerialNumber() != m_iSerialNumber ) ) {
			Reset();
			return false;
		}
        
		return true;
	}
    
	C_BaseEntity* get()  {
		return IsValid() ? m_pEntity : NULL;
	}
    
	operator C_BaseEntity*()  { return get(); }

	bool operator!()  { return !get(); }
    
	C_BaseEntity* operator->()  {
		return get();
	}

	bool operator==( C_BaseEntity* pOther )  {
		return get() == pOther;
	}

	bool operator!=( C_BaseEntity* pOther )  {
		return get() != pOther;
	}
    
	void Reset() {
		m_pEntity = NULL;
		m_iSerialNumber = 0;
		m_index = -1;
	}
};


inline bool CGameTrace::DidHitWorld() const
{
	return m_pEnt == (C_BaseEntity *)entitylist->GetClientEntity( 0 );
}


inline bool CGameTrace::DidHitNonWorldEntity() const
{
	return m_pEnt != NULL && !DidHitWorld();
}


inline int CGameTrace::GetEntityIndex() const
{
	if ( m_pEnt )
		return m_pEnt->entindex();
	else
		return -1;
}


class CTraceFilterSkipLocalPlayer : public CTraceFilter
{
public:
	virtual bool ShouldHitEntity( IHandleEntity *pServerEntity, int contentsMask )
	{
		if( engine->GetLocalPlayer() == pServerEntity->GetRefEHandle().GetEntryIndex() )
			return false;

		return true;
	}
};


inline void UTIL_TraceLine( const Vector& vecAbsStart, const Vector& vecAbsEnd, unsigned int mask, trace_t *ptr )
{
	Ray_t ray;
	CTraceFilterSkipLocalPlayer tracefilter;
    ray.Init( vecAbsStart, vecAbsEnd );
	enginetrace->TraceRay( ray, mask, &tracefilter, ptr );
}


inline C_BaseEntity *GetLocalPlayer()
{
	int playerIndex = engine->GetLocalPlayer();
	return (C_BaseEntity *)entitylist->GetClientEntity( playerIndex );
}


inline float GetCurTime()
{
	C_BaseEntity *pPlayer = GetLocalPlayer();
	if( !pPlayer )
		return 0.0f;

	int m_nTickBase = pPlayer->GetEntProp<int>( "m_nTickBase");

	return m_nTickBase * 0.015;
}


inline C_BaseEntity *FindClientEntityByClassname( const char *str, bool pOwner = true )
{
	for( int i=0; i < entitylist->GetMaxEntities(); i++ )
	{
		C_BaseEntity *pEntity = (C_BaseEntity *)entitylist->GetClientEntity( i );
		if( !pEntity )
			continue;

		ClientClass *cc = pEntity->GetClientClass();
		if( !Q_stricmp( cc->GetName(), str ) )
		{
			C_BaseEntity *pPlayer = GetLocalPlayer();
			if( !pPlayer )
				continue;

			C_BaseEntity *m_hOwner = pEntity->GetEntPropEnt("m_hOwner");
			C_BaseEntity *m_hOwnerEntity = pEntity->GetEntPropEnt("m_hOwnerEntity");
    
			if( pOwner && m_hOwner != pPlayer && m_hOwnerEntity != pPlayer )
				continue;

			return pEntity;
		}
	}

	return NULL;
}


inline C_BaseEntity *GetViewModel()
{
	C_BaseEntity *pPlayer = GetLocalPlayer();
	if( !pPlayer || !pPlayer->IsAlive() || !pPlayer->GetActiveWepon() )
		return NULL;

	static EHANDLE pViewModel;

	if( pViewModel )
	{
		//pViewModel->SetEntPropEnt("m_hOwnerEntity", NULL);
		//pViewModel->SetEntPropEnt("m_hOwner", NULL);
		return pViewModel;
	}
	//pViewModel = FindClientEntityByClassname( "CPredictedViewModel" );
	pViewModel = pPlayer->GetEntPropEnt("m_hViewModel");

	return pViewModel;
}


inline int Studio_BoneIndexByName( const CStudioHdr *pStudioHdr, const char *pName )
{
	if ( pStudioHdr )
	{
		// binary search for the bone matching pName
		int start = 0, end = pStudioHdr->numbones()-1;
		const byte *pBoneTable = pStudioHdr->GetBoneTableSortedByName();
		mstudiobone_t *pbones = pStudioHdr->pBone( 0 );
		while (start <= end)
		{
			int mid = (start + end) >> 1;
			int cmp = Q_stricmp( pbones[pBoneTable[mid]].pszName(), pName );
		
			if ( cmp < 0 )
			{
				start = mid + 1;
			}
			else if ( cmp > 0 )
			{
				end = mid - 1;
			}
			else
			{
				return pBoneTable[mid];
			}
		}
	}

	return -1;
}


inline void UpdateLaserDot( Vector endPos )
{
	static EHANDLE pDot;

	if( !pDot )
		pDot = FindClientEntityByClassname( "CLaserDot" );

	if( pDot )
	{
		pDot->SetAbsOrigin( endPos );
		pDot->SetEntPropEnt("m_hOwnerEntity", NULL);
	}
}

#include "beamdraw.h"
#include "mathlib/mathlib.h"

inline void DrawHalo(IMaterial* pMaterial, QAngle vecAngles, const Vector& source, float scale )
{
	Vector		point, pVecForward, pVecRight, pVecUp;

	AngleVectors( vecAngles, &pVecForward, &pVecRight, &pVecUp );

	CMatRenderContextPtr pRenderContext( materials );
	IMesh* pMesh = pRenderContext->GetDynamicMesh(true, NULL, NULL, pMaterial);

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

	meshBuilder.TexCoord2f (0, 0, 1);
	VectorMA (source, -scale, pVecUp, point);
	VectorMA (point, -scale, pVecRight, point);
	meshBuilder.Position3fv (point.Base());
	meshBuilder.AdvanceVertex();

	meshBuilder.TexCoord2f (0, 0, 0);
	VectorMA (source, scale, pVecUp, point);
	VectorMA (point, -scale, pVecRight, point);
	meshBuilder.Position3fv (point.Base());
	meshBuilder.AdvanceVertex();

	meshBuilder.TexCoord2f (0, 1, 0);
	VectorMA (source, scale, pVecUp, point);
	VectorMA (point, scale, pVecRight, point);
	meshBuilder.Position3fv (point.Base());
	meshBuilder.AdvanceVertex();

	meshBuilder.TexCoord2f (0, 1, 1);
	VectorMA (source, -scale, pVecUp, point);
	VectorMA (point, scale, pVecRight, point);
	meshBuilder.Position3fv (point.Base());
	meshBuilder.AdvanceVertex();
	
	meshBuilder.End();
	pMesh->Draw();
}


inline void DrawBeamQuadraticNew( IMaterial *material, const Vector &start, const Vector &control, const Vector &end, float width, const Vector &color, float scrollOffset )
{
	int subdivisions = 16;

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	CBeamSegDraw beamDraw;
	beamDraw.Start( pRenderContext, subdivisions+1, material );

	BeamSeg_t seg;
	seg.m_flAlpha = 1.0;
	seg.m_flWidth = width;
	
	float t = 0;
	float u = fmod( scrollOffset, 1 );
	float dt = 1.0 / (float)subdivisions;
	for( int i = 0; i <= subdivisions; i++, t += dt )
	{
		float omt = (1-t);
		float p0 = omt*omt;
		float p1 = 2*t*omt;
		float p2 = t*t;

		seg.m_vPos = p0 * start + p1 * control + p2 * end;
		seg.m_flTexCoord = u - t;
		if ( i == 0 || i == subdivisions )
		{
			// HACK: fade out the ends a bit
			seg.m_vColor = vec3_origin;
		}
		else
		{
			seg.m_vColor = color;
		}
		beamDraw.NextSeg( &seg );
	}

	beamDraw.End();
}


class CUserCmd
{
public:
	CUserCmd()
	{
		Reset();
	}

	virtual ~CUserCmd() { };

	void Reset()
	{
		command_number = 0;
		tick_count = 0;
		viewangles.Init();
		forwardmove = 0.0f;
		sidemove = 0.0f;
		upmove = 0.0f;
		buttons = 0;
		impulse = 0;
		weaponselect = 0;
		weaponsubtype = 0;
		random_seed = 0;
#ifdef GAME_DLL
		server_random_seed = 0;
#endif
		mousedx = 0;
		mousedy = 0;

		hasbeenpredicted = false;
#if defined( HL2_DLL ) || defined( HL2_CLIENT_DLL )
		entitygroundcontact.RemoveAll();
#endif
	}

	CUserCmd& operator =( const CUserCmd& src )
	{
		if ( this == &src )
			return *this;

		command_number		= src.command_number;
		tick_count			= src.tick_count;
		viewangles			= src.viewangles;
		forwardmove			= src.forwardmove;
		sidemove			= src.sidemove;
		upmove				= src.upmove;
		buttons				= src.buttons;
		impulse				= src.impulse;
		weaponselect		= src.weaponselect;
		weaponsubtype		= src.weaponsubtype;
		random_seed			= src.random_seed;
#ifdef GAME_DLL
		server_random_seed = src.server_random_seed;
#endif
		mousedx				= src.mousedx;
		mousedy				= src.mousedy;

		hasbeenpredicted	= src.hasbeenpredicted;

#if defined( HL2_DLL ) || defined( HL2_CLIENT_DLL )
		entitygroundcontact			= src.entitygroundcontact;
#endif

		return *this;
	}

	CUserCmd( const CUserCmd& src )
	{
		*this = src;
	}

	CRC32_t GetChecksum( void ) const
	{
		CRC32_t crc;

		CRC32_Init( &crc );
		CRC32_ProcessBuffer( &crc, &command_number, sizeof( command_number ) );
		CRC32_ProcessBuffer( &crc, &tick_count, sizeof( tick_count ) );
		CRC32_ProcessBuffer( &crc, &viewangles, sizeof( viewangles ) );    
		CRC32_ProcessBuffer( &crc, &forwardmove, sizeof( forwardmove ) );   
		CRC32_ProcessBuffer( &crc, &sidemove, sizeof( sidemove ) );      
		CRC32_ProcessBuffer( &crc, &upmove, sizeof( upmove ) );         
		CRC32_ProcessBuffer( &crc, &buttons, sizeof( buttons ) );		
		CRC32_ProcessBuffer( &crc, &impulse, sizeof( impulse ) );        
		CRC32_ProcessBuffer( &crc, &weaponselect, sizeof( weaponselect ) );	
		CRC32_ProcessBuffer( &crc, &weaponsubtype, sizeof( weaponsubtype ) );
		CRC32_ProcessBuffer( &crc, &random_seed, sizeof( random_seed ) );
		CRC32_ProcessBuffer( &crc, &mousedx, sizeof( mousedx ) );
		CRC32_ProcessBuffer( &crc, &mousedy, sizeof( mousedy ) );
		CRC32_Final( &crc );

		return crc;
	}

	// Allow command, but negate gameplay-affecting values
	void MakeInert( void )
	{
		viewangles = vec3_angle;
		forwardmove = 0.f;
		sidemove = 0.f;
		upmove = 0.f;
		buttons = 0;
		impulse = 0;
	}

	// For matching server and client commands for debugging
	int		command_number;
	
	// the tick the client created this command
	int		tick_count;
	
	// Player instantaneous view angles.
	QAngle	viewangles;     
	// Intended velocities
	//	forward velocity.
	float	forwardmove;   
	//  sideways velocity.
	float	sidemove;      
	//  upward velocity.
	float	upmove;         
	// Attack button states
	int		buttons;		
	// Impulse command issued.
	byte    impulse;        
	// Current weapon id
	int		weaponselect;	
	int		weaponsubtype;

	int		random_seed;	// For shared random functions
#ifdef GAME_DLL
	int		server_random_seed; // Only the server populates this seed
#endif

	short	mousedx;		// mouse accum in x from create move
	short	mousedy;		// mouse accum in y from create move

	// Client only, tracks whether we've predicted this command at least once
	bool	hasbeenpredicted;

	// Back channel to communicate IK state
#if defined( HL2_DLL ) || defined( HL2_CLIENT_DLL )
	CUtlVector< CEntityGroundContact > entitygroundcontact;
#endif

};


inline Vector GetAimPointOnPanel(const Vector& pUpperLeft, const Vector& pUpperRight, 
                                   const Vector& pLowerLeft, const Vector& controllerPos, 
                                   const QAngle& controllerAng)
{
    // Создаем луч из позиции контроллера
    Vector forward;
    AngleVectors(controllerAng, &forward);
    
    // Определяем плоскость панели
    Vector edge1 = pUpperRight - pUpperLeft;
    Vector edge2 = pLowerLeft - pUpperLeft;
    Vector normal = CrossProduct(edge1, edge2);
    normal.NormalizeInPlace();
    
    // Создаем структуру плоскости
    cplane_t plane;
    plane.normal = normal;
    plane.dist = DotProduct(normal, pUpperLeft);
    plane.type = 3; // произвольная плоскость
    
    // Находим пересечение луча с плоскостью
    float t = IntersectRayWithPlane(controllerPos, forward, plane);
    
    if (t > 0)
    {
        Vector intersectionPoint = controllerPos + forward * t;
        
        // Проверяем, находится ли точка в пределах панели
        Vector toPoint = intersectionPoint - pUpperLeft;
        
        // Проецируем на оси панели
        float u = DotProduct(toPoint, edge1.Normalized()) / edge1.Length();
        float v = DotProduct(toPoint, edge2.Normalized()) / edge2.Length();
        
        if (u >= 0 && u <= 1 && v >= 0 && v <= 1)
        {
            return intersectionPoint;
        }
    }
    
    return vec3_origin;
}