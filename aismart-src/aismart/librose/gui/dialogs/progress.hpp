/* $Id$ */
/*
   Copyright (C) 2011 by Sergey Popov <loonycyborg@gmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef GUI_DIALOGS_PROGRESS_HPP_INCLUDED
#define GUI_DIALOGS_PROGRESS_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include "gui/widgets/progress_bar.hpp"

namespace gui2 {

class tbutton;

/**
 * Dialog that tracks network transmissions
 *
 * It shows upload/download progress and allows the user
 * to cancel the transmission.
 */
class tprogress : public tdialog, public tprogress_
{
public:
	tprogress(const std::string& message, const boost::function<void (tprogress_&)>& did_first_drawn, int hidden_ms, bool show_cancel);

	void set_percentage(const int percentage) override;
	void set_message(const std::string& message) override;
	void cancel_task() override;
	bool is_ok() const override;

protected:
	/** Inherited from tdialog. */
	void pre_show(CVideo& video, twindow& window);

	/** Inherited from tdialog. */
	void post_show(twindow& window);

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

private:
	void app_first_drawn(twindow& window) override;
	void cancel(twindow& window);

private:
	// The title for the dialog.
	const std::string message_;
	boost::function<void (tprogress_&)> did_first_drawn_;
	bool show_cancel_;

	std::unique_ptr<tprogress_bar> progress_;
	
	tbutton* cancel_;

	Uint32 start_ticks_;
	Uint32 hidden_ticks_;
};

} // namespace gui2

#endif

