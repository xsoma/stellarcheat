#include "includes.h"

Chams g_chams{ };;

Chams::model_type_t Chams::GetModelType( const ModelRenderInfo_t& info ) {
	// model name.
	//const char* mdl = info.m_model->m_name;

	std::string mdl{ info.m_model->m_name };

	//static auto int_from_chars = [ mdl ]( size_t index ) {
	//	return *( int* )( mdl + index );
	//};

	// little endian.
	//if( int_from_chars( 7 ) == 'paew' ) { // weap
	//	if( int_from_chars( 15 ) == 'om_v' && int_from_chars( 19 ) == 'sled' )
	//		return model_type_t::arms;
	//
	//	if( mdl[ 15 ] == 'v' )
	//		return model_type_t::view_weapon;
	//}

	//else if( int_from_chars( 7 ) == 'yalp' ) // play
	//	return model_type_t::player;

	if ( mdl.find( XOR( "player" ) ) != std::string::npos && info.m_index >= 1 && info.m_index <= 64 )
		return model_type_t::player;

	return model_type_t::invalid;
}

bool Chams::IsInViewPlane( const vec3_t& world ) {
	float w;

	const VMatrix& matrix = g_csgo.m_engine->WorldToScreenMatrix( );

	w = matrix[ 3 ][ 0 ] * world.x + matrix[ 3 ][ 1 ] * world.y + matrix[ 3 ][ 2 ] * world.z + matrix[ 3 ][ 3 ];

	return w > 0.001f;
}

void Chams::SetColor( Color col, IMaterial* mat ) {
	if ( mat )
		mat->ColorModulate( col );

	else
		g_csgo.m_render_view->SetColorModulation( col );
}

void Chams::SetAlpha( float alpha, IMaterial* mat ) {
	if ( mat )
		mat->AlphaModulate( alpha );

	else
		g_csgo.m_render_view->SetBlend( alpha );
}

void Chams::SetupMaterial( IMaterial* mat, Color col, bool z_flag, bool wireframe ) {
	SetColor( col );

	// mat->SetFlag( MATERIAL_VAR_HALFLAMBERT, flags );
	mat->SetFlag( MATERIAL_VAR_ZNEARER, z_flag );
	mat->SetFlag( MATERIAL_VAR_NOFOG, z_flag );
	mat->SetFlag( MATERIAL_VAR_IGNOREZ, z_flag );
	mat->SetFlag( MATERIAL_VAR_WIREFRAME, wireframe );

	g_csgo.m_studio_render->ForcedMaterialOverride( mat );
}

