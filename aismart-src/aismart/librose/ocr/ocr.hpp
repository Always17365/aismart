#ifndef LIBROSE_OCR_OCR_HPP_INCLUDED
#define LIBROSE_OCR_OCR_HPP_INCLUDED

#include "util.hpp"
#include "config.hpp"
#include <set>

struct trect
{
	trect(const int x, const int y, const int w, const int h)
		: x(x)
		, y(y)
		, w(w)
		, h(h)
		{}

	trect(const SDL_Rect& rect)
		: x(rect.x)
		, y(rect.y)
		, w(rect.w)
		, h(rect.h)
		{}

	/** x coodinate. */
	int x;

	/** y coodinate. */
	int y;

	int w;
	int h;

	trect& operator=(const SDL_Rect& rect) 
	{ 
		x = rect.x; 
		y = rect.y;
		w = rect.w;
		h = rect.h;
		return *this;
	}

	bool operator==(const trect& rect) const 
	{ 
		return x == rect.x && y == rect.y && w == rect.w && h == rect.h;
	}

	bool operator!=(const trect& rect) const 
	{ 
		return x != rect.x || y != rect.y || w != rect.w || h != rect.h;
	}

	bool operator<(const trect& rect) const 
	{ 
		if (x != rect.x) {
			return x < rect.x;
		}
		if (y != rect.y) {
			return y < rect.y;
		}
		if (w != rect.w) {
			return w < rect.w;
		}
		return h < rect.h;
	}

	bool point_in(const int _x, const int _y) const
	{
		return _x >= x && _y >= y && _x < x + w && _y < y + h;
	}

	bool rect_overlap(const trect& rect) const
	{
		return rect.x < x + w && x < rect.x + rect.w && rect.y < y + h && y < rect.y + rect.h;
	}

	bool rect_overlap2(const SDL_Rect& rect) const
	{
		return rect.x < x + w && x < rect.x + rect.w && rect.y < y + h && y < rect.y + rect.h;
	}

	SDL_Rect to_SDL_Rect() const { return SDL_Rect{x, y, w, h}; }
};

class tocr_line 
{
public:
	tocr_line(const std::set<trect>& _chars)
		: chars(_chars)
		, field()
	{}

	void calculate_bounding_rect();

	SDL_Rect bounding_rect;
	std::set<trect> chars;
	std::string field; // coresponding field
};

struct tocr_result 
{
	explicit tocr_result(const std::string& chars, uint32_t used_ms)
		: chars(chars)
		, used_ms(used_ms)
	{}

	std::string chars;
	uint32_t used_ms;
};

#endif

