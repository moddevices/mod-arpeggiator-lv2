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
  because they are developed for usage inside the MOD, there
  might be things that won't work properly. Especially the host-sync
  might not work as intended on other plugin-hosts.
* DPF supports a range of plugins formats. However, this plugin
  uses some LV2 specific properties for the host sync function.
  Therefore this plugin is only available as an LV2 plugin for now.