void Chams::init( ) {
	// create custom materials
	std::ofstream( "csgo\\materials\\simple_ignorez_reflective.vmt" ) << R"#("VertexLitGeneric"{
           "$basetexture" "vgui/white_additive"
            "$ignorez"      "1"
            "$envmap"       "env_cubemap"
            "$normalmapalphaenvmapmask"  "1"
            "$envmapcontrast"             "1"
            "$nofog"        "1"
            "$model"        "1"
            "$nocull"       "0"
            "$selfillum"    "1"
            "$halflambert"  "1"
            "$znearer"      "0"
            "$flat"         "1"
    })#";

	std::ofstream( "csgo\\materials\\simple_regular_reflective.vmt" ) << R"#("VertexLitGeneric" {
            "$basetexture" "vgui/white_additive"
            "$ignorez"      "0"
            "$envmap"       "env_cubemap"
            "$normalmapalphaenvmapmask"  "1"
            "$envmapcontrast"             "1"
            "$nofog"        "1"
            "$model"        "1"
            "$nocull"       "0"
            "$selfillum"    "1"
            "$halflambert"  "1"
            "$znearer"      "0"
            "$flat"         "1"
    })#";

	std::ofstream( "csgo/materials/ghost.vmt" ) << R"#("VertexLitGeneric" {
		    "$basetexture" "models/effects/cube_white"
		    "$additive"                    "1"
		    "$envmap"                    "models/effects/cube_white"
		    "$envmaptint"                "[1.0 1.0. 1.0]"
		    "$envmapfresnel"            "1.0"
		    "$envmapfresnelminmaxexp"    "[0.0 1.0 2.0]"
		    "$alpha"                    "0.99"
	})#";


	// find stupid materials.
	debugambientcube = g_csgo.m_material_system->FindMaterial( XOR( "debug/debugambientcube" ), XOR( "Model textures" ) );
	debugambientcube->IncrementReferenceCount( );

	debugdrawflat = g_csgo.m_material_system->FindMaterial( XOR( "debug/debugdrawflat" ), XOR( "Model textures" ) );
	debugdrawflat->IncrementReferenceCount( );

	glow_armsrace = g_csgo.m_material_system->FindMaterial( XOR( "dev/glow_armsrace" ), XOR( "Model textures" ) );
	glow_armsrace->IncrementReferenceCount( );

	materialMetall = g_csgo.m_material_system->FindMaterial( "simple_ignorez_reflective", "Model textures" );
	materialMetall->IncrementReferenceCount( );

	materialMetallnZ = g_csgo.m_material_system->FindMaterial( "simple_regular_reflective", "Model textures" );
	materialMetallnZ->IncrementReferenceCount( );

	ghost = g_csgo.m_material_system->FindMaterial( XOR( "ghost" ), XOR( "Model textures" ) );
	ghost->IncrementReferenceCount( );
}

bool Chams::OverridePlayer( int index ) {
	Player* player = g_csgo.m_entlist->GetClientEntity< Player* >( index );
	if ( !player )
		return false;

	// always skip the local player in DrawModelExecute.
	// this is because if we want to make the local player have less alpha
	// the static props are drawn after the players and it looks like aids.
	// therefore always process the local player in scene end.
	if ( player->m_bIsLocalPlayer( ) )
		return true;

	// see if this player is an enemy to us.
	bool enemy = g_cl.m_local && player->enemy( g_cl.m_local );

	// we have chams on enemies.
	if ( enemy && g_menu.main.players.chams_enemy.get( 0 ) )
		return true;

	// we have chams on friendly.
	else if ( !enemy && g_menu.main.players.chams_friendly.get( 0 ) )
		return true;

	return false;
}

bool Chams::GenerateLerpedMatrix( int index, BoneArray* out ) {
	LagRecord* current_record;
	AimPlayer* data;

	Player* ent = g_csgo.m_entlist->GetClientEntity< Player* >( index );
	if ( !ent )
		return false;

	if ( !g_aimbot.IsValidTarget( ent ) )
		return false;

	data = &g_aimbot.m_players[ index - 1 ];
	if ( !data || data->m_records.empty( ) )
		return false;

	if ( data->m_records.size( ) < 2 )
		return false;

	auto* channel_info = g_csgo.m_engine->GetNetChannelInfo( );
	if ( !channel_info )
		return false;

	static float max_unlag = 0.2f;
	static auto sv_maxunlag = g_csgo.m_cvar->FindVar( HASH( "sv_maxunlag" ) );
	if ( sv_maxunlag ) {
		max_unlag = sv_maxunlag->GetFloat( );
	}

	for ( auto it = data->m_records.rbegin( ); it != data->m_records.rend( ); it++ ) {
		current_record = it->get( );

		bool end = it + 1 == data->m_records.rend( );

		if ( current_record && current_record->valid( ) && ( !end && ( ( it + 1 )->get( ) ) ) ) {
			if ( current_record->m_origin.dist_to( ent->GetAbsOrigin( ) ) < 1.f ) {
				return false;
			}

			vec3_t next = end ? ent->GetAbsOrigin( ) : ( it + 1 )->get( )->m_origin;
			float  time_next = end ? ent->m_flSimulationTime( ) : ( it + 1 )->get( )->m_sim_time;

			float total_latency = channel_info->GetAvgLatency( 0 ) + channel_info->GetAvgLatency( 1 );
			std::clamp( total_latency, 0.f, max_unlag );

			float correct = total_latency + g_cl.m_lerp;
			float time_delta = time_next - current_record->m_sim_time;
			float add = end ? 1.f : time_delta;
			float deadtime = current_record->m_sim_time + correct + add;

			float curtime = g_csgo.m_globals->m_curtime;
			float delta = deadtime - curtime;

			float mul = 1.f / add;
			vec3_t lerp = math::Interpolate( next, current_record->m_origin, std::clamp( delta * mul, 0.f, 1.f ) );

			matrix3x4_t ret[ 128 ];

			std::memcpy( ret,
				current_record->m_bones,
				sizeof( ret ) );

			for ( size_t i{ }; i < 128; ++i ) {
				vec3_t matrix_delta = current_record->m_bones[ i ].GetOrigin( ) - current_record->m_origin;
				ret[ i ].SetOrigin( matrix_delta + lerp );
			}

			std::memcpy( out,
				ret,
				sizeof( ret ) );

			return true;
		}
	}

	return false;
}

