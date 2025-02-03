# Noise generator

This is a generator for noise textures.

```
USAGE: noise [options]

OPTIONS:
  -h, --help              List all available options.
  -f <format>             Select the output format. Available: raw (default), text(.dec), text.hex, pgm.
  -s <width>[x<height]    Set the size of the generated noise. If only width is given, the height will be equal to width.
  --start <text>          For text output format this is outputted before the noise signal.
  --end <text>            For text output format this is outputted after the noise signal.
  --sep <text>            For text output format this is outputted between noise values.
  --count <num>           For text output format this is number of elements per line.
  -t <name>               Select a noise type to generate. A list of noise types is down below.
  -o <file>               Write the output to a file. By default the output is written to standard out.

NOISE TYPES:
  dither-blue             Dither blue noise pattern
  dither-bayer            Dither bayer pattern
```
