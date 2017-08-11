/* $Id: scrollbar_container.cpp 54604 2012-07-07 00:49:45Z loonycyborg $ */
/*
   Copyright (C) 2008 - 2012 by Mark de Wever <koraq@xs4all.nl>
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

#include "gui/widgets/scroll_container.hpp"

#include "gui/widgets/spacer.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/horizontal_scrollbar.hpp"
#include "gui/widgets/vertical_scrollbar.hpp"
#include "gui/widgets/toggle_panel.hpp"
#include "rose_config.hpp"

#include <boost/bind.hpp>
#include <boost/foreach.hpp>

#include "posix2.h"

namespace gui2 {

bool tscroll_container::refuse_scroll = false;

tscroll_container::tscroll_container(const unsigned canvas_count, bool listbox)
	: tcontainer_(canvas_count)
	, listbox_(listbox)
	, state_(ENABLED)
	, vertical_scrollbar_mode_(auto_visible)
	, horizontal_scrollbar_mode_(auto_visible)
	, vertical_scrollbar_(nullptr)
	, horizontal_scrollbar_(nullptr)
	, dummy_vertical_scrollbar_(nullptr)
	, dummy_horizontal_scrollbar_(nullptr)
	, vertical_scrollbar2_(nullptr)
	, horizontal_scrollbar2_(nullptr)
	, content_grid_(NULL)
	, content_(NULL)
	, content_visible_area_()
	, scroll_to_end_(false)
	, calculate_reduce_(false)
	, need_layout_(false)
	, find_content_grid_(false)
	, gc_next_precise_at_(0)
	, gc_first_at_(twidget::npos)
	, gc_last_at_(twidget::npos)
	, gc_current_content_width_(twidget::npos)
	, gc_locked_at_(twidget::npos)
	, gc_calculate_best_size_(false)
	, scroll_elapse_(std::make_pair(0, 0))
	, next_scrollbar_invisible_ticks_(0)
	, first_coordinate_(construct_null_coordinate())
	, can_scroll_(true)
{
	construct_spacer_grid(grid_);
	set_container_grid(grid_);
	grid_.set_parent(this);

	connect_signal<event::SDL_KEY_DOWN>(boost::bind(
			&tscroll_container::signal_handler_sdl_key_down
				, this, _2, _3, _5, _6));
}

tscroll_container::~tscroll_container() 
{ 
	if (vertical_scrollbar2_->widget->parent() == content_grid_) {
		reset_scrollbar(*get_window());
	}

	scroll_timer_.reset();

	delete dummy_vertical_scrollbar_;
	delete dummy_horizontal_scrollbar_;
	delete content_grid_; 
}

void tscroll_container::construct_spacer_grid(tgrid& grid)
{
	/*
	[grid]
		[row]
			[column]
				horizontal_alignment = "edge"
				vertical_alignment = "edge"
				[spacer]
					definition = "default"
				[/spacer]
			[/column]
		[/row]
	[/grid]
	*/
	::config grid_cfg;
	::config& sub = grid_cfg.add_child("row").add_child("column");
	sub["horizontal_alignment"] = "edge";
	sub["vertical_alignment"] = "edge";

	::config& sub2 = sub.add_child("spacer");
	sub2["definition"] = "default";

	tbuilder_grid grid_builder(grid_cfg);
	grid_builder.build(&grid);
}

void tscroll_container::layout_init(bool linked_group_only)
{
	content_grid_->layout_init(linked_group_only);
}

tpoint tscroll_container::mini_get_best_text_size() const
{
	return content_grid_->calculate_best_size();
}

tpoint tscroll_container::calculate_best_size() const
{
	// container_ must be (0, 0)
	return tcontrol::calculate_best_size();
}

tpoint tscroll_container::calculate_best_size_bh(const int width)
{
	// there are some devired class from tscroll_container, for example tscroll_panel.
	// tscroll_panel require impletement calculate_best_size_bh explicited.
	return tpoint(twidget::npos, twidget::npos);

	// return calculate_best_size();
}

void tscroll_container::set_scrollbar_mode(tscrollbar_& scrollbar,
		const tscroll_container::tscrollbar_mode& scrollbar_mode,
		const unsigned items, const unsigned visible_items)
{
	scrollbar.set_item_count(items);
	scrollbar.set_visible_items(visible_items);
	// old item_position maybe overflow the new boundary.
	if (scrollbar.get_item_position()) {
		scrollbar.set_item_position(scrollbar.get_item_position());
	}

	if (&scrollbar != dummy_horizontal_scrollbar_ && &scrollbar != dummy_vertical_scrollbar_) {
		if (scrollbar.float_widget()) {
			bool visible = false;
			if (scrollbar_mode == auto_visible) {
				const bool scrollbar_needed = items > visible_items;
				visible = scrollbar_needed;
			}
			get_window()->find_float_widget(scrollbar.id())->set_visible(visible);
		} else {
			const bool scrollbar_needed = items > visible_items;
			scrollbar.set_visible(scrollbar_needed? twidget::VISIBLE: twidget::HIDDEN);
		}
	}
}

void tscroll_container::place(const tpoint& origin, const tpoint& size)
{
	need_layout_ = false;

	// Inherited.
	tcontainer_::place(origin, size);

	if (!size.x || !size.y) {
		return;
	}

	const tpoint content_origin = content_->get_origin();
	const tpoint content_size = content_->get_size();
	
	const unsigned origin_y_offset = vertical_scrollbar_->get_item_position();

	tpoint content_grid_size = mini_calculate_content_grid_size(content_origin, content_size);

	if (content_grid_size.x < (int)content_->get_width()) {
		content_grid_size.x = content_->get_width();
	}
	/*
	if (content_grid_size.y < (int)content_->get_height()) {
		content_grid_size.y = content_->get_height();
	}
	*/
	// of couse, can use content_grid_origin(inclued x_offset/yoffset) as orgin, but there still use content_orgin. there is tow reason.
	// 1)mini_set_content_grid_origin maybe complex.
	// 2)I think place with xoffset/yoffset is not foten.
	content_grid_->place(content_origin, content_grid_size);
	// report's content_grid_size is vallid after tgrid2::place.
	content_grid_size = content_grid_->get_size();

	can_scroll_ = content_grid_size.x > size.x || content_grid_size.y > size.y;

	// Set vertical scrollbar. correct vertical item_position if necessary.
	set_scrollbar_mode(*vertical_scrollbar_,
			vertical_scrollbar_mode_,
			content_grid_size.y,
			content_->get_height());
	if (vertical_scrollbar_ != dummy_vertical_scrollbar_) {
		set_scrollbar_mode(*dummy_vertical_scrollbar_,
			vertical_scrollbar_mode_,
			content_grid_size.y,
			content_->get_height());
	}

	// Set horizontal scrollbar. correct horizontal item_position if necessary.
	set_scrollbar_mode(*horizontal_scrollbar_,
			horizontal_scrollbar_mode_,
			content_grid_size.x,
			content_->get_width());
	if (horizontal_scrollbar_ != dummy_horizontal_scrollbar_) {
		set_scrollbar_mode(*dummy_horizontal_scrollbar_,
			horizontal_scrollbar_mode_,
			content_grid_size.x,
			content_->get_width());
	}

	// now both vertical and horizontal item_postion are right.
	const unsigned x_offset = horizontal_scrollbar_->get_item_position();
	unsigned y_offset = vertical_scrollbar_->get_item_position();

	if (content_grid_size.x >= content_size.x) {
		VALIDATE(x_offset + horizontal_scrollbar_->get_visible_items() <= horizontal_scrollbar_->get_item_count(), null_str);
	}
	if (content_grid_size.y >= content_size.y) {
		VALIDATE(y_offset + vertical_scrollbar_->get_visible_items() <= vertical_scrollbar_->get_item_count(), null_str);
	}

	// posix_print("tscroll_container::place, content_grid.h: %i, call mini_handle_gc(, %i), origin_y_offset: %i\n", content_grid_size.y, y_offset, origin_y_offset);
	y_offset = mini_handle_gc(x_offset, y_offset);
	// posix_print("tscroll_container::place, post mini_handle_gc, y_offset: %i\n", y_offset);

	// const tpoint content_grid_origin = tpoint(content_->get_x() - x_offset, content_->get_y() - y_offset);

	// of couse, can use content_grid_origin(inclued x_offset/yoffset) as orgin, but there still use content_orgin. there is tow reason.
	// 1)mini_set_content_grid_origin maybe complex.
	// 2)I think place with xoffset/yoffset is not foten.
	// content_grid_size = content_grid_->get_size();
	// content_grid_->place(content_grid_origin, content_grid_size);

	// if (x_offset || y_offset) {
		// if previous exist item_postion, recover it.
		const tpoint content_grid_origin = tpoint(content_->get_x() - x_offset, content_->get_y() - y_offset);
		mini_set_content_grid_origin(content_->get_origin(), content_grid_origin);
	// }

	content_visible_area_ = content_->get_rect();
	// Now set the visible part of the content.
	mini_set_content_grid_visible_area(content_visible_area_);
}

