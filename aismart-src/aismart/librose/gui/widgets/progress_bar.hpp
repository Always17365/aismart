/* $Id: progress_bar.hpp 52533 2012-01-07 02:35:17Z shadowmaster $ */
/*
   Copyright (C) 2010 - 2012 by Mark de Wever <koraq@xs4all.nl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef GUI_WIDGETS_PROGRESS_BAR_HPP_INCLUDED
#define GUI_WIDGETS_PROGRESS_BAR_HPP_INCLUDED

#include "gui/widgets/control.hpp"

namespace gui2 {

class ttrack;

class tprogress_bar: public tbase_tpl_widget
{
public:
	enum {style_bar};
	explicit tprogress_bar(twindow& window, twidget& widget, const std::string& message, int style);

	ttrack& track() const { return *widget_; }
	
	bool set_percentage(const int percentage);
	int get_percentage() const { return percentage_; }

	bool set_message(const std::string& message);

private:
	void did_draw_bar(ttrack& widget, const SDL_Rect& widget_rect, const bool bg_drawn);

private:
	ttrack* widget_;

	int style_;
	int percentage_;
	std::string message_;
};

} // namespace gui2

#endif

