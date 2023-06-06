# Sculpt_Keyboard
A redesign of Microsoft Sculpt Keyboard electronics, with an ESP32S kit.

I got the idea from:
https://chrispaynter.medium.com/modding-the-microsoft-sculpt-ergonomic-keyboard-to-run-qmk-41d3d1caa7e6

And I saw another similar project using an ATMEGA128 self-designed pcb.
However I wanted to play with bluetooth, so I figured I might be able
to get it crammed into an ESP32 kit instead. Oh how naive I was.
Seems the ESP32S Node Dev Kit is basically an ESP32-WROOM, and you have like 20 usable gpios,
and another 2 as input only with no internal pull-up/downs, at least if you want bluetooth.

I plan on using:
https://github.com/T-vK/ESP32-BLE-Keyboard

Maybe migrate to some open source code once it's all prototyped.
