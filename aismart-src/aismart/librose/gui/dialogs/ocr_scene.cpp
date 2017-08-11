#define GETTEXT_DOMAIN "smsrobot-lib"

#include "gui/dialogs/helper.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/widgets/label.hpp"
#include "gui/dialogs/ocr_scene.hpp"

#include "ocr/ocr_controller.hpp"
#include "hotkeys.hpp"
#include "gettext.hpp"

#include <boost/bind.hpp>

namespace gui2 {

REGISTER_DIALOG(rose, ocr_scene);

tocr_scene::tocr_scene(ocr_controller& controller)
	: tdialog(&controller)
	, controller_(controller)
{
}

void tocr_scene::pre_show(CVideo& video, twindow& window)
{
	// prepare status report.
	reports_.insert(std::make_pair(ZOOM, "zoom"));
	reports_.insert(std::make_pair(POSITION, "position"));

	// prepare hotkey
	hotkey::insert_hotkey(HOTKEY_RETURN, "return", _("Return to title"));
	hotkey::insert_hotkey(HOTKEY_RECOGNIZE, "recognize", _("Recognize"));

	tbutton* widget = dynamic_cast<tbutton*>(get_object("return"));
	click_generic_handler(*widget, null_str);

	widget = dynamic_cast<tbutton*>(get_object("recognize"));
	click_generic_handler(*widget, null_str);
	widget->set_active(false);

	tlistbox* list = dynamic_cast<tlistbox*>(get_object("fields"));
	list->enable_select(false);
	fill_fields_list();
}

void tocr_scene::fill_fields_list()
{
	tlistbox* list = dynamic_cast<tlistbox*>(get_object("fields"));
	list->clear();

	const std::vector<std::string>& fields = controller_.fields();
	std::map<std::string, std::string> data;

	for (std::vector<std::string>::const_iterator it = fields.begin(); it != fields.end(); ++ it) {
		const std::string& field = *it;
		data["name"] = controller_.field_is_bound(field)? ht::generate_format(field, 0xff00ff00): field;
		ttoggle_panel& row = list->insert_row(data);

		row.connect_signal<event::LONGPRESS>(
				boost::bind(
					&tocr_scene::signal_handler_longpress_name
					, this
					, _4, _5, boost::ref(row))
				, event::tdispatcher::back_child);

		row.connect_signal<event::LONGPRESS>(
				boost::bind(
					&tocr_scene::signal_handler_longpress_name
					, this
					, _4, _5, boost::ref(row))
				, event::tdispatcher::back_post_child);
	}
}

void tocr_scene::signal_handler_longpress_name(bool& halt, const tpoint& coordinate, ttoggle_panel& row)
{
	halt = true;

	const std::string name = controller_.fields()[row.at()];
	surface surf = font::get_rendered_text(name, 0, font::SIZE_LARGEST, font::BLACK_COLOR);
	window_->set_drag_surface(surf, true);

	controller_.start_drag(row.at());

	window_->start_drag(coordinate, boost::bind(&ocr_controller::did_drag_mouse_motion, &controller_, _1, _2, boost::ref(*window_)),
		boost::bind(&ocr_controller::did_drag_mouse_leave, &controller_, _1, _2, _3));
}

} //end namespace gui2
