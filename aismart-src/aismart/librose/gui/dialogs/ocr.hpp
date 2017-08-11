#ifndef LIBROSE_GUI_DIALOGS_OCR_HPP_INCLUDED
#define LIBROSE_GUI_DIALOGS_OCR_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include "ocr/ocr.hpp"
#include "rtc_client.hpp"

namespace gui2 {

class ttrack;
class tbutton;

class tocr: public tdialog
{
public:
	explicit tocr(const surface& target, std::vector<std::unique_ptr<tocr_line> >& lines);

	~tocr();
	surface target(tpoint& offset) const;

private:
	/** Inherited from tdialog. */
	void pre_show(CVideo& video, twindow& window);

	/** Inherited from tdialog. */
	void post_show(twindow& window);

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

private:
	void did_draw_paper(ttrack& widget, const SDL_Rect& widget_rect, const bool bg_drawn);
	void did_mouse_leave_paper(ttrack& widget, const tpoint&, const tpoint& /*last_coordinate*/);
	void did_mouse_motion_paper(ttrack& widget, const tpoint& first, const tpoint& last);
	void save(twindow& window);

	void detect_and_blend_surf(surface& surf, std::vector<std::unique_ptr<tocr_line> >& lines);

	void start_avcapture();
	void stop_avcapture();
	SDL_Rect app_did_draw_frame(bool remote, cv::Mat& frame, const SDL_Rect& draw_rect);

	class texecutor: public tworker
	{
	public:
		texecutor(tocr& ocr)
			: ocr_(ocr)
		{
			thread_->Start();
		}

	private:
		void DoWork() override;
		void OnWorkStart() override {}
		void OnWorkDone() override {}

	private:
		tocr& ocr_;
	};

private:
	const surface target_;
	std::vector<std::unique_ptr<tocr_line> >& lines_;
	bool verbose_;

	ttrack* paper_;
	tbutton* save_;

	class tsetting_lock
	{
	public:
		tsetting_lock(tocr& ocr)
			: ocr_(ocr)
		{
			VALIDATE(!ocr_.setting_, null_str);
			ocr_.setting_ = true;
		}
		~tsetting_lock()
		{
			ocr_.setting_ = false;
		}

	private:
		tocr& ocr_;
	};
	bool setting_;
	bool recognition_thread_running_;

	std::pair<bool, surface> current_surf_; // first: valid. now can recognition.
	std::unique_ptr<tavcapture> avcapture_;

	threading::mutex variable_mutex_;
	threading::mutex recognition_mutex_;

	std::pair<bool, texture> blended_tex_;
	uint8_t* blended_tex_pixels_;
	surface camera_surf_;

	std::unique_ptr<texecutor> executor_;
	tpoint thumbnail_size_;
	tpoint original_local_offset_;
	tpoint current_local_offset_;
};

} // namespace gui2

#endif

