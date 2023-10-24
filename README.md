# DLIRever
Michael Leocádio @ 2017

[![C++](https://img.shields.io/static/v1?label=&message=C%2B%2B&color=%231E40AF&logo=C%2B%2B)](https://)

[![Status - Archived](https://img.shields.io/badge/Status-Archived-yellow)](https://)

Non detectable DirectX based keylogger (proof of concept)

This is just a proof of concept that a virus (keylogger class) can use DirectX's DirectInput in order to capture user input without being detected by antivirus engines (and it interestingly still works in 2023, 6 years later).
It was first developed in Windows 7, targeting Win 10, and is now tested on Win 11 (build 22621.2361).

It has a feature to detect which window is in focus and to record keystrokes for that specific process; I call it "ForeFocus" and it is very sensitive regarding focus change (a browser tab, for instance, or an intermediate caller process).

Another important feature is the "OnFlyCommandMode", where there is a trigger for turning it on, and you may access "operator" functions, like translating the logged keystrokes into human-readable stylized HTML or hidding or showing the console, for example.

If you analyze the Kernel module, you'll see there are some obsolete functions there, like DLL loading methods. This is because, at the time, I intended to extend DLIRever into a full trojan solution to keep my experiments against antivirus engines.
There was also a bootstrap (also DirectX-based to avoid the classic and heuristically obvious WinMain hidden window) and a telemetry (WinSocks) module being developed in the same solution; however, I decided not to include them here as they were premature.

Unfortunately, I suspended the progress of the entire thing.
DLIRever functions very well for the purpose of proof-of-concept. The only issue is that the logger function is unfinished. It logs the keystrokes to the disk and even has a key code translator; however, I halted the project before I could link the logger to "ForeFocus".

Sorry for the archaic changelog and creepy to-do files.
