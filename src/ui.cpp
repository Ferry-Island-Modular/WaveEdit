#include "WaveEdit.hpp"

#ifdef ARCH_LIN
	#include <linux/limits.h>
#endif
#ifdef ARCH_MAC
	#include <sys/syslimits.h>
#endif
#include <libgen.h>

#include <SDL.h>
#include <SDL_opengl.h>

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"

#include "theme.hpp"

#include "osdialog/osdialog.h"

#include "lodepng/lodepng.h"

#include "tablabels.hpp"


static bool showTestWindow = false;
static ImTextureID logoTextureLight;
static ImTextureID logoTextureDark;
static ImTextureID logoTexture;
char lastFilename[1024] = "";
static int currentThemeId = -1;
ImFont* fontMono = NULL;
int selectedId = 0;
int lastSelectedId = 0;


enum Page {
	EDITOR_PAGE = 0,
	EFFECT_PAGE,
	GRID_PAGE,
	WATERFALL_PAGE,
	IMPORT_PAGE,
	NUM_PAGES
};

Page currentPage = EDITOR_PAGE;


static ImVec4 lighten(ImVec4 col, float p) {
	col.x = crossf(col.x, 1.0, p);
	col.y = crossf(col.y, 1.0, p);
	col.z = crossf(col.z, 1.0, p);
	col.w = crossf(col.w, 1.0, p);
	return col;
}

static ImVec4 darken(ImVec4 col, float p) {
	col.x = crossf(col.x, 0.0, p);
	col.y = crossf(col.y, 0.0, p);
	col.z = crossf(col.z, 0.0, p);
	col.w = crossf(col.w, 0.0, p);
	return col;
}

static ImVec4 alpha(ImVec4 col, float a) {
	col.w *= a;
	return col;
}


static ImTextureID loadImage(const char *filename) {
	GLuint textureId;
	glGenTextures(1, &textureId);
	glBindTexture(GL_TEXTURE_2D, textureId);
	unsigned char *pixels;
	unsigned int width, height;
	unsigned err = lodepng_decode_file(&pixels, &width, &height, filename, LCT_RGBA, 8);
	assert(!err);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	glBindTexture(GL_TEXTURE_2D, 0);
	return (ImTextureID) textureId;
}


static void getImageSize(ImTextureID id, int *width, int *height) {
	GLuint textureId = (GLuint)(intptr_t) id;
	glBindTexture(GL_TEXTURE_2D, textureId);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, width);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, height);
	glBindTexture(GL_TEXTURE_2D, 0);
}


static void selectWave(int waveId) {
	selectedId = waveId;
	lastSelectedId = selectedId;
	morphX = (float)(selectedId % BANK_GRID_WIDTH);
	morphY = (float)(selectedId / BANK_GRID_WIDTH);
	morphZ = (float)selectedId;
}


static void refreshMorphSnap() {
	if (!morphInterpolate && morphZSpeed <= 0.f) {
		morphX = roundf(morphX);
		morphY = roundf(morphY);
		morphZ = roundf(morphZ);
	}
}

/** Focuses to a page which displays the current bank, useful when loading a new bank and showing the user some visual feedback that the bank has changed. */
static void showCurrentBankPage() {
	switch (currentPage) {
		case EFFECT_PAGE:
		case IMPORT_PAGE:
			currentPage = EDITOR_PAGE;
			break;
		default:
			break;
	}
}

static void menuManual() {
	openBrowser("manual.pdf");
}

static void menuWebsite() {
	openBrowser("http://synthtech.com/waveedit");
}

static void menuNewBank() {
	showCurrentBankPage();
	currentBank.clear();
	lastFilename[0] = '\0';
	historyPush();
}

/** Caller must free() return value, guaranteed to not be NULL */
static char *getLastDir() {
	if (lastFilename[0] == '\0') {
		return strdup(".");
	}
	else {
		char filename[PATH_MAX];
		strncpy(filename, lastFilename, sizeof(filename));
		char *dir = dirname(filename);
		return strdup(dir);
	}
}

