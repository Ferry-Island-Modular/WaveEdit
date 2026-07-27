#include "WaveEdit.hpp"
#include <string.h>
#include <stdint.h>
#include <sndfile.h>

namespace {

const char bankMagic[8] = {'W', 'A', 'V', 'E', 'E', 'D', 'I', 'T'};
const uint32_t bankFormatVersion = 1;

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

bool supportedWaveLength(int length) {
	return length == 256 || length == 512 || length == 1024 || length == 2048;
}

}

void Bank::clear() {
	for (int i = 0; i < BANK_LEN; i++) {
		waves[i].length = waveLength;
		waves[i].clear();
	}
}

bool Bank::setWaveLength(int length) {
	if (!supportedWaveLength(length))
		return false;
	if (length == waveLength)
		return true;

	for (int i = 0; i < BANK_LEN; i++) {
		float resized[MAX_WAVE_LEN] = {};
		resample(waves[i].samples, waveLength, resized, length, length / (double)waveLength);
		waves[i].length = length;
		memcpy(waves[i].samples, resized, sizeof(float) * length);
		waves[i].commitSamples();
	}
	waveLength = length;
	return true;
}


void Bank::swap(int i, int j) {
	Wave tmp = waves[i];
	waves[i] = waves[j];
	waves[j] = tmp;
}


void Bank::shuffle() {
	for (int j = BANK_LEN - 1; j >= 3; j--) {
		int i = rand() % j;
		swap(i, j);
	}
}


void Bank::setSamples(const float *in) {
	for (int j = 0; j < BANK_LEN; j++) {
		memcpy(waves[j].samples, &in[j * waveLength], sizeof(float) * waveLength);
		waves[j].commitSamples();
	}
}


void Bank::getPostSamples(float *out) {
	for (int j = 0; j < BANK_LEN; j++) {
		memcpy(&out[j * waveLength], waves[j].postSamples, sizeof(float) * waveLength);
	}
}


void Bank::duplicateToAll(int waveId) {
	for (int j = 0; j < BANK_LEN; j++) {
		if (j != waveId)
			waves[j] = waves[waveId];
		// No need to commit the wave because we're copying everything
	}
}


void Bank::save(const char *filename) {
	FILE *f = fopen(filename, "wb");
	if (!f)
		return;
	fwrite(bankMagic, sizeof(bankMagic), 1, f);
	fwrite(&bankFormatVersion, sizeof(bankFormatVersion), 1, f);
	uint32_t savedLength = waveLength;
	fwrite(&savedLength, sizeof(savedLength), 1, f);
	for (int i = 0; i < BANK_LEN; i++) {
		fwrite(waves[i].samples, sizeof(float), waveLength, f);
		fwrite(waves[i].effects, sizeof(float), EFFECTS_LEN, f);
		uint8_t flags = (waves[i].cycle ? 1 : 0) | (waves[i].normalize ? 2 : 0);
		fwrite(&flags, sizeof(flags), 1, f);
	}
	fclose(f);
}


void Bank::load(const char *filename) {
	FILE *f = fopen(filename, "rb");
	if (!f) {
		clear();
		return;
	}

	char magic[sizeof(bankMagic)];
	if (fread(magic, sizeof(magic), 1, f) == 1 && memcmp(magic, bankMagic, sizeof(magic)) == 0) {
		uint32_t version = 0;
		uint32_t savedLength = 0;
		if (fread(&version, sizeof(version), 1, f) != 1
				|| fread(&savedLength, sizeof(savedLength), 1, f) != 1
				|| version != bankFormatVersion
				|| !supportedWaveLength(savedLength)) {
			fclose(f);
			clear();
			return;
		}
		waveLength = savedLength;
		clear();
		for (int i = 0; i < BANK_LEN; i++) {
			uint8_t flags = 0;
			if (fread(waves[i].samples, sizeof(float), waveLength, f) != (size_t)waveLength
					|| fread(waves[i].effects, sizeof(float), EFFECTS_LEN, f) != EFFECTS_LEN
					|| fread(&flags, sizeof(flags), 1, f) != 1) {
				fclose(f);
				clear();
				return;
			}
			waves[i].cycle = flags & 1;
			waves[i].normalize = flags & 2;
		}
	}
	else {
		rewind(f);
		LegacyBank legacy = {};
		if (fread(&legacy, sizeof(legacy), 1, f) != 1) {
			fclose(f);
			clear();
			return;
		}
		waveLength = DEFAULT_WAVE_LEN;
		clear();
		for (int i = 0; i < BANK_LEN; i++) {
			memcpy(waves[i].samples, legacy.waves[i].samples, sizeof(legacy.waves[i].samples));
			memcpy(waves[i].effects, legacy.waves[i].effects, sizeof(legacy.waves[i].effects));
			waves[i].cycle = legacy.waves[i].cycle;
			waves[i].normalize = legacy.waves[i].normalize;
		}
	}
	fclose(f);

	for (int j = 0; j < BANK_LEN; j++) {
		waves[j].commitSamples();
	}
}


void Bank::saveWAV(const char *filename) {
	SF_INFO info;
	info.samplerate = 44100;
	info.channels = 1;
	info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16 | SF_ENDIAN_LITTLE;
	SNDFILE *sf = sf_open(filename, SFM_WRITE, &info);
	if (!sf)
		return;

	for (int j = 0; j < BANK_LEN; j++) {
		sf_write_float(sf, waves[j].postSamples, waveLength);
	}

	sf_close(sf);
}


void Bank::loadWAV(const char *filename) {
	int sampleCount = 0;
	float *samples = loadAudio(filename, &sampleCount);
	if (!samples)
		return;

	if (sampleCount % BANK_LEN == 0) {
		int inferredLength = sampleCount / BANK_LEN;
		if (supportedWaveLength(inferredLength))
			waveLength = inferredLength;
	}
	clear();

	for (int i = 0; i < BANK_LEN; i++) {
		int offset = i * waveLength;
		int remaining = maxi(0, sampleCount - offset);
		int copyLength = mini(waveLength, remaining);
		memcpy(waves[i].samples, samples + offset, sizeof(float) * copyLength);
		waves[i].commitSamples();
	}

	delete[] samples;
}


void Bank::saveWaves(const char *dirname) {
	for (int b = 0; b < BANK_LEN; b++) {
		char filename[1024];
		snprintf(filename, sizeof(filename), "%s/%02d.wav", dirname, b);

		waves[b].saveWAV(filename);
	}
}
