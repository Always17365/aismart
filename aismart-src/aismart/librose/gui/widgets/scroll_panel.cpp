/* $Id: scroll_panel.cpp 52533 2012-01-07 02:35:17Z shadowmaster $ */
/*
   Copyright (C) 2009 - 2012 by Mark de Wever <koraq@xs4all.nl>
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

#include "gui/widgets/scroll_panel.hpp"

#include "gui/auxiliary/widget_definition/scroll_panel.hpp"
#include "gui/auxiliary/window_builder/scroll_panel.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/scrollbar.hpp"

#include <boost/bind.hpp>

namespace gui2 {

REGISTER_WIDGET(scroll_panel)

tscroll_panel::tscroll_panel(const unsigned canvas_count)
	: tscroll_container(canvas_count)
{
	// why require_capture_ must be false, reference http://www.libsdl.cn/bbs/forum.php?mod=viewthread&tid=139&extra=page%3D1%26filter%3Dtypeid%26typeid%3D21
	// require_capture_ = false;

	find_content_grid_ = true;
}

tpoint tscroll_panel::mini_calculate_content_grid_size(const tpoint& content_origin, const tpoint& content_size)
{
	const tpoint actual_size = content_grid_->get_best_size();

	return tpoint(std::max(actual_size.x, content_size.x), std::max(actual_size.y, content_size.y));
}

// amone widget derived from scroll_container, there is only scroll_panel.
tpoint tscroll_panel::calculate_best_size_bh(const int width)
{
	//
	// best_size use content_grid_->calculate_best_size_bh is always full size. widget maybe from *.cfg.
	// below code is from tcontrol::calculate_best_size.
	//
	const tpoint cfg_size = best_size_from_builder();
	if (cfg_size.x != npos && cfg_size.y != npos && !size_is_max_) {
		return cfg_size;
	}

	tpoint text_size = content_grid_->calculate_best_size_bh(width);

	// text size must >= minimum size. 
	const tpoint minimum = get_config_min_size();
	if (minimum.x > 0 && text_size.x < minimum.x) {
		text_size.x = minimum.x;
	}
	if (minimum.y > 0 && text_size.y < minimum.y) {
		text_size.y = minimum.y;
	}

	tpoint result(text_size.x + config_->text_extra_width, text_size.y + config_->text_extra_height);
	if (!size_is_max_) {
		if (cfg_size.x != npos) {
			result.x = cfg_size.x;
		}
		if (cfg_size.y != npos) {
			result.y = cfg_size.y;
		}
	} else {
		if (cfg_size.x != npos && result.x >= cfg_size.x) {
			result.x = cfg_size.x;
		}
		if (cfg_size.y != npos && result.y >= cfg_size.y) {
			result.y = cfg_size.y;
		}
	}
	return result;
}

const std::string& tscroll_panel::get_control_type() const
{
    static const std::string type = "scroll_panel";
    return type;
}

} // namespace gui2