static void menuOpenBank() {
	char *dir = getLastDir();
	char *path = osdialog_file(OSDIALOG_OPEN, dir, NULL, NULL);
	if (path) {
		showCurrentBankPage();
		currentBank.loadWAV(path);
		snprintf(lastFilename, sizeof(lastFilename), "%s", path);
		historyPush();
		free(path);
	}
	free(dir);
}

static void menuSaveBankAs() {
	char *dir = getLastDir();
	char *path = osdialog_file(OSDIALOG_SAVE, dir, "Untitled.wav", NULL);
	if (path) {
		currentBank.saveWAV(path);
		snprintf(lastFilename, sizeof(lastFilename), "%s", path);
		free(path);
	}
	free(dir);
}

static void menuSaveBank() {
	if (lastFilename[0] != '\0')
		currentBank.saveWAV(lastFilename);
	else
		menuSaveBankAs();
}

static void menuSaveWaves() {
	char *dir = getLastDir();
	char *path = osdialog_file(OSDIALOG_OPEN_DIR, dir, NULL, NULL);
	if (path) {
		currentBank.saveWaves(path);
		free(path);
	}
	free(dir);
}

static void menuQuit() {
	SDL_Event event;
	event.type = SDL_QUIT;
	SDL_PushEvent(&event);
}

static void menuSelectAll() {
	selectedId = 0;
	lastSelectedId = BANK_LEN-1;
}

static void menuCopy() {
	currentBank.waves[selectedId].clipboardCopy();
}

static void menuCut() {
	currentBank.waves[selectedId].clipboardCopy();
	currentBank.waves[selectedId].clear();
	historyPush();
}

static void menuPaste() {
	currentBank.waves[selectedId].clipboardPaste();
	historyPush();
}

static void menuClear() {
	for (int i = mini(selectedId, lastSelectedId); i <= maxi(selectedId, lastSelectedId); i++) {
		currentBank.waves[i].clear();
	}
	historyPush();
}

static void menuRandomize() {
	for (int i = mini(selectedId, lastSelectedId); i <= maxi(selectedId, lastSelectedId); i++) {
		currentBank.waves[i].randomizeEffects();
	}
	historyPush();
}

static void incrementSelectedId(int delta) {
	selectWave(clampi(selectedId + delta, 0, BANK_LEN-1));
}

static void menuKeyCommands() {
	ImGuiContext &g = *GImGui;
	ImGuiIO &io = ImGui::GetIO();

	// Modern imgui (v1.87+) swaps Cmd<->Ctrl internally when ConfigMacOSXBehaviors
	// is enabled, so io.KeyCtrl represents the platform shortcut modifier
	// (Cmd on Mac, Ctrl elsewhere). Use it unconditionally.
	if (io.KeyCtrl) {
		if (ImGui::IsKeyPressed(ImGuiKey_N) && !io.KeyShift && !io.KeyAlt)
			menuNewBank();
		if (ImGui::IsKeyPressed(ImGuiKey_O) && !io.KeyShift && !io.KeyAlt)
			menuOpenBank();
		if (ImGui::IsKeyPressed(ImGuiKey_S) && !io.KeyShift && !io.KeyAlt)
			menuSaveBank();
		if (ImGui::IsKeyPressed(ImGuiKey_S) && io.KeyShift && !io.KeyAlt)
			menuSaveBankAs();
		if (ImGui::IsKeyPressed(ImGuiKey_Q) && !io.KeyShift && !io.KeyAlt)
			menuQuit();
		if (ImGui::IsKeyPressed(ImGuiKey_Z) && !io.KeyShift && !io.KeyAlt)
			historyUndo();
		if (ImGui::IsKeyPressed(ImGuiKey_Z) && io.KeyShift && !io.KeyAlt)
			historyRedo();
		if (ImGui::IsKeyPressed(ImGuiKey_A) && !io.KeyShift && !io.KeyAlt)
			menuSelectAll();
		if (ImGui::IsKeyPressed(ImGuiKey_C) && !io.KeyShift && !io.KeyAlt)
			menuCopy();
		if (ImGui::IsKeyPressed(ImGuiKey_X) && !io.KeyShift && !io.KeyAlt)
			menuCut();
		if (ImGui::IsKeyPressed(ImGuiKey_V) && !io.KeyShift && !io.KeyAlt)
			menuPaste();
	}
	// F1 was previously using a scancode instead of a keycode due to old SDL-native keymap quirks.
	// Modern imgui uses its own ImGuiKey enum, so the scancode/keycode distinction no longer applies.
	if (ImGui::IsKeyPressed(ImGuiKey_F1))
		menuManual();

	if (!io.KeySuper && !io.KeyCtrl && !io.KeyShift && !io.KeyAlt) {
		// Only trigger these key commands if no text box is focused
		if (!g.ActiveId || g.ActiveId != GImGui->InputTextState.ID) {
			if (ImGui::IsKeyPressed(ImGuiKey_R))
				menuRandomize();
			if (ImGui::IsKeyPressed(io.ConfigMacOSXBehaviors ? ImGuiKey_Backspace : ImGuiKey_Delete))
				menuClear();
			// Pages
			if (ImGui::IsKeyPressed(ImGuiKey_Space))
				playEnabled = !playEnabled;
			if (ImGui::IsKeyPressed(ImGuiKey_1))
				currentPage = EDITOR_PAGE;
			if (ImGui::IsKeyPressed(ImGuiKey_2))
				currentPage = EFFECT_PAGE;
			if (ImGui::IsKeyPressed(ImGuiKey_3))
				currentPage = GRID_PAGE;
			if (ImGui::IsKeyPressed(ImGuiKey_4))
				currentPage = WATERFALL_PAGE;
			if (ImGui::IsKeyPressed(ImGuiKey_5))
				currentPage = IMPORT_PAGE;
			if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
				incrementSelectedId(currentPage == GRID_PAGE ? -BANK_GRID_WIDTH : -1);
			if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
				incrementSelectedId(currentPage == GRID_PAGE ? BANK_GRID_WIDTH : 1);
			if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow))
				incrementSelectedId(-1);
			if (ImGui::IsKeyPressed(ImGuiKey_RightArrow))
				incrementSelectedId(1);
		}
	}
}

