/* $Id$ */
/*
   Copyright (C) 2011 Sergey Popov <loonycyborg@gmail.com>
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

#include "gui/dialogs/progress.hpp"

#include "gettext.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"

#include <boost/bind.hpp>

namespace gui2 {

REGISTER_DIALOG(rose, progress)

tprogress::tprogress(const std::string& message, const boost::function<void (tprogress_&)>& did_first_drawn, int hidden_ms, bool show_cancel)
	: message_(message)
	, start_ticks_(SDL_GetTicks())
	, did_first_drawn_(did_first_drawn)
	, hidden_ticks_(hidden_ms * 1000)
	, show_cancel_(show_cancel)
{
}

void tprogress::pre_show(CVideo& /*video*/, twindow& window)
{
	progress_.reset(new tprogress_bar(*window_, find_widget<twidget>(window_, "_progress", false), message_, tprogress_bar::style_bar));

	cancel_ = find_widget<tbutton>(&window, "_cancel", false, false);
	if (!show_cancel_) {
		cancel_->set_visible(twidget::INVISIBLE);
	}

	connect_signal_mouse_left_click(
		*cancel_
		, boost::bind(
		&tprogress::cancel
		, this
		, boost::ref(window)));

	window.set_visible(twidget::INVISIBLE);
}

void tprogress::post_show(twindow& /*window*/)
{
}

void tprogress::app_first_drawn(twindow& window)
{
	if (did_first_drawn_) {
		did_first_drawn_(*this);
	}
}

void tprogress::set_percentage(const int percentage)
{
	VALIDATE(percentage >= 0 && percentage <= 100, null_str);

	if (percentage == 100) {
		window_->set_retval(twindow::OK);

	} else {
		if (SDL_GetTicks() - start_ticks_ >= hidden_ticks_) {
			window_->set_visible(twidget::VISIBLE);
		}

		bool changed = progress_->set_percentage(percentage);
		if (changed && did_first_drawn_) {
			absolute_draw();
		}
	}
}

void tprogress::set_message(const std::string& message)
{
	bool changed = progress_->set_message(message);
	if (changed && did_first_drawn_) {
		absolute_draw();
	}
}

void tprogress::cancel_task()
{
	VALIDATE(window_, null_str);
	cancel(*window_);
}

bool tprogress::is_ok() const
{
	return get_retval() == twindow::OK;
}

void tprogress::cancel(twindow& window)
{
	window.set_retval(twindow::CANCEL);
}

} // namespace gui2