void Chams::RenderHistoryChams( int index ) {
	AimPlayer* data;
	LagRecord* record;

	Player* player = g_csgo.m_entlist->GetClientEntity< Player* >( index );
	if ( !player )
		return;

	if ( !g_aimbot.IsValidTarget( player ) )
		return;

	bool enemy = g_cl.m_local && player->enemy( g_cl.m_local );
	if ( enemy && g_menu.main.players.chams_enemy_history_enable.get( ) ) {
		data = &g_aimbot.m_players[ index - 1 ];
		if ( !data || data->m_records.empty( ) )
			return;

		//record = g_resolver.FindLastRecord(data);
		//if (!record)
		//	return;

		// get color.
		Color color = g_menu.main.players.chams_enemy_history_col.get( );

		// override blend.
		SetAlpha( g_menu.main.players.chams_enemy_history_blend.get( ) / 100.f );

		// fix color picker for custom mat.
		g_chams.glow_armsrace->FindVar( "$envmaptint", nullptr )->SetVecValue( color.r( ) / 255.f, color.g( ) / 255.f, color.b( ) / 255.f );
		g_chams.ghost->FindVar( "$envmaptint", nullptr )->SetVecValue( color.r( ) / 255.f, color.g( ) / 255.f, color.b( ) / 255.f );

		// set material.
		switch ( g_menu.main.players.chams_enemy_history_mat.get( ) ) {
		case 0:
			SetupMaterial( debugambientcube, color, true, g_menu.main.players.chams_enemy_history_wireframe.get( ) );
			break;
		case 1:
			SetupMaterial( debugdrawflat, color, true, g_menu.main.players.chams_enemy_history_wireframe.get( ) );
			break;
		case 2:
			SetupMaterial( materialMetallnZ, color, true, g_menu.main.players.chams_enemy_history_wireframe.get( ) );
			break;
		case 3:
			SetupMaterial( glow_armsrace, color, true, g_menu.main.players.chams_enemy_history_wireframe.get( ) );
			break;
		case 4:
			SetupMaterial( ghost, color, true, g_menu.main.players.chams_enemy_history_wireframe.get( ) );
			break;
		}

		// was the matrix properly setup?
		BoneArray arr[ 128 ];
		if ( Chams::GenerateLerpedMatrix( index, arr ) ) {
			// backup the bone cache before we fuck with it.
			auto backup_bones = player->m_BoneCache( ).m_pCachedBones;

			// replace their bone cache with our custom one.
			player->m_BoneCache( ).m_pCachedBones = arr;

			// manually draw the model.
			player->DrawModel( );

			// reset their bone cache to the previous one.
			player->m_BoneCache( ).m_pCachedBones = backup_bones;
		}
	}
}