void renderWaveMenu() {
	char menuName[128];
	int selectedStart = mini(selectedId, lastSelectedId);
	int selectedEnd = maxi(selectedId, lastSelectedId);
	if (selectedStart != selectedEnd)
		snprintf(menuName, sizeof(menuName), "(Wave %d to %d)", selectedStart, selectedEnd);
	else
		snprintf(menuName, sizeof(menuName), "(Wave %d)", selectedId);
	ImGui::MenuItem(menuName, NULL, false, false);

	if (ImGui::MenuItem("Clear", "Delete")) {
		menuClear();
	}
	if (ImGui::MenuItem("Randomize Effects", "R")) {
		menuRandomize();
	}

	if (selectedStart != selectedEnd) {
		ImGui::MenuItem("##spacer3", NULL, false, false);
		snprintf(menuName, sizeof(menuName), "(Wave %d)", selectedId);
		ImGui::MenuItem(menuName, NULL, false, false);
	}

	if (ImGui::MenuItem("Copy", ImGui::GetIO().ConfigMacOSXBehaviors ? "Cmd+C" : "Ctrl+C")) {
		menuCopy();
	}
	if (ImGui::MenuItem("Cut", ImGui::GetIO().ConfigMacOSXBehaviors ? "Cmd+X" : "Ctrl+X")) {
		menuCut();
	}
	if (ImGui::MenuItem("Paste", ImGui::GetIO().ConfigMacOSXBehaviors ? "Cmd+V" : "Ctrl+V", false, clipboardActive)) {
		menuPaste();
	}

	if (ImGui::MenuItem("Open Wave...")) {
		char *dir = getLastDir();
		char *path = osdialog_file(OSDIALOG_OPEN, dir, NULL, NULL);
		if (path) {
			currentBank.waves[selectedId].loadWAV(path);
			historyPush();
			snprintf(lastFilename, sizeof(lastFilename), "%s", path);
			free(path);
		}
		free(dir);
	}
	if (ImGui::MenuItem("Save Wave As...")) {
		char *dir = getLastDir();
		char *path = osdialog_file(OSDIALOG_SAVE, dir, "Untitled.wav", NULL);
		if (path) {
			currentBank.waves[selectedId].saveWAV(path);
			snprintf(lastFilename, sizeof(lastFilename), "%s", path);
			free(path);
		}
		free(dir);
	}
}

