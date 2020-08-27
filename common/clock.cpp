#include "clock.hpp"

PluginClock::PluginClock() :
	gate(false),
	trigger(false),
	sync(true),
	phaseReset(false),
	playing(false),
	previousPlaying(false),
	endOfBar(false),
	pos(0),
	bpm(120.0),
	internalBpm(120.0),
	previousBpm(0),
	sampleRate(48000.0),
	previousSyncMode(0),
	barLength(4),
	previousBeat(0)
{
	//TODO everything initialized?
}

PluginClock::~PluginClock()
{
}

void PluginClock::transmitHostInfo(const bool playing, const float beatsPerBar,
		const int hostBeat, const float hostBarBeat, const float hostBpm)
{
	this->beatsPerBar = beatsPerBar;
	this->hostBeat = hostBeat;
	this->hostBarBeat = hostBarBeat;
	this->hostBpm = hostBpm;
	this->playing = playing;

	if (playing && !previousPlaying && sync) {
		syncClock();
	}
	if (playing != previousPlaying) {
		previousPlaying = playing;
	}
}

void PluginClock::setSyncMode(int mode)
{
	switch (mode)
	{
		case FREE_RUNNING:
			sync = false;
			break;
		case HOST_SYNC:
			sync = true;
			break;
		case HOST_SYNC_QUANTIZED_START:
			sync = true;
			break;
	}

	this->syncMode = mode;
}

void PluginClock::setInternalBpmValue(float internalBpm)
{
	this->internalBpm = internalBpm;
}

void PluginClock::setBpm(float bpm)
{
	this->bpm = bpm;
}

void PluginClock::setSampleRate(float sampleRate)
{
	this->sampleRate = sampleRate;
}

void PluginClock::setDivision(int setDivision)
{
	this->division = setDivision;
	this->divisionValue = divisionValues[setDivision];
}

void PluginClock::syncClock()
{
	period = static_cast<uint32_t>(sampleRate * (60.0f / (bpm * (divisionValue / 2.0f))));
	period = (period <= 0) ? 1 : period;

    pos = static_cast<uint32_t>(fmod(sampleRate * (60.0f / bpm) * (hostBarBeat + (numBarsElapsed * beatsPerBar)), sampleRate * (60.0f / (bpm * (divisionValue / 2.0f)))));
}

void PluginClock::setPos(uint32_t pos)
{
	this->pos = pos;
}

void PluginClock::setNumBarsElapsed(uint32_t numBarsElapsed)
{
	this->numBarsElapsed = numBarsElapsed;
}

void PluginClock::closeGate()
{
	gate = false;
}

void PluginClock::reset()
{
	trigger = false;
}

float PluginClock::getSampleRate() const
{
	return sampleRate;
}

bool PluginClock::getGate() const
{
	return gate;
}

int PluginClock::getSyncMode() const
{
	return syncMode;
}

float PluginClock::getInternalBpmValue() const
{
	return internalBpm;
}

int PluginClock::getDivision() const
{
	return division;
}

uint32_t PluginClock::getPeriod() const
{
	return period;
}

uint32_t PluginClock::getPos() const
{
	return pos;
}

void PluginClock::tick()
{
	period = static_cast<uint32_t>(sampleRate * (60.0f / (bpm * (divisionValue / 2.0f))));
	halfWavelength = period / 2.0f;
	period = (period <= 0) ? 1 : period;

	int beat = static_cast<int>(hostBarBeat);

	if (beatsPerBar <= 1) {
		if (hostBarBeat > 0.99 && !endOfBar) {
			endOfBar = true;
		}
		else if (hostBarBeat < 0.1 && endOfBar) {
			numBarsElapsed++;
			endOfBar = false;
		}
	} else {
		if (beat != previousBeat) {
			numBarsElapsed = (beat == 0) ? numBarsElapsed + 1 : numBarsElapsed;
			previousBeat = beat;
		}
	}

	float threshold = 0.009; //TODO might not be needed

	switch (syncMode)
	{
		case FREE_RUNNING:
			if ((internalBpm != previousBpm) || (syncMode != previousSyncMode)) {
				setBpm(internalBpm);
				previousSyncMode = syncMode;
			}
			break;
		case HOST_SYNC:
			if ((hostBpm != previousBpm && (fabs(previousBpm - hostBpm) > threshold)) || (syncMode != previousSyncMode)) {
				setBpm(hostBpm);
				if (playing) {
					syncClock();
				}
				previousBpm = hostBpm;
				previousSyncMode = syncMode;
			}
			break;
		case HOST_SYNC_QUANTIZED_START: //TODO fix this duplicate
			if ((hostBpm != previousBpm && (fabs(previousBpm - hostBpm) > threshold)) || (syncMode != previousSyncMode)) {
				setBpm(hostBpm);
				if (playing) {
					syncClock();
				}
				previousBpm = hostBpm;
				previousSyncMode = syncMode;
			}
			break;
	}

	if (pos > period) {
		pos = 0;
	}

	if (pos < halfWavelength/2 && !trigger) {
		gate = true;
		trigger = true;
	} else if (pos > halfWavelength && trigger) {
		if (playing && sync) {
			syncClock();
		}
		trigger = false;
	}

	if (playing && sync) {
		syncClock(); //hard-sync to host position
	} else {
		pos++;
	}
}
