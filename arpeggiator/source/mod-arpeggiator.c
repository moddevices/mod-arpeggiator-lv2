#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <lv2/lv2plug.in/ns/lv2core/lv2.h>
#include <lv2/lv2plug.in/ns/ext/atom/atom.h>
#include <lv2/lv2plug.in/ns/ext/atom/forge.h>
#include <lv2/lv2plug.in/ns/ext/log/logger.h>
#include <lv2/lv2plug.in/ns/ext/midi/midi.h>
#include "lv2/lv2plug.in/ns/ext/time/time.h"
#include <lv2/lv2plug.in/ns/ext/urid/urid.h>

#ifndef DEBUG
#define DEBUG 0
#endif
#define debug_print(...) \
    ((void)((DEBUG) ? fprintf(stderr, __VA_ARGS__) : 0))

#define NUM_VOICES 16
#define PLUGIN_URI "http://moddevices.com/plugins/mod-devel/arpeggiator"


// Struct for a 3 byte MIDI event
typedef struct {
    LV2_Atom_Event event;
    uint8_t        msg[3];
} LV2_Atom_MIDI;


typedef enum {
    MIDI_IN = 0,
    MIDI_OUT,
    CV_GATE,
    BPM_PORT,
    ARP_MODE,
    LATCH_MODE,
    DIVISIONS_PORT,
    SYNC_PORT,
    NOTELENGTH,
    OCTAVESPREAD,
    OCTAVEMODE,
    VELOCITY,
    TIME_OUT,
    BYPASS
} PortIndex;

typedef enum {
    ARP_UP = 0,
    ARP_DOWN,
    ARP_UP_DOWN,
    ARP_UP_DOWN_ALT,
    ARP_PLAYED,
    ARP_RANDOM
} ArpEnum;

typedef enum {
    OCTAVE_UP = 0,
    OCTAVE_DOWN,
    OCTAVE_UP_DOWN,
    OCTAVE_DOWN_UP,
    OCTAVE_CYCLE
} OctaveEnum;


typedef struct {
    LV2_URID atom_Blank;
    LV2_URID atom_Float;
    LV2_URID atom_Object;
    LV2_URID atom_Path;
    LV2_URID atom_Resource;
    LV2_URID atom_Sequence;
    LV2_URID time_Position;
    LV2_URID time_barBeat;
    LV2_URID time_beatsPerMinute;
    LV2_URID time_speed;
} ClockURIs;

typedef struct {
    LV2_URID_Map*          map; // URID map feature
    LV2_Log_Log* 	       log;
    LV2_Log_Logger      logger; // Logger API
    ClockURIs             uris; // Cache of mapped URIDs

    // URIDs
    LV2_URID urid_midiEvent;

    const LV2_Atom_Sequence* MIDI_in;
    LV2_Atom_Sequence*       MIDI_out;

    float     divisions;
    double    samplerate;
    int       prev_sync;
    // Variables to keep track of the tempo information sent by the host
    float     bpm; // Beats per minute (tempo)
    float     host_bpm;
    float     previous_bpm;
    uint32_t  pos;
    uint32_t  period;
    uint32_t  h_wavelength;
    uint8_t   midi_notes[NUM_VOICES];
    uint8_t   previous_midinote;
    uint32_t  noteoff_buffer[NUM_VOICES][2];
    size_t    active_notes_index;
    int       note_played;
    int       first_note_timer;
    size_t    active_notes;
    size_t    notes_pressed;
    int       octave_index;
    int       events_counter;
    bool      triggered;
    bool      octave_up;
    bool      arp_up;
    bool      latch_playing;
    bool      first_note;
    bool      phase_reset;
    bool      first;
    bool      note_on_received;
    bool      switched_on;
    float     speed; // Transport speed (usually 0=stop, 1=play)
    float     beat_in_measure;
    float     previous_beat_in_measure;
    float     previous_latch;
    float     time_position;
    int       previous_octave_mode;
    int       bar_length;
    int       active_notes_bypassed;

    float*    cv_gate;
    float*    changeBpm;
    float*    arp_mode;
    float*    latch_mode;
    float*    changedDiv;
    float*    sync;
    float*    note_length;
    float*    octaveSpreadParam;
    float*    octaveModeParam;
    float*    velocity;
    float*    plugin_enabled;
    float*    time_out;
} Arpeggiator;


