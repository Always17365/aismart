#define GETTEXT_DOMAIN "rose-lib"

#include "ocr/ocr.hpp"
#include "serialization/string_utils.hpp"
#include "sdl_utils.hpp"
#include "wml_exception.hpp"

void tocr_line::calculate_bounding_rect()
{
	VALIDATE(!chars.empty(), null_str);
	bounding_rect = create_rect(INT_MAX, INT_MAX, -1, -1);
	for (std::set<trect>::const_iterator it = chars.begin(); it != chars.end(); ++ it) {
		const trect& rect = *it;
		if (rect.x < bounding_rect.x) {
			bounding_rect.x = rect.x;
		}
		if (rect.y < bounding_rect.y) {
			bounding_rect.y = rect.y;
		}

		if (rect.x + rect.w > bounding_rect.w) {
			bounding_rect.w = rect.x + rect.w;
		}
		if (rect.y + rect.h > bounding_rect.h) {
			bounding_rect.h = rect.y + rect.h;
		}
	}

	bounding_rect.w -= bounding_rect.x;
	bounding_rect.h -= bounding_rect.y;
}

