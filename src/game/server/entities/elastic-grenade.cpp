/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/math.h>
#include <base/vmath.h>
#include <game/generated/protocol.h>
#include <game/server/gamecontext.h>
#include <game/server/entities/growingexplosion.h>

#include "elastic-grenade.h"

CElasticGrenade::CElasticGrenade(CGameWorld *pGameWorld, int Owner, int Weapon, vec2 Pos, vec2 Dir)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_ELASTIC_GRENADE)
{
	m_Pos = Pos;
	m_ActualPos = Pos;
	m_ActualDir = Dir;
	m_Direction = Dir;
	m_Owner = Owner;
	m_Weapon = Weapon;
	m_CollisionNum = 0;
	m_LifeSpan =  g_Config.m_InfElasticGrenadeLifeSpan * Server()->TickSpeed();
	m_StartTick = Server()->Tick();

	GameWorld()->InsertEntity(this);
}

void CElasticGrenade::Reset()
{
	GameServer()->m_World.DestroyEntity(this);
}

vec2 CElasticGrenade::GetPos(float Time)
{
	float Curvature = GameServer()->Tuning()->m_GrenadeCurvature;
	float Speed = GameServer()->Tuning()->m_GrenadeSpeed;

	return CalcPos(m_Pos, m_Direction, Curvature, Speed, Time);
}

void CElasticGrenade::TickPaused()
{
	m_StartTick++;
}

void CElasticGrenade::Tick()
{
	if(!GameServer()->GetPlayerChar(m_Owner) || GameServer()->GetPlayerChar(m_Owner)->IsZombie())
	{
		GameServer()->m_World.DestroyEntity(this);
	}
	float Pt = (Server()->Tick()-m_StartTick-1)/(float)Server()->TickSpeed();
	float Ct = (Server()->Tick()-m_StartTick)/(float)Server()->TickSpeed();
	vec2 PrevPos = GetPos(Pt);
	vec2 CurPos = GetPos(Ct);
	
	m_ActualPos = CurPos;
	m_ActualDir = normalize(CurPos - PrevPos);
	
	m_LifeSpan--;

	for(CCharacter *pChr = (CCharacter*) GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER); pChr; pChr = (CCharacter *)pChr->TypeNext())
	{
		if(pChr->IsHuman()) continue;
		float Len = distance(pChr->m_Pos, m_ActualPos);
		if(Len < pChr->m_ProximityRadius)
		{
			Explode();
		}
	}

	
	if(GameLayerClipped(CurPos))
	{
		GameServer()->m_World.DestroyEntity(this);
		return;
	}
	
	vec2 LastPos;
	int Collide = GameServer()->Collision()->IntersectLine(PrevPos, CurPos, NULL, &LastPos);
	if(Collide)
	{
		m_CollisionNum++;
		//Thanks to TeeBall 0.6
		vec2 CollisionPos;
		CollisionPos.x = LastPos.x;
		CollisionPos.y = CurPos.y;
		int CollideY = GameServer()->Collision()->IntersectLine(PrevPos, CollisionPos, NULL, NULL);
		CollisionPos.x = CurPos.x;
		CollisionPos.y = LastPos.y;
		int CollideX = GameServer()->Collision()->IntersectLine(PrevPos, CollisionPos, NULL, NULL);
		
		m_Pos = LastPos;
		m_ActualPos = m_Pos;
		vec2 vel;
		vel.x = m_Direction.x;
		vel.y = m_Direction.y + 2*GameServer()->Tuning()->m_GrenadeCurvature/10000*Ct*GameServer()->Tuning()->m_GrenadeSpeed;
		
		if (CollideX && !CollideY)
		{
			m_Direction.x = -vel.x;
			m_Direction.y = vel.y;
		}
		else if (!CollideX && CollideY)
		{
			m_Direction.x = vel.x;
			m_Direction.y = -vel.y;
		}
		else
		{
			m_Direction.x = -vel.x;
			m_Direction.y = -vel.y;
		}
		
		m_Direction.x *= (100 - 50) / 100.0;
		m_Direction.y *= (100 - 50) / 100.0;
		m_StartTick = Server()->Tick();
		
		m_ActualDir = normalize(m_Direction);
	}

	if(m_LifeSpan <= 0 || m_CollisionNum >= g_Config.m_InfElasticGrenadeCollisionNum)
	{
		Explode();
	}
	
}

void CElasticGrenade::FillInfo(CNetObj_Projectile *pProj)
{
	pProj->m_X = (int)m_Pos.x;
	pProj->m_Y = (int)m_Pos.y;
	pProj->m_VelX = (int)(m_Direction.x*100.0f);
	pProj->m_VelY = (int)(m_Direction.y*100.0f);
	pProj->m_StartTick = m_StartTick;
	pProj->m_Type = WEAPON_GRENADE;
}

void CElasticGrenade::Snap(int SnappingClient)
{
	float Ct = (Server()->Tick()-m_StartTick)/(float)Server()->TickSpeed();
	
	if(NetworkClipped(SnappingClient, GetPos(Ct)))
		return;
	
	CNetObj_Projectile *pProj = static_cast<CNetObj_Projectile *>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, m_ID, sizeof(CNetObj_Projectile)));
	if(pProj)
		FillInfo(pProj);
}
	
void CElasticGrenade::Explode()
{
	GameServer()->CreateExplosion(m_ActualPos, m_Owner, m_Weapon, false, TAKEDAMAGEMODE_NOINFECTION);
	GameServer()->CreateSound(m_ActualPos, SOUND_GRENADE_EXPLODE);
	
	GameServer()->m_World.DestroyEntity(this);
	
}
