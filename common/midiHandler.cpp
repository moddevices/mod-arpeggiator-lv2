#include "midiHandler.hpp"

MidiHandler::MidiHandler()
{
}

MidiHandler::~MidiHandler()
{
}

void MidiHandler::emptyMidiBuffer()
{
	buffer.numBufferedEvents = 0;
	buffer.numBufferedThroughEvents = 0;
}

void MidiHandler::appendMidiMessage(MidiEvent event)
{
	buffer.bufferedEvents[buffer.numBufferedEvents] = event;
	buffer.numBufferedEvents = (buffer.numBufferedEvents + 1) % buffer.maxBufferSize;
}

void MidiHandler::appendMidiThroughMessage(MidiEvent event)
{
	buffer.bufferedMidiThroughEvents[buffer.numBufferedThroughEvents] = event;
	buffer.numBufferedThroughEvents = (buffer.numBufferedThroughEvents + 1) % buffer.maxBufferSize;
}

void MidiHandler::mergeBuffers()
{
	for (unsigned e = 0; e < buffer.numBufferedThroughEvents; e++) {
		buffer.bufferedEvents[e + buffer.numBufferedEvents] = buffer.bufferedMidiThroughEvents[e];
	}
}

struct MidiBuffer MidiHandler::getMidiBuffer()
{
	mergeBuffers();
	return buffer;
}
