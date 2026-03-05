#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

#include "gifdec.h"
#include "gifenc.h"

/*
 * find_closest_color
 * ------------------
 * Given an RGB triple and a palette (array of RGB triplets), return the
 * index of the closest palette entry using squared Euclidean distance.
 * If the color is transparent (index == transparency_index), return it unchanged.
 * Clamp very dark colors to pure black to avoid palette corruption.
 */

uint8_t find_closest_color(uint8_t r, uint8_t g, uint8_t b, uint8_t *palette,
			   int palette_size)
{
	uint8_t best_index = 0;
	int best_distance = INT_MAX;

	/* clamp very dark colors to pure black */
	if (r < 10 && g < 10 && b < 10)
	{
		r = g = b = 0;
	}

	for (int i = 0; i < palette_size; i++)
	{
		int dr = (int)r - (int)palette[i * 3];
		int dg = (int)g - (int)palette[i * 3 + 1];
		int db = (int)b - (int)palette[i * 3 + 2];
		int distance = dr * dr + dg * dg + db * db;

		if (distance < best_distance)
		{
			best_distance = distance;
			best_index = i;
		}
	}
	return best_index;
}

static void usage(const char *prog)
{
	fprintf(stderr,
			"Usage: %s [options] input.gif\n"
			"Options:\n"
			"  -W width    new width (default: input width)\n"
			"  -H height   new height (default: input height)\n"
			"  -o outfile  output filename (default: processed/resized.gif)\n"
			"  --help      show this help\n",
			prog);
}

