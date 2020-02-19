This repository contains an arpeggiator and a MIDI-pattern plugin developed for the MOD platform.

# Arpeggiator

The arpeggiator has the following options:
* Tempo options:
    * There are a few options too set the tempo of the plugin. The first one is
      the `Free Running` mode. In this mode the
      tempo is controlled by the `BPM` control of the plugin.
    * On top of the `BPM` control there is a `Divisions`
      control.
    * The plugin can also be synced to the host.

* Arpeggiator modes:
    * The arpeggiator has the following modes:

    ```
    Up
    Down
    Up-Down
    Up-Down(alternative)
    Played
    Random
    ```

* Octave Modes:
    * The arpeggiator has `octave spread` control.
    The arpeggiator can iterate the incoming notes over multiple octaves.
    This control sets the range of the octaves that will be iterated over the
    original pitch. The way how this octaves will be added to the original notes
    is determent by the `octave mode` control.

# MIDI-pattern

The MIDI-pattern plugin can be used to create rhythmic
patterns. The plugins takes the incoming note and replaces
the velocity of the note by a value which is set by one of the
faders of the plugin. Because the plugin iterates through
the faders it generates a sort of rhythmic sequence. The CV control of the plugin
can be used to retrigger the sequence.

# Installation

To install the plugins do:
```
git clone https://github.com/moddevices/mod-arpeggiator-lv2.git
cd mod-arpeggiator-lv2
make
make install
```

# Caveats

* The plugins can be used outside of the MOD ecosystem. But
  because they are developed for usage inside the MOD there
  might be things that won't work properly.
* The plugins contain lv2 "CV ports", this feature is not supported by all
plugin hosts (yet).
