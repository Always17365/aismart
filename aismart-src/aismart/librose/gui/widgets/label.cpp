/* $Id: label.cpp 54038 2012-04-30 19:37:24Z mordante $ */
/*
   Copyright (C) 2008 - 2012 by Mark de Wever <koraq@xs4all.nl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#define GETTEXT_DOMAIN "rose-lib"

#include "gui/widgets/label.hpp"

#include "gui/auxiliary/widget_definition/label.hpp"
#include "gui/auxiliary/window_builder/label.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "font.hpp"

#include <boost/bind.hpp>

namespace gui2 {

REGISTER_WIDGET(label)

tlabel::tlabel()
	: tcontrol(COUNT)
	, state_(ENABLED)
	, multiline_(false)
{}

tpoint tlabel::calculate_best_size_bh(const int width)
{
	return calculate_best_size_bh_multiline(width, multiline_);
}

void tlabel::set_label(const std::string& label)
{
	set_label_multiline(label, multiline_);
}

void tlabel::set_state(const tstate state)
{
	if (state != state_) {
		state_ = state;
		set_dirty();
	}
}

const std::string& tlabel::get_control_type() const
{
	static const std::string type = "label";
	return type;
}

} // namespace gui2