static void
swap(uint8_t *a, uint8_t *b)
{
    int temp = *a;
    *a = *b;
    *b = temp;
}


//got the code for the quick sort algorithm here https://medium.com/human-in-a-machine-world/quicksort-the-best-sorting-algorithm-6ab461b5a9d0
static void
quicksort(uint8_t arr[], int l, int r)
{
    if (l >= r)
    {
        return;
    }

    int pivot = arr[r];

    int cnt = l;

    for (int i = l; i <= r; i++)
    {
        if (arr[i] <= pivot)
        {
            swap(&arr[cnt], &arr[i]);
            cnt++;
        }
    }
    quicksort(arr, l, cnt-2);
    quicksort(arr, cnt, r);
}


static uint8_t
octaveHandler(Arpeggiator* self)
{
    uint8_t octave = 0;

    int octaveMode = *self->octaveModeParam;

    if (octaveMode != self->previous_octave_mode) {
        switch ((OctaveEnum)octaveMode)
        {
            case OCTAVE_UP:
                self->octave_index = self->note_played % (int)*self->octaveSpreadParam;
                break;
            case OCTAVE_DOWN:
                self->octave_index = self->note_played % (int)*self->octaveSpreadParam;
                self->octave_index = (int)*self->octaveSpreadParam;
                break;
            case OCTAVE_UP_DOWN:
                self->octave_index = self->note_played % (int)(*self->octaveSpreadParam * 2);
                if (self->octave_index > (int)*self->octaveSpreadParam) {
                    self->octave_index = abs((int)*self->octaveSpreadParam - (self->octave_index - (int)*self->octaveSpreadParam)) % (int)*self->octaveSpreadParam;
                }
                self->octave_up = !self->octave_up;
                break;
            case OCTAVE_DOWN_UP:
                self->octave_index = (int)*self->octaveSpreadParam;
                self->octave_up = !self->octave_up;
                break;
            case OCTAVE_CYCLE:
                self->octave_index = self->note_played % (int)*self->octaveSpreadParam;
                break;
        }
        self->previous_octave_mode = octaveMode;
    }

    if (*self->octaveSpreadParam > 1) {

        if (self->octave_index < 0) {
            self->octave_index += *self->octaveSpreadParam;
        }

        octave = 12 * (self->octave_index % (int)*self->octaveSpreadParam);

        switch (octaveMode)
        {
            case OCTAVE_UP:
                self->octave_index = (self->octave_index + 1) % (int)*self->octaveSpreadParam;
                break;
            case OCTAVE_DOWN:
                self->octave_index--;
                self->octave_index = (self->octave_index < 0) ? (int)*self->octaveSpreadParam - 1 : self->octave_index;
                break;
            case OCTAVE_UP_DOWN:
                if (self->octave_up) {
                    self->octave_index++;
                    self->octave_up = (self->octave_index >= (int)*self->octaveSpreadParam - 1) ? false : true;
                } else {
                    self->octave_index--;
                    self->octave_up = (self->octave_index <= 0) ? true : false;
                }
                break;
            case OCTAVE_DOWN_UP:
                if (!self->octave_up) {
                    self->octave_index--;
                    self->octave_up = (self->octave_index <= 0) ? true : false;
                } else {
                    self->octave_index = (self->octave_index + 1) % (int)*self->octaveSpreadParam;
                    self->octave_up = (self->octave_index >= (int)*self->octaveSpreadParam - 1) ? false : true;
                }
                break;
            case OCTAVE_CYCLE:
                if (self->note_played == 0) {
                    self->octave_index = (self->octave_index + 1) % (int)*self->octaveSpreadParam;
                }
                break;
        }
    } else {
        self->octave_index = 0;
    }

    return octave;
}