void renderMenu() {
	menuKeyCommands();

	// HACK
	// Display a window on top of the menu with the logo, since I'm too lazy to make my own custom MenuImageItem widget
	{
		int width, height;
		getImageSize(logoTexture, &width, &height);
		ImVec2 padding = ImVec2(8, 4);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(0, 0));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, padding);
		ImGui::SetNextWindowPos(ImVec2(0, 0));
		ImGui::SetNextWindowSize(ImVec2(width + 2 * padding.x, height + 2 * padding.y));
		if (ImGui::Begin("Logo", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoInputs)) {
			ImGui::Image(logoTexture, ImVec2(width, height));
			ImGui::End();
		}
		ImGui::PopStyleVar();
		ImGui::PopStyleVar();
	}

	// Draw main menu
	if (ImGui::BeginMenuBar()) {
		// This will be hidden by the window with the logo
		if (ImGui::BeginMenu("                        v" TOSTRING(VERSION), false)) {
			ImGui::EndMenu();
		}
		// File
		if (ImGui::BeginMenu("File")) {
			if (ImGui::MenuItem("New Bank", ImGui::GetIO().ConfigMacOSXBehaviors ? "Cmd+N" : "Ctrl+N"))
				menuNewBank();
			if (ImGui::MenuItem("Open Bank...", ImGui::GetIO().ConfigMacOSXBehaviors ? "Cmd+O" : "Ctrl+O"))
				menuOpenBank();
			if (ImGui::MenuItem("Save Bank", ImGui::GetIO().ConfigMacOSXBehaviors ? "Cmd+S" : "Ctrl+S"))
				menuSaveBank();
			if (ImGui::MenuItem("Save Bank As...", ImGui::GetIO().ConfigMacOSXBehaviors ? "Cmd+Shift+S" : "Ctrl+Shift+S"))
				menuSaveBankAs();
			if (ImGui::MenuItem("Save Waves to Folder...", NULL))
				menuSaveWaves();
			if (ImGui::MenuItem("Quit", ImGui::GetIO().ConfigMacOSXBehaviors ? "Cmd+Q" : "Ctrl+Q"))
				menuQuit();

			ImGui::EndMenu();
		}
		// Edit
		if (ImGui::BeginMenu("Edit")) {
			if (ImGui::MenuItem("Undo", ImGui::GetIO().ConfigMacOSXBehaviors ? "Cmd+Z" : "Ctrl+Z"))
				historyUndo();
			if (ImGui::MenuItem("Redo", ImGui::GetIO().ConfigMacOSXBehaviors ? "Cmd+Shift+Z" : "Ctrl+Shift+Z"))
				historyRedo();
			if (ImGui::MenuItem("Select All", ImGui::GetIO().ConfigMacOSXBehaviors ? "Cmd+A" : "Ctrl+A"))
				menuSelectAll();
			ImGui::MenuItem("##spacer", NULL, false, false);
			renderWaveMenu();
			ImGui::EndMenu();
		}
		// Audio Output
		if (ImGui::BeginMenu("Audio Output")) {
			int deviceCount = audioGetDeviceCount();
			for (int deviceId = 0; deviceId < deviceCount; deviceId++) {
				const char *deviceName = audioGetDeviceName(deviceId);
				if (ImGui::MenuItem(deviceName, NULL, false)) audioOpen(deviceId);
			}
			ImGui::EndMenu();
		}
		// Theme
		if (ImGui::BeginMenu("Theme")) {
			for (int i = 0; i < themeCount(); i++) {
				ImGui::PushID(i);
				bool selected = (currentThemeId == i);
				if (ImGui::MenuItem(themeName(i), NULL, selected)) {
					currentThemeId = i;
					bool isDark = true;
					themeApply(currentThemeId, &isDark);
					logoTexture = isDark ? logoTextureLight : logoTextureDark;
				}
				ImGui::PopID();
			}
			ImGui::EndMenu();
		}
		// Help
		if (ImGui::BeginMenu("Help")) {
			if (ImGui::MenuItem("Manual PDF", "F1", false))
				menuManual();
			if (ImGui::MenuItem("Webpage", "", false))
				menuWebsite();
			// if (ImGui::MenuItem("imgui Demo", NULL, showTestWindow)) showTestWindow = !showTestWindow;
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}
}


