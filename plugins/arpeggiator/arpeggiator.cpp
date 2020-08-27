#include "arpeggiator.hpp"

Arpeggiator::Arpeggiator()
{
	clock.transmitHostInfo(0, 4, 1, 1, 120.0);
	clock.setSampleRate(static_cast<float>(48000.0));
	clock.setDivision(7);

	arpPattern = new Pattern*[6];

	arpPattern[0] = new PatternUp();
	arpPattern[1] = new PatternDown();
	arpPattern[2] = new PatternUpDown();
	arpPattern[3] = new PatternUpDownAlt();
	arpPattern[4] = new PatternUp();
	arpPattern[5] = new PatternRandom();


	octavePattern = new Pattern*[6];

	octavePattern[0] = new PatternUp();
	octavePattern[1] = new PatternDown();
	octavePattern[2] = new PatternUpDown();
	octavePattern[3] = new PatternUpDownAlt();
	octavePattern[4] = new PatternCycle();

	for (unsigned i = 0; i < NUM_VOICES; i++) {
		midiNotes[i][0] = EMPTY_SLOT;
		midiNotes[i][1] = 0;
	}
	for (unsigned i = 0; i < NUM_VOICES; i++) {
		for (unsigned x = 0; x < 3; x++) {
			noteOffBuffer[i][x] = 0;
		}
	}
}

Arpeggiator::~Arpeggiator()
{
	delete[] arpPattern;
	arpPattern = nullptr;
	delete[] octavePattern;
	octavePattern = nullptr;
}

void Arpeggiator::setArpEnabled(bool arpEnabled)
{
	this->arpEnabled = arpEnabled;
}

void Arpeggiator::setLatchMode(bool latchMode)
{
	this->latchMode = latchMode;
}

void Arpeggiator::setSampleRate(float sampleRate)
{
	clock.setSampleRate(sampleRate);
}

void Arpeggiator::setSyncMode(int mode) //TODO implement something for quantized start
{
	switch (mode)
	{
		case FREE_RUNNING:
			clock.setSyncMode(FREE_RUNNING);
			quantizedStart = false;
			break;
		case HOST_SYNC:
			clock.setSyncMode(HOST_SYNC);
			quantizedStart = false;
			break;
		case HOST_SYNC_QUANTIZED_START:
			clock.setSyncMode(HOST_SYNC_QUANTIZED_START);
			quantizedStart = true;
			break;
	}
}

void Arpeggiator::setBpm(double bpm)
{
	clock.setInternalBpmValue(static_cast<float>(bpm));
}

void Arpeggiator::setDivision(int division)
{
	clock.setDivision(division);
}

void Arpeggiator::setVelocity(uint8_t velocity)
{
	this->velocity = velocity;
}

void Arpeggiator::setNoteLength(float noteLength)
{
	this->noteLength = noteLength;
}

void Arpeggiator::setOctaveSpread(int octaveSpread)
{
	this->octaveSpread = octaveSpread;
}

void Arpeggiator::setArpMode(int arpMode)
{
	arpPattern[arpMode]->setStep(arpPattern[this->arpMode]->getStep());
	arpPattern[arpMode]->setDirection(arpPattern[this->arpMode]->getDirection());

	this->arpMode = arpMode;
}

void Arpeggiator::setOctaveMode(int octaveMode)
{
	octavePattern[octaveMode]->setStep(octavePattern[this->octaveMode]->getStep());
	octavePattern[octaveMode]->setDirection(octavePattern[this->octaveMode]->getDirection());

	this->octaveMode = octaveMode;
}

void Arpeggiator::setTimeOut(int timeOutTime)
{
	this->timeOutTime = timeOutTime;
}

bool Arpeggiator::getArpEnabled() const
{
	return arpEnabled;
}

bool Arpeggiator::getLatchMode() const
{
	return latchMode;
}

float Arpeggiator::getSampleRate() const
{
	return clock.getSampleRate();
}

int Arpeggiator::getSyncMode() const
{
	return clock.getSyncMode();
}

float Arpeggiator::getBpm() const
{
	return clock.getInternalBpmValue();
}

int Arpeggiator::getDivision() const
{
	return clock.getDivision();
}

uint8_t Arpeggiator::getVelocity() const
{
	return velocity;
}

float Arpeggiator::getNoteLength() const
{
	return 0.8;
}

int Arpeggiator::getOctaveSpread() const
{
	return 0;
}

int Arpeggiator::getArpMode() const
{
	return arpMode;
}

int Arpeggiator::getOctaveMode() const
{
	return octaveMode;
}

int Arpeggiator::getTimeOut() const
{
	return timeOutTime;
}

void Arpeggiator::transmitHostInfo(const bool playing, const float beatsPerBar,
		const int beat, const float barBeat, const double bpm)
{
	clock.transmitHostInfo(playing, beatsPerBar, beat, barBeat, bpm);
	this->barBeat = barBeat;
}

