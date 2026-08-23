# Curves Guide

## Input report splitting

Linux collects mouse input in a small buffer before passing it to input handlers such as Curves. If one mouse report is too large for that buffer, Linux may send it in pieces. After a split, Curves cannot always tell where the original report ended. The next input it sees could be the rest of that report or the start of a new one. Curves does not guess. It passes the split input through unchanged. It keeps passing input through until it sees a complete report that clearly ends. Curves leaves that report unchanged too, then resumes acceleration with the next one.

We expect this to be rare, but we do not yet have enough real-world data to know how often it happens. If a device produces split reports all the time, Curves may leave that device mostly or completely unaccelerated.

When input processing is enabled, Curves will write a warning to the kernel log (`dmesg`) when this happens. If the warning keeps appearing, please file a bug report. Include the mouse model, connection type, polling rate, kernel version, and the Curves log messages.