void renderPreview() {
	ImGui::Checkbox("Play", &playEnabled);
	ImGui::SameLine();
	ImGui::PushItemWidth(300.0);
	ImGui::PushFont(fontMono);
	ImGui::SliderFloat("##playVolume", &playVolume, -60.0f, 0.0f, "Volume: %.2f dB");
	ImGui::PopFont();
	ImGui::PushItemWidth(-1.0);
	ImGui::SameLine();
	ImGui::PushFont(fontMono);
	ImGui::SliderFloat("##playFrequency", &playFrequency, 1.0f, 10000.0f, "Frequency: %.2f Hz", ImGuiSliderFlags_Logarithmic);
	ImGui::PopFont();

	ImGui::Checkbox("Morph Interpolate", &morphInterpolate);
	if (playModeXY) {
		ImGui::SameLine();
		ImGui::PushItemWidth(-1.0);
		float width = ImGui::CalcItemWidth() / 2.0 - ImGui::GetStyle().FramePadding.y;
		ImGui::PushItemWidth(width);
		ImGui::PushFont(fontMono);
		ImGui::SliderFloat("##Morph X", &morphX, 0.0, BANK_GRID_WIDTH - 1, "Morph X: %.3f");
		ImGui::PopFont();
		ImGui::SameLine();
		ImGui::PushFont(fontMono);
		ImGui::SliderFloat("##Morph Y", &morphY, 0.0, BANK_GRID_HEIGHT - 1, "Morph Y: %.3f");
		ImGui::PopFont();
	}
	else {
		ImGui::SameLine();
		ImGui::PushItemWidth(-1.0);
		float width = ImGui::CalcItemWidth() / 2.0 - ImGui::GetStyle().FramePadding.y;
		ImGui::PushItemWidth(width);
		ImGui::PushFont(fontMono);
		ImGui::SliderFloat("##Morph Z", &morphZ, 0.0, BANK_LEN - 1, "Morph Z: %.3f");
		ImGui::PopFont();
		ImGui::SameLine();
		ImGui::PushFont(fontMono);
		ImGui::SliderFloat("##Morph Z Speed", &morphZSpeed, 0.f, 10.f, "Morph Z Speed: %.3f Hz", ImGuiSliderFlags_Logarithmic);
		ImGui::PopFont();
	}

	refreshMorphSnap();
}


void renderToolSelector(Tool *tool) {
	if (ImGui::RadioButton("Pencil", *tool == PENCIL_TOOL)) *tool = PENCIL_TOOL;
	ImGui::SameLine();
	if (ImGui::RadioButton("Brush", *tool == BRUSH_TOOL)) *tool = BRUSH_TOOL;
	ImGui::SameLine();
	if (ImGui::RadioButton("Grab", *tool == GRAB_TOOL)) *tool = GRAB_TOOL;
	ImGui::SameLine();
	if (ImGui::RadioButton("Line", *tool == LINE_TOOL)) *tool = LINE_TOOL;
	ImGui::SameLine();
	if (ImGui::RadioButton("Eraser", *tool == ERASER_TOOL)) *tool = ERASER_TOOL;
}


void effectSlider(EffectID effect) {
	char id[64];
	snprintf(id, sizeof(id), "##%s", effectNames[effect]);
	char text[64];
	snprintf(text, sizeof(text), "%s: %%.3f", effectNames[effect]);
	ImGui::PushFont(fontMono);
	bool sliderEdited = ImGui::SliderFloat(id, &currentBank.waves[selectedId].effects[effect], 0.0f, 1.0f, text);
	ImGui::PopFont();
	if (sliderEdited) {
		currentBank.waves[selectedId].updatePost();
		historyPush();
	}
}