tpoint tscroll_container::validate_content_grid_origin(const tpoint& content_origin, const tpoint& content_size, const tpoint& origin, const tpoint& size) const
{
	// verify desire_origin
	//  content_grid origin---> | <--- desire_origin
	//                          | <--- vertical scrollbar's item_position
	//  content origin -------> |
	//  content size            |
	tpoint origin2 = origin;
	VALIDATE(origin2.y <= content_origin.y, "y of content_grid must <= content.y!");
	VALIDATE(size.y >= content_size.y, "content_grid must >= content!");

	int item_position = (int)vertical_scrollbar_->get_item_position();
	if (origin2.y + item_position != content_origin.y) {
		origin2.y = content_origin.y - item_position;
	}
	if (item_position + content_size.y > size.y) {
		item_position = size.y - content_size.y;
		vertical_scrollbar_->set_item_position2(item_position);

		origin2.y = content_origin.y - item_position;
	}

	VALIDATE(origin2.y <= content_origin.y, "(2)y of content_grid must <= content.y!");
	return origin2;
}

void tscroll_container::set_origin(const tpoint& origin)
{
	// Inherited.
	tcontainer_::set_origin(origin);

	int x_offset = horizontal_scrollbar_->get_item_position();
	int y_offset = vertical_scrollbar_->get_item_position();
	const tpoint content_grid_origin(content_->get_x() - x_offset, content_->get_y() - y_offset);

	mini_set_content_grid_origin(origin, content_grid_origin);

	// Changing the origin also invalidates the visible area.
	mini_set_content_grid_visible_area(content_visible_area_);
}

void tscroll_container::mini_set_content_grid_origin(const tpoint& origin, const tpoint& content_grid_origin)
{
	content_grid_->set_origin(content_grid_origin);
}

void tscroll_container::mini_set_content_grid_visible_area(const SDL_Rect& area)
{
	content_grid_->set_visible_area(area);
}

void tscroll_container::set_visible_area(const SDL_Rect& area)
{
	// Inherited.
	tcontainer_::set_visible_area(area);

	// Now get the visible part of the content.
	content_visible_area_ = intersect_rects(area, content_->get_rect());

	content_grid_->set_visible_area(content_visible_area_);
}

void tscroll_container::dirty_under_rect(const SDL_Rect& clip)
{
	if (visible_ != VISIBLE) {
		return;
	}
	content_grid_->dirty_under_rect(clip);
}

twidget* tscroll_container::find_at(const tpoint& coordinate, const bool must_be_active)
{
	if (visible_ != VISIBLE) {
		return nullptr;
	}

	VALIDATE(content_ && content_grid_, null_str);

	twidget* result = tcontainer_::find_at(coordinate, must_be_active);
	if (result == content_) {
		result = content_grid_->find_at(coordinate, must_be_active);
		if (!result) {
			// to support SDL_WHEEL_DOWN/SDL_WHEEL_UP, must can find at "empty" area.
			result = content_grid_;
		}
	}
	return result;
}

twidget* tscroll_container::find(const std::string& id, const bool must_be_active)
{
	// Inherited.
	twidget* result = tcontainer_::find(id, must_be_active);

	// Can be called before finalize so test instead of assert for the grid.
	// if (!result && find_content_grid_ && content_grid_) {
	if (!result) {
		result = content_grid_->find(id, must_be_active);
	}

	return result;
}

const twidget* tscroll_container::find(const std::string& id, const bool must_be_active) const
{
	// Inherited.
	const twidget* result = tcontainer_::find(id, must_be_active);

	// Can be called before finalize so test instead of assert for the grid.
	if (!result && find_content_grid_ && content_grid_) {
		result = content_grid_->find(id, must_be_active);
	}
	return result;
}

SDL_Rect tscroll_container::get_float_widget_ref_rect() const
{
	SDL_Rect ret{x_, y_, (int)w_, (int)h_};
	if (content_grid_->get_height() < content_->get_height()) {
		ret.h = content_grid_->get_height();
	}
	return ret;
}

bool tscroll_container::disable_click_dismiss() const
{
	return content_grid_->disable_click_dismiss();
}

int tscroll_container::gc_calculate_total_height() const
{
	if (gc_next_precise_at_ == 0) {
		return 0;
	}
	
	const ttoggle_panel* row = gc_widget_from_children(gc_next_precise_at_ - 1);
	const int precise_height = row->gc_distance_ + row->gc_height_;
	const int average_height = precise_height / gc_next_precise_at_;
	return precise_height + (gc_handle_rows() - gc_next_precise_at_) * average_height;
}

int tscroll_container::gc_which_precise_row(const int distance) const
{
	VALIDATE(gc_next_precise_at_ > 0, null_str);

	// 1. check in precise rows
	const ttoggle_panel* current_gc_row = gc_widget_from_children(gc_next_precise_at_ - 1);
	if (distance > current_gc_row->gc_distance_ + current_gc_row->gc_height_) {
		return twidget::npos;
	} else if (distance == current_gc_row->gc_distance_ + current_gc_row->gc_height_) {
		return gc_next_precise_at_;
	}
	const int max_one_by_one_range = 2;
	int start = 0;
	int end = gc_next_precise_at_ - 1;
	int mid = (end - start) / 2 + start;
	while (mid - start > 1) {
		current_gc_row = gc_widget_from_children(mid);
		if (distance < current_gc_row->gc_distance_) {
			end = mid;

		} else if (distance > current_gc_row->gc_distance_) {
			start = mid;

		} else {
			return mid;
		}
		mid = (end - start) / 2 + start;
	}

	int row = start;
	for (; row <= end; row ++) {
		current_gc_row = gc_widget_from_children(row);
		if (distance >= current_gc_row->gc_distance_ && distance < current_gc_row->gc_distance_ + current_gc_row->gc_height_) {
			break;
		}
	}
	VALIDATE(row <= end, null_str);

	return row;
}

int tscroll_container::gc_which_children_row(const int distance) const
{
	// return twidget::npos;

	const int childs = gc_handle_rows();

	VALIDATE(childs > 0, null_str);

	// const int first_at = dynamic_cast<ttoggle_panel*>(children[0].widget_)->at_;
	// const int last_at = dynamic_cast<ttoggle_panel*>(children[childs - 1].widget_)->at_;
	const int first_at = gc_first_at_;
	const int last_at = gc_last_at_;

	const ttoggle_panel* last_precise_row = gc_widget_from_children(gc_next_precise_at_ - 1);
	const int precise_height = last_precise_row->gc_distance_ + last_precise_row->gc_height_;
	const int average_height = precise_height / gc_next_precise_at_;

	ttoggle_panel* current_gc_row = gc_widget_from_children(first_at);
	if (distance >= gc_distance_value(current_gc_row->gc_distance_) - average_height && distance < gc_distance_value(current_gc_row->gc_distance_)) {
		return first_at - 1;
	}
	current_gc_row = gc_widget_from_children(last_at);
	int children_end_distance = gc_distance_value(current_gc_row->gc_distance_) + current_gc_row->gc_height_;
	if (distance >= children_end_distance && distance < children_end_distance + average_height) {
		return last_at + 1;
	}

	for (int n = first_at; n <= last_at; n ++) {
		current_gc_row = gc_widget_from_children(n);
		const int distance2 = gc_distance_value(current_gc_row->gc_distance_) + current_gc_row->gc_height_;
		if (distance >= gc_distance_value(current_gc_row->gc_distance_) && distance < gc_distance_value(current_gc_row->gc_distance_) + current_gc_row->gc_height_) {
			return n;
		}
	}
	return twidget::npos;
}

