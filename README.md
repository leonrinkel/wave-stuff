# wave-stuff

This example code demonstrates how to read/write uncompressed WAVE files
(`.wav`) in C. It reads an uncompressed 16 bit stereo WAVE file, applies `sin()`
and `cos()` to the left and right channel respectively and outputs the resulting
audio to a new file. The result sounds as if the audio source moves back and
forth between the left and right channel.

```
$ gcc -o wave wave.c && ./wave out.wav test.wav
chunk: id=RIFF, size=35586022
riff.format=WAVE
chunk: id=fmt , size=16
fmt.audio_format=1
fmt.num_channels=2
fmt.sample_rate=44100
fmt.byte_rate=176400
fmt.block_align=4
fmt.bits_per_sample=16
chunk: id=LIST, size=186
chunk: id=data, size=35585792
nsamples=8896448, nseconds=202
```