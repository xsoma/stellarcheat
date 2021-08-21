#include "includes.h"

void Checkbox::draw( ) {
	Rect area{ m_parent->GetElementsRect( ) };
	Point p{ area.x + m_pos.x, area.y + m_pos.y };

	// get gui color.
	Color color = g_gui.m_color;
	color.a( ) = m_parent->m_alpha;

	// render black outline on checkbox.
	render::rect( p.x, p.y, CHECKBOX_SIZE, CHECKBOX_SIZE, { 0, 0, 0, m_parent->m_alpha } );

	// render checkbox title.
	if( m_use_label )
	    render::menu_shade.string( p.x + LABEL_OFFSET, p.y - 3, { 205, 205, 205, m_parent->m_alpha }, m_label );

	// render border.
	render::rect( p.x + 1, p.y + 1, CHECKBOX_SIZE - 2, CHECKBOX_SIZE - 2, { 0, 0, 0, m_parent->m_alpha } );

	// render checked.
	if( m_checked ) {
		render::gradient( p.x + 1, p.y + 1, CHECKBOX_SIZE - 2, CHECKBOX_SIZE - 2, color, { 0, 0, 0, 0 } );
		render::rect( p.x + 1, p.y + 1, CHECKBOX_SIZE - 2, CHECKBOX_SIZE - 2, color );

		//left right
		render::gradient1337( p.x + CHECKBOX_SIZE - 2, p.y + 1, 3, CHECKBOX_SIZE - 2, color, { 0, 0, 0, 0 } );
		render::gradient1337( p.x - 1, p.y + 1, 3, CHECKBOX_SIZE - 2, { 0, 0, 0, 0 }, color );

		//up down
		render::gradient( p.x + 1, p.y - 1, CHECKBOX_SIZE - 2, 3, { 0, 0, 0, 0 }, color );
		render::gradient( p.x + 1, p.y - 1 + CHECKBOX_SIZE - 2, CHECKBOX_SIZE - 2, 3, color, { 0, 0, 0, 0 } );
	}

	else
		render::gradient( p.x + 1, p.y + 1, CHECKBOX_SIZE - 2, CHECKBOX_SIZE - 2, { 75, 75, 75, m_parent->m_alpha }, { 50, 50, 50, m_parent->m_alpha } );

	//render::rect( el.x + m_pos.x, el.y + m_pos.y, m_w, m_pos.h, { 255, 0, 0 } );
}

void Checkbox::think( ) {
	// set the click area to the length of the string, so we can also press the string to toggle.
	if( m_use_label )
		m_w = LABEL_OFFSET + render::menu_shade.size( m_label ).m_width;
}

void Checkbox::click( ) {
	// toggle.
	m_checked = !m_checked;

	if( m_callback )
		m_callback( );
}