int tscroll_container::mini_handle_gc(const int x_offset, const int y_offset)
{
	const int rows = gc_handle_rows();
	if (!rows) {
		return y_offset;
	}
	ttoggle_panel* current_gc_row = nullptr;

	const SDL_Rect content_rect = content_->get_rect();
	if (gc_calculate_best_size_) {
		VALIDATE(gc_first_at_ == twidget::npos, null_str);
	} else {
		VALIDATE(content_rect.h > 0, null_str);
	}

	if (gc_locked_at_ != twidget::npos) {
		VALIDATE(gc_locked_at_ >= 0 && gc_locked_at_ < rows, null_str);
		VALIDATE(!gc_calculate_best_size_, null_str);
	}

	int least_height = content_rect.h * 2; // 2 * content_.height
	const int half_content_height = content_rect.h / 2;

	if (!gc_calculate_best_size_ && (gc_locked_at_ != twidget::npos || gc_first_at_ != twidget::npos)) {
		// posix_print("mini_handle_gc, gc_locked_at_(%i), top, pre [%i, %i], gc_next_precise_at_: %i\n", gc_locked_at_, gc_first_at_, gc_last_at_, gc_next_precise_at_);
		VALIDATE(gc_last_at_ >= gc_first_at_ && gc_next_precise_at_ > 0, null_str);

		current_gc_row = gc_widget_from_children(gc_next_precise_at_ - 1);
		VALIDATE(gc_is_precise(current_gc_row->gc_distance_) && current_gc_row->gc_height_ != twidget::npos, null_str);
		const int precise_height = current_gc_row->gc_distance_ + current_gc_row->gc_height_;
		const int average_height = precise_height / gc_next_precise_at_;

		const int first_at = gc_first_at_;
		const int last_at = gc_last_at_;
		const ttoggle_panel* first_gc_row = gc_first_at_ != twidget::npos? gc_widget_from_children(first_at): nullptr;
		const ttoggle_panel* last_gc_row = gc_first_at_ != twidget::npos? gc_widget_from_children(last_at): nullptr;
		const int start_distance = y_offset >= half_content_height? y_offset - half_content_height: 0;
		int start_distance2 = twidget::npos;
		int estimated_start_row;
		bool children_changed = false;
		std::pair<int, int> smooth_scroll(twidget::npos, 0);

		if (gc_locked_at_ != twidget::npos) {
			// 1) calculate start row.
			if (gc_locked_at_ >= first_at && gc_locked_at_ <= last_at) {
				start_distance2 = gc_distance_value(first_gc_row->gc_distance_);

				int valid_height = gc_distance_value(last_gc_row->gc_distance_) + last_gc_row->gc_height_ - gc_distance_value(first_gc_row->gc_distance_);
				// posix_print("mini_handle_gc, gc_locked_at_(%i), branch1, pre [%i, %i]\n", gc_locked_at_, gc_first_at_, gc_last_at_);
				int row = last_at + 1;
				while (valid_height < least_height && row < rows) {
					current_gc_row = gc_widget_from_children(row);
					if (current_gc_row->gc_height_ == twidget::npos) {
						ttoggle_panel& panel = *current_gc_row;

						tpoint size = gc_handle_calculate_size(panel, content_rect.w);
						panel.gc_width_ = size.x;
						panel.gc_height_ = size.y;

						panel.twidget::set_size(tpoint(0, 0));
					}
					VALIDATE(current_gc_row->gc_height_ != twidget::npos, null_str);
					valid_height += current_gc_row->gc_height_;
					row ++;
				}
				gc_last_at_ = row - 1;

				row = first_at - 1;
				while (valid_height < least_height && row >= 0) {
					current_gc_row = gc_widget_from_children(row);
					if (current_gc_row->gc_height_ == twidget::npos) {
						ttoggle_panel& panel = *current_gc_row;

						tpoint size = gc_handle_calculate_size(panel, content_rect.w);

						panel.gc_width_ = size.x;
						panel.gc_height_ = size.y;

						panel.twidget::set_size(tpoint(0, 0));
					}
					VALIDATE(current_gc_row->gc_height_ != twidget::npos, null_str);
					valid_height += current_gc_row->gc_height_;

					start_distance2 -= current_gc_row->gc_height_;
					VALIDATE(start_distance2 >= 0, null_str);

					row --;

				}
				gc_first_at_ = row + 1;

			} else {
				// posix_print("mini_handle_gc, gc_locked_at_(%i), branch2, pre [%i, %i]\n", gc_locked_at_, gc_first_at_, gc_last_at_);

				if (gc_locked_at_ < gc_next_precise_at_) {
					current_gc_row = gc_widget_from_children(gc_locked_at_);
					start_distance2 = current_gc_row->gc_distance_;
				} else {
					start_distance2 = precise_height + (gc_locked_at_ - gc_next_precise_at_) * average_height;
				}

				int valid_height = 0;
				int row = gc_locked_at_;
				while (valid_height < least_height && row < rows) {
					current_gc_row = gc_widget_from_children(row);
					if (current_gc_row->gc_height_ == twidget::npos) {
						ttoggle_panel& panel = *current_gc_row;

						tpoint size = gc_handle_calculate_size(panel, content_rect.w);

						panel.gc_width_ = size.x;
						panel.gc_height_ = size.y;

						panel.twidget::set_size(tpoint(0, 0));
					}
					VALIDATE(current_gc_row->gc_height_ != twidget::npos, null_str);
					valid_height += current_gc_row->gc_height_;
					row ++;
				}
				gc_last_at_ = row - 1;

				row = gc_locked_at_ - 1;
				while (valid_height < least_height && row >= 0) {
					current_gc_row = gc_widget_from_children(row);
					if (current_gc_row->gc_height_ == twidget::npos) {
						ttoggle_panel& panel = *current_gc_row;

						tpoint size = gc_handle_calculate_size(panel, content_rect.w);

						panel.gc_width_ = size.x;
						panel.gc_height_ = size.y;

						panel.twidget::set_size(tpoint(0, 0));
					}
					VALIDATE(current_gc_row->gc_height_ != twidget::npos, null_str);
					valid_height += current_gc_row->gc_height_;
					row --;
				}
				gc_first_at_ = row + 1;
				
				if (first_at != twidget::npos) {
					if (last_at < gc_first_at_) {
						for (int n = first_at; n <= last_at; n ++) {
							// Garbage Collection!
							current_gc_row = gc_widget_from_children(n);
							gc_handle_garbage_collection(*current_gc_row);
						}
					} else {
						// first_at--gc_first_at_--gc_last_at--last_at
						for (int n = first_at; n < gc_first_at_; n ++) {
							// Garbage Collection!
							current_gc_row = gc_widget_from_children(n);
							gc_handle_garbage_collection(*current_gc_row);
						}
						for (int n = gc_last_at_ + 1; n <= last_at; n ++) {
							// Garbage Collection!
							current_gc_row = gc_widget_from_children(n);
							gc_handle_garbage_collection(*current_gc_row);
						}
					}
				}
			}

			// posix_print("mini_handle_gc, gc_locked_at_(%i), post [%i, %i], children_changed(%s)\n", gc_locked_at_, gc_first_at_, gc_last_at_, children_changed? "true": "false");

			estimated_start_row = gc_first_at_;

			// as if all from gc_next_precise_at_, distance maybe change becase of linked_group.
			// let all enter calculate distance/height again.
			children_changed = true;

		} else if (y_offset < gc_distance_value(first_gc_row->gc_distance_)) {
			if (first_at == 0 || last_at == 0) { // first row maybe enough height.
				return y_offset;
			}

			// posix_print("mini_handle_gc, y_offset: %i, start_distance: %i, content_grid.h - content.h: %i\n", y_offset, start_distance, content_grid_->get_height() - content_->get_height());
			// posix_print("mini_handle_gc, pre[%i, %i], gc_next_precise_at_: %i\n", gc_first_at_, gc_last_at_, gc_next_precise_at_);

			// yoffset----
			// --first_gc_row---
			// 1) calculate start row.
			estimated_start_row = gc_which_precise_row(start_distance);
			if (estimated_start_row == twidget::npos) {
				estimated_start_row = gc_which_children_row(start_distance);
				if (estimated_start_row == twidget::npos) {
					estimated_start_row = gc_next_precise_at_ + (start_distance - precise_height) / average_height;
					VALIDATE(estimated_start_row < rows, null_str);
				}
			}
			if (estimated_start_row >= first_at) {
				estimated_start_row = first_at - 1;
			}
			// since drag up, up one at least.
			VALIDATE(estimated_start_row < first_at, null_str);

			// 2)at top, insert row if necessary.
			int valid_height = 0;
			if (estimated_start_row < gc_next_precise_at_) {
				// start_distance - current_gc_row->gc_distance_ maybe verty large.
				current_gc_row = gc_widget_from_children(estimated_start_row);
				VALIDATE(start_distance >= current_gc_row->gc_distance_ && start_distance < current_gc_row->gc_distance_ + current_gc_row->gc_height_, null_str);
				valid_height = -1 * (start_distance - current_gc_row->gc_distance_);
			}
			int row = estimated_start_row;
			while (valid_height < least_height && row < rows) {
				current_gc_row = gc_widget_from_children(row);
				if (current_gc_row->gc_height_ == twidget::npos) {
					ttoggle_panel& panel = *current_gc_row;

					tpoint size = gc_handle_calculate_size(panel, content_rect.w);

					panel.gc_width_ = size.x;
					panel.gc_height_ = size.y;

					panel.twidget::set_size(tpoint(0, 0));
				}
				VALIDATE(current_gc_row->gc_height_ != twidget::npos, null_str);

				valid_height += current_gc_row->gc_height_;

				if (estimated_start_row == first_at - 1 && row == estimated_start_row) {
					// slow move. make sure current row(first_at) can be on top.
					if (valid_height > least_height || least_height - valid_height < content_rect.h) {
						valid_height = least_height - content_rect.h;
					}
				}

				row ++;
			}
			VALIDATE(row > estimated_start_row, null_str);

			// 3) at bottom, erase children
			if (row > first_at) { // this allocate don't include row, so don't inlucde "==".
				smooth_scroll = std::make_pair(first_at, gc_distance_value(first_gc_row->gc_distance_));
			}

			const int start_at = row > first_at? row: first_at;
			for (int n = start_at; n <= last_at; n ++) {
				// Garbage Collection!
				current_gc_row = gc_widget_from_children(n);
				gc_handle_garbage_collection(*current_gc_row);
			}

			gc_first_at_ = estimated_start_row;
			gc_last_at_ = row - 1;

			// posix_print("mini_handle_gc, erase bottom(from %i to %i), post[%i, %i]\n", start_at, last_at, gc_first_at_, gc_last_at_);

			start_distance2 = start_distance;
			children_changed = true;


		} else if (y_offset + content_rect.h >= gc_distance_value(last_gc_row->gc_distance_) + last_gc_row->gc_height_) {
			if (last_at == rows - 1) {
				return y_offset;
			}

			// posix_print("mini_handle_gc, y_offset: %i, start_distance: %i, content_grid.h - content.h: %i\n", y_offset, start_distance, content_grid_->get_height() - content_->get_height());
			// posix_print("mini_handle_gc, pre[%i, %i], gc_next_precise_at_: %i\n", gc_first_at_, gc_last_at_, gc_next_precise_at_);

			// --last_gc_row---
			// yoffset----
			// 1) calculate start row.
			estimated_start_row = gc_which_precise_row(start_distance);
			if (estimated_start_row == twidget::npos) {
				estimated_start_row = gc_which_children_row(start_distance);
				if (estimated_start_row == twidget::npos) {
					estimated_start_row = gc_next_precise_at_ + (start_distance - precise_height) / average_height;
					VALIDATE(estimated_start_row < rows, null_str);

				}
			}

			//
			// don't use below statment. if first_at height enogh, it will result to up-down jitter.
			// 
			// if (estimated_start_row == first_at) {
			//	estimated_start_row = first_at + 1;
			// }

			// 2)at bottom, insert row if necessary.
			if (estimated_start_row <= last_at) {
				current_gc_row = gc_widget_from_children(estimated_start_row);
				smooth_scroll = std::make_pair(estimated_start_row, gc_distance_value(current_gc_row->gc_distance_));
			}

			int valid_height = 0;
			if (estimated_start_row < gc_next_precise_at_) {
				// start_distance - current_gc_row->gc_distance_ maybe verty large.
				current_gc_row = gc_widget_from_children(estimated_start_row);
				VALIDATE(start_distance >= current_gc_row->gc_distance_ && start_distance < current_gc_row->gc_distance_ + current_gc_row->gc_height_, null_str);
				valid_height = -1 * (start_distance - current_gc_row->gc_distance_);
			}
			int row = estimated_start_row;
			while (valid_height < least_height && row < rows) {
				current_gc_row = gc_widget_from_children(row);
				if (current_gc_row->gc_height_ == twidget::npos) {
					ttoggle_panel& panel = *current_gc_row;
					
					tpoint size = gc_handle_calculate_size(panel, content_rect.w);
					
					current_gc_row->gc_width_ = size.x;
					current_gc_row->gc_height_ = size.y;

					panel.twidget::set_size(tpoint(0, 0));
				}

				VALIDATE(current_gc_row->gc_height_ != twidget::npos, null_str);
				valid_height += current_gc_row->gc_height_;

				if (valid_height >= least_height && row <= last_at) {
					least_height += current_gc_row->gc_height_;
				}
								
				row ++;
			}

			if (row == rows) {
				// it is skip alloate and will to end almost.
				// avoid start large height and end samll height, it will result top part not displayed.
				if (valid_height < content_rect.h && estimated_start_row > 1) {
					int at = estimated_start_row - 1;
					while (valid_height < content_rect.h && at >= 0) {
						current_gc_row = gc_widget_from_children(at);
						if (current_gc_row->gc_height_ == twidget::npos) {
							ttoggle_panel& panel = *current_gc_row;
							
							tpoint size = gc_handle_calculate_size(panel, content_rect.w);
							
							current_gc_row->gc_width_ = size.x;
							current_gc_row->gc_height_ = size.y;

							panel.twidget::set_size(tpoint(0, 0));
						}

						VALIDATE(current_gc_row->gc_height_ != twidget::npos, null_str);
						valid_height += current_gc_row->gc_height_;
						at --;
					}
					estimated_start_row = at + 1;
					VALIDATE(estimated_start_row > 0, null_str);
				}
			}

			// 3) at top, erase children
			const int stop_at = estimated_start_row - 1 > last_at? last_at: estimated_start_row - 1;
			for (int at = first_at; at <= stop_at; at ++) {
				// Garbage Collection!
				current_gc_row = gc_widget_from_children(at);
				gc_handle_garbage_collection(*current_gc_row);
			}

			gc_first_at_ = estimated_start_row;
			gc_last_at_ = row - 1;

			// posix_print("mini_handle_gc, erase top(from %i to %i), post[%i, %i]\n", first_at, stop_at, gc_first_at_, gc_last_at_);

			start_distance2 = start_distance;
			children_changed = true;
		}

		if (children_changed) {
			VALIDATE(estimated_start_row >= 0, null_str);

			layout_init2();

			// 4) from top to bottom fill distance
			bool is_precise = true;

			int next_distance = twidget::npos;
			for (int at = gc_first_at_; at <= gc_last_at_; at ++) {
				current_gc_row = gc_widget_from_children(at);
				if (at != gc_first_at_) {
					if (is_precise) {
						current_gc_row->gc_distance_ = next_distance;
					} else {
						// if before is estimated, next must not be precise.
						VALIDATE(!gc_is_precise(current_gc_row->gc_distance_), null_str);
						current_gc_row->gc_distance_ = gc_join_estimated(next_distance);
					}

				} else {
					if (!gc_is_precise(current_gc_row->gc_distance_)) {
						// must be skip.
						VALIDATE(at == estimated_start_row || current_gc_row->gc_distance_ != twidget::npos, null_str);
						VALIDATE(start_distance2 != twidget::npos, null_str);
						current_gc_row->gc_distance_ = gc_join_estimated(start_distance2);
					}
					is_precise = !gc_is_estimated(current_gc_row->gc_distance_);
				}
				if (is_precise && at == gc_next_precise_at_) {
					gc_next_precise_at_ ++;
				}
				next_distance = gc_distance_value(current_gc_row->gc_distance_) + current_gc_row->gc_height_;
			}

			bool extend_precise_distance = is_precise && gc_next_precise_at_ == gc_last_at_ + 1;
			if (gc_next_precise_at_ == gc_first_at_) {
				// new first_at_ is equal to gc_next_precise_at_.
				VALIDATE(!is_precise, null_str);
				current_gc_row = gc_widget_from_children(gc_next_precise_at_ - 1);
				next_distance = current_gc_row->gc_distance_ + current_gc_row->gc_height_;

				extend_precise_distance = true;
			}
			if (extend_precise_distance) {
				VALIDATE(gc_next_precise_at_ >= 1, null_str);
				for (; gc_next_precise_at_ < rows; gc_next_precise_at_ ++) {
					current_gc_row = gc_widget_from_children(gc_next_precise_at_);
					if (current_gc_row->gc_height_ == twidget::npos) {
						break;
					} else {
						VALIDATE(!gc_is_precise(current_gc_row->gc_distance_), null_str);
						current_gc_row->gc_distance_ = next_distance;
					}
					next_distance = current_gc_row->gc_distance_ + current_gc_row->gc_height_;
				}
			}

			const int height = gc_calculate_total_height();
			int new_item_position = vertical_scrollbar_->get_item_position();

			if (smooth_scroll.first != twidget::npos) {
				// exist align-row, modify [gc_first_at_, gc_last_at_]'s distance best it.
				current_gc_row = gc_widget_from_children(smooth_scroll.first);
				int distance_diff = gc_distance_value(current_gc_row->gc_distance_) - smooth_scroll.second;

				// posix_print("base align-row(#%i), distance_diff: %i, adjust new_item_position from %i to %i\n", smooth_scroll.first, distance_diff, new_item_position, new_item_position + distance_diff);
				new_item_position = new_item_position + distance_diff;

				ttoggle_panel* first_children_row = gc_widget_from_children(gc_first_at_);
				ttoggle_panel* last_children_row = gc_widget_from_children(gc_last_at_);
				if (new_item_position < gc_distance_value(first_children_row->gc_distance_) || (new_item_position + content_rect.h >= gc_distance_value(last_children_row->gc_distance_) + last_children_row->gc_height_)) {
					// posix_print("base align-row(#%i), adjust new_item_position from %i to %i\n", smooth_scroll.first, new_item_position, new_item_position - distance_diff);
					// new_item_position = new_item_position - distance_diff;
					new_item_position = gc_distance_value(last_children_row->gc_distance_) + last_children_row->gc_height_ - content_rect.h;
				}

			}

			// if height called base this time distance > gc_calculate_total_height, substruct children's distance to move upward.
			int height2 = height;

			if (gc_last_at_ > gc_next_precise_at_) {
				VALIDATE(gc_first_at_ > gc_next_precise_at_, null_str);
				ttoggle_panel* last_children_row = gc_widget_from_children(gc_last_at_);

				const ttoggle_panel* next_precise_row = gc_widget_from_children(gc_next_precise_at_ - 1);
				const int precise_height2 = next_precise_row->gc_distance_ + next_precise_row->gc_height_;
				const int average_height2 = precise_height / gc_next_precise_at_;

				height2 = gc_distance_value(last_children_row->gc_distance_) + last_children_row->gc_height_;
				// bottom out children.
				height2 += average_height2 * (rows - gc_last_at_ - 1);
			}

			const int bonus_height = height2 - height;
			if (bonus_height) {
				for (int n = gc_first_at_; n <= gc_last_at_; n ++) {
					current_gc_row = gc_widget_from_children(n);
					VALIDATE(gc_is_estimated(current_gc_row->gc_distance_), null_str);
					// posix_print("row#%i, adjust distance from %i to %i\n", n, gc_distance_value(current_gc_row->gc_distance_), gc_distance_value(current_gc_row->gc_distance_) - bonus_height);
					current_gc_row->gc_distance_ = gc_join_estimated(gc_distance_value(current_gc_row->gc_distance_)) - bonus_height;
				}
				new_item_position -= bonus_height;
			}

			// update height
			const int diff = gc_handle_update_height(height);
			// posix_print("mini_handle_gc, [%i, %i], gc_next_precise_at_: %i\n", gc_first_at_, gc_last_at_, gc_next_precise_at_);


			if (diff != 0) {
				set_scrollbar_mode(*vertical_scrollbar_,
					vertical_scrollbar_mode_,
					content_grid_->get_height(),
					content_->get_height());

				if (vertical_scrollbar_ != dummy_vertical_scrollbar_) {
					set_scrollbar_mode(*dummy_vertical_scrollbar_,
						vertical_scrollbar_mode_,
						content_grid_->get_height(),
						content_->get_height());
				}
			}

			if (new_item_position != vertical_scrollbar_->get_item_position()) {
				vertical_scrollbar_->set_item_position(new_item_position);
				if (vertical_scrollbar_ != dummy_vertical_scrollbar_) {
					dummy_vertical_scrollbar_->set_item_position(new_item_position);
				}
			}

			if (gc_locked_at_ != twidget::npos) {
				show_row_rect(gc_locked_at_);
				gc_locked_at_ = twidget::npos;
			}

			validate_children_continuous();
		}

	} else {
		VALIDATE(gc_locked_at_ == twidget::npos, null_str);
		// since no row, must first place.
		VALIDATE(!x_offset && !y_offset, null_str);
		VALIDATE(gc_first_at_ == twidget::npos && gc_first_at_ == twidget::npos, null_str);
		VALIDATE(rows > 0, null_str);

		if (gc_calculate_best_size_) {
			// must be at tgrid3::calculate_best_size. only calculate first least_height's rows.
			least_height = (int)settings::screen_height;
		}

		// 1. construct first row, and calculate row height.
		int last_distance = 0, row = 0;
		gc_first_at_ = 0;
		while ((!least_height || last_distance < least_height) && row < rows) {
			current_gc_row = gc_widget_from_children(row);
			if (row < gc_next_precise_at_) {
				VALIDATE(current_gc_row->gc_distance_ == last_distance && current_gc_row->gc_height_ != twidget::npos, null_str);
			} else {
				if (current_gc_row->gc_height_ == twidget::npos) {
					ttoggle_panel& panel = *current_gc_row;
					
					tpoint size = gc_handle_calculate_size(panel, content_rect.w);
					panel.gc_width_ = size.x;
					panel.gc_height_ = size.y;

					panel.twidget::set_size(tpoint(0, 0));
				}
				current_gc_row->gc_distance_ = last_distance;
				gc_next_precise_at_ ++;
			}

			last_distance = current_gc_row->gc_distance_ + current_gc_row->gc_height_;
			row ++;
		}
		gc_last_at_ = row - 1;

		if (gc_next_precise_at_ == row && gc_next_precise_at_ < rows) {
			// current_gc_row = dynamic_cast<ttoggle_panel*>(children[gc_next_precise_at_].widget_);
			// current_gc_row->gc_distance_ = current_gc_row->gc_height_ = twidget::npos;

			ttoggle_panel* widget = gc_widget_from_children(gc_next_precise_at_ - 1);
			int next_distance = widget->gc_distance_ + widget->gc_height_;
			for (; gc_next_precise_at_ < rows; gc_next_precise_at_ ++) {
				ttoggle_panel* current_gc_row = gc_widget_from_children(gc_next_precise_at_);
				if (current_gc_row->gc_height_ == twidget::npos) {
					break;
				} else {
					VALIDATE(!gc_is_precise(current_gc_row->gc_distance_), null_str);
					current_gc_row->gc_distance_ = next_distance;
				}
				next_distance = current_gc_row->gc_distance_ + current_gc_row->gc_height_;
			}

		}
		validate_children_continuous();

		// why require layout_init2 when gc_calculate_best_size_ = true? for example below list height:
		// #0(14, 24), #1(118, 54), #2(145, 54), if not call layout_int2, get_best_size will result 122.
		// but in fact, #0's height may equal #1/#2, so require height is 162!
		layout_init2();
		if (!gc_calculate_best_size_) {
			// update height.
			const int height = gc_calculate_total_height();
			gc_handle_update_height(height);
		}

		// remember this content_width.
		gc_current_content_width_ = gc_calculate_best_size_? 0: content_rect.w;
	}

	return vertical_scrollbar_->get_item_position();
}