static LV2_Atom_MIDI
createMidiEvent(Arpeggiator* self, uint8_t status, uint8_t note, uint8_t velocity)
{
    LV2_Atom_MIDI msg;
    memset(&msg, 0, sizeof(LV2_Atom_MIDI));

    msg.event.body.size = 3;
    msg.event.body.type = self->urid_midiEvent;

    msg.msg[0] = status;
    msg.msg[1] = note;
    msg.msg[2] = velocity;

    return msg;
}



static void
handle_note_on(Arpeggiator* self, const uint32_t outCapacity)
{
    size_t searched_voices = 0;
    bool   note_found = false;

    while (!note_found && searched_voices < NUM_VOICES)
    {
        self->note_played = (self->note_played < 0) ? 0 : self->note_played;

        if (self->midi_notes[self->note_played] > 0
                && self->midi_notes[self->note_played] < 128)
        {
            uint8_t octave = octaveHandler(self);
            uint8_t velocity = (uint8_t)*self->velocity;

            //create MIDI note on message
            uint8_t midi_note = self->midi_notes[self->note_played] + octave;
            self->previous_midinote = midi_note;

            if (*self->plugin_enabled == 1) {
                LV2_Atom_MIDI onMsg = createMidiEvent(self, 144, midi_note, velocity);
                debug_print("midi_note = %i\n", midi_note);
                debug_print("self->note_played = %i\n", self->note_played);
                for (unsigned t = 0; t < 3; t++) {

                    debug_print("note array[%i] = %i\n", t, self->midi_notes[t]);
                }
                lv2_atom_sequence_append_event(self->MIDI_out, outCapacity, (LV2_Atom_Event*)&onMsg);
                self->noteoff_buffer[self->active_notes_index][0] = (uint32_t)midi_note;
            }
            self->active_notes_index = (self->active_notes_index + 1) % NUM_VOICES;
            note_found = true;
        }
        if ((ArpEnum)*self->arp_mode == ARP_UP || ((ArpEnum)*self->arp_mode == ARP_UP_DOWN && self->active_notes < 3)
                || (ArpEnum)*self->arp_mode == ARP_PLAYED ) {
            self->note_played = (self->note_played + 1) % NUM_VOICES;
        } else if ((ArpEnum)*self->arp_mode == ARP_DOWN) {
            self->note_played--;
            self->note_played = (self->note_played < 0) ? (int)self->active_notes : self->note_played;
        } else if ((ArpEnum)*self->arp_mode == ARP_RANDOM) {
            int active_div = (self->active_notes <= 0) ? 1 : (int)self->active_notes;
            self->note_played = random() % active_div;
        } else{
            if (self->arp_up) {
                self->note_played++;
                if (self->note_played >= self->active_notes) {
                   self->arp_up = false;
                   if ((ArpEnum)*self->arp_mode != ARP_UP_DOWN_ALT) {
                       self->note_played = (self->active_notes > 1) ? self->note_played - 2 : self->note_played;
                   }
                }
            } else {
                self->note_played--;
                if ((ArpEnum)*self->arp_mode != ARP_UP_DOWN_ALT) {
                    self->arp_up = (self->note_played <= 0) ? true : false;
                } else {
                    self->arp_up = (self->note_played < 0) ? true : false;
                }
            }
        }
        searched_voices++;
    }
}



static void
handle_note_off(Arpeggiator* self, const uint32_t outCapacity)
{
    for (size_t i = 0; i < NUM_VOICES; i++) {
        if (self->noteoff_buffer[i][0] > 0) {
            self->noteoff_buffer[i][1] += 1;
            if (self->noteoff_buffer[i][1] > (uint32_t)(self->period * *self->note_length)) {
                LV2_Atom_MIDI offMsg = createMidiEvent(self, 128, (uint8_t)self->noteoff_buffer[i][0], 0);
                lv2_atom_sequence_append_event(self->MIDI_out, outCapacity, (LV2_Atom_Event*)&offMsg);
                self->noteoff_buffer[i][0] = 0;
                self->noteoff_buffer[i][1] = 0;
            }
        }
    }
}