void editorPage() {
	ImGui::BeginChild("Sidebar", ImVec2(200, 0), true);
	{
		float dummyZ = 0.0;
		ImGui::PushItemWidth(-1);
		renderBankGrid("SidebarGrid", BANK_LEN * 35.0, 1, &dummyZ, &morphZ);
		refreshMorphSnap();
	}
	ImGui::EndChild();

	ImGui::SameLine();
	ImGui::BeginChild("Editor", ImVec2(0, 0), true);
	{
		Wave *wave = &currentBank.waves[selectedId];
		float *effects = wave->effects;

		ImGui::PushItemWidth(-1);

		static enum Tool tool = PENCIL_TOOL;
		renderToolSelector(&tool);

		ImGui::SameLine();
		if (ImGui::Button("Clear")) {
			currentBank.waves[selectedId].clear();
			historyPush();
		}


		for (const CatalogCategory &catalogCategory : catalogCategories) {
			ImGui::SameLine();
			if (ImGui::Button(catalogCategory.name.c_str())) ImGui::OpenPopup(catalogCategory.name.c_str());
			if (ImGui::BeginPopup(catalogCategory.name.c_str())) {
				for (const CatalogFile &catalogFile : catalogCategory.files) {
					if (ImGui::Selectable(catalogFile.name.c_str())) {
						memcpy(currentBank.waves[selectedId].samples, catalogFile.samples, sizeof(float) * WAVE_LEN);
						currentBank.waves[selectedId].commitSamples();
						historyPush();
					}
				}
				ImGui::EndPopup();
			}
		}

		// ImGui::SameLine();
		// if (ImGui::RadioButton("Smooth", tool == SMOOTH_TOOL)) tool = SMOOTH_TOOL;

		ImGui::Text("Waveform");
		const int oversample = 4;
		float waveOversample[WAVE_LEN * oversample];
		cyclicOversample(wave->postSamples, waveOversample, WAVE_LEN, oversample);
		if (renderWave("WaveEditor", 200.0, wave->samples, WAVE_LEN, waveOversample, WAVE_LEN * oversample, tool)) {
			currentBank.waves[selectedId].commitSamples();
			historyPush();
		}

		ImGui::Text("Harmonics");
		if (renderHistogram("HarmonicEditor", 200.0, wave->harmonics, WAVE_LEN / 2, wave->postHarmonics, WAVE_LEN / 2, tool)) {
			currentBank.waves[selectedId].commitHarmonics();
			historyPush();
		}

		ImGui::Text("Effects");
		for (int i = 0; i < EFFECTS_LEN; i++) {
			effectSlider((EffectID) i);
		}

		if (ImGui::Checkbox("Cycle", &currentBank.waves[selectedId].cycle)) {
			currentBank.waves[selectedId].updatePost();
			historyPush();
		}
		ImGui::SameLine();
		if (ImGui::Checkbox("Normalize", &currentBank.waves[selectedId].normalize)) {
			currentBank.waves[selectedId].updatePost();
			historyPush();
		}
		ImGui::SameLine();
		if (ImGui::Button("Randomize")) {
			currentBank.waves[selectedId].randomizeEffects();
			historyPush();
		}
		ImGui::SameLine();
		if (ImGui::Button("Reset")) {
			currentBank.waves[selectedId].clearEffects();
			historyPush();
		}
		ImGui::SameLine();
		if (ImGui::Button("Bake")) {
			currentBank.waves[selectedId].bakeEffects();
			historyPush();
		}

		ImGui::PopItemWidth();
	}
	ImGui::EndChild();
}