tpoint tscroll_container::gc_handle_calculate_size(ttoggle_panel& widget, const int width) const
{
	return widget.get_best_size();
}

void tscroll_container::gc_handle_garbage_collection(ttoggle_panel& widget)
{
	widget.clear_texture();
}

tgrid* tscroll_container::mini_create_content_grid()
{
	return new tgrid;
}

void tscroll_container::finalize_setup(const tbuilder_grid_const_ptr& grid_builder)
{
	const std::string horizontal_id = settings::horizontal_scrollbar_id;
	const std::string vertical_id = settings::vertical_scrollbar_id;

	/***** Setup vertical scrollbar *****/
	vertical_scrollbar2_ = twindow::init_instance->find_float_widget(vertical_id);
	VALIDATE(vertical_scrollbar2_, null_str);

	// dummy_vertical_scrollbar_ = vertical_scrollbar_ = find_widget<tscrollbar_>(this, "_vertical_scrollbar", false, true);
	dummy_vertical_scrollbar_ = vertical_scrollbar_ = new tvertical_scrollbar();
	vertical_scrollbar_->set_definition("default");
	vertical_scrollbar_->set_visible(twidget::INVISIBLE);

	/***** Setup horizontal scrollbar *****/
	horizontal_scrollbar2_ = twindow::init_instance->find_float_widget(horizontal_id);
	VALIDATE(horizontal_scrollbar2_, null_str);

	// dummy_horizontal_scrollbar_ = horizontal_scrollbar_ = find_widget<tscrollbar_>(this, "_horizontal_scrollbar", false, true);
	dummy_horizontal_scrollbar_ = horizontal_scrollbar_ = new thorizontal_scrollbar();
	horizontal_scrollbar_->set_definition("default");
	horizontal_scrollbar_->set_visible(twidget::INVISIBLE);

	/***** Setup the content *****/
	VALIDATE(grid().children_vsize() == 1, null_str);
	content_ = dynamic_cast<tspacer*>(grid().widget(0, 0));
	VALIDATE(content_, null_str);

	content_grid_ = mini_create_content_grid();
	grid_builder->build(content_grid_);
	VALIDATE(content_grid_->id() == "_content_grid", null_str);

	content_grid_->set_parent(this);
	/***** Let our subclasses initialize themselves. *****/
	mini_finalize_subclass();

	{
		content_grid_->connect_signal<event::WHEEL_UP>(
			boost::bind(
				  &tscroll_container::signal_handler_sdl_wheel_up
				, this
				, _3
				, _6)
			, event::tdispatcher::back_post_child);

		content_grid_->connect_signal<event::WHEEL_UP>(
			boost::bind(
				  &tscroll_container::signal_handler_sdl_wheel_up
				, this
				, _3
				, _6)
			, event::tdispatcher::back_child);

		content_grid_->connect_signal<event::WHEEL_DOWN>(
				boost::bind(
					&tscroll_container::signal_handler_sdl_wheel_down
					, this
					, _3
					, _6)
				, event::tdispatcher::back_post_child);

		content_grid_->connect_signal<event::WHEEL_DOWN>(
				boost::bind(
					&tscroll_container::signal_handler_sdl_wheel_down
					, this
					, _3
					, _6)
				, event::tdispatcher::back_child);

		content_grid_->connect_signal<event::WHEEL_LEFT>(
				boost::bind(
					&tscroll_container::signal_handler_sdl_wheel_left
					, this
					, _3
					, _6)
				, event::tdispatcher::back_post_child);

		content_grid_->connect_signal<event::WHEEL_LEFT>(
				boost::bind(
					&tscroll_container::signal_handler_sdl_wheel_left
					, this
					, _3
					, _6)
				, event::tdispatcher::back_child);

		content_grid_->connect_signal<event::WHEEL_RIGHT>(
				boost::bind(
					&tscroll_container::signal_handler_sdl_wheel_right
					, this
					, _3
					, _6)
				, event::tdispatcher::back_post_child);

		content_grid_->connect_signal<event::WHEEL_RIGHT>(
				boost::bind(
					&tscroll_container::signal_handler_sdl_wheel_right
					, this
					, _3
					, _6)
				, event::tdispatcher::back_child);

		content_grid_->connect_signal<event::MOUSE_ENTER>(
				boost::bind(
					&tscroll_container::signal_handler_mouse_enter
					, this
					, _5
					, true)
				, event::tdispatcher::back_pre_child);

		content_grid_->connect_signal<event::MOUSE_ENTER>(
				boost::bind(
					&tscroll_container::signal_handler_mouse_enter
					, this
					, _5
					, false)
				, event::tdispatcher::back_child);

		content_grid_->connect_signal<event::LEFT_BUTTON_DOWN>(
				boost::bind(
					&tscroll_container::signal_handler_left_button_down
					, this
					, _5
					, true)
				, event::tdispatcher::back_pre_child);

		content_grid_->connect_signal<event::LEFT_BUTTON_DOWN>(
				boost::bind(
					&tscroll_container::signal_handler_left_button_down
					, this
					, _5
					, false)
				, event::tdispatcher::back_child);

		content_grid_->connect_signal<event::LEFT_BUTTON_UP>(
				boost::bind(
					&tscroll_container::signal_handler_left_button_up
					, this
					, _5
					, true)
				, event::tdispatcher::back_pre_child);

		content_grid_->connect_signal<event::LEFT_BUTTON_UP>(
				boost::bind(
					&tscroll_container::signal_handler_left_button_up
					, this
					, _5
					, false)
				, event::tdispatcher::back_child);

		content_grid_->connect_signal<event::MOUSE_LEAVE>(boost::bind(
					&tscroll_container::signal_handler_mouse_leave
					, this
					, _5
					, true)
				 , event::tdispatcher::back_pre_child);

		content_grid_->connect_signal<event::MOUSE_LEAVE>(boost::bind(
					&tscroll_container::signal_handler_mouse_leave
					, this
					, _5
					, false)
				 , event::tdispatcher::back_child);

		content_grid_->connect_signal<event::MOUSE_MOTION>(
				boost::bind(
					&tscroll_container::signal_handler_mouse_motion
					, this
					, _3
					, _4
					, _5
					, _6
					, false)
				, event::tdispatcher::back_child);

		content_grid_->connect_signal<event::MOUSE_MOTION>(
				boost::bind(
					&tscroll_container::signal_handler_mouse_motion
					, this
					, _3
					, _4
					, _5
					, _6
					, true)
				, event::tdispatcher::back_post_child);

		connect_signal<event::OUT_OF_CHAIN>(
				boost::bind(
					&tscroll_container::signal_handler_out_of_chain
					, this
					, _5
					, _6)
				, event::tdispatcher::back_child);
	}
}

