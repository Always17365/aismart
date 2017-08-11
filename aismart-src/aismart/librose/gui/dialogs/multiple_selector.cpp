/* $Id: simple_item_selector.cpp 48440 2011-02-07 20:57:31Z mordante $ */
/*
   Copyright (C) 2010 - 2011 by Ignacio Riquelme Morelle <shadowm2006@gmail.com>
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

#include "display.hpp"
#include "gui/dialogs/multiple_selector.hpp"

#include "gui/widgets/button.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/toggle_panel.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"

#include <boost/bind.hpp>

namespace gui2 {

REGISTER_DIALOG(rose, multiple_selector)

tmultiple_selector::tmultiple_selector(const std::string& title, const std::vector<tval_str>& items, const std::set<int>& initial_selected)
	: title_(title)
	, items_(items)
	, initial_selected_(initial_selected)
{
}

void tmultiple_selector::pre_show(CVideo& /*video*/, twindow& window)
{
	window.set_label("misc/white-background.png");
	window.set_border_space(0, 0, 0, 0);

	tlabel& title = find_widget<tlabel>(&window, "title", false);
	tlistbox& list = find_widget<tlistbox>(&window, "listbox", false);
	window.keyboard_capture(&list);

	title.set_label(title_);

	std::map<std::string, std::string> data;
	for (std::vector<tval_str>::const_iterator it = items_.begin(); it != items_.end(); ++ it) {
		data["item"] = it->str;

		ttoggle_panel& row = list.insert_row(data);
		ttoggle_button* widget = dynamic_cast<ttoggle_button*>(row.find("select", false));
		if (initial_selected_.find(row.at()) != initial_selected_.end()) {
			widget->set_value(true);
		}
	}

	connect_signal_mouse_left_click(
			  find_widget<tbutton>(window_, "save", false)
			, boost::bind(
				&tmultiple_selector::save
				, this, boost::ref(window)));
}

void tmultiple_selector::post_show(twindow& window)
{
	tlistbox& list = find_widget<tlistbox>(&window, "listbox", false);
	int rows = list.rows();

	for (int at = 0; at < rows; at ++) {
		ttoggle_panel& row = list.row_panel(at);
		ttoggle_button* widget = dynamic_cast<ttoggle_button*>(row.find("select", false));
		if (widget->get_value()) {
			selected_.insert(at);
		}
	}
}

void tmultiple_selector::save(twindow& window)
{
	window.set_retval(twindow::OK);
}

}
