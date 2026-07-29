#include "WaveEdit.hpp"
#include <SDL.h>
#include <string.h>


Bank currentBank;

struct BankSnapshot {
	int waveLength;
	std::vector<float> samples;
	float effects[BANK_LEN][EFFECTS_LEN];
	bool cycle[BANK_LEN];
	bool normalize[BANK_LEN];
};

static const int historyLimit = 100;
static std::vector<BankSnapshot> history;
static int currentIndex = -1;
static double previousTime = -INFINITY;
static const double delayTime = 0.2;

static BankSnapshot captureBank() {
	BankSnapshot snapshot;
	snapshot.waveLength = currentBank.waveLength;
	snapshot.samples.resize(BANK_LEN * snapshot.waveLength);
	for (int i = 0; i < BANK_LEN; i++) {
		memcpy(&snapshot.samples[i * snapshot.waveLength], currentBank.waves[i].samples,
			sizeof(float) * snapshot.waveLength);
		memcpy(snapshot.effects[i], currentBank.waves[i].effects, sizeof(snapshot.effects[i]));
		snapshot.cycle[i] = currentBank.waves[i].cycle;
		snapshot.normalize[i] = currentBank.waves[i].normalize;
	}
	return snapshot;
}

static void restoreBank(const BankSnapshot &snapshot) {
	currentBank.waveLength = snapshot.waveLength;
	currentBank.clear();
	for (int i = 0; i < BANK_LEN; i++) {
		memcpy(currentBank.waves[i].samples, &snapshot.samples[i * snapshot.waveLength],
			sizeof(float) * snapshot.waveLength);
		memcpy(currentBank.waves[i].effects, snapshot.effects[i], sizeof(snapshot.effects[i]));
		currentBank.waves[i].cycle = snapshot.cycle[i];
		currentBank.waves[i].normalize = snapshot.normalize[i];
		currentBank.waves[i].commitSamples();
	}
}

void historyPush() {
	double time = SDL_GetTicks() / 1000.0;
	if (time - previousTime >= delayTime) {
		currentIndex++;
	}

	// Delete redo history
	history.resize(currentIndex + 1);

	history[currentIndex] = captureBank();
	if ((int)history.size() > historyLimit) {
		history.erase(history.begin());
		currentIndex--;
	}
	previousTime = time;
}

void historyUndo() {
	if (currentIndex >= 1) {
		currentIndex--;
		restoreBank(history[currentIndex]);
		previousTime = -INFINITY;
	}
}

void historyRedo() {
	if ((int) history.size() > currentIndex + 1) {
		currentIndex++;
		restoreBank(history[currentIndex]);
		previousTime = -INFINITY;
	}
}

void historyClear() {
	history.clear();
	currentIndex = -1;
	previousTime = -INFINITY;
}