void tscroll_container::set_vertical_scrollbar_mode(const tscrollbar_mode scrollbar_mode)
{
	vertical_scrollbar_mode_ = scrollbar_mode;
}

void tscroll_container::set_horizontal_scrollbar_mode(const tscrollbar_mode scrollbar_mode)
{
	horizontal_scrollbar_mode_ = scrollbar_mode;
	{
		int ii = 0;
		horizontal_scrollbar_mode_ = always_invisible;
	}
}

void tscroll_container::impl_draw_children(
		  texture& frame_buffer
		, int x_offset
		, int y_offset)
{
	assert(get_visible() == twidget::VISIBLE
			&& content_grid_->get_visible() == twidget::VISIBLE);

	// Inherited.
	tcontainer_::impl_draw_children(frame_buffer, x_offset, y_offset);

	content_grid_->draw_children(frame_buffer, x_offset, y_offset);
}

void tscroll_container::broadcast_frame_buffer(texture& frame_buffer)
{
	tcontainer_::broadcast_frame_buffer(frame_buffer);

	content_grid_->broadcast_frame_buffer(frame_buffer);
}

void tscroll_container::clear_texture()
{
	tcontainer_::clear_texture();
	content_grid_->clear_texture();
}

void tscroll_container::layout_children()
{
	if (need_layout_) {
		place(get_origin(), get_size());

		// since place scroll_container again, set it dirty.
		set_dirty();

	} else {
		// Inherited.
		tcontainer_::layout_children();

		content_grid_->layout_children();
	}
}