bool Chams::DrawModel( uintptr_t ctx, const DrawModelState_t& state, const ModelRenderInfo_t& info, matrix3x4_t* bone ) {
	// store and validate model type.
	model_type_t type = GetModelType( info );
	if ( type == model_type_t::invalid )
		return true;

	// is a valid player.
	if ( type == model_type_t::player ) {
		// do not cancel out our own calls from SceneEnd
		// also do not cancel out calls from the glow.
		if ( !m_running && !g_csgo.m_studio_render->m_pForcedMaterial && OverridePlayer( info.m_index ) )
			return false;
	}

	return true;
}

void Chams::SceneEnd( ) {
	// store and sort ents by distance.
	if ( SortPlayers( ) ) {
		// iterate each player and render them.
		for ( const auto& p : m_players )
			RenderPlayer( p );
	}

	// restore.
	g_csgo.m_studio_render->ForcedMaterialOverride( nullptr );
	g_csgo.m_render_view->SetColorModulation( colors::white );
	g_csgo.m_render_view->SetBlend( 1.f );
}

void Chams::RenderPlayer( Player* player ) {
	// prevent recruisive model cancelation.
	m_running = true;

	// restore.
	g_csgo.m_studio_render->ForcedMaterialOverride( nullptr );
	g_csgo.m_render_view->SetColorModulation( colors::white );
	g_csgo.m_render_view->SetBlend( 1.f );

	// this is the local player.
	// we always draw the local player manually in drawmodel.
	if ( player->m_bIsLocalPlayer( ) ) {
		if ( g_menu.main.players.chams_local_scope.get( ) && player->m_bIsScoped( ) )
			SetAlpha( 0.5f );

		else if ( g_menu.main.players.chams_local_enable.get( ) ) {
			// get color.
			Color color = g_menu.main.players.chams_local_col.get( );

			// override blend.
			SetAlpha( g_menu.main.players.chams_local_blend.get( ) / 100.f );

			// fix color picker for custom mat.
			g_chams.glow_armsrace->FindVar( "$envmaptint", nullptr )->SetVecValue( color.r( ) / 255.f, color.g( ) / 255.f, color.b( ) / 255.f );
			g_chams.ghost->FindVar( "$envmaptint", nullptr )->SetVecValue( color.r( ) / 255.f, color.g( ) / 255.f, color.b( ) / 255.f );

			// set material.
			switch ( g_menu.main.players.chams_local_mat.get( ) ) {
			case 0:
				SetupMaterial( debugambientcube, color, false, g_menu.main.players.chams_local_wireframe.get( ) );
				break;
			case 1:
				SetupMaterial( debugdrawflat, color, false, g_menu.main.players.chams_local_wireframe.get( ) );
				break;
			case 2:
				SetupMaterial( materialMetall, color, false, g_menu.main.players.chams_local_wireframe.get( ) );
				break;
			case 3:
				SetupMaterial( glow_armsrace, color, false, g_menu.main.players.chams_local_wireframe.get( ) );
				break;
			case 4:
				SetupMaterial( ghost, color, false, g_menu.main.players.chams_local_wireframe.get( ) );
				break;
			}
		}

		// manually draw the model.
		player->DrawModel( );
	}

	// check if is an enemy.
	bool enemy = g_cl.m_local && player->enemy( g_cl.m_local );

	if ( enemy && g_menu.main.players.chams_enemy_history_enable.get( ) ) {
		RenderHistoryChams( player->index( ) );
	}

	/* enemy invisible chams */

	if ( enemy && g_menu.main.players.chams_enemy.get( 0 ) && g_menu.main.players.chams_enemy_enable.get( ) ) {
		if ( g_menu.main.players.chams_enemy.get( 1 ) ) {
			// get color.
			Color color = g_menu.main.players.chams_enemy_invis.get( );

			// override blend.
			SetAlpha( g_menu.main.players.chams_enemy_blend.get( ) / 100.f );

			// fix color picker for custom mat.
			g_chams.glow_armsrace->FindVar( "$envmaptint", nullptr )->SetVecValue( color.r( ) / 255.f, color.g( ) / 255.f, color.b( ) / 255.f );
			g_chams.ghost->FindVar( "$envmaptint", nullptr )->SetVecValue( color.r( ) / 255.f, color.g( ) / 255.f, color.b( ) / 255.f );

			// set material.
			switch ( g_menu.main.players.chams_enemy_mat.get( ) ) {
			case 0:
				SetupMaterial( debugambientcube, color, true, g_menu.main.players.chams_enemy_wireframe.get( ) );
				break;
			case 1:
				SetupMaterial( debugdrawflat, color, true, g_menu.main.players.chams_enemy_wireframe.get( ) );
				break;
			case 2:
				SetupMaterial( materialMetallnZ, color, true, g_menu.main.players.chams_enemy_wireframe.get( ) );
				break;
			case 3:
				SetupMaterial( glow_armsrace, color, true, g_menu.main.players.chams_enemy_wireframe.get( ) );
				break;
			case 4:
				SetupMaterial( ghost, color, true, g_menu.main.players.chams_enemy_wireframe.get( ) );
				break;
			}

			player->DrawModel( );
		}

		/* enemy visible chams */

		// get color.
		Color color = g_menu.main.players.chams_enemy_vis.get( );

		// override blend.
		SetAlpha( g_menu.main.players.chams_enemy_blend.get( ) / 100.f );

		// fix color picker for custom mat.
		g_chams.glow_armsrace->FindVar( "$envmaptint", nullptr )->SetVecValue( color.r( ) / 255.f, color.g( ) / 255.f, color.b( ) / 255.f );
		g_chams.ghost->FindVar( "$envmaptint", nullptr )->SetVecValue( color.r( ) / 255.f, color.g( ) / 255.f, color.b( ) / 255.f );

		// set material.
		switch ( g_menu.main.players.chams_enemy_mat.get( ) ) {
		case 0:
			SetupMaterial( debugambientcube, color, false, g_menu.main.players.chams_enemy_wireframe.get( ) );
			break;
		case 1:
			SetupMaterial( debugdrawflat, color, false, g_menu.main.players.chams_enemy_wireframe.get( ) );
			break;
		case 2:
			SetupMaterial( materialMetall, color, false, g_menu.main.players.chams_enemy_wireframe.get( ) );
			break;
		case 3:
			SetupMaterial( glow_armsrace, color, false, g_menu.main.players.chams_enemy_wireframe.get( ) );
			break;
		case 4:
			SetupMaterial( ghost, color, false, g_menu.main.players.chams_enemy_wireframe.get( ) );
			break;
		}

		player->DrawModel( );
	}

	/* friendly invsible chams */

	else if ( !enemy && !player->m_bIsLocalPlayer( ) && g_menu.main.players.chams_friendly.get( 0 ) && g_menu.main.players.chams_friendly_enable.get( ) ) {
		if ( g_menu.main.players.chams_friendly.get( 1 ) ) {
			// get color.
			Color color = g_menu.main.players.chams_friendly_invis.get( );

			// override blend.
			SetAlpha( g_menu.main.players.chams_friendly_blend.get( ) / 100.f );

			// fix color picker for custom mat.
			g_chams.glow_armsrace->FindVar( "$envmaptint", nullptr )->SetVecValue( color.r( ) / 255.f, color.g( ) / 255.f, color.b( ) / 255.f );
			g_chams.ghost->FindVar( "$envmaptint", nullptr )->SetVecValue( color.r( ) / 255.f, color.g( ) / 255.f, color.b( ) / 255.f );

			// set material.
			switch ( g_menu.main.players.chams_friendly_mat.get( ) ) {
			case 0:
				SetupMaterial( debugambientcube, color, true, g_menu.main.players.chams_friendly_wireframe.get( ) );
				break;
			case 1:
				SetupMaterial( debugdrawflat, color, true, g_menu.main.players.chams_friendly_wireframe.get( ) );
				break;
			case 2:
				SetupMaterial( materialMetallnZ, color, true, g_menu.main.players.chams_friendly_wireframe.get( ) );
				break;
			case 3:
				SetupMaterial( glow_armsrace, color, true, g_menu.main.players.chams_friendly_wireframe.get( ) );
				break;
			case 4:
				SetupMaterial( ghost, color, true, g_menu.main.players.chams_friendly_wireframe.get( ) );
				break;
			}

			player->DrawModel( );
		}

		/* friendly visible chams */

		// get color.
		Color color = g_menu.main.players.chams_friendly_vis.get( );

		// override blend.
		SetAlpha( g_menu.main.players.chams_friendly_blend.get( ) / 100.f );

		// fix color picker for custom mat.
		g_chams.glow_armsrace->FindVar( "$envmaptint", nullptr )->SetVecValue( color.r( ) / 255.f, color.g( ) / 255.f, color.b( ) / 255.f );
		g_chams.ghost->FindVar( "$envmaptint", nullptr )->SetVecValue( color.r( ) / 255.f, color.g( ) / 255.f, color.b( ) / 255.f );

		// set material.
		switch ( g_menu.main.players.chams_friendly_mat.get( ) ) {
		case 0:
			SetupMaterial( debugambientcube, color, false, g_menu.main.players.chams_friendly_wireframe.get( ) );
			break;
		case 1:
			SetupMaterial( debugdrawflat, color, false, g_menu.main.players.chams_friendly_wireframe.get( ) );
			break;
		case 2:
			SetupMaterial( materialMetall, color, false, g_menu.main.players.chams_friendly_wireframe.get( ) );
			break;
		case 3:
			SetupMaterial( glow_armsrace, color, false, g_menu.main.players.chams_friendly_wireframe.get( ) );
			break;
		case 4:
			SetupMaterial( ghost, color, false, g_menu.main.players.chams_friendly_wireframe.get( ) );
			break;
		}

		player->DrawModel( );
	}


	m_running = false;
}

