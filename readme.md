Compile:
gcc -Iinclude main.c src/gifdec.c src/gifenc.c -lm -o main

```markdown
# gif-resizer

Small utility to resize GIFs using `stb_image_resize` together with the
`gifdec`/`gifenc` helpers in `src/`.

Behavior summary
- If the input filename is given without a path, the program will look for it
	in the `unprocessed/` folder.
- If no output filename is given, the program writes to
	`processed/<input-basename>-resized.gif` (the `processed/` directory is
	created if needed).

Build
-----
Use the included `Makefile`:

```sh
make
```

This produces the executable `main` in the project root.

Run / Usage
-----------
Basic usage:

```sh
./main [options] input.gif
```

Options:

- `-W width`    new width (default: input width)
- `-H height`   new height (default: input height)
- `-o outfile`  output filename (if no path is given, it will be placed in
								 `processed/`)
- `--help`      show help

Examples:

```sh
# resize and write default processed/<name>-resized.gif
./main unprocessed/test.gif

# resize to 200x200 and write to processed/test-200x200.gif
./main -W 200 -H 200 unprocessed/test.gif

# specify output filename (will be placed in processed/ if no path given)
./main -W 300 -H 200 -o custom-out.gif test.gif
```

Debugging
---------
Run under Valgrind to check for memory errors:

```sh
valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./main unprocessed/test.gif
```

Notes
-----
- The repository expects you to place inputs in `unprocessed/`. Processed
	outputs land in `processed/`.
- If you want different defaults, edit `main.c` or provide explicit
	arguments.

### How it handles GIFs

Each frame is decoded by **gifdec** and rendered onto an internal canvas; this
represents what a typical GIF player would show at that moment.  The program
then resizes the *entire* canvas, so every frame in the output is a complete
picture rather than a small “dirty” rectangle.  The encoder (`gifenc`) writes
those frames with disposal method 2 (restore to background) because the image
already contains all of the previous content – this makes the output
playback predictable and avoids flickering/artefacts that occur when only the
changed portion is written.

Transparency is also preserved.  Since `gifenc` uses its `bgindex` field as
the transparency index, the code temporarily overrides that value for each
frame based on the source frame's transparent colour.  No extra palettes are
needed; the original global palette is reused.

Credit
---------
GIFDEC: https://github.com/lecram/gifdec
GIFENC: https://github.com/lecram/gifenc
Frame resizer (stb_image_resize): https://github.com/nothings/obbg/blob/master/stb/stb_image_resize.h