void tscroll_container::invalidate_layout()
{
	need_layout_ = true;
}

void tscroll_container::child_populate_dirty_list(twindow& caller, const std::vector<twidget*>& call_stack)
{
	// Inherited.
	tcontainer_::child_populate_dirty_list(caller, call_stack);

	std::vector<twidget*> child_call_stack(call_stack);
	content_grid_->populate_dirty_list(caller, child_call_stack);
}

void tscroll_container::popup_new_window()
{
	if (vertical_scrollbar2_->widget->parent() == content_grid_) {
		reset_scrollbar(*get_window());
	}
	// content_grid_->popup_new_window();
}

void tscroll_container::show_content_rect(const SDL_Rect& rect)
{
	if (content_grid_->get_height() <= content_->get_height()) {
		return;
	}

	VALIDATE(rect.y >= 0, null_str);
	VALIDATE(horizontal_scrollbar_ && vertical_scrollbar_, null_str);

	// posix_print("show_content_rect------rect(%i, %i, %i, %i), item_position: %i\n", 
	//	rect.x, rect.y, rect.w, rect.h, vertical_scrollbar_->get_item_position());

	// bottom. make rect's bottom align to content_'s bottom.
	const int wanted_bottom = rect.y + rect.h;
	const int current_bottom = vertical_scrollbar_->get_item_position() + content_->get_height();
	int distance = wanted_bottom - current_bottom;
	if (distance > 0) {
		// posix_print("show_content_rect, setp1, move from %i, to %i\n", vertical_scrollbar_->get_item_position(), vertical_scrollbar_->get_item_position() + distance);
		vertical_set_item_position(vertical_scrollbar_->get_item_position() + distance);
	}

	// top. make rect's top align to content_'s top.
	if (rect.y < static_cast<int>(vertical_scrollbar_->get_item_position())) {
		// posix_print("show_content_rect, setp2, move from %i, to %i\n", vertical_scrollbar_->get_item_position(), rect.y);
		vertical_set_item_position(rect.y);
	}

	if (vertical_scrollbar_ != dummy_vertical_scrollbar_) {
		dummy_vertical_scrollbar_->set_item_position(vertical_scrollbar_->get_item_position());
	}

	// Update.
	// scrollbar_moved(true);
}

void tscroll_container::vertical_set_item_position(const unsigned item_position)
{
	vertical_scrollbar_->set_item_position(item_position);
	if (dummy_vertical_scrollbar_ != vertical_scrollbar_) {
		dummy_vertical_scrollbar_->set_item_position(item_position);
	}
}

void tscroll_container::horizontal_set_item_position(const unsigned item_position)
{
	horizontal_scrollbar_->set_item_position(item_position);
	if (dummy_horizontal_scrollbar_ != horizontal_scrollbar_) {
		dummy_horizontal_scrollbar_->set_item_position(item_position);
	}
}

void tscroll_container::vertical_scrollbar_moved()
{ 
	VALIDATE(get_visible() == twidget::VISIBLE, null_str);

	scrollbar_moved();

	VALIDATE(vertical_scrollbar_ != dummy_vertical_scrollbar_, null_str);
	dummy_vertical_scrollbar_->set_item_position(vertical_scrollbar_->get_item_position());
}

void tscroll_container::horizontal_scrollbar_moved()
{ 
	VALIDATE(get_visible() == twidget::VISIBLE, null_str);

	scrollbar_moved();

	VALIDATE(horizontal_scrollbar_ != dummy_horizontal_scrollbar_, null_str);
	dummy_horizontal_scrollbar_->set_item_position(horizontal_scrollbar_->get_item_position());
}

void tscroll_container::scroll_vertical_scrollbar(
		const tscrollbar_::tscroll scroll)
{
	assert(vertical_scrollbar_);

	vertical_scrollbar_->scroll(scroll);
	scrollbar_moved();
}

void tscroll_container::handle_key_home(SDL_Keymod /*modifier*/, bool& handled)
{
	assert(vertical_scrollbar_ && horizontal_scrollbar_);

	vertical_scrollbar_->scroll(tscrollbar_::BEGIN);
	horizontal_scrollbar_->scroll(tscrollbar_::BEGIN);
	scrollbar_moved();

	handled = true;
}

void tscroll_container::handle_key_end(SDL_Keymod /*modifier*/, bool& handled)
{
	assert(vertical_scrollbar_);

	vertical_scrollbar_->scroll(tscrollbar_::END);
	scrollbar_moved();

	handled = true;
}

void tscroll_container::
		handle_key_page_up(SDL_Keymod /*modifier*/, bool& handled)
{
	assert(vertical_scrollbar_);

	vertical_scrollbar_->scroll(tscrollbar_::JUMP_BACKWARDS);
	scrollbar_moved();

	handled = true;
}

void tscroll_container::
		handle_key_page_down(SDL_Keymod /*modifier*/, bool& handled)