static void
connect_port(LV2_Handle instance,
        uint32_t   port,
        void*      data)
{
    Arpeggiator* self = (Arpeggiator*)instance;

    switch ((PortIndex)port) {
        case MIDI_IN:
            self->MIDI_in = (const LV2_Atom_Sequence*)data;
            break;
        case MIDI_OUT:
            self->MIDI_out = (LV2_Atom_Sequence*)data;
            break;
        case CV_GATE:
            self->cv_gate = (float*)data;
            break;
        case BPM_PORT:
            self->changeBpm = (float*)data;
            break;
        case ARP_MODE:
            self->arp_mode = (float*)data;
            break;
        case LATCH_MODE:
            self->latch_mode = (float*)data;
            break;
        case DIVISIONS_PORT:
            self->changedDiv = (float*)data;
            break;
        case SYNC_PORT:
            self->sync = (float*)data;
            break;
        case NOTELENGTH:
            self->note_length = (float*)data;
            break;
        case OCTAVESPREAD:
            self->octaveSpreadParam = (float*)data;
            break;
        case OCTAVEMODE:
            self->octaveModeParam = (float*)data;
            break;
        case VELOCITY:
            self->velocity = (float*)data;
            break;
        case TIME_OUT:
            self->time_out = (float*)data;
            break;
        case BYPASS:
            self->plugin_enabled = (float*)data;
            break;
    }
}



static void
activate(LV2_Handle instance)
{
    Arpeggiator* self = (Arpeggiator*)instance;

    self->bpm = *self->changeBpm;
    self->divisions =*self->changedDiv;
    self->pos = 0;
}

static void
clear(Arpeggiator* self)
{
    //self->prev_sync   = 0;
    self->triggered = false;
    self->octave_up = false;
    self->arp_up    = true;
    self->active_notes_index = 0;
    self->note_played = 0;
    self->active_notes = 0;
    self->previous_octave_mode = 0;
    self->octave_index = 0;
    self->previous_latch = 0;
    self->previous_midinote = 0;
    self->notes_pressed = 0;
    self->active_notes_bypassed = 0;
    self->latch_playing = false;
    self->first_note = false;
    self->phase_reset = false;
    self->first = true;
    self->note_on_received = false;
    self->switched_on = false;
    self->bar_length = 4; //TODO make this variable

    for (unsigned i = 0; i < NUM_VOICES; i++) {
        self->midi_notes[i] = 200;
    }
}