void effectHistogram(EffectID effect, Tool tool) {
	float value[BANK_LEN];
	float average = 0.0;
	for (int i = 0; i < BANK_LEN; i++) {
		value[i] = currentBank.waves[i].effects[effect];
		average += value[i];
	}
	average /= BANK_LEN;
	float oldAverage = average;

	ImGui::Text("%s", effectNames[effect]);

	char id[64];
	snprintf(id, sizeof(id), "##%sAverage", effectNames[effect]);
	char text[64];
	snprintf(text, sizeof(text), "Average %s: %%.3f", effectNames[effect]);
	ImGui::PushFont(fontMono);
	bool sliderEdited = ImGui::SliderFloat(id, &average, 0.0f, 1.0f, text);
	ImGui::PopFont();
	if (sliderEdited) {
		// Change the average effect level to the new average
		float deltaAverage = average - oldAverage;
		for (int i = 0; i < BANK_LEN; i++) {
			if (0.0 < average && average < 1.0) {
				currentBank.waves[i].effects[effect] = clampf(currentBank.waves[i].effects[effect] + deltaAverage, 0.0, 1.0);
			}
			else {
				currentBank.waves[i].effects[effect] = average;
			}
			currentBank.waves[i].updatePost();
			historyPush();
		}
	}

	if (renderHistogram(effectNames[effect], 120, value, BANK_LEN, NULL, 0, tool)) {
		for (int i = 0; i < BANK_LEN; i++) {
			if (currentBank.waves[i].effects[effect] != value[i]) {
				// TODO This always selects the highest index. Select the index the mouse is hovering (requires renderHistogram() to return an int)
				selectWave(i);
				currentBank.waves[i].effects[effect] = value[i];
				currentBank.waves[i].updatePost();
				historyPush();
			}
		}
	}
}


