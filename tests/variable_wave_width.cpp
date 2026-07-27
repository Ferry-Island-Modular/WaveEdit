#include "src/WaveEdit.hpp"

#include <math.h>
#include <stdio.h>
#include <unistd.h>

#include <memory>

struct LegacyWave {
	float samples[DEFAULT_WAVE_LEN];
	float spectrum[DEFAULT_WAVE_LEN];
	float harmonics[DEFAULT_WAVE_LEN / 2];
	float postSamples[DEFAULT_WAVE_LEN];
	float postSpectrum[DEFAULT_WAVE_LEN];
	float postHarmonics[DEFAULT_WAVE_LEN / 2];
	float effects[EFFECTS_LEN];
	bool cycle;
	bool normalize;
};

struct LegacyBank {
	LegacyWave waves[BANK_LEN];
};

static void fillSine(Wave &wave) {
	for (int i = 0; i < wave.length; i++)
		wave.samples[i] = sinf(2.0f * M_PI * i / wave.length);
	wave.commitSamples();
}

int main() {
	std::unique_ptr<Bank> bank(new Bank);
	bank->clear();
	fillSine(bank->waves[0]);

	const int widths[] = {512, 1024, 2048, 256};
	for (int width : widths) {
		assert(bank->setWaveLength(width));
		assert(bank->waveLength == width);
		for (int i = 0; i < BANK_LEN; i++)
			assert(bank->waves[i].length == width);
		assert(fabsf(bank->waves[0].samples[width / 4] - 1.0f) < 0.02f);
	}
	assert(!bank->setWaveLength(300));

	assert(bank->setWaveLength(2048));
	char statePath[] = "/tmp/waveedit-state-XXXXXX";
	int stateFd = mkstemp(statePath);
	assert(stateFd >= 0);
	close(stateFd);
	bank->save(statePath);

	std::unique_ptr<Bank> restored(new Bank);
	restored->load(statePath);
	assert(restored->waveLength == 2048);
	assert(fabsf(restored->waves[0].samples[512] - 1.0f) < 0.02f);
	unlink(statePath);

	char legacyPath[] = "/tmp/waveedit-legacy-XXXXXX";
	int legacyFd = mkstemp(legacyPath);
	assert(legacyFd >= 0);
	FILE *legacyFile = fdopen(legacyFd, "wb");
	assert(legacyFile);
	std::unique_ptr<LegacyBank> legacy(new LegacyBank());
	legacy->waves[0].samples[DEFAULT_WAVE_LEN / 4] = 1.0f;
	legacy->waves[0].effects[PRE_GAIN] = 0.25f;
	legacy->waves[0].cycle = true;
	assert(fwrite(legacy.get(), sizeof(*legacy), 1, legacyFile) == 1);
	fclose(legacyFile);

	restored->load(legacyPath);
	assert(restored->waveLength == DEFAULT_WAVE_LEN);
	assert(restored->waves[0].samples[DEFAULT_WAVE_LEN / 4] == 1.0f);
	assert(restored->waves[0].effects[PRE_GAIN] == 0.25f);
	assert(restored->waves[0].cycle);
	unlink(legacyPath);

	assert(restored->setWaveLength(2048));
	fillSine(restored->waves[0]);
	char wavPath[] = "/tmp/waveedit-bank-XXXXXX";
	int wavFd = mkstemp(wavPath);
	assert(wavFd >= 0);
	close(wavFd);
	restored->saveWAV(wavPath);

	std::unique_ptr<Bank> reopened(new Bank);
	reopened->loadWAV(wavPath);
	assert(reopened->waveLength == 2048);
	assert(fabsf(reopened->waves[0].samples[512] - 1.0f) < 0.03f);
	unlink(wavPath);

	printf("variable wave width tests passed\n");
	return 0;
}