static LV2_Handle
instantiate(const LV2_Descriptor*     descriptor,
        double                    rate,
        const char*               bundle_path,
        const LV2_Feature* const* features)
{
    Arpeggiator* self = (Arpeggiator*)calloc(1, sizeof(Arpeggiator));
    if (!self)
    {
        return NULL;
    }

    for (uint32_t i=0; features[i]; ++i)
    {
        if (!strcmp (features[i]->URI, LV2_URID__map))
        {
            self->map = (LV2_URID_Map*)features[i]->data;
        }
        else if (!strcmp (features[i]->URI, LV2_LOG__log))
        {
            self->log = (LV2_Log_Log*)features[i]->data;
        }
    }

    lv2_log_logger_init (&self->logger, self->map, self->log);

    if (!self->map) {
        lv2_log_error (&self->logger, "arpeggiator.lv2 error: Host does not support urid:map\n");
        free (self);
        return NULL;
    }

    // Map URIS
    ClockURIs* const    uris  = &self->uris;
    LV2_URID_Map* const map   = self->map;
    self->urid_midiEvent      = map->map(map->handle, LV2_MIDI__MidiEvent);
    uris->atom_Blank          = map->map(map->handle, LV2_ATOM__Blank);
    uris->atom_Float          = map->map(map->handle, LV2_ATOM__Float);
    uris->atom_Object         = map->map(map->handle, LV2_ATOM__Object);
    uris->atom_Path           = map->map(map->handle, LV2_ATOM__Path);
    uris->atom_Resource       = map->map(map->handle, LV2_ATOM__Resource);
    uris->atom_Sequence       = map->map(map->handle, LV2_ATOM__Sequence);
    uris->time_Position       = map->map(map->handle, LV2_TIME__Position);
    uris->time_barBeat        = map->map(map->handle, LV2_TIME__barBeat);
    uris->time_beatsPerMinute = map->map(map->handle, LV2_TIME__beatsPerMinute);
    uris->time_speed          = map->map(map->handle, LV2_TIME__speed);

    debug_print("DEBUGING");


    self->first_note_timer = 0;
    self->bpm = 120.0;
    self->samplerate = rate;
    self->prev_sync   = 0;
    self->beat_in_measure = 0.0;
    self->previous_beat_in_measure = 0.0;
    self->triggered = false;
    self->octave_up = false;
    self->arp_up    = true;
    self->active_notes_index = 0;
    self->note_played = 0;
    self->active_notes = 0;
    self->previous_octave_mode = 0;
    self->octave_index = 0;
    self->previous_latch = 0;
    self->previous_midinote = 0;
    self->notes_pressed = 0;
    self->active_notes_bypassed = 0;
    self->latch_playing = false;
    self->first_note = false;
    self->phase_reset = false;
    self->first = true;
    self->note_on_received = false;
    self->switched_on = false;
    self->bar_length = 4; //TODO make this variable

    self->events_counter = 0;

    for (unsigned i = 0; i < NUM_VOICES; i++) {
        self->midi_notes[i] = 200;
    }
    for (unsigned i = 0; i < NUM_VOICES; i++) {
        for (unsigned x = 0; x < 2; x++) {
            self->noteoff_buffer[i][x] = 0;
        }
    }

    return (LV2_Handle)self;
}




static void
update_position(Arpeggiator* self, const LV2_Atom_Object* obj)
{
    const ClockURIs* uris = &self->uris;

    // Received new transport position/speed
    LV2_Atom *beat = NULL, *bpm = NULL, *speed = NULL;
    lv2_atom_object_get(obj,
            uris->time_barBeat, &beat,
            uris->time_beatsPerMinute, &bpm,
            uris->time_speed, &speed,
            NULL);
    if (bpm && bpm->type == uris->atom_Float)
    {
        // Tempo changed, update BPM
        self->host_bpm = ((LV2_Atom_Float*)bpm)->body;
    }
    if (speed && speed->type == uris->atom_Float)
    {
        // Speed changed, e.g. 0 (stop) to 1 (play)
        self->speed = ((LV2_Atom_Float*)speed)->body;
    }
    if (beat && beat->type == uris->atom_Float)
    {
        // Received a beat position, synchronise
        self->beat_in_measure = ((LV2_Atom_Float*)beat)->body;
    }
}



static uint32_t
reset_phase(Arpeggiator* self)
{
    uint32_t pos = (uint32_t)fmod(self->samplerate * (60.0f / self->bpm) * self->beat_in_measure, (self->samplerate * (60.0f / (self->bpm * (self->divisions / 2.0f)))));

    return pos;
}


