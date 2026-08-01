#include "uiscale.hpp"

#include <stdlib.h>

static float clampScale(float s) {
	if (s < 0.5f) return 0.5f;
	if (s > 4.0f) return 4.0f;
	return s;
}

// Parse a positive float from an environment variable value.
// Returns 0 for NULL, empty, unparseable, zero, or negative input.
static float parseEnvScale(const char *s) {
	if (!s || !*s) return 0.0f;
	char *end = NULL;
	float v = strtof(s, &end);
	if (end == s || v <= 0.0f) return 0.0f;
	return v;
}

float uiScaleResolve(float manualOverride, const char *envApp,
	const char *envGdkScale, const char *envGdkDpiScale, float systemScale,
	const char **outSource) {
	float appScale = parseEnvScale(envApp);
	float gdkScale = parseEnvScale(envGdkScale);
	float gdkDpiScale = parseEnvScale(envGdkDpiScale);

	const char *source;
	float scale;
	if (manualOverride > 0.0f) {
		source = "manual";
		scale = manualOverride;
	}
	else if (appScale > 0.0f) {
		source = "WAVEEDIT_UI_SCALE";
		scale = appScale;
	}
	else if (gdkScale > 0.0f || gdkDpiScale > 0.0f) {
		// GTK convention: GDK_SCALE (integer window scale) and GDK_DPI_SCALE
		// (fractional font scale) multiply; either may be set alone.
		source = "GDK_SCALE";
		scale = (gdkScale > 0.0f ? gdkScale : 1.0f)
			* (gdkDpiScale > 0.0f ? gdkDpiScale : 1.0f);
	}
	else if (systemScale > 0.0f) {
		source = "system";
		scale = systemScale;
	}
	else {
		source = "default";
		scale = 1.0f;
	}

	if (outSource) *outSource = source;
	return clampScale(scale);
}
