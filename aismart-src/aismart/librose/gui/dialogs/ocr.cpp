#define GETTEXT_DOMAIN "rose-lib"

#include "gui/dialogs/ocr.hpp"

#include "gui/widgets/label.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/track.hpp"
#include "gui/widgets/window.hpp"
#include "gettext.hpp"
#include "rose_config.hpp"

#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/features2d/features2d.hpp>
#include "mser2.hpp"

#include <boost/bind.hpp>
#include <numeric>

#include "video.hpp"

namespace gui2 {

REGISTER_DIALOG(rose, ocr)

tocr::tocr(const surface& target, std::vector<std::unique_ptr<tocr_line> >& lines)
	: target_(target)
	, lines_(lines)
	, verbose_(false)
	, paper_(nullptr)
	, save_(nullptr)
	, current_surf_(std::make_pair(false, surface()))
	, blended_tex_(std::make_pair(false, texture()))
	, blended_tex_pixels_(nullptr)
	, setting_(false)
	, recognition_thread_running_(false)
	, original_local_offset_(0, 0)
	, current_local_offset_(0, 0)
#ifdef _WIN32
	, thumbnail_size_(640 / 2, 480 / 2)
#else
	, thumbnail_size_(480 / 2, 640 / 2)
#endif
{
	VALIDATE(lines.empty(), "lines must be empty");
}

tocr::~tocr()
{
	if (!target_.get()) {
		stop_avcapture();
	}
}

surface tocr::target(tpoint& offset) const
{
	VALIDATE(!lines_.empty(), null_str);
	surface target = target_;
	if (!target_.get()) {
		VALIDATE(camera_surf_.get(), null_str);
		target = camera_surf_;
	}
	int new_width = ceil(1.0 * target->w / 72) * 72;
	int new_height = ceil(1.0 * target->h / 72) * 72;
	surface ret = create_neutral_surface(new_width, new_height);

	offset.x = (new_width - target->w) / 2;
	offset.y = (new_height - target->h) / 2;

	fill_surface(ret, 0xff808080);
	SDL_Rect dst_rect = ::create_rect(offset.x, offset.y, target->w, target->h);
	sdl_blit(target, nullptr, ret, &dst_rect);
	return ret;
}

void tocr::pre_show(CVideo& video, twindow& window)
{
	window.set_label("misc/white-background.png");
	window.set_border_space(0, 0, 0, 0);

	paper_ = find_widget<ttrack>(&window, "paper", false, true);
	paper_->set_did_draw(boost::bind(&tocr::did_draw_paper, this, _1, _2, _3));
	paper_->set_did_mouse_leave(boost::bind(&tocr::did_mouse_leave_paper, this, _1, _2, _3));
	paper_->set_did_mouse_motion(boost::bind(&tocr::did_mouse_motion_paper, this, _1, _2, _3));

	save_ = find_widget<tbutton>(&window, "save", false, true);
	connect_signal_mouse_left_click(
			  *save_
			, boost::bind(
				&tocr::save
				, this, boost::ref(window)));
	save_->set_active(false);

	if (!target_.get()) {
		start_avcapture();
	}
}

void tocr::post_show(twindow& window)
{
	if (!target_.get()) {
		stop_avcapture();
	}
}

const trect* point_in_conside_histogram(const std::set<trect>& conside_region, const SDL_Rect& next_char_allow_rect)
{
	for (std::set<trect>::const_iterator it = conside_region.begin(); it != conside_region.end(); ++ it) {
		const trect& rect = *it;

		if (rect.x == next_char_allow_rect.x && rect.x + rect.w <= next_char_allow_rect.x + next_char_allow_rect.w && 
			rect.y >= next_char_allow_rect.y && rect.y + rect.h <= next_char_allow_rect.y + next_char_allow_rect.h) {
			return &rect;
		}
	}
	return nullptr;
}

bool rect_overlap_rect_set(const std::set<trect>& rect_set, const trect& rect)
{
	for (std::set<trect>::const_iterator it = rect_set.begin(); it != rect_set.end(); ++ it) {
		if (it->rect_overlap(rect)) {
			return true;
		}
	}
	return false;
}

cv::Mat mark_bboxes_2_mat(cv::Mat& src, const std::set<trect>& bboxes, bool clone)
{
	cv::Mat ret = clone? src.clone(): src;
	{
		return ret;
	}
	for (std::set<trect>::const_iterator it = bboxes.begin(); it != bboxes.end(); ++ it) {
		const trect& rect = *it;
		cv::Scalar color(0, 0, 255, 255);
		cv::rectangle(ret, cv::Rect(rect.x, rect.y, rect.w, rect.h), color, cv::FILLED);
	}
	return ret;
}

void generate_mark_line_png(const std::vector<std::unique_ptr<tocr_line> >& lines, surface& src_surf, const std::string& file)
{
	surface surf = !file.empty()? clone_surface(src_surf): src_surf;

	tsurface_2_mat_lock lock(surf);

	// mark line color
	std::vector<cv::Scalar> line_colors;
	line_colors.push_back(cv::Scalar(0, 0, 0, 255));
	line_colors.push_back(cv::Scalar(255, 255, 255, 255));
	line_colors.push_back(cv::Scalar(0, 0, 255, 255));
	line_colors.push_back(cv::Scalar(0, 255, 0, 255));
	line_colors.push_back(cv::Scalar(255, 0, 0, 255));
	line_colors.push_back(cv::Scalar(0, 255, 255, 255)); // yellow
	line_colors.push_back(cv::Scalar(255, 255, 0, 255)); // cyan
	line_colors.push_back(cv::Scalar(255, 0, 255, 255)); // deep red
	line_colors.push_back(cv::Scalar(119, 119, 119, 255)); // gray
/*
	line_colors.push_back(cv::Scalar(48, 59, 255, 255));
	line_colors.push_back(cv::Scalar(105, 215, 83, 255));
	line_colors.push_back(cv::Scalar(0, 128, 128, 255));
	line_colors.push_back(cv::Scalar(128, 128, 0, 255));
	line_colors.push_back(cv::Scalar(128, 0, 128, 255));
*/
	int at = 0;
	for (std::vector<std::unique_ptr<tocr_line> >::const_iterator it = lines.begin(); it != lines.end(); ++ it, at ++) {
		const tocr_line& line = *it->get();
		cv::Scalar color = line_colors[at % line_colors.size()];
		for (std::set<trect>::const_iterator it2 = line.chars.begin(); it2 != line.chars.end(); ++ it2) {
			const trect& rect = *it2;
			// cv::rectangle(lock.mat, cv::Rect(rect.x, rect.y, rect.w, rect.h), color, cv::FILLED);
			cv::rectangle(lock.mat, cv::Rect(rect.x, rect.y, rect.w, rect.h), color);
		}
		cv::rectangle(lock.mat, cv::Rect(line.bounding_rect.x, line.bounding_rect.y, line.bounding_rect.w, line.bounding_rect.h), cv::Scalar(255, 255, 255, 255));
	}

	if (!file.empty()) {
		save_surface_to_file(surf, file);
	}
}

SDL_Rect calculate_min_max_foreground_pixel(const cv::Mat& src, uint8_t* gray_per_row_ptr, uint8_t* gray_per_col_ptr, int min_foreground_pixels)
{
	SDL_Rect ret{INT_MAX, INT_MAX, -1, -1};
	VALIDATE(src.channels() == 1, null_str);
	bool row_valided = false;
	for (int row = 0; row < src.rows; row ++) {
		const uint8_t* data = src.ptr<uint8_t>(row);
		for (int col = 0; col < src.cols; col ++) {
			int gray_value = data[col];
			if (gray_value == 0) {
				if (col < ret.x) {
					ret.x = col;
				}
				if (col > ret.w) {
					ret.w = col;
				}
				if (row < ret.y) {
					ret.y = row;
				}
				if (row > ret.h) {
					ret.h = row;
				}
				gray_per_row_ptr[row] ++;
				gray_per_col_ptr[col] ++;
			}
		}
		if (!row_valided && ret.x != INT_MAX) {
			if (gray_per_row_ptr[row] >= min_foreground_pixels) {
				row_valided = true;
			} else {
				ret = ::create_rect(INT_MAX, INT_MAX, -1, -1);
			}
		}
	}
	if (row_valided) {
		for (int row = ret.h; row >= ret.y; row --) {
			if (gray_per_row_ptr[row] >= min_foreground_pixels) {
				break;
			}
			ret.h = row - 1;
		}
		VALIDATE(ret.y <= ret.h, null_str);

	} else {
		VALIDATE(ret.x == INT_MAX && ret.y == INT_MAX && ret.w == -1 && ret.h == -1, null_str);
	}

	return ret;
}

SDL_Rect calculate_out_foreground_pixel(const cv::Mat& src, const SDL_Rect& base_rect, uint8_t* gray_per_row_ptr, uint8_t* gray_per_col_ptr, int min_foreground_pixels)
{
	VALIDATE(src.channels() == 1, null_str);
	for (int row = 0; row < src.rows; row ++) {
		const uint8_t* data = src.ptr<uint8_t>(row);
		for (int col = 0; col < src.cols; col ++) {
			int gray_value = data[col];
			if (gray_value == 0) {
				gray_per_row_ptr[row] ++;
				gray_per_col_ptr[col] ++;
			}
		}
	}

	SDL_Rect ret {base_rect.x, base_rect.y, base_rect.x + base_rect.w - 1, base_rect.y + base_rect.h - 1};
	// x-axis
	if (base_rect.x > 0) {
		// x: <---base_rect.x 
		for (ret.x = base_rect.x - 1; ret.x >= 0; ret.x --) {
			if (gray_per_col_ptr[ret.x] < min_foreground_pixels) {
				break;
			}
		}
		ret.x += 1;
	}
	if (base_rect.x + base_rect.w != src.cols) {
		// w: base_rect.x + base_rect.w --->
		for (ret.w = base_rect.x + base_rect.w; ret.w < src.cols; ret.w ++) {
			if (gray_per_col_ptr[ret.w] < min_foreground_pixels) {
				break;
			}
		}
		ret.w -= 1;
	}
	
	// y-axis
	if (base_rect.y > 0) {
		// y: <---base_rect.y
		for (ret.y = base_rect.y - 1; ret.y >= 0; ret.y --) {
			if (gray_per_row_ptr[ret.y] < min_foreground_pixels) {
				break;
			}
		}
		ret.y += 1;
	}
	if (base_rect.y + base_rect.h != src.rows) {
		// h: base_rect.y + base_rect.h --->
		for (ret.h = base_rect.y + base_rect.h; ret.h < src.rows; ret.h ++) {
			if (gray_per_row_ptr[ret.h] < min_foreground_pixels) {
				break;
			}
		}
		ret.h -= 1;
	}

	if (ret.x < 0) {
		int ii = 0;
	}

	ret.w = ret.w - ret.x + 1;
	ret.h = ret.h - ret.y + 1;

	return ret;
}

std::string join_file_png(int round, int step, const std::string& short_name)
{
	std::stringstream ss;
	ss << round << "-" << step << "-" << short_name;
	return game_config::preferences_dir + "/" + ss.str();
}

// @src[IN]: pixel maybe changed during function. 
bool locate_character(const cv::Mat& src, const SDL_Rect& base_rect, const int min_estimate_width, bool right, SDL_Rect& result_rect, bool verbose, int step)
{
	if (SDL_RectEmpty(&base_rect)) {
		VALIDATE(min_estimate_width > 0, null_str);
	} else {
		// from base_rect will ignore min_estimate_width, right.
		VALIDATE(base_rect.x >= 0 && base_rect.y >= 0 && 
			base_rect.x + base_rect.w <= src.cols && base_rect.y + base_rect.h <= src.rows, null_str);
		VALIDATE(min_estimate_width == -1, null_str);
	}
	result_rect.x = result_rect.y = 0;
	result_rect.w = src.cols;
	result_rect.h = src.rows;

	const int min_threshold_width = 2;

	cv::Mat tmp, clip_origin, clip_threshold, clip_isolated, clip_first_cut, clip_hsv_closed, clip_hsv_contour;
	cv::threshold(src, tmp, 0, 255, CV_THRESH_BINARY | CV_THRESH_OTSU);

	if (verbose) {
		clip_origin = src.clone();
		clip_threshold = tmp.clone();
	}

	erase_isolated_pixel(tmp, 0, 255);

	if (verbose) {
		clip_isolated = tmp.clone();
	}

	std::unique_ptr<uint8_t> gray_per_col(new uint8_t[src.cols]);
	uint8_t* gray_per_col_ptr = gray_per_col.get();
	memset(gray_per_col_ptr, 0, src.cols);

	std::unique_ptr<uint8_t> gray_per_row(new uint8_t[src.rows]);
	uint8_t* gray_per_row_ptr = gray_per_row.get();
	memset(gray_per_row_ptr, 0, src.rows);

	SDL_Rect threshold_rect = empty_rect;
	if (SDL_RectEmpty(&base_rect)) {
		threshold_rect = calculate_min_max_foreground_pixel(tmp, gray_per_row_ptr, gray_per_col_ptr, 1);
		if (verbose) {
			int ii = 0;
		}
		if (threshold_rect.x != INT_MAX) {
			if (right) {
				// first col maybe noise.
				for (int col = threshold_rect.x + 1; col < threshold_rect.w; col ++) {
					if (gray_per_col_ptr[col] > 0) {
						break;
					} else {
						threshold_rect.x = col + 1;
					}
				}

				for (int col = threshold_rect.x + min_estimate_width; col < threshold_rect.w; col ++) {
					if (gray_per_col_ptr[col] < 2) {
						threshold_rect.w = col - 1;
						break;
					}
				}
			} else {
				for (int col = threshold_rect.w - min_estimate_width; col >= threshold_rect.x; col --) {
					if (gray_per_col_ptr[col] < 2) {
						threshold_rect.x = col + 1;
						break;
					}
				}
			}
			VALIDATE(threshold_rect.w >= threshold_rect.x, null_str);

			threshold_rect.w = threshold_rect.w - threshold_rect.x + 1;

		} else {
			// all pixel in (0, 0, tmp.cols, tmp.rows) is background.
			result_rect.x = 0;
			result_rect.y = 0;
			result_rect.w = tmp.cols;
			result_rect.h = tmp.rows;
			return false;
		}
	} else {
		threshold_rect = calculate_out_foreground_pixel(tmp, base_rect, gray_per_row_ptr, gray_per_col_ptr, 2);
		VALIDATE(threshold_rect.x <= base_rect.x && threshold_rect.y <= base_rect.y && 
			threshold_rect.w >= base_rect.w && threshold_rect.h >= base_rect.h, null_str);
	}

	VALIDATE(threshold_rect.w <= tmp.cols, null_str);
	if (threshold_rect.w != tmp.cols) {
		VALIDATE(src.cols >= threshold_rect.x + threshold_rect.w, null_str);
		result_rect.x += threshold_rect.x;
		result_rect.w = threshold_rect.w;

		cv::Rect clip(threshold_rect.x, 0, threshold_rect.w, tmp.rows);
		VALIDATE(clip.width <= src.cols, null_str);
		tmp = tmp(clip).clone();
	}

	if (tmp.cols < min_threshold_width) {
		VALIDATE(tmp.cols > 0, null_str);
		// on android, when tmp.cols == 1, morphologyEx will result to:
		// heap corruption detected by dlmalloc_read
		return false;
	}

	if (verbose) {
		clip_first_cut = tmp.clone();
	}

	cv::Mat element = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
	// it is white backgraound/black foreground.
	cv::morphologyEx(tmp, clip_hsv_closed, cv::MORPH_OPEN, element);


	VALIDATE(clip_hsv_closed.rows <= src.rows && clip_hsv_closed.cols <= src.cols, null_str);
	memset(gray_per_row_ptr, 0, clip_hsv_closed.rows);
	memset(gray_per_col_ptr, 0, clip_hsv_closed.cols);

	SDL_Rect contour_rect = empty_rect;
	if (SDL_RectEmpty(&base_rect)) {
		// second calculate_min_max_foreground_pixel, get this char's min rectangle.
		if (verbose) {
			int ii = 0;
		}
		contour_rect = calculate_min_max_foreground_pixel(clip_hsv_closed, gray_per_row_ptr, gray_per_col_ptr, 2);				
		if (contour_rect.x != INT_MAX) {
			VALIDATE(contour_rect.x != INT_MAX && contour_rect.w != -1 && contour_rect.h != -1, null_str);
			if (verbose) {
				int ii = 0;
			}
			// first row maybe noise.
			for (int row = contour_rect.y + 1; row < contour_rect.h; row ++) {
				if (gray_per_row_ptr[row] > 0) {
					break;
				} else {
					contour_rect.y = row + 1;
				}
			}
			// last row maybe noise
			for (int row = contour_rect.h - 1; row > contour_rect.y; row --) {
				if (gray_per_row_ptr[row] > 0) {
					break;
				} else {
					contour_rect.h = row - 1;
				}
			}
			VALIDATE(contour_rect.h >= contour_rect.y, null_str);

		} else {
			contour_rect = ::create_rect(0, 0, clip_hsv_closed.cols - 1, clip_hsv_closed.rows - 1);
		}
		contour_rect.w = contour_rect.w - contour_rect.x + 1;
		contour_rect.h = contour_rect.h - contour_rect.y + 1;

	} else {
		SDL_Rect base_rect2 {base_rect.x - threshold_rect.x, base_rect.y, base_rect.w, base_rect.h};
		contour_rect = calculate_out_foreground_pixel(clip_hsv_closed, base_rect2, gray_per_row_ptr, gray_per_col_ptr, 2);

		VALIDATE(contour_rect.x <= base_rect2.x && contour_rect.y <= base_rect2.y && 
			contour_rect.w >= base_rect2.w && contour_rect.h >= base_rect2.h, null_str);
		VALIDATE(contour_rect.x + contour_rect.w <= clip_hsv_closed.cols && contour_rect.y + contour_rect.h <= clip_hsv_closed.rows, null_str);
	}

	const int min_char_height = 4;
	if (contour_rect.h < min_char_height) {
		// on android, when tmp.cols == 1, morphologyEx will result to:
		// heap corruption detected by dlmalloc_read
		result_rect.x = 0;
		result_rect.y = 0;
		result_rect.w = src.cols;
		result_rect.h = src.rows;
		return false;
	}

	clip_hsv_contour = clip_hsv_closed(cv::Rect(contour_rect.x, contour_rect.y, contour_rect.w, contour_rect.h)).clone();

	result_rect.x += contour_rect.x;
	result_rect.y += contour_rect.y;
	result_rect.w = contour_rect.w;
	result_rect.h = contour_rect.h;

	if (verbose) {
		cv::cvtColor(clip_origin, clip_origin, cv::COLOR_GRAY2BGR);
		cv::cvtColor(clip_threshold, clip_threshold, cv::COLOR_GRAY2BGR);
		cv::cvtColor(clip_isolated, clip_isolated, cv::COLOR_GRAY2BGR);
		cv::cvtColor(clip_first_cut, clip_first_cut, cv::COLOR_GRAY2BGR);
		cv::cvtColor(clip_hsv_closed, clip_hsv_closed, cv::COLOR_GRAY2BGR);
		cv::cvtColor(clip_hsv_contour, clip_hsv_contour, cv::COLOR_GRAY2BGR);

		save_surface_to_file(surface(clip_origin), join_file_png(0, step, "0-clip-origin.png"));
		save_surface_to_file(surface(clip_threshold), join_file_png(0, step, "1-clip-threshold.png"));
		save_surface_to_file(surface(clip_isolated), join_file_png(0, step, "2-clip-isolated.png"));
		save_surface_to_file(surface(clip_first_cut), join_file_png(0, step, "3-clip-first_cut.png"));
		save_surface_to_file(surface(clip_hsv_closed), join_file_png(0, step, "4-clip-closed.png"));
		save_surface_to_file(surface(clip_hsv_contour), join_file_png(0, step, "5-clip-contour.png"));
	}
	return true;
}

void grow_line(const surface& src_surf, bool verbose, const cv::Mat& gray, const std::vector<std::unique_ptr<tocr_line> >& lines, 
	const std::vector<trect>& line_first_chars, const std::vector<trect>& line_last_chars, 
	const int line_at, SDL_Rect& average_rect, const bool right)
{
	tocr_line& line = *lines[line_at].get();
	std::vector<trect> vchars;
	for (std::set<trect>::const_iterator it2 = line.chars.begin(); it2 != line.chars.end(); ++ it2) {
		const trect& rect = *it2;
		vchars.push_back(rect);
	}

	std::vector<int> maybe_same_line_left, maybe_same_line_right;

	for (int at = 0; at < (int)lines.size(); at ++) {
		if (at == line_at) {
			continue;
		}
		const trect& first_char = line_first_chars[at];
		const trect& this_first_char = *line.chars.begin();
		if (first_char.y < this_first_char.y + this_first_char.h && this_first_char.y < first_char.y + first_char.h) {
			if (at < line_at) {
				maybe_same_line_left.push_back(at);
			} else {
				maybe_same_line_right.push_back(at);
			}
		}
	}

	int hbonus_size = 1;
	int vbonus_size = 2;
	int next_seed_chat_at = 1;
	const int detect_width = average_rect.w * 3 / 2; // it will effect cv::threshold(...)
	// new char can use maybe_current_x pixel.
	int maybe_current_x = vchars[0].x + vchars[0].w + hbonus_size;
	if (!right) {
		maybe_current_x = vchars[0].x - hbonus_size - detect_width;
	}
	int inserted_at = 0;
	while (true) {
		if (right) {
			if (maybe_current_x >= gray.cols) {
				break;
			}
		}

		trect next_cut_rect(right? gray.cols: -1, 0, 0, 0);
		const int min_estimate_width = average_rect.w / 3;
		const int min_threshold_width = 2;
		if (right) {
			if (next_seed_chat_at < (int)vchars.size()) {
				next_cut_rect = vchars[next_seed_chat_at];
			} else if (!maybe_same_line_right.empty()) {
				const trect& next_line_first_char = line_first_chars[maybe_same_line_right[0]];
				next_cut_rect.x = next_line_first_char.x;
			}
			if (next_cut_rect.x - maybe_current_x < min_estimate_width) {
				maybe_current_x = next_cut_rect.x + next_cut_rect.w + hbonus_size;
				if (next_seed_chat_at < (int)vchars.size()) {
					next_seed_chat_at ++;
					continue;
				} else {
					// has reach to next line.
					break;
				}
			}

		} else {
			if (!maybe_same_line_left.empty()) {
				const trect& next_line_last_char = line_last_chars[maybe_same_line_left.back()];
				next_cut_rect.x = next_line_last_char.x + next_line_last_char.w - 1;
			}
			if (maybe_current_x - next_cut_rect.x < min_estimate_width) {
				// has reach to before line or 0.
				break;
			}
		}

		// grow to left/right
		cv::Rect clip_rect { maybe_current_x, average_rect.y - vbonus_size, 
			detect_width, average_rect.h + vbonus_size * 2};
		if (right) {
			if (clip_rect.x + clip_rect.width > next_cut_rect.x) {
				clip_rect.width = next_cut_rect.x - clip_rect.x;
				VALIDATE(clip_rect.width > 0, null_str);
			}
		} else {
			if (clip_rect.x <= next_cut_rect.x) {
				clip_rect.width -= next_cut_rect.x - clip_rect.x;
				VALIDATE(clip_rect.width > 0, null_str);
				clip_rect.x = next_cut_rect.x + 1;
			}
		}

		if (clip_rect.y < 0) {
			clip_rect.height -= 0 - clip_rect.y;
			VALIDATE(clip_rect.height > 0, null_str);
			clip_rect.y = 0;
		}
		if (clip_rect.y + clip_rect.height > gray.rows) {
			clip_rect.height = gray.rows - clip_rect.y;
			VALIDATE(clip_rect.height > 0, null_str);
		}

		VALIDATE(clip_rect.width > 0 && clip_rect.height > 0, null_str);

		SDL_Rect char_rect;
		bool has_char = locate_character(gray(clip_rect), empty_rect, min_estimate_width, right, char_rect, verbose && !right && line_at == 9 && inserted_at == 1, 6);
		VALIDATE(char_rect.w > 0, null_str);
		if (has_char) {
			clip_rect.x += char_rect.x;
			clip_rect.y += char_rect.y;

			SDL_Rect result_rect = char_rect;
			result_rect.x = clip_rect.x;
			result_rect.y = clip_rect.y;

			trect new_rect(result_rect);
			for (std::set<trect>::const_iterator it = line.chars.begin(); it != line.chars.end(); ++ it) {
				if (it->rect_overlap(new_rect)) {
					// save_surface_to_file(src_surf, game_config::preferences_dir + "/3.png");
				}
				VALIDATE(!it->rect_overlap(new_rect), null_str);
			}
			line.chars.insert(new_rect);
			if (right) {
				maybe_current_x = result_rect.x + result_rect.w + hbonus_size;
			} else {
				maybe_current_x = result_rect.x - hbonus_size - detect_width;
			}
			inserted_at ++;

		} else {
			// no character. but char_rect indicate require skip's rect.
			if (right) {
				clip_rect.x += char_rect.x;
				maybe_current_x = clip_rect.x + char_rect.w;
			} else {
				maybe_current_x = clip_rect.x - hbonus_size - detect_width;
			}
		}
	}
}

void grow_lines(const surface& src_surf, bool verbose_, const cv::Mat& gray, std::vector<std::unique_ptr<tocr_line> >& lines)
{
	std::vector<trect> line_first_chars, line_last_chars;

	for (std::vector<std::unique_ptr<tocr_line> >::iterator it = lines.begin(); it != lines.end(); ++ it) {
		tocr_line& line = *it->get();
		line_first_chars.push_back(*line.chars.begin());
		line_last_chars.push_back(*line.chars.rbegin());
	}

	// grow line.
	int line_at = 0;
	for (std::vector<std::unique_ptr<tocr_line> >::iterator it = lines.begin(); it != lines.end(); ++ it, line_at ++) {
		tocr_line& line = *it->get();
		SDL_Rect accumulate_rect {0, INT_MAX, 0, -1};
		std::vector<trect> vchars;
		for (std::set<trect>::const_iterator it2 = line.chars.begin(); it2 != line.chars.end(); ++ it2) {
			const trect& rect = *it2;
			vchars.push_back(rect);
			if (rect.y < accumulate_rect.y) {
				accumulate_rect.y = rect.y;
			}
			accumulate_rect.w += rect.w;
			if (rect.y + rect.h > accumulate_rect.h) {
				accumulate_rect.h = rect.y + rect.h;
			}
		}

		SDL_Rect average_rect {0, accumulate_rect.y, 
			accumulate_rect.w / (int)vchars.size(), accumulate_rect.h - accumulate_rect.y};

		grow_line(src_surf, verbose_, gray, lines, line_first_chars, line_last_chars, line_at, average_rect, true);
		line_last_chars[line_at] = *line.chars.rbegin();

		grow_line(src_surf, verbose_, gray, lines, line_first_chars, line_last_chars, line_at, average_rect, false);
		line_first_chars[line_at] = *line.chars.begin();

		line.calculate_bounding_rect();
	}
}

void erase_bboxes_base_lines(std::set<trect>& bboxes, const std::vector<std::unique_ptr<tocr_line> >& lines)
{
	const int count = lines.size();
	std::unique_ptr<SDL_Rect> bounding_rects(new SDL_Rect[count]);
	SDL_Rect* bounding_rects_ptr = bounding_rects.get();
	int at = 0;
	for (std::vector<std::unique_ptr<tocr_line> >::const_iterator it = lines.begin(); it != lines.end(); ++ it, at ++) {
		const tocr_line& line = *it->get();
		memcpy(bounding_rects_ptr + at, &line.bounding_rect, sizeof(SDL_Rect));
	}

	bool erase;
	for (std::set<trect>::iterator it = bboxes.begin(); it != bboxes.end(); ) {
		const trect& rect = *it;
		erase = false;
		for (int at = 0; at < count; at ++) {
			if (rect.rect_overlap2(bounding_rects_ptr[at])) {
				erase = true;
				break;
			}
		}
		if (erase) {
			bboxes.erase(it ++);
		} else {
			++ it;
		}
	}
}

void tocr::detect_and_blend_surf(surface& surf, std::vector<std::unique_ptr<tocr_line> >& lines)
{
	lines.clear();

	tsurface_2_mat_lock lock(surf);
	cv::Mat rgb, gray;
	cv::cvtColor(lock.mat, rgb, cv::COLOR_BGRA2BGR);
	cv::cvtColor(lock.mat, gray, cv::COLOR_BGRA2GRAY);
	const int image_area = gray.rows * gray.cols;
	const int delta = 1;
	const int min_area = 30;
	const double max_area_ratio = 0.05;

	std::vector<std::vector<cv::Point> > inv_msers, msers;
    std::vector<cv::Rect> inv_bboxes, bboxes;
/*
	cv::Ptr<cv::MSER> mser = cv::MSER::create(delta, min_area, int(max_area_ratio * image_area), 0.25, 0.2);
	mser->detectRegions(gray, msers, bboxes);
*/

	cv::Ptr<cv::MSER2> mser = cv::MSER2::create(delta, min_area, int(max_area_ratio * image_area));
	mser->detectRegions(gray, inv_msers, inv_bboxes, msers, bboxes);

	const int min_char_width = 8;
	const int min_char_height = 8;
	std::set<trect> bboxes2;
	for (std::vector<cv::Rect>::const_iterator it = bboxes.begin(); it != bboxes.end(); ++ it) {
		const cv::Rect& rect = *it;
		if (rect.width < min_char_width) {
			continue;
		}
		if (rect.height < min_char_height) {
			continue;
		}
		bboxes2.insert(trect(rect.x, rect.y, rect.width, rect.height));
	}

	std::map<tpoint, int> regions;

	std::map<tpoint, int>::iterator find;
	int max_value = 0;
	tpoint max_point(0, 0);
	for (std::set<trect>::const_iterator it = bboxes2.begin(); it != bboxes2.end(); ++ it) {
		const trect& rect = *it;
		find = regions.find(tpoint(rect.w, rect.h));
		if (find != regions.end()) {
			find->second ++;
			if (find->second > max_value) {
				max_value = find->second;
				max_point = tpoint(rect.w, rect.h);
			}
		} else {
			regions.insert(std::make_pair(tpoint(rect.w, rect.h), 1));
		}
	}

	int min_conside_width = max_point.x - 2;
	int max_conside_width = max_point.x + 2;
	int min_conside_height = max_point.y - 2;
	int max_conside_height = max_point.y + 2;

	std::set<trect> conside_region;
	const int conside_min_value = (int)(0.618 * max_value);
	for (std::set<trect>::const_iterator it = bboxes2.begin(); it != bboxes2.end(); ++ it) {
		const trect& rect = *it;
/*
		if (rect.w >= min_conside_width && rect.w <= max_conside_width && rect.h >= min_conside_height && rect.h >= max_conside_height) {
			VALIDATE(!conside_region.count(trect(rect.x, rect.y, rect.w, rect.h)), null_str);
			conside_region.insert(rect);
		}
*/
		find = regions.find(tpoint(rect.w, rect.h));
		VALIDATE(find != regions.end(), null_str);
		if (find != regions.end()) {
			if (find->second >= conside_min_value) {
				VALIDATE(!conside_region.count(trect(rect.x, rect.y, rect.w, rect.h)), null_str);
				conside_region.insert(rect);
			}
		}
	}

	if (verbose_) {
		// mser and conside png
		cv::Mat mser_mat = lock.mat.clone();
		cv::Mat conside_mat = lock.mat.clone();
		for (std::vector<cv::Rect>::const_iterator it = bboxes.begin(); it != bboxes.end(); ++ it) {
			const cv::Rect& rect = *it;
			if (conside_region.count(trect(rect.x, rect.y, rect.width, rect.height))) {
				cv::rectangle(conside_mat, rect, cv::Scalar(0, 0, 255, 255));
			}
			cv::rectangle(mser_mat, rect, cv::Scalar(255, 255, 255, 255));
		}
		save_surface_to_file(surface(mser_mat), join_file_png(0, 0, "mser.png"));
		save_surface_to_file(surface(conside_mat), join_file_png(0, 1, "conside.png"));

		// histogam png.
		int hscale = 16, vscale = 24;
		cv::Mat histogram_mat = cv::Mat::zeros(max_value * vscale, regions.size() * hscale, CV_8UC3);
		int at = 0;
		for (std::map<tpoint, int>::const_iterator it = regions.begin(); it != regions.end(); ++ it, at ++) {
			int height = it->second * vscale;
			cv::Scalar color(255, 255, 255);
			if (at) {
				if (at % 100 == 0) {
					color = cv::Scalar(255, 0, 0);
				} else if (at % 50 == 0) {
					color = cv::Scalar(0, 255, 0);
				} else if (at % 10 == 0) {
					color = cv::Scalar(0, 0, 255);
				} 
			}
			cv::rectangle(histogram_mat, cv::Rect(at * hscale, max_value * vscale - height, hscale, height), color);
		}
		cv::line(histogram_mat, cv::Point(0, (max_value - conside_min_value) * vscale), cv::Point(histogram_mat.cols - 1, (max_value - conside_min_value) * vscale), cv::Scalar(0, 0, 255));
		save_surface_to_file(surface(histogram_mat), join_file_png(0, 2, "histogram.png"));
	}


	std::set<trect> conside_rect;
	for (std::set<trect>::const_iterator it = conside_region.begin(); it != conside_region.end(); ++ it) {
		const trect& rect = *it;
		if (rect_overlap_rect_set(conside_rect, rect)) {
			continue;
		}
		int x = rect.x + rect.w;
		SDL_Rect accumulate_rect {rect.x, rect.y, rect.w, rect.h};
		SDL_Rect average_rect = accumulate_rect;

		std::set<trect> valid_chars;
		valid_chars.insert(trect(rect.x, rect.y, rect.w, rect.h));
		const int can_line_valid_chars = 3;
		const int min_char_gap = 2, max_char_gap = average_rect.w + average_rect.w / 1;
		cv::Rect new_rect;
		for (int bonus = min_char_gap; bonus < max_char_gap; bonus ++) {
			const int x2 = x + bonus;
			// const SDL_Rect next_char_allow_rect{x2, average_rect.y - average_rect.h / 4, average_rect.w * 3 / 2, average_rect.h * 3 / 2};
			const SDL_Rect next_char_allow_rect{x2, average_rect.y - average_rect.h / 5, average_rect.w * 3 / 2, average_rect.h * 3 / 2};
			const trect* new_rect = point_in_conside_histogram(conside_region, next_char_allow_rect);
			if (new_rect && !rect_overlap_rect_set(conside_rect, *new_rect)) {
				VALIDATE(!rect_overlap_rect_set(valid_chars, *new_rect), null_str);
				valid_chars.insert(*new_rect);
				bonus = min_char_gap; // find a valid char, reset relative variable.
				x = new_rect->x + new_rect->w;
				accumulate_rect.y += new_rect->y;
				accumulate_rect.w += new_rect->w;
				accumulate_rect.h += new_rect->h;
				average_rect.y = accumulate_rect.y / valid_chars.size();
				average_rect.w = accumulate_rect.w / valid_chars.size();
				average_rect.h = accumulate_rect.h / valid_chars.size();
			}
		}
		if (valid_chars.size() >= can_line_valid_chars) {
			conside_rect.insert(valid_chars.begin(), valid_chars.end());
			lines.push_back(std::unique_ptr<tocr_line>(new tocr_line(valid_chars)));
		}
	}

	if (verbose_) {
		generate_mark_line_png(lines, surf, join_file_png(0, 3, "line-strong-seed-1.png"));
	}

	// grid rid of rect in bboxes2
	int line_at = 0;
	for (std::vector<std::unique_ptr<tocr_line> >::iterator it = lines.begin(); it != lines.end(); ++ it, line_at ++) {
		tocr_line& line = *it->get();
		SDL_Rect accumulate_rect {0, 0, 0, 0};
		std::vector<trect> vchars;
		for (std::set<trect>::const_iterator it2 = line.chars.begin(); it2 != line.chars.end(); ++ it2) {
			const trect& rect = *it2;
			vchars.push_back(rect);
			accumulate_rect.y += rect.y;
			accumulate_rect.w += rect.w;
			accumulate_rect.h += rect.h;
		}

		const std::set<trect> chars2 = line.chars;

		line.chars.clear();

		SDL_Rect average_rect {0, accumulate_rect.y / (int)vchars.size(), 
			accumulate_rect.w / (int)vchars.size(), accumulate_rect.h / (int)vchars.size()};
		int bonus_size = 1;
		int next_min_start_x = 0;
		int char_at = 0;
		const int min_bonus = 2;
		for (std::vector<trect>::iterator it2 = vchars.begin(); it2 != vchars.end(); ++ it2, char_at ++) {
			trect& rect = *it2;
			cv::Rect clip_rect;
			// x
			clip_rect.x = rect.x - rect.w / 4;
			if (clip_rect.x < next_min_start_x) {
				clip_rect.x = next_min_start_x;
			} else if (clip_rect.x < 0) {
				clip_rect.x = 0;
			}
			// width
			clip_rect.width = rect.x - clip_rect.x + rect.w * 5 / 4;
			const int next_start_x = char_at == (int)(vchars.size() - 1)? gray.cols: vchars[char_at + 1].x; 
			if (clip_rect.x + clip_rect.width > next_start_x) {
				clip_rect.width = next_start_x - clip_rect.x;
			}
			VALIDATE(clip_rect.width > 0, null_str);

			// y
			clip_rect.y = average_rect.y - average_rect.h / 4;
			if (clip_rect.y > rect.y) {
				clip_rect.y = rect.y - min_bonus;
			}
			if (clip_rect.y < 0) {
				clip_rect.y = 0;
			}
			// height
			clip_rect.height = rect.y - clip_rect.y + (rect.h < average_rect.h? average_rect.h * 5 / 4: rect.h + (rect.y - clip_rect.y));
			if (clip_rect.y + clip_rect.height > gray.rows) {
				clip_rect.height = gray.rows - clip_rect.y;
			}
			VALIDATE(clip_rect.height >= rect.h, null_str);

			const SDL_Rect base_rect {rect.x - clip_rect.x, rect.y - clip_rect.y, rect.w, rect.h};
			SDL_Rect char_rect;
			bool has_char = locate_character(gray(clip_rect), base_rect, -1, true, char_rect, verbose_ && line_at == 12 && char_at == 1, 4);
			VALIDATE(has_char, null_str);

			char_rect.x += clip_rect.x;
			char_rect.y += clip_rect.y;

			accumulate_rect.y += char_rect.y - rect.y;
			accumulate_rect.w += char_rect.w - rect.w;
			accumulate_rect.h += char_rect.h - rect.h;
			average_rect.y = accumulate_rect.y / (int)vchars.size();
			average_rect.h = accumulate_rect.h / (int)vchars.size();

			rect = char_rect;

			next_min_start_x = rect.x + rect.w;
			line.chars.insert(rect);		
		}
	}

	if (verbose_) {
		generate_mark_line_png(lines, surf, join_file_png(0, 4, "line-strong-seed-2.png"));
	}

	grow_lines(surf, verbose_, gray, lines);

	if (verbose_) {
		generate_mark_line_png(lines, surf, join_file_png(0, 5, "line-grown.png"));
	}

	erase_bboxes_base_lines(bboxes2, lines);
}

void tocr::texecutor::DoWork()
{
	ocr_.recognition_thread_running_ = true;
	while (ocr_.avcapture_.get()) {
		if (ocr_.setting_) {
			// main thread is setting.
			// seting action may be risk recognition_mutex_.
			SDL_Delay(10);
			continue;
		}
		threading::lock lock(ocr_.recognition_mutex_);
		if (!ocr_.avcapture_.get()) {
			continue;
		}
		if (!ocr_.current_surf_.first) {
			SDL_Delay(10);
			continue;
		}

		std::vector<std::unique_ptr<tocr_line> > lines;
		ocr_.detect_and_blend_surf(ocr_.current_surf_.second, lines);

		{
			threading::lock lock(ocr_.variable_mutex_);
			ocr_.current_surf_.first = false;

			if (ocr_.avcapture_.get() && !lines.empty()) {
				// user maybe want last look ocr_.lines_
				surface& src_surf = ocr_.current_surf_.second;

				if (ocr_.camera_surf_.get()) {
					VALIDATE(ocr_.camera_surf_->w == src_surf->w && ocr_.camera_surf_->h == src_surf->h, null_str);
					surface_lock lock(ocr_.camera_surf_);
					Uint32* pixels = lock.pixels();
					memcpy(pixels, src_surf->pixels, src_surf->w * src_surf->h * 4); 

				} else {
					ocr_.camera_surf_ = clone_surface(src_surf);
				}

				generate_mark_line_png(ocr_.lines_, src_surf, null_str);

				memcpy(ocr_.blended_tex_pixels_, src_surf->pixels, src_surf->w * src_surf->h * 4); 
				ocr_.blended_tex_.first = true;

				ocr_.lines_.clear();
				for (std::vector<std::unique_ptr<tocr_line> >::const_iterator it = lines.begin(); it != lines.end(); ++ it) {
					ocr_.lines_.push_back(std::unique_ptr<tocr_line>(new tocr_line(*it->get())));
				}
			}
		}
	}
	ocr_.recognition_thread_running_ = false;
}

void tocr::start_avcapture()
{
	VALIDATE(!target_.get(), null_str);
	VALIDATE(!recognition_thread_running_ && !avcapture_.get(), null_str);

	tsetting_lock setting_lock(*this);
	threading::lock lock(recognition_mutex_);

	avcapture_.reset(new tavcapture());
	paper_->set_timer_interval(30);

	current_surf_ = std::make_pair(false, surface());

	executor_.reset(new texecutor(*this));
}

void tocr::stop_avcapture()
{
	VALIDATE(!target_.get(), null_str);

	{
		tsetting_lock setting_lock(*this);
		threading::lock lock(recognition_mutex_);
		avcapture_.reset();
		paper_->set_timer_interval(0);

		current_surf_ = std::make_pair(false, surface());
	}
	executor_.reset();
}

SDL_Rect tocr::app_did_draw_frame(bool remote, cv::Mat& frame, const SDL_Rect& draw_rect)
{
	VALIDATE(!target_.get(), null_str);

	if (!blended_tex_.second.get()) {
		blended_tex_.second = SDL_CreateTexture(get_renderer(), get_screen_format().format, SDL_TEXTUREACCESS_STREAMING, frame.cols, frame.rows);
		int pitch = 0;
		SDL_LockTexture(blended_tex_.second.get(), NULL, (void**)&blended_tex_pixels_, &pitch);
	}
	int tex_width, tex_height;
	SDL_QueryTexture(blended_tex_.second.get(), nullptr, nullptr, &tex_width, &tex_height);
	VALIDATE(tex_width == frame.cols || tex_height == frame.rows, null_str); 

	if (!current_surf_.first) {
		threading::lock lock(variable_mutex_);
		if (!current_surf_.second.get()) {
			current_surf_.second = create_neutral_surface(frame.cols, frame.rows);
		}
		memcpy(current_surf_.second->pixels, frame.data, frame.cols * frame.rows * 4); 
		current_surf_.first = true;
	}

	return draw_rect;
}

void tocr::did_draw_paper(ttrack& widget, const SDL_Rect& widget_rect, const bool bg_drawn)
{
	SDL_Renderer* renderer = get_renderer();
	ttrack::tdraw_lock lock(renderer, widget);

	if (!bg_drawn) {
		SDL_RenderCopy(renderer, widget.background_texture().get(), NULL, &widget_rect);
	}

	surface surf;
	SDL_Rect dst;
	texture thumbnail_tex;
	if (target_.get()) {
		surf = clone_surface(target_);
	
	
		detect_and_blend_surf(surf, lines_);
		generate_mark_line_png(lines_, surf, null_str);

		render_surface(renderer, surf, nullptr, &widget_rect);

		thumbnail_tex = SDL_CreateTextureFromSurface(renderer, target_);

	} else {
		surface text_surf;
		SDL_Rect dst;
		tavcapture::VideoRenderer* local_renderer = avcapture_->vrenderer(false);
		if (!local_renderer) {
			return;
		}
		texture& local_tex = avcapture_->get_texture(false);

		if (local_renderer->dirty()) {
			threading::lock lock(avcapture_->get_mutex(false));

			ttexture_2_mat_lock lock2(local_tex);
			dst = app_did_draw_frame(false, lock2.mat, widget_rect);

			if (local_renderer->dirty()) {
				SDL_UnlockTexture(local_tex.get());
			}
			thumbnail_tex = local_tex;
		}

		threading::lock lock(variable_mutex_);
		if (camera_surf_.get()) {
			VALIDATE(blended_tex_.second.get() && !lines_.empty(), null_str);
			if (blended_tex_.first) {
				SDL_UnlockTexture(blended_tex_.second.get());
				blended_tex_.first = false;
			}
		
			SDL_RenderCopy(renderer, blended_tex_.second.get(), nullptr, nullptr);
		}
	}

	if (thumbnail_size_.x * 2 > widget_rect.w) {
		thumbnail_size_.x /= 2;
		thumbnail_size_.y /= 2;
	}
	dst.w = thumbnail_size_.x;
	dst.h = thumbnail_size_.y;
	dst.x = widget_rect.x + original_local_offset_.x + current_local_offset_.x;
	dst.y = widget_rect.y + original_local_offset_.y + current_local_offset_.y;

	if (!target_.get()) {
		// when target_.get() is slow. require optimaze.
		SDL_RenderCopy(renderer, thumbnail_tex.get(), NULL, &dst);
	}

	if (!lines_.empty()) {
		save_->set_active(true);
	}
}

void tocr::did_mouse_leave_paper(ttrack& widget, const tpoint&, const tpoint& /*last_coordinate*/)
{
	original_local_offset_ += current_local_offset_;
	current_local_offset_.x = current_local_offset_.y = 0;

	int width = widget.get_width();
	int height = widget.get_height();
	if (original_local_offset_.x < 0) {
		original_local_offset_.x = 0;
	} else if (original_local_offset_.x > width - thumbnail_size_.x) {
		original_local_offset_.x = width - thumbnail_size_.x;
	}
	if (original_local_offset_.y < 0) {
		original_local_offset_.y = 0;
	} else if (original_local_offset_.y > height - thumbnail_size_.y) {
		original_local_offset_.y = height - thumbnail_size_.y;
	}
}

void tocr::did_mouse_motion_paper(ttrack& widget, const tpoint& first, const tpoint& last)
{
	if (is_null_coordinate(first)) {
		return;
	}
	current_local_offset_ = last - first;
	did_draw_paper(*paper_, paper_->get_draw_rect(), false);
}

void tocr::save(twindow& window)
{
	window.set_retval(twindow::OK);
}

} // namespace gui2