{
	assert(vertical_scrollbar_);

	vertical_scrollbar_->scroll(tscrollbar_::JUMP_FORWARD);
	scrollbar_moved();

	handled = true;
}

void tscroll_container::
		handle_key_up_arrow(SDL_Keymod /*modifier*/, bool& handled)
{
	assert(vertical_scrollbar_);

	vertical_scrollbar_->scroll(tscrollbar_::ITEM_BACKWARDS);
	scrollbar_moved();

	handled = true;
}

void tscroll_container::
		handle_key_down_arrow( SDL_Keymod /*modifier*/, bool& handled)
{
	assert(vertical_scrollbar_);

	vertical_scrollbar_->scroll(tscrollbar_::ITEM_FORWARD);
	scrollbar_moved();

	handled = true;
}

void tscroll_container
		::handle_key_left_arrow(SDL_Keymod /*modifier*/, bool& handled)
{
	assert(horizontal_scrollbar_);

	horizontal_scrollbar_->scroll(tscrollbar_::ITEM_BACKWARDS);
	scrollbar_moved();

	handled = true;
}

void tscroll_container
		::handle_key_right_arrow(SDL_Keymod /*modifier*/, bool& handled)
{
	assert(horizontal_scrollbar_);

	horizontal_scrollbar_->scroll(tscrollbar_::ITEM_FORWARD);
	scrollbar_moved();

	handled = true;
}

void tscroll_container::scrollbar_moved(bool gc_handled)
{
	/*** Update the content location. ***/
	int x_offset = horizontal_scrollbar_->get_item_position();
	int y_offset = vertical_scrollbar_->get_item_position();

	if (!gc_handled) {
		// if previous handled gc, do not, until mini_set_content_grid_origin, miin_set_content_grid_visible_area.
		y_offset = mini_handle_gc(x_offset, y_offset);
	}

	adjust_offset(x_offset, y_offset);
	const tpoint content_grid_origin(content_->get_x() - x_offset, content_->get_y() - y_offset);

	mini_set_content_grid_origin(content_->get_origin(), content_grid_origin);
	mini_set_content_grid_visible_area(content_visible_area_);
}

const std::string& tscroll_container::get_control_type() const
{
	static const std::string type = "scrollbar_container";
	return type;
}

void tscroll_container::signal_handler_sdl_key_down(
		const event::tevent event
		, bool& handled
		, const SDL_Keycode key
		, SDL_Keymod modifier)
{
	switch(key) {
		case SDLK_HOME :
			handle_key_home(modifier, handled);
			break;

		case SDLK_END :
			handle_key_end(modifier, handled);
			break;


		case SDLK_PAGEUP :
			handle_key_page_up(modifier, handled);
			break;

		case SDLK_PAGEDOWN :
			handle_key_page_down(modifier, handled);
			break;


		case SDLK_UP :
			handle_key_up_arrow(modifier, handled);
			break;

		case SDLK_DOWN :
			handle_key_down_arrow(modifier, handled);
			break;

		case SDLK_LEFT :
			handle_key_left_arrow(modifier, handled);
			break;

		case SDLK_RIGHT :
			handle_key_right_arrow(modifier, handled);
			break;
		default:
			/* ignore */
			break;
		}
}

void tscroll_container::scroll_timer_handler(const bool vertical, const bool up, const int level)
{
	if (scroll_elapse_.first != scroll_elapse_.second) {
		VALIDATE(scroll_elapse_.first > scroll_elapse_.second, null_str);
		const bool scrolled = scroll(vertical, up, level, false);
		if (scrolled) {
			scrollbar_moved();
			scroll_elapse_.second ++;
		} else {
			scroll_elapse_.first = scroll_elapse_.second;
		}
	}
	if (next_scrollbar_invisible_ticks_) {
		if (SDL_GetTicks() >= next_scrollbar_invisible_ticks_) {
			reset_scrollbar(*get_window());
		}
	}
	bool scroll_can_remove = scroll_elapse_.first == scroll_elapse_.second;
	bool invisible_can_remove = !next_scrollbar_invisible_ticks_;
	if (scroll_can_remove && invisible_can_remove) {
		// posix_print("------tscroll_container::scroll_timer_handler, will remove timer.\n");
		scroll_timer_.reset();
	}
}

bool tscroll_container::scroll(const bool vertical, const bool up, const int level, const bool first)
{
	VALIDATE(level > 0, null_str);

	tscrollbar_& scrollbar = vertical? *vertical_scrollbar_: *horizontal_scrollbar_;

	const bool wheel = level > tevent_handler::swipe_wheel_level_gap;
	int level2 = wheel? level - tevent_handler::swipe_wheel_level_gap: level;
	const int offset = level2 * scrollbar.get_visible_items() / tevent_handler::swipe_max_normal_level;
	const unsigned int item_position = scrollbar.get_item_position();
	const unsigned int item_count = scrollbar.get_item_count();
	const unsigned int visible_items = scrollbar.get_visible_items();
	unsigned int item_position2;
	if (up) {
		item_position2 = item_position + offset;
		item_position2 = item_position2 > item_count - visible_items? item_count - visible_items: item_position2;
	} else {
		item_position2 = (int)item_position >= offset? item_position - offset : 0;
	}
	if (item_position2 == item_position) {
		return false;
	}
	scrollbar.set_item_position(item_position2);
	if (!wheel && first) {
		// [3, 10]
		const int min_times = 3;
		const int max_times = 10;
		int times = min_times + (max_times - min_times) * level2 / tevent_handler::swipe_max_normal_level;
		scroll_elapse_ = std::make_pair(times, 0);
		scroll_timer_.reset(add_timer(200, boost::bind(&tscroll_container::scroll_timer_handler, this, vertical, up, level)));
	}
	if (vertical) {
		if (dummy_vertical_scrollbar_ != vertical_scrollbar_) {
			dummy_vertical_scrollbar_->set_item_position(item_position2);
		}
	} else {
		if (dummy_horizontal_scrollbar_ != horizontal_scrollbar_) {
			dummy_horizontal_scrollbar_->set_item_position(item_position2);
		}
	}
	return true;
}

void tscroll_container::signal_handler_sdl_wheel_up(bool& handled, const tpoint& coordinate2)
{
	VALIDATE(vertical_scrollbar_, null_str);
	mini_wheel();

	if (scroll(true, true, coordinate2.y, true)) {
		scrollbar_moved();
	}
	handled = true;
}

void tscroll_container::signal_handler_sdl_wheel_down(bool& handled, const tpoint& coordinate2)
{
	VALIDATE(vertical_scrollbar_, null_str);
	mini_wheel();

	if (scroll(true, false, coordinate2.y, true)) {
		scrollbar_moved();
	}
	handled = true;
}

void tscroll_container::signal_handler_sdl_wheel_left(bool& handled, const tpoint& coordinate2)
{
	VALIDATE(horizontal_scrollbar_, null_str);
	mini_wheel();

	if (scroll(false, true, coordinate2.x, true)) {
		scrollbar_moved();
	}
	handled = true;
}

void tscroll_container::signal_handler_sdl_wheel_right(bool& handled, const tpoint& coordinate2)
{
	VALIDATE(horizontal_scrollbar_, null_str);
	mini_wheel();

	if (scroll(false, false, coordinate2.x, true)) {
		scrollbar_moved();
	}
	handled = true;
}

void tscroll_container::signal_handler_mouse_enter(const tpoint& coordinate, bool pre_child)
{
	twindow* window = get_window();

	if (next_scrollbar_invisible_ticks_) {
		next_scrollbar_invisible_ticks_ = 0;
	}

	if (vertical_scrollbar_ == dummy_vertical_scrollbar_) {
		// posix_print("tscroll_container::signal_handler_mouse_enter------, %s\n", id().empty()? "<nil>": id().c_str());
		VALIDATE(horizontal_scrollbar_ == dummy_horizontal_scrollbar_, null_str);
		if (vertical_scrollbar2_->widget.get()->parent() != window) {
			// scrollbar is in other scroll_container.
			VALIDATE(vertical_scrollbar2_->widget.get()->parent() != content_grid_, null_str);
			window->reset_scrollbar();
		}

		vertical_scrollbar2_->set_ref_widget(this, null_point);
		vertical_scrollbar2_->set_visible(true);
		vertical_scrollbar2_->widget->set_parent(content_grid_);
		tscrollbar_& vertical_scrollbar = *dynamic_cast<tscrollbar_*>(vertical_scrollbar2_->widget.get());
		set_scrollbar_mode(vertical_scrollbar, vertical_scrollbar_mode_, vertical_scrollbar_->get_item_count(), vertical_scrollbar_->get_visible_items());
		vertical_scrollbar.set_item_position(vertical_scrollbar_->get_item_position());
		vertical_scrollbar.set_did_modified(boost::bind(&tscroll_container::vertical_scrollbar_moved, this));

		vertical_scrollbar_ = &vertical_scrollbar;

		VALIDATE(horizontal_scrollbar2_->widget.get()->parent() == window, null_str);
		horizontal_scrollbar2_->set_ref_widget(this, null_point);
		horizontal_scrollbar2_->set_visible(true);
		horizontal_scrollbar2_->widget->set_parent(content_grid_);
		tscrollbar_& horizontal_scrollbar = *dynamic_cast<tscrollbar_*>(horizontal_scrollbar2_->widget.get());
		set_scrollbar_mode(horizontal_scrollbar, horizontal_scrollbar_mode_, horizontal_scrollbar_->get_item_count(), horizontal_scrollbar_->get_visible_items());
		horizontal_scrollbar.set_item_position(horizontal_scrollbar_->get_item_position());
		horizontal_scrollbar.set_did_modified(boost::bind(&tscroll_container::horizontal_scrollbar_moved, this));
		horizontal_scrollbar_ = &horizontal_scrollbar;

		// posix_print("------tscroll_container::signal_handler_mouse_enter\n");
	}
}