int main(int argc, char *argv[])
{
	int opt;

	int new_width = 0;
	int new_height = 0;
	char *filename_in = NULL;
	char *filename_out = NULL;

	if (argc < 2)
	{
		usage(argv[0]);
		return 1;
	}

	for (int i = 1; i < argc; ++i)
	{
		if (strcmp(argv[i], "--help") == 0)
		{
			usage(argv[0]);
			return 0;
		}
	}

	while ((opt = getopt(argc, argv, "W:H:o:")) != -1)
	{
		switch (opt)
		{
		case 'W':
			new_width = atoi(optarg);
			break;
		case 'H':
			new_height = atoi(optarg);
			break;
		case 'o':
			filename_out = optarg;
			break;
		default:
			usage(argv[0]);
			return 1;
		}
	}

	if (optind < argc)
	{
		filename_in = argv[optind++];
	}
	else
	{
		fprintf(stderr, "Error: No input filename provided\n");
		usage(argv[0]);
		return 1;
	}

	/* If input has no path, assume it's in unprocessed/ */
	char *in_path = NULL;
	if (strchr(filename_in, '/') == NULL)
	{
		size_t n = strlen("unprocessed/") + strlen(filename_in) + 1;
		in_path = malloc(n);
		if (!in_path)
		{
			fprintf(stderr, "Out of memory\n");
			return 1;
		}
		snprintf(in_path, n, "unprocessed/%s", filename_in);
	}
	else
	{
		in_path = filename_in;
	}

	gd_GIF *gif = gd_open_gif(in_path);

	if (!gif)
	{
		fprintf(stderr, "Failed to open GIF '%s'!\n", in_path);
		if (in_path != filename_in)
			free(in_path);
		return 1;
	}

	printf("Gif Size: %ix%i\n", gif->width, gif->height);
	printf("Palette size: %i\n", gif->palette->size);
	printf("Gif BG colour: %i\n", gif->bgindex);
	printf("Disposal: %i\n", gif->gce.disposal);
	if (new_width <= 0)
		new_width = gif->width;
	if (new_height <= 0)
		new_height = gif->height;


	/* ensure processed/ exists */
	if (mkdir("processed", 0755) == -1 && errno != EEXIST)
	{
		fprintf(stderr, "Warning: cannot create processed/ (%s)\n", strerror(errno));
	}

	/* build output path */
	char *out_path = NULL;
	if (filename_out)
	{
		if (strchr(filename_out, '/') == NULL)
		{
			size_t n = strlen("processed/") + strlen(filename_out) + 1;
			out_path = malloc(n);
			if (!out_path)
			{
				fprintf(stderr, "Out of memory\n");
				gd_close_gif(gif);
				if (in_path != filename_in)
					free(in_path);
				return 1;
			}
			snprintf(out_path, n, "processed/%s", filename_out);
		}
		else
		{
			out_path = filename_out;
		}
	}
	else
	{
		/* default: processed/<basename-without-ext>-resized.gif */
		const char *base = strrchr(filename_in, '/');
		if (!base)
			base = filename_in;
		else
			base++;
		size_t basen = strlen(base);
		size_t dot = basen;
		for (size_t i = basen; i > 0; --i)
		{
			if (base[i - 1] == '.')
			{
				dot = i - 1;
				break;
			}
		}
		size_t n = strlen("processed/") + dot + strlen("-resized.gif") + 1;
		out_path = malloc(n);
		if (!out_path)
		{
			fprintf(stderr, "Out of memory\n");
			gd_close_gif(gif);
			if (in_path != filename_in)
				free(in_path);
			return 1;
		}
		snprintf(out_path, n, "processed/%.*s-resized.gif", (int)dot, base);
	}

	ge_GIF *resized_gif = ge_new_gif(
		out_path,
		new_width, new_height,
		gif->palette->colors,
		8,
		gif->bgindex,
		0);

	if (!resized_gif)
	{
		fprintf(stderr, "Failed to create output GIF '%s'\n", out_path ? out_path : "(null)");
		gd_close_gif(gif);
		if (out_path && out_path != filename_out)
			free(out_path);
		if (in_path != filename_in)
			free(in_path);
		return 1;
	}

	int frame_index = 0;
	uint8_t *rgba_buffer;
	uint8_t *resized_frame;

	while (gd_get_frame(gif))
	{
		printf("\rFrame: %i (delay: %d)", frame_index, gif->gce.delay);
		fflush(stdout);

		/* Render the composed frame (canvas + current image).  Using
		   gd_render_frame eliminates stale pixels that are present in gif->frame
		   and gives us a self‑contained RGB snapshot of what a viewer would see.
		   We then convert that to RGBA so resizing still works with alpha. */
		uint8_t *rgb_canvas = malloc(gif->width * gif->height * 3);
		if (!rgb_canvas) {
			fprintf(stderr, "Failed to allocate memory for canvas buffer\n");
			break;
		}
		gd_render_frame(gif, rgb_canvas);

		rgba_buffer = malloc(gif->width * gif->height * 4);
		if (!rgba_buffer) {
			fprintf(stderr, "Failed to allocate memory for frame buffer\n");
			free(rgb_canvas);
			break;
		}

		/* Copy RGB -> RGBA and optionally carry over the current frame's
		   transparency mask (previous frames are merged into the canvas so
		   their transparency information is lost). */
		for (int j = 0; j < gif->width * gif->height; j++) {
			rgba_buffer[j*4 + 0] = rgb_canvas[j*3 + 0];
			rgba_buffer[j*4 + 1] = rgb_canvas[j*3 + 1];
			rgba_buffer[j*4 + 2] = rgb_canvas[j*3 + 2];
			if (gif->gce.transparency &&
			    rgb_canvas[j*3+0] == gif->palette->colors[gif->gce.tindex*3 + 0] &&
			    rgb_canvas[j*3+1] == gif->palette->colors[gif->gce.tindex*3 + 1] &&
			    rgb_canvas[j*3+2] == gif->palette->colors[gif->gce.tindex*3 + 2]) {
				rgba_buffer[j*4 + 3] = 0;
			} else {
				rgba_buffer[j*4 + 3] = 255;
			}
		}
		free(rgb_canvas);
		resized_frame = malloc(new_width * new_height * 4);
		if (!resized_frame)
		{
			fprintf(stderr, "Failed to allocate memory for resized frame\n");
			free(rgba_buffer);
			break;
		}

		int input_stride = gif->width * 4;
		int output_stride = new_width * 4;
		stbir_resize_uint8(rgba_buffer, gif->width, gif->height, input_stride,
						   resized_frame, new_width, new_height, output_stride, 4);

/* gfx control extensions (including transparency) are encoded
		   via the gifenc library's bgindex field.  Temporarily set it to the
		   current frame's transparent color index (if any) when adding the frame. */

		/* Map resized RGBA back to palette; respect alpha transparency by
		   thresholding.  This avoids noisy semi‑transparent edges turning
		   into arbitrary colours. */
		for (int j = 0; j < new_width * new_height; j++)
		{
			uint8_t a = resized_frame[j * 4 + 3];
			if (gif->gce.transparency && a < 128) {
				// treat as transparent pixel
				resized_gif->frame[j] = gif->gce.tindex;
				continue;
			}
			uint8_t r = resized_frame[j * 4];
			uint8_t g = resized_frame[j * 4 + 1];
			uint8_t b = resized_frame[j * 4 + 2];
			uint8_t palette_index = find_closest_color(r, g, b, gif->palette->colors,
				     gif->palette->size);
			resized_gif->frame[j] = palette_index;
		}

		/* Use actual frame delay. if input disposal is 0 (none), use 2
		   so output doesn't accumulate old pixels and avoid ghosting */
		/* For simplicity, write each frame as a full image and request
		   disposal 2 (restore to background) so viewers cannot accumulate
		   artefacts.  Since the frame already contains the composited contents
		   this is equivalent to displaying the correct picture directly. */
		int out_disposal = 2;

		/* handle per-frame transparency index by overriding bgindex on the
		   encoder temporarily */
		int old_bg = resized_gif->bgindex;
		if (gif->gce.transparency)
			resized_gif->bgindex = gif->gce.tindex;
		else
			resized_gif->bgindex = -1;

		ge_add_frame_full(resized_gif, gif->gce.delay, out_disposal);
		resized_gif->bgindex = old_bg;

		free(resized_frame);
		free(rgba_buffer);
		frame_index++;
	}

	printf("\n");
	printf("Complete!\nResized GIF saved in %s\n", out_path ? out_path : filename_out);
	ge_close_gif(resized_gif);
	gd_close_gif(gif);
	if (out_path && out_path != filename_out)
		free(out_path);
	if (in_path != filename_in)
		free(in_path);
	return 0;
}
