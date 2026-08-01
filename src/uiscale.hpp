#pragma once

// Global UI scale factor (HiDPI support).
//
// uiScaleResolve() is pure logic so it can be unit-tested (tests/ui_scale.cpp).
// Precedence, highest first:
//   1. manualOverride  — menu selection persisted in ui.dat (0 = Auto)
//   2. envApp          — WAVEEDIT_UI_SCALE environment variable
//   3. envGdkScale × envGdkDpiScale — GTK convention environment variables
//   4. systemScale     — content scale reported by the windowing system
//                        (0 = unavailable)
//   5. 1.0
// The result is clamped to [0.5, 4.0]. Unparseable, zero, or negative
// strings are ignored. If outSource is non-NULL it receives a static string
// naming the layer that won ("manual", "WAVEEDIT_UI_SCALE", "GDK_SCALE",
// "system", "default") for the startup log line.
float uiScaleResolve(float manualOverride, const char *envApp,
	const char *envGdkScale, const char *envGdkDpiScale, float systemScale,
	const char **outSource = 0);
