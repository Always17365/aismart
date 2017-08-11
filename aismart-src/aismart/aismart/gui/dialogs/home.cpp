#define GETTEXT_DOMAIN "aismart-lib"

#include "gui/dialogs/home.hpp"

#include "display.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/report.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/track.hpp"
#include "gui/widgets/stack.hpp"
#include "gui/widgets/slider.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/widgets/toggle_panel.hpp"
#include "gui/widgets/window.hpp"
#include "gettext.hpp"
#include "formula_string_utils.hpp"
#include "rose_config.hpp"
#include "filesystem.hpp"

#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/objdetect.hpp>
#include <opencv2/video.hpp>

#include <boost/bind.hpp>

#include "tensorflow/core/framework/op_kernel.h"
#include "tensorflow/core/public/session.h"
#include "tensorflow/core/protobuf/meta_graph.pb.h"

#include "easypr/core/plate_recognize.h"

namespace gui2 {

REGISTER_DIALOG(aismart, home)

thome::thome(const config& app_cfg, display& disp, std::vector<std::string>& fields, const std::map<std::string, tocr_result>& ocr_results, int start_layer)
	: app_cfg_(app_cfg)
	, disp_(disp)
	, fields_(fields)
	, ocr_results_(ocr_results)
	, start_layer_(start_layer)
	, last_coordinate_(construct_null_coordinate())
	, recognition_thread_running_(false)
	, rng_(12345)
	, current_example_(twidget::npos)
	, current_surf_(std::make_pair(false, surface()))
	, setting_(false)
{
}

thome::~thome()
{
	avcapture_.reset(nullptr);
}

void thome::pre_show(CVideo& video, twindow& window)
{
	// window.set_border_space(0, 0, 0, 0);
	window.set_label("misc/white-background.png");

	body_ = find_widget<tstack>(&window, "body", false, true);
	pre_base(window);
	pre_ocr(window);
	pre_more(window);

	treport* report = find_widget<treport>(&window, "navigation", false, true);
	tcontrol* item = &report->insert_item(null_str, _("Base"));
	item->set_icon("misc/base.png");

	item = &report->insert_item(null_str, _("OCR"));
	item->set_icon("misc/ocr.png");

	item = &report->insert_item(null_str, _("More"));
	item->set_icon("misc/more.png");

	report->set_did_item_changed(boost::bind(&thome::did_body_changed, this, _1, _2));
	report->select_item(start_layer_);
}

void thome::pre_base(twindow& window)
{
	tgrid* base_layer = body_->layer(BASE_LAYER);

	paper_ = find_widget<ttrack>(base_layer, "paper", false, true);
	paper_->set_did_draw(boost::bind(&thome::did_draw_paper, this, _1, _2, _3));
	paper_->set_did_left_button_down(boost::bind(&thome::did_left_button_down_paper, this, _1, _2));
	paper_->set_did_mouse_leave(boost::bind(&thome::did_mouse_leave_paper, this, _1, _2, _3));
	paper_->set_did_mouse_motion(boost::bind(&thome::did_mouse_motion_paper, this, _1, _2, _3));

	treport* report = find_widget<treport>(base_layer, "examples", false, true);
	tcontrol* item = &report->insert_item(null_str, "mouse");
	item->set_cookie(mouse);
	item = &report->insert_item(null_str, "inception5h");
	item->set_cookie(inception5h);
	item = &report->insert_item(null_str, "Classifier");
	item->set_cookie(classifier);
	item = &report->insert_item(null_str, "Detector");
	item->set_cookie(detector);
	item = &report->insert_item(null_str, "Plate");
	item->set_cookie(pr);
	report->set_did_item_changed(boost::bind(&thome::did_example_item_changed, this, _2));

	tslider* slider = find_widget<tslider>(base_layer, "slider1", false, true);
	slider->set_did_value_change(boost::bind(&thome::did_slider_value_changed, this, _1, _2, 0));
	slider = find_widget<tslider>(base_layer, "slider2", false, true);
	slider->set_did_value_change(boost::bind(&thome::did_slider_value_changed, this, _1, _2, 1));
}

void thome::did_body_changed(treport& report, ttoggle_button& widget)
{
	tgrid* current_layer = body_->layer(widget.at());
	body_->set_radio_layer(widget.at());
/*
	tbutton* button = find_widget<tbutton>(current_layer, "notification", false, true);
	refresh_notification_widget(widget.at());
*/
	stop_avcapture();

	if (widget.at() == BASE_LAYER) {

	} else if (widget.at() == OCR_LAYER) {

	} else {
		VALIDATE(widget.at() == MORE_LAYER, null_str);
	}
}

void thome::app_first_drawn(twindow& window)
{
	treport* report = find_widget<treport>(window_, "examples", false, true);
	report->select_item(0);
}

void thome::DoWork()
{
	recognition_thread_running_ = true;
	while (avcapture_.get()) {
		if (setting_) {
			// main thread is setting.
			// seting action may be risk recognition_mutex_.
			SDL_Delay(10);
			continue;
		}
		threading::lock lock(recognition_mutex_);
		if (!avcapture_.get()) {
			continue;
		}
		if (!current_surf_.first) {
			SDL_Delay(10);
			continue;
		}
		if (current_example_ == classifier) {
			VALIDATE(current_session_.second.get(), null_str);

			std::string result = example_inception5h_internal2(current_surf_.second);
			current_surf_.first = false;
			{
				threading::lock lock(variable_mutex_);
				result_ = result;
			}

		} else if (current_example_ == detector) {
			VALIDATE(current_session_.second.get(), null_str);

			std::string result = example_detector_internal(current_surf_.second);
			current_surf_.first = false;
			{
				threading::lock lock(variable_mutex_);
				result_ = result;
			}

		} else if (current_example_ == pr) {
			std::string result = example_pr_internal(current_surf_.second);
			current_surf_.first = false;
			{
				threading::lock lock(variable_mutex_);
				result_ = result;
			}

		} else {
			SDL_Delay(10);
		}
	}
	recognition_thread_running_ = false;
}

void thome::OnWorkStart()
{
}

void thome::OnWorkDone()
{
}

void thome::example_mouse()
{
	persist_surf_ = create_neutral_surface(paper_->get_width(), paper_->get_height());
	temperate_surf_ = create_neutral_surface(paper_->get_width(), paper_->get_height());

	blits_.clear();
	surface text_surf = font::get_rendered_text(_("Press left mouse and motion to draw rectangle."), 0, font::SIZE_DEFAULT, font::BLACK_COLOR);
	blits_.push_back(image::tblit(text_surf, 0, 0, text_surf->w, text_surf->h));
	blits_.push_back(image::tblit(persist_surf_, 0, 0, persist_surf_->w, persist_surf_->h));
	blits_.push_back(image::tblit(temperate_surf_, 0, 0, temperate_surf_->w, temperate_surf_->h));
	did_draw_paper(*paper_, paper_->get_draw_rect(), false);
}

// Returns the top N confidence values over threshold in the provided vector,
// sorted by confidence in descending order.
static void GetTopN(
	const Eigen::TensorMap<Eigen::Tensor<float, 1, Eigen::RowMajor>,
	Eigen::Aligned>& prediction,
	const int num_results, const float threshold,
	std::vector<std::pair<float, int> >* top_results) {
	// Will contain top N results in ascending order.
	std::priority_queue<std::pair<float, int>,
		std::vector<std::pair<float, int> >,
		std::greater<std::pair<float, int> > > top_result_pq;

	const long count = prediction.size();
	for (int i = 0; i < count; ++i) {
		const float value = prediction(i);

		// Only add it if it beats the threshold and has a chance at being in
		// the top N.
		if (value < threshold) {
			continue;
		}

		top_result_pq.push(std::pair<float, int>(value, i));

		// If at capacity, kick the smallest value out.
		if (top_result_pq.size() > num_results) {
			top_result_pq.pop();
		}
	}

	// Copy to output vector and reverse into descending order.
	while (!top_result_pq.empty()) {
		top_results->push_back(top_result_pq.top());
		top_result_pq.pop();
	}
	std::reverse(top_results->begin(), top_results->end());
}

std::string thome::example_inception5h_internal()
{
	std::stringstream result;

	const std::string data_path = game_config::path + "/" + game_config::generate_app_dir(game_config::app) + "/tensorflow";
	// const std::string pb_path = data_path + "/model.ckpt.meta";
	const std::string pb_path = data_path + "/inception5h/tensorflow_inception_graph.pb";

	tensorflow::Status s = tensorflow2::load_model(pb_path, current_session_);
	if (!s.ok()) {
		result << "load model fail: " << s;
		return result.str();
	}
	std::unique_ptr<tensorflow::Session>& session = current_session_.second;

	// Read the label list
	std::vector<std::string> label_strings;
	{
		const std::string imagenet_comp_path = data_path + "/inception5h/imagenet_comp_graph_label_strings.txt";
		tfile file(imagenet_comp_path, GENERIC_READ, OPEN_EXISTING);
		VALIDATE(file.valid(), null_str);
		int64_t fsize = file.read_2_data();
		int start = gui2::twidget::npos;
		const char* ptr = nullptr;
		for (int at = 0; at < fsize; at ++) {
			const char ch = file.data[at];
			if (ch == '\r' || ch == '\n') {
				if (start != gui2::twidget::npos) {
					label_strings.push_back(std::string(file.data + start, at - start));
					start = gui2::twidget::npos;
				}
			} else if (start == gui2::twidget::npos) {
				start = at;
			}
		}
		if (start != gui2::twidget::npos) {
			label_strings.push_back(std::string(file.data + start, fsize - start));
		}
	}

	// Read the Grace Hopper image.
	surface surf = image::get_image("misc/grace_hopper.png");

	{
		tsurface_2_mat_lock lock(surf);
		cv::cvtColor(lock.mat, lock.mat, cv::COLOR_BGRA2RGBA);
	}

	int image_width = surf->w;
	int image_height = surf->h;
	int image_channels = 4;
	const int wanted_width = 224;
	const int wanted_height = 224;
	const int wanted_channels = 3;
	const float input_mean = 117.0f;
	const float input_std = 1.0f;
	assert(image_channels >= wanted_channels);
	tensorflow::Tensor image_tensor(
		tensorflow::DT_FLOAT,
		tensorflow::TensorShape({
		1, wanted_height, wanted_width, wanted_channels }));
	auto image_tensor_mapped = image_tensor.tensor<float, 4>();

	surface_lock dst_lock(surf);
	tensorflow::uint8* in = (uint8_t*)(dst_lock.pixels());
	// tensorflow::uint8* in_end = (in + (image_height * image_width * image_channels));
	float* out = image_tensor_mapped.data();
	for (int y = 0; y < wanted_height; ++y) {
		const int in_y = (y * image_height) / wanted_height;
		tensorflow::uint8* in_row = in + (in_y * image_width * image_channels);
		float* out_row = out + (y * wanted_width * wanted_channels);
		for (int x = 0; x < wanted_width; ++x) {
			const int in_x = (x * image_width) / wanted_width;
			tensorflow::uint8* in_pixel = in_row + (in_x * image_channels);
			float* out_pixel = out_row + (x * wanted_channels);
			for (int c = 0; c < wanted_channels; ++c) {
				out_pixel[c] = (in_pixel[c] - input_mean) / input_std;
			}
		}
	}

	result << " - loaded!";
	result << " - " << label_strings.size() << ", " << label_strings[0] << " - " << image_width << "x" << image_height;

	uint32_t start = SDL_GetTicks();

	std::string input_layer = "input";
	std::string output_layer = "output";
	std::vector<tensorflow::Tensor> outputs;
	tensorflow::Status run_status = session->Run({ { input_layer, image_tensor } },
	{ output_layer }, {}, &outputs);
	if (!run_status.ok()) {
		result << "Running model failed: " << run_status;
		tensorflow::LogAllRegisteredKernels();
		return result.str();
	}

	uint32_t end = SDL_GetTicks();

	tensorflow::string status_string = run_status.ToString();
	result << " - " << status_string << "\n";

	tensorflow::Tensor* output = &outputs[0];
	const int kNumResults = 5;
	const float kThreshold = 0.1f;
	std::vector<std::pair<float, int> > top_results;
	GetTopN(output->flat<float>(), kNumResults, kThreshold, &top_results);

	std::stringstream ss;
	ss.precision(3);
	for (const auto& result : top_results) {
		const float confidence = result.first;
		const int index = result.second;

		ss << " - " << index << " " << confidence << "  ";

		// Write out the result as a string
		if (index < label_strings.size()) {
			// just for safety: theoretically, the output is under 1000 unless there
			// is some numerical issues leading to a wrong prediction.
			ss << label_strings[index];
		}
		else {
			ss << "Prediction: " << index;
		}

		ss << "\n";
	}


	LOG(INFO) << "Predictions: " << ss.str();

	tensorflow::string predictions = ss.str();
	result << predictions;
	result << " - use " << end - start << " ms";

	{
		tsurface_2_mat_lock lock(surf);
		cv::cvtColor(lock.mat, lock.mat, cv::COLOR_RGBA2BGRA);
	}

	return result.str();
}


std::string thome::example_inception5h_internal2(surface& surf)
{
	std::stringstream result;

	const std::string data_path = game_config::path + "/" + game_config::generate_app_dir(game_config::app) + "/tensorflow";
	std::unique_ptr<tensorflow::Session>& session = current_session_.second;

	// Read the label list
	if (label_strings_.empty()) {
		const std::string imagenet_comp_path = data_path + "/inception5h/imagenet_comp_graph_label_strings.txt";
		tfile file(imagenet_comp_path, GENERIC_READ, OPEN_EXISTING);
		VALIDATE(file.valid(), null_str);
		int64_t fsize = file.read_2_data();
		int start = gui2::twidget::npos;
		const char* ptr = nullptr;
		for (int at = 0; at < fsize; at ++) {
			const char ch = file.data[at];
			if (ch == '\r' || ch == '\n') {
				if (start != gui2::twidget::npos) {
					label_strings_.push_back(std::string(file.data + start, at - start));
					start = gui2::twidget::npos;
				}
			} else if (start == gui2::twidget::npos) {
				start = at;
			}
		}
		if (start != gui2::twidget::npos) {
			label_strings_.push_back(std::string(file.data + start, fsize - start));
		}
	}

	// Read the Grace Hopper image.
	{
		tsurface_2_mat_lock lock(surf);
		cv::cvtColor(lock.mat, lock.mat, cv::COLOR_BGRA2RGBA);
	}

	const int image_channels = 4;
	int image_width = surf->w;
	int image_height = surf->h;
	const int sourceRowBytes = image_width * image_channels;

	surface_lock dst_lock(surf);
	uint8_t* sourceStartAddr = (uint8_t*)dst_lock.pixels();
/*
	if (image_height > image_width) {
		const int marginY = ((image_height - image_width) / 2);
		sourceStartAddr = ((uint8_t*)dst_lock.pixels() + (marginY * sourceRowBytes));
		image_height = image_width;
	}
*/
	const int wanted_width = 224;
	const int wanted_height = 224;
	const int wanted_channels = 3;
	const float input_mean = 117.0f;
	const float input_std = 1.0f;
	VALIDATE(image_channels >= wanted_channels, null_str);
	tensorflow::Tensor image_tensor(
		tensorflow::DT_FLOAT,
		tensorflow::TensorShape({
		1, wanted_height, wanted_width, wanted_channels }));
	auto image_tensor_mapped = image_tensor.tensor<float, 4>();

	tensorflow::uint8* in = sourceStartAddr;
	// tensorflow::uint8* in_end = (in + (image_height * image_width * image_channels));
	float* out = image_tensor_mapped.data();
	for (int y = 0; y < wanted_height; ++y) {
		const int in_y = (y * image_height) / wanted_height;
		tensorflow::uint8* in_row = in + (in_y * image_width * image_channels);
		float* out_row = out + (y * wanted_width * wanted_channels);
		for (int x = 0; x < wanted_width; ++x) {
			const int in_x = (x * image_width) / wanted_width;
			tensorflow::uint8* in_pixel = in_row + (in_x * image_channels);
			float* out_pixel = out_row + (x * wanted_channels);
			for (int c = 0; c < wanted_channels; ++c) {
				out_pixel[c] = (in_pixel[c] - input_mean) / input_std;
			}
		}
	}

	result << " - " << label_strings_.size() << ", " << label_strings_[0] << " - " << image_width << "x" << image_height;

	uint32_t start = SDL_GetTicks();

	std::string input_layer = "input";
	std::string output_layer = "output";
	std::vector<tensorflow::Tensor> outputs;
	tensorflow::Status run_status = session->Run({ { input_layer, image_tensor } },
	{ output_layer }, {}, &outputs);
	if (!run_status.ok()) {
		result << "Running model failed: " << run_status;
		tensorflow::LogAllRegisteredKernels();
		return result.str();
	}

	uint32_t end = SDL_GetTicks();

	tensorflow::string status_string = run_status.ToString();
	result << " - " << status_string << "\n";

	tensorflow::Tensor* output = &outputs[0];
	const int kNumResults = 5;
	const float kThreshold = 0.1f;
	std::vector<std::pair<float, int> > top_results;
	GetTopN(output->flat<float>(), kNumResults, kThreshold, &top_results);

	std::stringstream ss;
	ss.precision(3);
	for (const auto& result : top_results) {
		const float confidence = result.first;
		const int index = result.second;

		ss << " - " << index << " " << confidence << "  ";

		// Write out the result as a string
		if (index < label_strings_.size()) {
			// just for safety: theoretically, the output is under 1000 unless there
			// is some numerical issues leading to a wrong prediction.
			ss << label_strings_[index];
		}
		else {
			ss << "Prediction: " << index;
		}

		ss << "\n";
	}


	LOG(INFO) << "Predictions: " << ss.str();

	tensorflow::string predictions = ss.str();
	result << predictions;
	result << " - use " << end - start << " ms";

	{
		tsurface_2_mat_lock lock(surf);
		cv::cvtColor(lock.mat, lock.mat, cv::COLOR_RGBA2BGRA);
	}

	return result.str();
}

void thome::example_inception5h()
{
	surface surf = image::get_image("misc/grace_hopper.png");
	std::string result = example_inception5h_internal();

	blits_.clear();
	surface text_surf = font::get_rendered_text(result, paper_->get_width(), font::SIZE_DEFAULT, font::BLACK_COLOR);
	blits_.push_back(image::tblit(text_surf, (paper_->get_width() - text_surf->w) / 2, 0, text_surf->w, text_surf->h));
	blits_.push_back(image::tblit(surf, paper_->get_width() >= surf->w? (paper_->get_width() - surf->w) / 2: 0, text_surf->h, surf->w, surf->h));
	did_draw_paper(*paper_, paper_->get_draw_rect(), false);
}

void thome::example_classifier()
{
	blits_.clear();
	did_draw_paper(*paper_, paper_->get_draw_rect(), false);
}

// Converts an encoded location to an actual box placement with the provided
// box priors.
void DecodeLocation(const float* encoded_location, const float* box_priors, float* decoded_location) 
{
	bool non_zero = false;
	for (int i = 0; i < 4; ++i) {
		const float curr_encoding = encoded_location[i];
		non_zero = non_zero || curr_encoding != 0.0f;

		const float mean = box_priors[i * 2];
		const float std_dev = box_priors[i * 2 + 1];

		float currentLocation = curr_encoding * std_dev + mean;

		currentLocation = std::max(currentLocation, 0.0f);
		currentLocation = std::min(currentLocation, 1.0f);
		decoded_location[i] = currentLocation;
	}

	if (!non_zero) {
		LOG(WARNING) << "No non-zero encodings; check log for inference errors.";
	}
}

float DecodeScore(float encoded_score) 
{ 
	return 1 / (1 + exp(-encoded_score)); 
}

static void cv_draw_rectangle(cv::RNG& rng, const cv::Mat& img, const cv::Rect& rect)
{
	cv::rectangle(img, rect.tl(), rect.br(), cv::Scalar(rng.uniform(0, 255), rng.uniform(0, 255), rng.uniform(0, 255), 255));
}

static void cv_draw_rectangle2(const cv::Mat& img, const cv::Rect& rect, const uint32_t color)
{
	cv::rectangle(img, rect.tl(), rect.br(), cv::Scalar(color & 0xff, (color & 0xff00) >> 8, (color & 0xff0000) >> 16, (color & 0xff000000) >> 24));
}

std::string thome::example_detector_internal(surface& surf)
{
	std::stringstream result;

	const std::string data_path = game_config::path + "/" + game_config::generate_app_dir(game_config::app) + "/tensorflow";
	const std::string pb_path = data_path + "/mobile_multibox_v1a/multibox_model.pb";

	tensorflow::Status s = tensorflow2::load_model(pb_path, current_session_);
	if (!s.ok()) {
		result << "load model fail: " << s;
		return result.str();
	}
	std::unique_ptr<tensorflow::Session>& session = current_session_.second;

	int32_t num_detections = 5;
	int32_t num_boxes = 784;
	if (locations_.empty()) {
		const std::string imagenet_comp_path = data_path + "/mobile_multibox_v1a/multibox_location_priors.txt";

		tfile file(imagenet_comp_path, GENERIC_READ, OPEN_EXISTING);
		VALIDATE(file.valid(), null_str);
		int64_t fsize = file.read_2_data();
		int start = gui2::twidget::npos;
		const char* ptr = nullptr;
		for (int at = 0; at < fsize; at ++) {
			const char ch = file.data[at];
			if (ch == '\r' || ch == '\n') {
				if (start != gui2::twidget::npos) {
					std::string line(file.data + start, at - start);
					std::vector<float> tokens;
					CHECK(tensorflow::str_util::SplitAndParseAsFloats(line, ',', &tokens));
					for (std::vector<float>::const_iterator it = tokens.begin(); it != tokens.end(); ++ it) {
						locations_.push_back(*it);
					}

					start = gui2::twidget::npos;
				}
			} else if (start == gui2::twidget::npos) {
				start = at;
			}
		}
		if (start != gui2::twidget::npos) {
			std::string line(file.data + start, fsize - start);
			std::vector<float> tokens;
			CHECK(tensorflow::str_util::SplitAndParseAsFloats(line, ',', &tokens));
			for (std::vector<float>::const_iterator it = tokens.begin(); it != tokens.end(); ++ it) {
				locations_.push_back(*it);
			}
		}
	}
	CHECK_EQ(locations_.size(), num_boxes * 8);

	// Read the Grace Hopper image.
	{
		tsurface_2_mat_lock lock(surf);
		cv::cvtColor(lock.mat, lock.mat, cv::COLOR_BGRA2RGBA);
	}

	int image_width = surf->w;
	int image_height = surf->h;
	int image_channels = 4;
	const int wanted_width = 224;
	const int wanted_height = 224;
	const int wanted_channels = 3;
	const float input_mean = 128.0f;
	const float input_std = 128.0f;
	VALIDATE(image_channels >= wanted_channels, null_str);
	tensorflow::Tensor image_tensor(
		tensorflow::DT_FLOAT,
		tensorflow::TensorShape({
		1, wanted_height, wanted_width, wanted_channels }));
	auto image_tensor_mapped = image_tensor.tensor<float, 4>();

	surface_lock dst_lock(surf);
	tensorflow::uint8* in = (uint8_t*)(dst_lock.pixels());
	// tensorflow::uint8* in_end = (in + (image_height * image_width * image_channels));
	float* out = image_tensor_mapped.data();
	for (int y = 0; y < wanted_height; ++y) {
		const int in_y = (y * image_height) / wanted_height;
		tensorflow::uint8* in_row = in + (in_y * image_width * image_channels);
		float* out_row = out + (y * wanted_width * wanted_channels);
		for (int x = 0; x < wanted_width; ++x) {
			const int in_x = (x * image_width) / wanted_width;
			tensorflow::uint8* in_pixel = in_row + (in_x * image_channels);
			float* out_pixel = out_row + (x * wanted_channels);
			for (int c = 0; c < wanted_channels; ++c) {
				out_pixel[c] = (in_pixel[c] - input_mean) / input_std;
			}
		}
	}

	result << " - " << locations_.size() << ", " << image_width << "x" << image_height;

	uint32_t start = SDL_GetTicks();

	std::string input_layer = "ResizeBilinear";
	std::string output_score_layer = "output_scores/Reshape";
	std::string output_location_layer = "output_locations/Reshape";
	std::vector<tensorflow::Tensor> outputs;
	tensorflow::Status run_status = session->Run({ {input_layer, image_tensor} },
		{output_score_layer, output_location_layer}, {}, &outputs);

	if (!run_status.ok()) {
		result << "Running model failed: " << run_status;
		tensorflow::LogAllRegisteredKernels();
		return result.str();
	}

	uint32_t end = SDL_GetTicks();

	tensorflow::string status_string = run_status.ToString();
	result << " - " << status_string << "\n";

	tensorflow::Tensor& scores = outputs[0];
	const tensorflow::TTypes<float>::Flat scores_flat = scores.flat<float>();
	const int how_many_labels = scores_flat.size();

	const tensorflow::Tensor& encoded_locations = outputs[1];
	auto locations_encoded = encoded_locations.flat<float>();

	tsurface_2_mat_lock lock(surf);
	cv::cvtColor(lock.mat, lock.mat, cv::COLOR_RGBA2BGRA);

	threading::lock variable_lock(variable_mutex_);
	rects_.clear();
	const float kThreshold = 0.1f;
	for (int pos = 0; pos < how_many_labels; ++pos) {
		const float score = DecodeScore(scores_flat(pos));

		if (score < kThreshold) {
			continue;
		}

		float decoded_location[4];
		DecodeLocation(&locations_encoded(pos * 4), &locations_[pos * 8], decoded_location);

		float left = decoded_location[0] * image_width;
		float top = decoded_location[1] * image_height;
		float right = decoded_location[2] * image_width;
		float bottom = decoded_location[3] * image_height;

		rects_.push_back(std::make_pair(score, SDL_Rect({(int)left, (int)top, (int)(right - left), (int)(bottom - top)})));
	}

	result << " - use " << end - start << " ms";

	return result.str();
}

void thome::example_detector()
{
	blits_.clear();
	did_draw_paper(*paper_, paper_->get_draw_rect(), false);
}

std::string thome::example_pr_internal(surface& surf)
{
/*
	surface surf = image::get_image("misc/test.png");
	surf = scale_surface(surf, surf1->w, surf1->h);
*/
	tsurface_2_mat_lock lock(surf);
	cv::Mat src = lock.mat.clone();
	cv::cvtColor(src, src, cv::COLOR_BGRA2BGR);

	uint32_t start = SDL_GetTicks();

	easypr::CPlateRecognize pr;
	pr.setLifemode(true);
	pr.setDebug(false);
	// pr.setMaxPlates(1);
	// pr.setDetectType(easypr::PR_DETECT_COLOR | easypr::PR_DETECT_SOBEL);
	pr.setDetectType(easypr::PR_DETECT_CMSER);

	//vector<string> plateVec;
	std::vector<easypr::CPlate> plateVec;

	std::stringstream result_ss;
	int result = pr.plateRecognize(src, plateVec);

	threading::lock variable_lock(variable_mutex_);
	rects_.clear();

	if (result == 0) {
		size_t num = plateVec.size();
		for (std::vector<easypr::CPlate>::const_iterator it = plateVec.begin(); it != plateVec.end(); ++ it) {
			const easypr::CPlate& plate = *it;
			result_ss << " - " << conv_ansi_utf8_2(plate.getPlateStr(), true) << "\n";
			SDL_Rect rect = {(int)(plate.getPlateLeftPoint().x / plate.getPlateScale()), (int)(plate.getPlateLeftPoint().y / plate.getPlateScale()), 
				(int)(plate.getPlateRightPoint().x / plate.getPlateScale()), (int)(plate.getPlateRightPoint().y / plate.getPlateScale())};
			rects_.push_back(std::make_pair(plate.getPlateScore(), rect));
		}
	}

	uint32_t end = SDL_GetTicks();

	result_ss << " - use " << end - start << " ms";
	return result_ss.str();
}

void thome::example_pr()
{
	blits_.clear();
	did_draw_paper(*paper_, paper_->get_draw_rect(), false);
}

void thome::stop_avcapture()
{
	tsetting_lock setting_lock(*this);
	threading::lock lock(recognition_mutex_);

	avcapture_.reset();
	paper_->set_timer_interval(0);

	current_surf_ = std::make_pair(false, surface());
	result_.clear();
	rects_.clear();
}

void thome::avcapture_switch_scenario(const std::string& name)
{
	posix_print("%i, avcapture_switch_scenario(1)------name: %s\n", SDL_GetTicks(), name.empty()? "<nil>": name.c_str());

	tsetting_lock setting_lock(*this);
	threading::lock lock(recognition_mutex_);

	if (!avcapture_.get()) {
		posix_print("%i, avcapture_switch_scenario(2), new\n", SDL_GetTicks());
		avcapture_.reset(new tavcapture());
		paper_->set_timer_interval(30);
	} else {
		posix_print("%i, avcapture_switch_scenario(2), reuse\n", SDL_GetTicks());
	}

	current_surf_ = std::make_pair(false, surface());
	result_.clear();
	rects_.clear();

	if (!name.empty()) {
		tensorflow::Status s = tensorflow2::load_model(name, current_session_);
		VALIDATE(s.ok(), null_str);
	}
}

void thome::did_example_item_changed(ttoggle_button& widget)
{
	current_example_ = widget.cookie();
	if (current_example_ == mouse) {
		find_widget<tslider>(window_, "slider1", false).set_visible(twidget::INVISIBLE);
		find_widget<tslider>(window_, "slider2", false).set_visible(twidget::INVISIBLE);

		stop_avcapture();

		example_mouse();

	} else if (current_example_ == inception5h) {
		find_widget<tslider>(window_, "slider1", false).set_visible(twidget::INVISIBLE);
		find_widget<tslider>(window_, "slider2", false).set_visible(twidget::INVISIBLE);

		stop_avcapture();

		example_inception5h();

	} else if (current_example_ == classifier) {
		find_widget<tslider>(window_, "slider1", false).set_visible(twidget::INVISIBLE);
		find_widget<tslider>(window_, "slider2", false).set_visible(twidget::INVISIBLE);

		const std::string data_path = game_config::path + "/" + game_config::generate_app_dir(game_config::app) + "/tensorflow";
		const std::string pb_path = data_path + "/inception5h/tensorflow_inception_graph.pb";
		avcapture_switch_scenario(pb_path);

		if (!recognition_thread_running_) {
			thread_->Start();
		}

		example_classifier();

	} else if (current_example_ == detector) {
		find_widget<tslider>(window_, "slider1", false).set_visible(twidget::INVISIBLE);
		find_widget<tslider>(window_, "slider2", false).set_visible(twidget::INVISIBLE);

		const std::string data_path = game_config::path + "/" + game_config::generate_app_dir(game_config::app) + "/tensorflow";
		const std::string pb_path = data_path + "/mobile_multibox_v1a/multibox_model.pb";
		avcapture_switch_scenario(pb_path);

		if (!recognition_thread_running_) {
			thread_->Start();
		}

		example_detector();

	} else if (current_example_ == pr) {
		find_widget<tslider>(window_, "slider1", false).set_visible(twidget::INVISIBLE);
		find_widget<tslider>(window_, "slider2", false).set_visible(twidget::INVISIBLE);

		avcapture_switch_scenario(null_str);

		if (!recognition_thread_running_) {
			thread_->Start();
		}

		example_pr();

	}
}

SDL_Rect thome::app_did_draw_frame(bool remote, cv::Mat& frame, const SDL_Rect& draw_rect)
{
	if (current_example_ == classifier || current_example_ == detector || current_example_ == pr) {
		if (!current_surf_.first) {
			if (!current_surf_.second.get()) {
				current_surf_.second = create_neutral_surface(frame.cols, frame.rows);
			}
			memcpy(current_surf_.second->pixels, frame.data, frame.cols * frame.rows * 4); 
			current_surf_.first = true;
		}

	}

	return draw_rect;
}

void thome::did_left_button_down_paper(ttrack& widget, const tpoint& coordinate)
{
	last_coordinate_ = coordinate;
}

void thome::did_mouse_leave_paper(ttrack& widget, const tpoint& first, const tpoint& last)
{
	if (is_null_coordinate(first)) {
		return;
	}
	
	if (current_example_ == mouse) {
		cv::Rect rect(first.x - widget.get_x(), first.y - widget.get_y(), last_coordinate_.x - first.x, last_coordinate_.y - first.y);
	
		tsurface_2_mat_lock lock(persist_surf_);
		cv_draw_rectangle(rng_, lock.mat, rect);
		did_draw_paper(*paper_, paper_->get_draw_rect(), false);
	}
}

void thome::did_mouse_motion_paper(ttrack& widget, const tpoint& first, const tpoint& last)
{
	if (is_null_coordinate(first)) {
		return;
	}

	last_coordinate_ = last;

	if (current_example_ == mouse) {
		cv::Rect rect(first.x - widget.get_x(), first.y - widget.get_y(), last.x - first.x, last.y - first.y);
		fill_surface(temperate_surf_, 0);
		tsurface_2_mat_lock lock(temperate_surf_);
		cv_draw_rectangle(rng_, lock.mat, rect);
		did_draw_paper(*paper_, paper_->get_draw_rect(), false);
	}
}

void thome::did_draw_paper(ttrack& widget, const SDL_Rect& draw_rect, const bool bg_drawn)
{
	SDL_Renderer* renderer = get_renderer();
	ttrack::tdraw_lock lock(renderer, widget);
	if (!bg_drawn) {
		SDL_RenderCopy(renderer, widget.background_texture().get(), nullptr, &draw_rect);
	}

	for (std::vector<image::tblit>::const_iterator it = blits_.begin(); it != blits_.end(); ++ it) {
		const image::tblit& blit = *it;
		image::render_blit(renderer, blit, widget.get_x(), widget.get_y());
	}

	if (avcapture_.get()) {
		surface text_surf;
		SDL_Rect dst;
		tavcapture::VideoRenderer* local_renderer = avcapture_->vrenderer(false);
		texture& local_tex = avcapture_->get_texture(false);

		bool require_render_local = local_renderer && local_renderer->dirty();

		if (require_render_local) {
			threading::lock lock(avcapture_->get_mutex(false));

			ttexture_2_mat_lock lock2(local_tex);
			dst = app_did_draw_frame(false, lock2.mat, draw_rect);

			if (local_renderer->dirty()) {
				SDL_UnlockTexture(local_tex.get());
			}

			SDL_RenderCopy(renderer, local_tex.get(), NULL, &dst);

			std::vector<std::pair<float, SDL_Rect> > rects;
			std::string result;
			{
				threading::lock lock(variable_mutex_);
				rects = rects_;
				result = result_;
			}
			char score_str[32];
			for (std::vector<std::pair<float, SDL_Rect> >::const_iterator it = rects.begin(); it != rects.end(); ++ it) {
				const SDL_Rect& rect = it->second;
				if (current_example_ != pr) {
					cv_draw_rectangle2(lock2.mat, cv::Rect(rect.x, rect.y, rect.w, rect.h), 0xffff0000);

				} else {
					cv::line(lock2.mat, cv::Point(rect.x, rect.y), cv::Point(rect.w, rect.h), cv::Scalar(0, 0, 0xff));
				}

				sprintf(score_str, "%0.2f", it->first);
				text_surf = font::get_rendered_text(score_str, 0, font::SIZE_DEFAULT, font::GOOD_COLOR);
				SDL_Rect score_dst{(int)(dst.x + 1.0 * rect.x * dst.w / lock2.mat.cols), (int)(dst.y + 1.0 * rect.y * dst.h / lock2.mat.rows), text_surf->w, text_surf->h};
				render_surface(renderer, text_surf, NULL, &score_dst);
			}

			if (!result.empty()) {
				text_surf = font::get_rendered_text(result, 0, font::SIZE_LARGER, font::GOOD_COLOR);
				dst.w = text_surf->w;
				dst.h = text_surf->h;
				render_surface(renderer, text_surf, NULL, &dst);
			}
		}
	}
}

void thome::did_slider_value_changed(tslider& widget, int value, int at)
{
	if (current_example_ == inception5h) {
		if (at != 0) {
			return;
		}
		// double alpha = 1.0 * value / 100;
	}
}

//
// ocr
//
void thome::pre_ocr(twindow& window)
{
	tgrid* ocr_layer = body_->layer(OCR_LAYER);

	if (fields_.empty()) {
		fields_.push_back(_("Name"));
		fields_.push_back(_("Mobile"));
		fields_.push_back(_("e-mail"));
		fields_.push_back(_("Address"));
	}

	std::stringstream ss;
	::utils::string_map symbols;

	tlistbox* list = find_widget<tlistbox>(ocr_layer, "fields", false, true);
	list->enable_select(false);
	std::map<std::string, std::string> data;
	std::map<std::string, tocr_result>::const_iterator find;
	for (std::vector<std::string>::const_iterator it = fields_.begin(); it != fields_.end(); ++ it) {
		data["id"] = *it;
		find = ocr_results_.find(*it);
		if (find != ocr_results_.end()) {
			ss.str("");
			ss << 1.0 * find->second.used_ms / 1000;
			symbols["count"] = ss.str();

			data["used_ms"] = vgettext2("$count seconds", symbols);
			data["message"] = find->second.chars;
		} else {
			data["used_ms"] = null_str;
			data["message"] = null_str;
		}
		list->insert_row(data);
	}

	tbutton* button = find_widget<tbutton>(ocr_layer, "ocr", false, true);
	connect_signal_mouse_left_click(
			*button
			, boost::bind(
				&thome::handle_ocr
				, this
				, boost::ref(window)));
}

void thome::handle_ocr(twindow& window)
{
	window_->set_retval(OCR);
}

//
// more
//
void thome::pre_more(twindow& window)
{
	tgrid* more_layer = body_->layer(MORE_LAYER);

	tbutton* button = find_widget<tbutton>(more_layer, "chat", false, true);
	button->set_icon("misc/chat.png");
	button->set_canvas_variable("value", variant(null_str));
	connect_signal_mouse_left_click(
			*button
			, boost::bind(
				&thome::handle_chat
				, this
				, boost::ref(window)));
}

void thome::handle_chat(twindow& window)
{
	window_->set_retval(CHAT);
}

} // namespace gui2

