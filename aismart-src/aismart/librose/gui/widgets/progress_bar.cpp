/* $Id: progress_bar.cpp 54604 2012-07-07 00:49:45Z loonycyborg $ */
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

#define GETTEXT_DOMAIN "rose-lib"

#include "gui/widgets/progress_bar.hpp"

#include "gui/widgets/settings.hpp"
#include "gui/widgets/track.hpp"
#include "wml_exception.hpp"

#include "font.hpp"
#include <boost/bind.hpp>

namespace gui2 {

tprogress_bar::tprogress_bar(twindow& window, twidget& widget, const std::string& message, int style)
	: tbase_tpl_widget(window, widget)
	, widget_(dynamic_cast<ttrack*>(&widget))
	, message_(message)
	, style_(style)
	, percentage_(0)
{
	widget_->set_did_draw(boost::bind(&tprogress_bar::did_draw_bar, this, _1, _2, _3));
}

bool tprogress_bar::set_percentage(const int percentage)
{
	VALIDATE(percentage >= 0 && percentage <= 100, null_str);

	if (percentage == percentage_) {
		return false;
	}

	percentage_ = percentage;
	SDL_Rect rect = widget_->get_draw_rect();
	did_draw_bar(*widget_, rect, false);

	return true;
}

bool tprogress_bar::set_message(const std::string& message)
{
	if (message == message_) {
		return false;
	}

	message_ = message;
	SDL_Rect rect = widget_->get_draw_rect();
	did_draw_bar(*widget_, rect, false);

	return true;
}

void tprogress_bar::did_draw_bar(ttrack& widget, const SDL_Rect& widget_rect, const bool bg_drawn)
{
	SDL_Renderer* renderer = get_renderer();
	ttrack::tdraw_lock lock(renderer, widget);

	if (!bg_drawn) {
		SDL_RenderCopy(renderer, widget.background_texture().get(), NULL, &widget_rect);
	}

	render_rect_frame(renderer, widget_rect, 0x8000ff00);
	render_rect(renderer, SDL_Rect{widget_rect.x, widget_rect.y, widget_rect.w * percentage_ / 100, widget_rect.h}, 0x8000ff00);

	if (!message_.empty()) {
		surface surf = font::get_rendered_text(message_, 0, font::SIZE_DEFAULT, uint32_to_color(0xff000000));
		SDL_Rect dest_rect {widget_rect.x + (widget_rect.w - surf->w) / 2, widget_rect.y + (widget_rect.h - surf->h) / 2, surf->w, surf->h};
		render_surface(renderer, surf, nullptr, &dest_rect);
	}
}

} // namespace gui2

