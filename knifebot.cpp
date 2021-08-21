#include "includes.h"

void Aimbot::knife( ) {
	//removed, i wont gonna leak private stuff
}

bool Aimbot::CanKnife( LagRecord* record, ang_t angle, bool& stab ) {
	// convert target angle to direction.
	vec3_t forward;
	math::AngleVectors( angle, &forward );

	// see if we can hit the player with full range
	// this means no stab.
	CGameTrace trace;
	KnifeTrace( forward, false, &trace );

	// we hit smthing else than we were looking for.
	if( !trace.m_entity || trace.m_entity != record->m_player )
		return false;

	bool armor = record->m_player->m_ArmorValue( ) > 0;
	bool first = g_cl.m_weapon->m_flNextPrimaryAttack( ) + 0.4f < g_csgo.m_globals->m_curtime;
	bool back  = KnifeIsBehind( record );

	int stab_dmg  = m_knife_dmg.stab[ armor ][ back ];
	int slash_dmg = m_knife_dmg.swing[ first ][ armor ][ back ];
	int swing_dmg = m_knife_dmg.swing[ false ][ armor ][ back ];

	// smart knifebot.
	int health = record->m_player->m_iHealth( );
	if( health <= slash_dmg )
		stab = false;

	else if( health <= stab_dmg )
		stab = true;

	else if( health > ( slash_dmg + swing_dmg + stab_dmg ) )
		stab = true;

	else
		stab = false;

	// damage wise a stab would be sufficient here.
	if( stab && !KnifeTrace( forward, true, &trace ) )
		return false;

	return true;
}

bool Aimbot::KnifeTrace( vec3_t dir, bool stab, CGameTrace* trace ) {
	float range = stab ? 32.f : 48.f;

	vec3_t start = g_cl.m_shoot_pos;
	vec3_t end   = start + ( dir * range );

	CTraceFilterSimple filter;
	filter.SetPassEntity( g_cl.m_local );
	g_csgo.m_engine_trace->TraceRay( Ray( start, end ), MASK_SOLID, &filter, trace );

	// if the above failed try a hull trace.
	if( trace->m_fraction >= 1.f ) {
		g_csgo.m_engine_trace->TraceRay( Ray( start, end, { -16.f, -16.f, -18.f }, { 16.f, 16.f, 18.f } ), MASK_SOLID, &filter, trace );
		return trace->m_fraction < 1.f;
	}

	return true;
}

bool Aimbot::KnifeIsBehind( LagRecord* record ) {
	vec3_t delta{ record->m_origin - g_cl.m_shoot_pos };
	delta.z = 0.f;
	delta.normalize( );

	vec3_t target;
	math::AngleVectors( record->m_abs_ang, &target );
	target.z = 0.f;

	return delta.dot( target ) > 0.475f;
}