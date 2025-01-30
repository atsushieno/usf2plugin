
# USF2Plugin

USF2Plugin is a simple soundfont player plugin.

Not very interesting at the moment, except that the entire code can be released under very liberal license (MIT | ISC).

## TODOs

- implement GUI.
- add app configuration to store SF2 directory catalog (app settings, GUI).
- save SF2 relative file paths as a plugin state item.
- make it possible to specify initial program and save it as a plugin state item
  (alongside program change MIDI message).

## Licenses

USF2Plugin is released under the MIT license.

USF2Plugin uses the following third-party libraries and resources.

- [DISTRHO/DPF](https://github.com/DISTRHO/DPF/) - the ISC license.
- [schellingb/TinySoundFont](https://github.com/schellingb/TinySoundFont) - the MIT license. I made some changes and this app references my forked version (for better float buffer rendering).
- [mrbumpy409/GeneralUser-GS](https://github.com/mrbumpy409/GeneralUser-GS/) - ["do whatever you want" kind of](https://github.com/mrbumpy409/GeneralUser-GS/blob/d0fc360abafa736f11a1fa18c721f65bfc3a6991/documentation/LICENSE.txt)
- [Tracktion/choc](https://github.com/Tracktion/choc/) - the ISC license.