void tscroll_container::signal_handler_left_button_down(const tpoint& coordinate, bool pre_child)
{
	twindow* window = get_window();
	const twidget* keyboard_capture_widget = window->keyboard_capture_widget();
	if (!keyboard_capture_widget || keyboard_capture_widget->captured_keyboard_can_to_scroll_container()) {
		// why scroll_container require capture keyboard?
		// 1)down/up move row in listbox.
		// 2)down/up/left/right move content_grid_ when move.
		// but scroll_text_box must not capture keyboard. it's text_box require capture keyboqrd always.
		get_window()->keyboard_capture(this);
	}

	if (pre_child) {
		if (vertical_scrollbar2_->widget->get_visible() == twidget::VISIBLE) {
			if (point_in_rect(coordinate.x, coordinate.y, vertical_scrollbar2_->widget->get_rect())) {
				return;
			}
		}
		if (horizontal_scrollbar2_->widget->get_visible() == twidget::VISIBLE) {
			if (point_in_rect(coordinate.x, coordinate.y, horizontal_scrollbar2_->widget->get_rect())) {
				return;
			}
		}
	}

	VALIDATE(point_in_rect(coordinate.x, coordinate.y, content_->get_rect()), null_str);

	if (scroll_timer_.valid()) {
		scroll_elapse_.first = scroll_elapse_.second = 0;
	}

	VALIDATE(is_null_coordinate(first_coordinate_), null_str);
	first_coordinate_ = coordinate;

	mini_mouse_down(first_coordinate_);
}

void tscroll_container::signal_handler_left_button_up(const tpoint& coordinate, bool pre_child)
{
	if (is_null_coordinate(first_coordinate_)) {
		return;
	}

	twindow* window = get_window();
	VALIDATE(window->mouse_focus_widget() || point_in_rect(coordinate.x, coordinate.y, content_->get_rect()), null_str);
	set_null_coordinate(first_coordinate_);
	mini_mouse_leave(first_coordinate_, coordinate);
}

void tscroll_container::reset_scrollbar(twindow& window)
{
	// posix_print("------tscroll_container::reset_scrollbar------%s\n", id().empty()? "<nil>": id().c_str());

	VALIDATE(vertical_scrollbar_ == vertical_scrollbar2_->widget.get() && horizontal_scrollbar_ == horizontal_scrollbar2_->widget.get(), null_str);

	vertical_scrollbar2_->set_ref_widget(nullptr, null_point);
	vertical_scrollbar2_->widget->set_parent(&window);

	tscrollbar_* scrollbar = dynamic_cast<tscrollbar_*>(vertical_scrollbar2_->widget.get());
	scrollbar->set_did_modified(NULL);
	vertical_scrollbar_ = dummy_vertical_scrollbar_;

	horizontal_scrollbar2_->set_ref_widget(nullptr, null_point);
	horizontal_scrollbar2_->widget->set_parent(&window);

	scrollbar = dynamic_cast<tscrollbar_*>(horizontal_scrollbar2_->widget.get());
	scrollbar->set_did_modified(NULL);
	horizontal_scrollbar_ = dummy_horizontal_scrollbar_;

	if (next_scrollbar_invisible_ticks_) {
		next_scrollbar_invisible_ticks_ = 0;
	}
}

void tscroll_container::signal_handler_mouse_leave(const tpoint& coordinate, bool pre_child)
{
	twindow* window = get_window();

	// maybe will enter float_widget, and this float_widget is in content_.
	if (vertical_scrollbar_ == vertical_scrollbar2_->widget.get()) {
		// althogh enter set vertical_scrollbar2_ to vertical_scroolbar_, but maybe not equal.
		// 1. popup new dialog. tdialog::show will call reset_scrollbar. reseted.
		// 2. mouse_leave call this function. 
		VALIDATE(horizontal_scrollbar_ == horizontal_scrollbar2_->widget.get(), null_str);
		if (!point_in_rect(coordinate.x, coordinate.y, content_->get_rect())) {
			const int scrollbar_out_threshold = 1000;
			next_scrollbar_invisible_ticks_ = SDL_GetTicks() + scrollbar_out_threshold;
			if (!scroll_timer_.valid()) {
				scroll_timer_.reset(add_timer(200, boost::bind(&tscroll_container::scroll_timer_handler, this, false, false, 0)));
			}
		}
	}

	if (is_null_coordinate(first_coordinate_)) {
		return;
	}

	if (is_magic_coordinate(coordinate) || (window->mouse_focus_widget() != content_grid_ && !window->point_in_normal_widget(coordinate.x, coordinate.y, *content_))) {
		set_null_coordinate(first_coordinate_);
		mini_mouse_leave(first_coordinate_, coordinate);
	}
}

void tscroll_container::signal_handler_mouse_motion(bool& handled, bool& halt, const tpoint& coordinate, const tpoint& coordinate2, bool pre_child)
{
/*
	posix_print("tscroll_container::signal_handler_mouse_motion, coordinate(%i, %i), coordinate2(%i, %i), id: %s, first_coordinate_(%i, %i)\n", 
		coordinate.x, coordinate.y, coordinate2.x, coordinate2.y,
		id().empty()? "<nil>": id().c_str(), first_coordinate_.x, first_coordinate_.y);
*/
	if (is_null_coordinate(first_coordinate_)) {
		return;
	}

	twindow* window = get_window();
	VALIDATE(window->mouse_focus_widget() || point_in_rect(coordinate.x, coordinate.y, content_->get_rect()), null_str);

	const int abs_diff_x = abs(coordinate.x - first_coordinate_.x);
	const int abs_diff_y = abs(coordinate.y - first_coordinate_.y);
	const twidget* current_mouse_focus = window->mouse_focus_widget();
	// first scroll_container must capture mouse.
	if (!current_mouse_focus || current_mouse_focus->captured_mouse_can_to_scroll_container()) {
		// content_grid_ must not change captured_mouse to scroll_container. so current_mouse_focus must not is this's content_grid_.
		VALIDATE(current_mouse_focus != content_grid_, null_str);

		if (abs_diff_x >= settings::clear_click_threshold || abs_diff_x >= settings::clear_click_threshold) {
			if (pre_child) {
				window->mouse_capture(content_grid_);

			} else {
				window->mouse_capture(content_grid_);
			}
		}
	}

	if (window->mouse_click_widget()) {
		if (abs_diff_x >= settings::clear_click_threshold || abs_diff_x >= settings::clear_click_threshold) {
			// since will move, must not trigger mouse_click event.
			window->clear_mouse_click();
		}
	}

	if (!mini_mouse_motion(first_coordinate_, coordinate)) {
		return;
	}

	if (refuse_scroll) {
		return;
	}

	if (!can_scroll_) {
		return;
	}

	current_mouse_focus = window->mouse_focus_widget();
	if (current_mouse_focus && !current_mouse_focus->parent()->is_scroll_container()) {
		// current has one control capturing mouse(for example text_box), 
		// and can not change captured mouse to this. --->halt.
		return;
	}

	VALIDATE(vertical_scrollbar_ && horizontal_scrollbar_, null_str);
	int abs_x_offset = abs(coordinate2.x);
	int abs_y_offset = abs(coordinate2.y);
/*	
	if (!game_config::mobile) {
		abs_y_offset = 0;
	}
*/
	if (abs_y_offset >= abs_x_offset) {
		abs_x_offset = 0;
	} else {
		abs_y_offset = 0;
	}

	if (abs_y_offset) {
		unsigned int item_position = vertical_scrollbar_->get_item_position();
		if (coordinate2.y < 0) {
			item_position = item_position + abs_y_offset;
		} else {
			item_position = (int)item_position >= abs_y_offset? item_position - abs_y_offset: 0;
		}
		vertical_set_item_position(item_position);
	}

	if (abs_x_offset) {
		unsigned int item_position = horizontal_scrollbar_->get_item_position();
		if (coordinate2.x < 0) {
			item_position = item_position + abs_x_offset;
		} else {
			item_position = (int)item_position >= abs_x_offset? item_position - abs_x_offset: 0;
		}
		horizontal_set_item_position(item_position);
	}

	if (coordinate2.x || coordinate2.y) {
		scrollbar_moved();
	}

	// this scroll_container control handle this scroll. other scroll_container control don't handle it.
	halt = true;
}

} // namespace gui2

