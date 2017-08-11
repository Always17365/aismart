/* Require Rose v1.0.10 or above. $ */

#define GETTEXT_DOMAIN "aismart-lib"

#include "base_instance.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/chat.hpp"
#include "gui/dialogs/home.hpp"
#include "gui/widgets/window.hpp"
#include "game_end_exceptions.hpp"
#include "wml_exception.hpp"
#include "gettext.hpp"
#include "loadscreen.hpp"
#include "formula_string_utils.hpp"
#include "help.hpp"
#include "version.hpp"
#include "tensorflow_link.hpp"


namespace easypr {
std::string kDefaultSvmPath;
std::string kLBPSvmPath;
std::string kHistSvmPath;

std::string kDefaultAnnPath;
std::string kChineseAnnPath;
std::string kGrayAnnPath;

//This is important to for key transform to chinese
std::string kChineseMappingPath;
}

class game_instance: public base_instance
{
public:
	game_instance(rtc::PhysicalSocketServer& ss, int argc, char** argv);

	std::map<std::string, tocr_result> handle_ocr(const std::vector<std::string>& fields);

private:
	void app_tensorflow_link() override;
	void app_load_settings_config(const config& cfg) override;
	void load_pb() override;
};

game_instance::game_instance(rtc::PhysicalSocketServer& ss, int argc, char** argv)
	: base_instance(ss, argc, argv)
{
}

void game_instance::app_tensorflow_link()
{
	tensorflow_link_modules();
}

void game_instance::app_load_settings_config(const config& cfg)
{
	game_config::version = cfg["version"].str();
	game_config::wesnoth_version = version_info(game_config::version);
}

void copy_model_directory(const std::string& src_dir)
{
	Uint32 start = SDL_GetTicks();

	// copy font directory from res/model to user_data_dir/model
	std::vector<std::string> v;
	v.push_back("svm_hist.xml");
	v.push_back("svm_lbp.xml");
	v.push_back("svm_hist.xml");
	v.push_back("ann.xml");
	v.push_back("ann_chinese.xml");
	v.push_back("annCh.xml");
	v.push_back("province_mapping");
	create_directory_if_missing(game_config::preferences_dir_utf8 + "/model");
	for (std::vector<std::string>::const_iterator it = v.begin(); it != v.end(); ++ it) {
		const std::string src = src_dir + "/" + *it;
		const std::string dst = game_config::preferences_dir_utf8 + "/model/" + *it;
		if (!file_exists(dst)) {
			tfile src_file(src, GENERIC_READ, OPEN_EXISTING);
			tfile dst_file(dst, GENERIC_WRITE, CREATE_ALWAYS);
			if (src_file.valid() && dst_file.valid()) {
				int32_t fsize = src_file.read_2_data();
				posix_fwrite(dst_file.fp, src_file.data, fsize);
			}
		}
	}
}

void game_instance::load_pb()
{
	// copy models from res to preferences
	const std::string src = game_config::path + "/" + game_config::generate_app_dir(game_config::app) + "/model";
	const std::string dst = game_config::preferences_dir;
	copy_model_directory(src);

	easypr::kDefaultSvmPath = game_config::preferences_dir + "/model/svm_hist.xml";
	easypr::kLBPSvmPath = game_config::preferences_dir + "/model/svm_lbp.xml";
	easypr::kHistSvmPath = game_config::preferences_dir + "/model/svm_hist.xml";

	easypr::kDefaultAnnPath = game_config::preferences_dir + "/model/ann.xml";
	easypr::kChineseAnnPath = game_config::preferences_dir + "/model/ann_chinese.xml";
	easypr::kGrayAnnPath = game_config::preferences_dir + "/model/annCh.xml";

	//This is important to for key transform to chinese
	easypr::kChineseMappingPath = game_config::preferences_dir + "/model/province_mapping";

#ifdef _WIN32
	conv_ansi_utf8(easypr::kDefaultSvmPath, false);
	conv_ansi_utf8(easypr::kLBPSvmPath, false);
	conv_ansi_utf8(easypr::kHistSvmPath, false);

	conv_ansi_utf8(easypr::kDefaultAnnPath, false);
	conv_ansi_utf8(easypr::kChineseAnnPath, false);
	conv_ansi_utf8(easypr::kGrayAnnPath, false);

	conv_ansi_utf8(easypr::kChineseMappingPath, false);
#endif
}

std::map<std::string, tocr_result> game_instance::handle_ocr(const std::vector<std::string>& fields)
{
	const std::string pb_short_path = "combined_model.pb";

	// surface surf = image::get_image("misc/template_ocr.png");
	return tensorflow2::ocr(app_cfg(), disp(), nullptr, fields, pb_short_path);
}

/**
 * Setups the game environment and enters
 * the titlescreen or game loops.
 */
static int do_gameloop(int argc, char** argv)
{
	rtc::PhysicalSocketServer ss;
	instance_manager<game_instance> manager(ss, argc, argv, "aismart", "#rose", false);
	game_instance& game = manager.get();

	try {
		std::vector<std::string> fields;
		std::map<std::string, tocr_result> ocr_results;
		int start_layer = gui2::thome::BASE_LAYER;

		for (;;) {
			game.loadscreen_manager().reset();
			const font::floating_label_context label_manager;

			cursor::set(cursor::NORMAL);

			gui2::thome::tresult res;
			{
				gui2::thome dlg(game.app_cfg(), game.disp(), fields, ocr_results, start_layer);
				dlg.show(game.disp().video());
				res = static_cast<gui2::thome::tresult>(dlg.get_retval());
			}

			if (res == gui2::thome::OCR) {
				ocr_results = game.handle_ocr(fields);
				start_layer = gui2::thome::OCR_LAYER;

			} else if (res == gui2::thome::CHAT) {
				gui2::tchat2 dlg(game.disp(), "chat_module");
				dlg.show(game.disp().video());
				start_layer = gui2::thome::MORE_LAYER;
			}
		}

	} catch (twml_exception& e) {
		e.show(game.disp());

	} catch (CVideo::quit&) {
		//just means the game should quit
		posix_print("SDL_main, catched CVideo::quit\n");

	} catch (game_logic::formula_error& e) {
		gui2::show_error_message(game.disp().video(), e.what());
	} 

	return 0;
}

int main(int argc, char** argv)
{
	try {
		do_gameloop(argc, argv);
	} catch (twml_exception& e) {
		// this exception is generated when create instance.
		posix_print_mb("%s\n", e.user_message.c_str());
	}

	return 0;
}