bool Chams::SortPlayers( ) {
	// lambda-callback for std::sort.
	// to sort the players based on distance to the local-player.
	static auto distance_predicate = [ ]( Entity* a, Entity* b ) {
		vec3_t local = g_cl.m_local->GetAbsOrigin( );

		// note - dex; using squared length to save out on sqrt calls, we don't care about it anyway.
		float len1 = ( a->GetAbsOrigin( ) - local ).length_sqr( );
		float len2 = ( b->GetAbsOrigin( ) - local ).length_sqr( );

		return len1 < len2;
	};

	// reset player container.
	m_players.clear( );

	// find all players that should be rendered.
	for ( int i{ 1 }; i <= g_csgo.m_globals->m_max_clients; ++i ) {
		// get player ptr by idx.
		Player* player = g_csgo.m_entlist->GetClientEntity< Player* >( i );

		// validate.
		if ( !player || !player->IsPlayer( ) || !player->alive( ) )
			continue;

		// do not draw players occluded by view plane.
		if ( !IsInViewPlane( player->WorldSpaceCenter( ) ) )
			continue;

		// this player was not skipped to draw later.
		// so do not add it to our render list.
		if ( !OverridePlayer( i ) )
			continue;

		m_players.push_back( player );
	}

	// any players?
	if ( m_players.empty( ) )
		return false;

	// sorting fixes the weird weapon on back flickers.
	// and all the other problems regarding Z-layering in this shit game.
	std::sort( m_players.begin( ), m_players.end( ), distance_predicate );

	return true;
}