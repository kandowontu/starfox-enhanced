# MSU timeline fixture

`msu-tone-320.flac` contains 320 stereo frames of a generated 440 Hz sine wave
at 32,000 Hz, encoded as 16-bit FLAC. It is original synthetic test audio and
contains no game recording.

Generated with:

```powershell
ffmpeg -hide_banner -loglevel error -f lavfi -i "aevalsrc=0.25*sin(2*PI*440*t):s=32000:d=0.01" -ac 2 -c:a flac -sample_fmt s16 -fflags +bitexact -flags:a +bitexact -map_metadata -1 tests/data/msu-tone-320.flac
```

The short duration deliberately ends inside an output packet and before most
of the companion pack's configured loop points.