void Arpeggiator::reset()
{
	clock.reset();
	clock.setNumBarsElapsed(0);
	arpPattern[arpMode]->reset(); //TODO reset all
	octavePattern[octaveMode]->setStep(0);

	activeNotesIndex = 0;
	firstNoteTimer  = 0;
	notePlayed = 0;
	activeNotes = 0;
	previousLatch = 0;
	notesPressed = 0;
	activeNotesBypassed = 0;
	latchPlaying = false;
	firstNote = false;
	first = true;

	for (unsigned i = 0; i < NUM_VOICES; i++) {
		midiNotes[i][MIDI_NOTE] = EMPTY_SLOT;
		midiNotes[i][MIDI_CHANNEL] = 0;
	}
}

void Arpeggiator::emptyMidiBuffer()
{
	midiHandler.emptyMidiBuffer();
}

struct MidiBuffer Arpeggiator::getMidiBuffer()
{
	return midiHandler.getMidiBuffer();
}

void Arpeggiator::process(const MidiEvent* events, uint32_t eventCount, uint32_t n_frames)
{
    struct MidiEvent midiEvent;
    struct MidiEvent midiThroughEvent;

	if (!latchMode && previousLatch && notesPressed <= 0) {
		reset();
	}
	if (latchMode != previousLatch) {
		previousLatch = latchMode;
	}

	for (uint32_t i=0; i<eventCount; ++i) {

		uint8_t status = events[i].data[0] & 0xF0;

		uint8_t midiNote = events[i].data[1];


		if (arpEnabled) {

			uint8_t noteToFind;
			size_t findFreeVoice;
			size_t searchNote;
			bool voiceFound;

			if (midiNote == 0x7b && events[i].size == 3) {
				activeNotes = 0;
				for (unsigned i = 0; i < NUM_VOICES; i++) {
					midiNotes[i][0] = EMPTY_SLOT;
					midiNotes[i][1] = 0;
				}
			}

			uint8_t channel = events[i].data[0] & 0x0F;

			switch(status) {
				case MIDI_NOTEON:
					if (notesPressed == 0) {
						if (!latchPlaying) { //TODO check if there needs to be an exception when using sync
							octavePattern[octaveMode]->reset();
							clock.reset();
							notePlayed = 0;
							firstNote = true;
						}
						if (latchMode) {
							latchPlaying = true;
							activeNotes = 0;
							for (unsigned i = 0; i < NUM_VOICES; i++) {
								midiNotes[i][MIDI_NOTE] = EMPTY_SLOT;
								midiNotes[i][MIDI_CHANNEL] = 0;
							}
						}
						resetPattern = true;
					}
					notesPressed++;
					activeNotes++;
					findFreeVoice = 0;
					voiceFound = false;
					while (findFreeVoice < NUM_VOICES && !voiceFound)
					{
						if (midiNotes[findFreeVoice][MIDI_NOTE] == EMPTY_SLOT) {
							midiNotes[findFreeVoice][MIDI_NOTE] = midiNote;
							midiNotes[findFreeVoice][MIDI_CHANNEL] = channel;
							voiceFound = true;
						}
						findFreeVoice++;
					}
					if (arpMode != ARP_PLAYED)
						utils.quicksort(midiNotes, 0, NUM_VOICES - 1);
					if (midiNote < midiNotes[notePlayed - 1][MIDI_NOTE] && notePlayed > 0) {
						notePlayed++;
					}
					break;
				case MIDI_NOTEOFF:
					searchNote = 0;
					noteToFind = midiNote;
					if (!latchMode) {
						latchPlaying = false;
					} else {
						latchPlaying = true;
					}
					if (!latchPlaying) {
						notesPressed = (notesPressed > 0) ? notesPressed - 1 : 0;
						activeNotes = notesPressed;
					}
					else {
						while (searchNote < NUM_VOICES)
						{
							if (midiNotes[searchNote][MIDI_NOTE] == noteToFind) {
								searchNote = NUM_VOICES;
								notesPressed = (notesPressed > 0) ? notesPressed - 1 : 0;
							}
							searchNote++;
						}
					}
					if (!latchMode) {
						while (searchNote < NUM_VOICES)
						{
							if (midiNotes[searchNote][MIDI_NOTE] == noteToFind)
							{
								midiNotes[searchNote][MIDI_NOTE] = EMPTY_SLOT;
								midiNotes[searchNote][MIDI_CHANNEL] = 0;
								searchNote = NUM_VOICES;
							}
							searchNote++;
						}
						if (arpMode != ARP_PLAYED)
							utils.quicksort(midiNotes, 0, NUM_VOICES - 1);
					}
					if (activeNotes == 0 && !latchPlaying && !latchMode) {
						reset();
					}
					break;
				default:
					midiThroughEvent.frame = events[i].frame;
					midiThroughEvent.size = events[i].size;
					for (unsigned d = 0; d < midiThroughEvent.size; d++) {
						midiThroughEvent.data[d] = events[i].data[d];
					}
					midiHandler.appendMidiThroughMessage(midiThroughEvent);
					break;
			}
		} else { //if arpeggiator is off
			if (!latchMode) {
				for (unsigned clear_notes = 0; clear_notes < NUM_VOICES; clear_notes++) {
					midiNotes[clear_notes][MIDI_NOTE] = EMPTY_SLOT;
					midiNotes[clear_notes][MIDI_CHANNEL] = 0;
				}
			}
			//send MIDI message through
			midiHandler.appendMidiThroughMessage(events[i]);
			first = true;
		}
	}

	arpPattern[arpMode]->setPatternSize(activeNotes);


	switch (octaveMode)
	{
		case ONE_OCT_UP_PER_CYCLE:
			octavePattern[octaveMode]->setPatternSize(activeNotes);
			octavePattern[octaveMode]->setCycleRange(octaveSpread);
			break;
		default:
			octavePattern[octaveMode]->setPatternSize(octaveSpread);
			break;
	}


	for (unsigned s = 0; s < n_frames; s++) {

		bool timeOut = (firstNoteTimer > (int)timeOutTime) ? false : true;

		if (firstNote) {
			clock.closeGate(); //close gate to prevent opening before timeOut
			firstNoteTimer++;
		}

		if (clock.getSyncMode() == 0 && first) {
			clock.setPos(0);
			clock.reset();
		}

		clock.tick();

		bool hold = (clock.getPos() < (clock.getPeriod() * 0.8)) ? false : true;

		if ((clock.getGate() && !timeOut && !hold) || (firstNote && !timeOut && !hold && clock.getSyncMode() == 1)) {

			if (first && arpEnabled) { //clear all notes before begining off sequence //TODO addopt this for all channels

				if (resetPattern) {
					octavePattern[octaveMode]->reset();
					if (octaveMode == ARP_DOWN) {
						octavePattern[octaveMode]->setStep(activeNotes - 1); //TODO put this in reset()
					}
					octavePattern[octaveMode]->reset();

					arpPattern[arpMode]->reset();
					if (arpMode == ARP_DOWN) {
						arpPattern[arpMode]->setStep(activeNotes - 1); //TODO put this in reset()
					}
					octavePattern[octaveMode]->reset();
					resetPattern = false;
					notePlayed = arpPattern[arpMode]->getStep();
				}

				for (uint8_t c = 0; c < NUM_MIDI_CHANNELS; c++) {
					//send note off for everything
					midiEvent.frame = s;
					midiEvent.size = 3;
					midiEvent.data[0] = 0xb0 | c;
					midiEvent.data[1] = 0x7b;
					midiEvent.data[2] = 0;

					midiHandler.appendMidiMessage(midiEvent);
					first = false;
				}
			}

			size_t searchedVoices = 0;
			bool   noteFound = false;

			while (!noteFound && searchedVoices < NUM_VOICES)
			{
				notePlayed = (notePlayed < 0) ? 0 : notePlayed;

				if (midiNotes[notePlayed][MIDI_NOTE] > 0
						&& midiNotes[notePlayed][MIDI_NOTE] < 128)
				{
					//create MIDI note on message
					uint8_t midiNote = midiNotes[notePlayed][MIDI_NOTE];
					uint8_t channel = midiNotes[notePlayed][MIDI_CHANNEL];

					if (arpEnabled) {

						uint8_t octave = octavePattern[octaveMode]->getStep() * 12;
						octavePattern[octaveMode]->goToNextStep();

						midiNote = midiNote + octave;

						midiEvent.frame = s;
						midiEvent.size = 3;
						midiEvent.data[0] = MIDI_NOTEON | channel;
						midiEvent.data[1] = midiNote;
						midiEvent.data[2] = velocity;

						midiHandler.appendMidiMessage(midiEvent);

						noteOffBuffer[activeNotesIndex][MIDI_NOTE] = (uint32_t)midiNote;
						noteOffBuffer[activeNotesIndex][MIDI_CHANNEL] = (uint32_t)channel;
						activeNotesIndex = (activeNotesIndex + 1) % NUM_NOTE_OFF_SLOTS;
						noteFound = true;
						firstNote = false;
					}
				}
				arpPattern[arpMode]->goToNextStep();
				notePlayed = arpPattern[arpMode]->getStep();
				searchedVoices++;
			}
			clock.closeGate();
		}

		for (size_t i = 0; i < NUM_NOTE_OFF_SLOTS; i++) {
			if (noteOffBuffer[i][MIDI_NOTE] > 0) { //TODO check for other number than zero
				noteOffBuffer[i][TIMER] += 1;
				if (noteOffBuffer[i][TIMER] > static_cast<uint32_t>(clock.getPeriod() * noteLength)) {
					midiEvent.frame = s;
					midiEvent.size = 3;
					midiEvent.data[0] = MIDI_NOTEOFF | noteOffBuffer[i][MIDI_CHANNEL];
					midiEvent.data[1] = static_cast<uint8_t>(noteOffBuffer[i][MIDI_NOTE]);
					midiEvent.data[2] = 0;

					midiHandler.appendMidiMessage(midiEvent);

					noteOffBuffer[i][MIDI_NOTE] = 0;
					noteOffBuffer[i][MIDI_CHANNEL] = 0;
					noteOffBuffer[i][TIMER] = 0;

				}
			}
		}
	}
	midiHandler.mergeBuffers();
}