void effectPage() {
	ImGui::BeginChild("Effect Editor", ImVec2(0, 0), true); {
		static Tool tool = PENCIL_TOOL;
		renderToolSelector(&tool);

		ImGui::PushItemWidth(-1);
		for (int i = 0; i < EFFECTS_LEN; i++) {
			effectHistogram((EffectID) i, tool);
		}
		ImGui::PopItemWidth();

		if (ImGui::Button("Cycle All")) {
			for (int i = 0; i < BANK_LEN; i++) {
				currentBank.waves[i].cycle = true;
				currentBank.waves[i].updatePost();
				historyPush();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Cycle None")) {
			for (int i = 0; i < BANK_LEN; i++) {
				currentBank.waves[i].cycle = false;
				currentBank.waves[i].updatePost();
				historyPush();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Normalize All")) {
			for (int i = 0; i < BANK_LEN; i++) {
				currentBank.waves[i].normalize = true;
				currentBank.waves[i].updatePost();
				historyPush();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Normalize None")) {
			for (int i = 0; i < BANK_LEN; i++) {
				currentBank.waves[i].normalize = false;
				currentBank.waves[i].updatePost();
				historyPush();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Randomize")) {
			for (int i = 0; i < BANK_LEN; i++) {
				currentBank.waves[i].randomizeEffects();
				historyPush();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Reset")) {
			for (int i = 0; i < BANK_LEN; i++) {
				currentBank.waves[i].clearEffects();
				historyPush();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Bake")) {
			for (int i = 0; i < BANK_LEN; i++) {
				currentBank.waves[i].bakeEffects();
				historyPush();
			}
		}
	}
	ImGui::EndChild();
}


void gridPage() {
	playModeXY = true;
	ImGui::BeginChild("Grid Page", ImVec2(0, 0), true);
	{
		ImGui::PushItemWidth(-1.0);
		renderBankGrid("WaveGrid", -1.f, BANK_GRID_WIDTH, &morphX, &morphY);
		refreshMorphSnap();
	}
	ImGui::EndChild();
}


void waterfallPage() {
	ImGui::BeginChild("3D View", ImVec2(0, 0), true);
	{
		ImGui::PushItemWidth(-1.0);
		static float amplitude = 0.25;
		ImGui::PushFont(fontMono);
		ImGui::SliderFloat("##amplitude", &amplitude, 0.01, 1.0, "Scale: %.3f", ImGuiSliderFlags_Logarithmic);
		ImGui::PopFont();
		static float angle = 1.0;
		ImGui::PushFont(fontMono);
		ImGui::SliderFloat("##angle", &angle, 0.0, 1.0, "Angle: %.3f");
		ImGui::PopFont();

		renderWaterfall("##waterfall", -1.0, amplitude, angle, &morphZ);
	}
	ImGui::EndChild();
}


void renderMain() {
	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(ImVec2((int)ImGui::GetIO().DisplaySize.x, (int)ImGui::GetIO().DisplaySize.y));

	ImGui::Begin("WaveEditMainWindow", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_MenuBar);
	{
		// Menu bar
		renderMenu();
		renderPreview();
		// Tab bar
		{
			static const char *tabLabels[NUM_PAGES] = {
				"Waveform Editor",
				"Effect Editor",
				"Grid XY View",
				"Waterfall View",
				"Import",
			};
			static int hoveredTab = 0;
			ImGui::TabLabels(NUM_PAGES, tabLabels, (int*)&currentPage, NULL, false, &hoveredTab);
		}

		// Page
		// Reset some audio variables. These might be changed within the pages.
		playModeXY = false;
		playingBank = &currentBank;
		switch (currentPage) {
		case EDITOR_PAGE: editorPage(); break;
		case EFFECT_PAGE: effectPage(); break;
		case GRID_PAGE: gridPage(); break;
		case WATERFALL_PAGE: waterfallPage(); break;
		case IMPORT_PAGE: importPage(); break;
		default: break;
		}
	}
	ImGui::End();

	if (showTestWindow) {
		ImGui::ShowDemoWindow(&showTestWindow);
	}
}




void uiInit() {
	ImGui::GetIO().IniFilename = NULL;

	// Load fonts. UI font is Inter (default), monospace numerics are
	// JetBrains Mono. Both rendered via FreeType (see imconfig_user.h).
	ImGuiIO &io = ImGui::GetIO();
	io.Fonts->Clear();
	io.FontDefault = io.Fonts->AddFontFromFileTTF("fonts/Inter-Regular.ttf", 14.0f);
	fontMono = io.Fonts->AddFontFromFileTTF("fonts/JetBrainsMono-Regular.ttf", 14.0f);
	IM_ASSERT(io.FontDefault != NULL && "fonts/Inter-Regular.ttf failed to load");
	IM_ASSERT(fontMono != NULL && "fonts/JetBrainsMono-Regular.ttf failed to load");
	logoTextureLight = loadImage("logo-light.png");
	logoTextureDark = loadImage("logo-dark.png");

	// Discover and load themes from disk.
	themeInit("themes");

	// Read the persisted theme name from ui.dat (versioned format).
	// File layout (v2):
	//   uint32_t version = 2
	//   uint8_t  nameLen
	//   char     name[nameLen]
	// File missing, empty, or in the old 4-byte int-only format → fall back
	// to "Tokyo Night Dark" by name; if that's not available, fall back to id 0.
	char savedName[64] = "Tokyo Night Dark";
	{
		FILE *f = fopen("ui.dat", "rb");
		if (f) {
			fseek(f, 0, SEEK_END);
			long size = ftell(f);
			fseek(f, 0, SEEK_SET);
			if (size >= 5) {
				uint32_t version;
				if (fread(&version, sizeof(version), 1, f) == 1 && version == 2) {
					uint8_t nameLen;
					if (fread(&nameLen, 1, 1, f) == 1 && nameLen < sizeof(savedName)) {
						if (fread(savedName, 1, nameLen, f) == nameLen) {
							savedName[nameLen] = '\0';
						}
					}
				}
			}
			fclose(f);
		}
	}

	currentThemeId = themeByName(savedName);
	if (currentThemeId < 0) currentThemeId = themeByName("Tokyo Night Dark");
	if (currentThemeId < 0) currentThemeId = 0;

	// Default to true so that if themeApply() ever no-ops (e.g., out-of-range
	// id), isDark has a sensible value and the logo logic doesn't read UB.
	bool isDark = true;
	themeApply(currentThemeId, &isDark);
	logoTexture = isDark ? logoTextureLight : logoTextureDark;
}


void uiDestroy() {
	// Save UI settings — versioned ui.dat format (see uiInit for layout).
	{
		FILE *f = fopen("ui.dat", "wb");
		if (f) {
			uint32_t version = 2;
			const char *name = themeName(currentThemeId);
			size_t nameLen = strlen(name);
			if (nameLen > 255) nameLen = 255;  // fits in uint8_t
			uint8_t nameLenByte = (uint8_t)nameLen;
			fwrite(&version, sizeof(version), 1, f);
			fwrite(&nameLenByte, 1, 1, f);
			fwrite(name, 1, nameLen, f);
			fclose(f);
		}
	}
}


void uiRender() {
	renderMain();
}
