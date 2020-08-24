#ifndef _H_CLOCK_
#define _H_CLOCK_

#include <cstdint>
#include <math.h>

enum SyncMode {
	FREE_RUNNING = 0,
	HOST_SYNC,
	HOST_SYNC_QUANTIZED_START
};

class PluginClock {
public:
	PluginClock();
	~PluginClock();
	void transmitHostInfo(const bool playing, const float beatstPerBar,
			const int hostBeat, const float hostBarBeat, const float hostBpm);
	void setSampleRate(float sampleRate);
	void setSyncMode(int mode);
	void setInternalBpmValue(float internalBpm);
	void setDivision(int division);
	void syncClock();
	void setPos(uint32_t pos);
	void setNumBarsElapsed(uint32_t numBarsElapsed);
	void closeGate();
	void reset();
	bool getGate() const;
	float getSampleRate() const;
	int getSyncMode() const;
	float getInternalBpmValue() const;
	int getDivision() const;
	uint32_t getPeriod() const;
	uint32_t getPos() const;
	void tick();

private:
	void setBpm(float bpm);

	bool gate;
	bool trigger;
	bool sync;
	bool phaseReset;
	bool playing;
	bool previousPlaying;
	bool endOfBar;

	uint32_t period;
	uint32_t halfWavelength;
	uint32_t pos;

	float beatsPerBar;
	float bpm;
	float internalBpm;
	float hostBpm;
	float previousBpm;
	float sampleRate;
	int division;
	float divisionValue;

	float hostBarBeat;
	float beatTick;
	int syncMode;
	int previousSyncMode;
	int hostTick;
	int hostBeat;
	int barLength;
	int numBarsElapsed;
	int previousBeat;

	int arpMode;

    // "1/1" "1/2" "1/3" "1/4" "1/4." "1/4T" "1/8" "1/8." "1/8T" "1/16" "1/16." "1/16T" "1/32"
	float divisionValues[13] {0.5, 1, 1.5, 2, 2.66666, 3, 4, 5.33333, 6, 8, 10.66666, 12, 16};
};

#endif
