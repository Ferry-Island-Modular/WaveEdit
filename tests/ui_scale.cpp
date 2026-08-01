#include "src/uiscale.hpp"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static bool eq(float a, float b) {
	return fabsf(a - b) < 1e-6f;
}

int main() {
	const char *source = NULL;

	// Nothing set anywhere: default 1.0
	assert(eq(uiScaleResolve(0.0f, NULL, NULL, NULL, 0.0f, &source), 1.0f));
	assert(strcmp(source, "default") == 0);

	// System content scale used when no overrides
	assert(eq(uiScaleResolve(0.0f, NULL, NULL, NULL, 2.0f, &source), 2.0f));
	assert(strcmp(source, "system") == 0);

	// GDK_SCALE beats system scale
	assert(eq(uiScaleResolve(0.0f, NULL, "2", NULL, 1.5f), 2.0f));

	// GDK_SCALE and GDK_DPI_SCALE multiply (GTK convention); either alone works
	assert(eq(uiScaleResolve(0.0f, NULL, "2", "0.75", 1.0f), 1.5f));
	assert(eq(uiScaleResolve(0.0f, NULL, NULL, "2", 1.0f), 2.0f));

	// WAVEEDIT_UI_SCALE beats GDK and system
	assert(eq(uiScaleResolve(0.0f, "1.75", "2", NULL, 3.0f, &source), 1.75f));
	assert(strcmp(source, "WAVEEDIT_UI_SCALE") == 0);

	// Manual override beats everything
	assert(eq(uiScaleResolve(1.25f, "1.75", "2", NULL, 3.0f, &source), 1.25f));
	assert(strcmp(source, "manual") == 0);

	// Unparseable, empty, zero, or negative env values are ignored
	assert(eq(uiScaleResolve(0.0f, "banana", NULL, NULL, 1.5f), 1.5f));
	assert(eq(uiScaleResolve(0.0f, NULL, "", NULL, 1.0f), 1.0f));
	assert(eq(uiScaleResolve(0.0f, "0", NULL, NULL, 1.5f), 1.5f));
	assert(eq(uiScaleResolve(0.0f, "-2", NULL, NULL, 1.5f), 1.5f));

	// Results clamp to [0.5, 4.0] no matter the layer
	assert(eq(uiScaleResolve(0.0f, "9", NULL, NULL, 1.0f), 4.0f));
	assert(eq(uiScaleResolve(0.0f, "0.1", NULL, NULL, 1.0f), 0.5f));
	assert(eq(uiScaleResolve(8.0f, NULL, NULL, NULL, 1.0f), 4.0f));
	assert(eq(uiScaleResolve(0.0f, NULL, NULL, NULL, 7.0f), 4.0f));

	printf("ui scale tests passed\n");
	return 0;
}