static void
run(LV2_Handle instance, uint32_t n_samples)
{
    Arpeggiator* self = (Arpeggiator*)instance;
    const ClockURIs* uris = &self->uris;

    float current_beat_pos = self->beat_in_measure;

    self->MIDI_out->atom.type = self->MIDI_in->atom.type;
    const uint32_t out_capacity = self->MIDI_out->atom.size;

    // Write an empty Sequence header to the output
    lv2_atom_sequence_clear(self->MIDI_out);

    if (*self->latch_mode == 0 && self->previous_latch == 1 && self->notes_pressed <= 0) {
        clear(self);
        //for (unsigned i = 0; i < NUM_VOICES; i++) {
        //    self->midi_notes[i] = 200;
        //    self->note_played = 0;
        //}
    }
    if (*self->latch_mode != self->previous_latch) {
        self->previous_latch = *self->latch_mode;
    }

    // Read incoming events
    LV2_ATOM_SEQUENCE_FOREACH(self->MIDI_in, ev)
    {
        size_t search_note;
        if (ev->body.type == uris->atom_Object ||
                ev->body.type == uris->atom_Blank) {
            const LV2_Atom_Object* obj = (const LV2_Atom_Object*)&ev->body;
            if (obj->body.otype == uris->time_Position) {
                update_position(self, obj);
            }
        }
        else if (ev->body.type == self->urid_midiEvent)
        {
            const uint8_t* const msg = (const uint8_t*)(ev + 1);

            const uint8_t status = msg[0] & 0xF0;

            if (*self->plugin_enabled == 1) {

                uint8_t midi_note = msg[1];
                uint8_t note_to_find;
                size_t find_free_voice;
                bool voice_found;

                if (midi_note == 0x7b) {
                    self->active_notes = 0;
                    for (unsigned i = 0; i < NUM_VOICES; i++) {
                        self->midi_notes[i] = 200;
                    }
                }

                switch (status)
                {
                    case LV2_MIDI_MSG_NOTE_ON:
                        if (self->notes_pressed == 0) {
                            if (!self->latch_playing) { //TODO check if there needs to be an exception when using sync
                                if (*self->sync == 0.0) {
                                    self->pos = 0;
                                }
                                self->octave_index = 0;
                                self->note_played = 0;
                                self->triggered = false;
                            }
                            if (*self->latch_mode == 1) {
                                self->latch_playing = true;
                                self->active_notes = 0;
                                for (unsigned i = 0; i < NUM_VOICES; i++) {
                                    self->midi_notes[i] = 200;
                                }
                            }
                            if (*self->sync == 1.0 && !self->latch_playing) {
                                self->first_note = true;
                            }
                        }
                        self->notes_pressed++;
                        self->active_notes++;
                        find_free_voice = 0;
                        voice_found = false;
                        while (find_free_voice < NUM_VOICES && !voice_found)
                        {
                            if (self->midi_notes[find_free_voice] == 200) {
                                self->midi_notes[find_free_voice] = midi_note;
                                voice_found = true;
                            }
                            find_free_voice++;
                        }
                        if ((ArpEnum)*self->arp_mode != ARP_PLAYED)
                            quicksort(self->midi_notes, 0, NUM_VOICES - 1);
                        if (midi_note < self->midi_notes[self->note_played - 1] &&
                                self->note_played > 0) {
                            self->note_played++;
                        }
                        break;
                    case LV2_MIDI_MSG_NOTE_OFF:
                        //self->notes_pressed--;
                        debug_print("============================================\n");
                        search_note = 0;
                        note_to_find = midi_note;
                        if (!self->latch_playing) {
                            self->notes_pressed--;
                            self->active_notes = self->notes_pressed;
                        }
                        else {
                            while (search_note < NUM_VOICES)
                            {
                                if (self->midi_notes[search_note] == note_to_find) {
                                    search_note = NUM_VOICES;
                                    self->notes_pressed--;
                                }
                                search_note++;
                            }
                        }
                        if (*self->latch_mode == 0) {
                            self->latch_playing = false;
                            while (search_note < NUM_VOICES)
                            {
                                if (self->midi_notes[search_note] == note_to_find)
                                {
                                    self->midi_notes[search_note] = 200;
                                    search_note = NUM_VOICES;
                                }
                                search_note++;
                            }
                            if ((ArpEnum)*self->arp_mode != ARP_PLAYED)
                                quicksort(self->midi_notes, 0, NUM_VOICES - 1);
                        }
                        break;
                    default:
                        break;
                }
            }
            else {

                if (*self->latch_mode == 0) {
                    for (unsigned clear_notes = 0; clear_notes < NUM_VOICES; clear_notes++)
                        self->midi_notes[clear_notes] = 200;
                }
                if (!self->switched_on) {
                    self->active_notes_bypassed = self->active_notes;
                    self->switched_on = true;
                }

                switch (status)
                {
                    case LV2_MIDI_MSG_NOTE_ON:
                        self->note_on_received = true;
                        self->active_notes_bypassed++;
                        break;
                    case LV2_MIDI_MSG_NOTE_OFF:
                        if (self->note_on_received) {
                            self->active_notes_bypassed--;
                        }
                        break;
                    default:
                        break;
                }
                //send MIDI message through
                lv2_atom_sequence_append_event(self->MIDI_out, out_capacity, ev);
                self->first = true;

            }
        }
    }
    for(uint32_t i = 0; i < n_samples; i ++) {
        //map bpm to host or to bpm parameter
        if (*self->sync == 0.0) {
            self->bpm = *self->changeBpm;
        } else {
            self->bpm = self->host_bpm;
        }

        if  (*self->sync > 0.0) {
            if (self->beat_in_measure < 0.5 && !self->phase_reset) {
                self->pos = reset_phase(self);
                self->phase_reset = true;
            } else if (self->beat_in_measure >= 1.0 && self->phase_reset && self->bar_length > 1.0) {
                self->phase_reset = false;
            } else if (self->beat_in_measure > 0.9 && self->phase_reset && self->bar_length == 0.0) {
                self->phase_reset = false;
            }
        }

        //resync phase when tempo is changed
        if (self->bpm != self->previous_bpm && *self->sync > 0.0) {
            self->pos = reset_phase(self);
            self->previous_bpm = self->bpm;
        }
        //reset phase when sync is turned on
        if (*self->sync != self->prev_sync) {
            self->pos = reset_phase(self);
            self->prev_sync = *self->sync;
        }
        //reset phase when there is a new division
        if (self->divisions != *self->changedDiv) {
            self->divisions = *self->changedDiv;
            self->pos = reset_phase(self);
        }
        //set CV gate
        if (self->notes_pressed > 0) {
            self->cv_gate[i] = 1.0;
        } else {
            self->cv_gate[i] = 0.0;
        }

        self->period = (uint32_t)(self->samplerate * (60.0f / (self->bpm * (self->divisions / 2.0f))));
        self->h_wavelength = (self->period/2.0f);

        if(self->pos >= self->period && i < n_samples) {
            self->pos = 0;
        } else {
            if (self->first_note) {
                self->first_note_timer++;
            }
            if(((self->pos < self->h_wavelength && !self->triggered) && !self->first_note) || (self->first_note_timer > (int)*self->time_out)) {

                
                debug_print("note off time = %i\n", self->first_note_timer);
                //debug_print("self->events_coutner = %i\n", self->events_counter);
                //debug_print("self->pos %i\n", self->pos);
                self->events_counter = 0;

                if (self->first && *self->plugin_enabled == 1.0) { //clear all notes before begining off sequence
                    for (uint8_t note_off = 0; note_off < 127; note_off++) {
                        LV2_Atom_MIDI offMsg = createMidiEvent(self, 128, note_off, 0);
                        lv2_atom_sequence_append_event(self->MIDI_out, out_capacity, (LV2_Atom_Event*)&offMsg);
                    }
                    self->first = false;
                }

                //trigger MIDI message
                handle_note_on(self, out_capacity);

                self->triggered = true;
                self->first_note = false;
                self->first_note_timer = 0;
            } else if (self->pos > self->h_wavelength && self->triggered) {
                //set gate
                self->triggered = false;
                reset_phase(self);
            }
        }
        handle_note_off(self, out_capacity);
        self->pos += 1;
        self->events_counter++;
    }
    self->previous_beat_in_measure = current_beat_pos;
}



static void
deactivate(LV2_Handle instance)
{
}

static void
cleanup(LV2_Handle instance)
{
    free(instance);
}

static const void*
extension_data(const char* uri)
{
    return NULL;
}

static const LV2_Descriptor descriptor = {
    PLUGIN_URI,
    instantiate,
    connect_port,
    activate,
    run,
    deactivate,
    cleanup,
    extension_data
};

LV2_SYMBOL_EXPORT
    const LV2_Descriptor*
lv2_descriptor(uint32_t index)
{
    switch (index) {
        case 0:  return &descriptor;
        default: return NULL;
    }
}
