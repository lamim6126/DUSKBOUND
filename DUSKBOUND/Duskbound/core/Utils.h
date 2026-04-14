#ifndef UTILS_H
#define UTILS_H

// Inline utility function for clamping values
// Avoids std::max/min to prevent conflicts with Windows macros
inline float clampf(float v, float lo, float hi) {
	if (v < lo) return lo;
	if (v > hi) return hi;
	return v;
}

#endif