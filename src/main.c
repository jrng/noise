#include <math.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float f32;

#define U32MAX 0xFFFFFFFF

#define F32MAX FLT_MAX

#define strcmp_literal(str, lit) strncmp((str), lit, sizeof(lit))

typedef struct
{
    u32 state;
} RandomSeries;

static inline u32
xor_shift(RandomSeries *series)
{
    u32 x = series->state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    series->state = x;
    return x;
}

static inline f32
random_unilateral(RandomSeries *series)
{
    return (f32) xor_shift(series) / (f32) U32MAX;
}

static inline f32
random_bilateral(RandomSeries *series)
{
    return 2.0f * random_unilateral(series) - 1.0f;
}

static inline u32
random_choice(RandomSeries *series, u32 choice_count)
{
    return xor_shift(series) % choice_count;
}

static inline u32
hash(u32 x)
{
    x = (x << 13) ^ x;
    return (x * (x * x * 15731 + 789221) + 1376312589);
}

typedef struct
{
    s32 width;
    s32 height;
    u8 *data;
} Noise;

typedef enum
{
    NOISE_TYPE_NONE         = 0,
    NOISE_TYPE_DITHER_BLUE  = 1,
} NoiseType;

static Noise
allocate_noise(s32 width, s32 height)
{
    Noise noise;

    noise.width = width;
    noise.height = height;
    noise.data = (u8 *) malloc(width * height);

    return noise;
}

typedef enum
{
    OUTPUT_FORMAT_RAW   = 0,
    OUTPUT_FORMAT_TEXT  = 1,
    OUTPUT_FORMAT_PGM   = 2,
} OutputFormat;

typedef enum
{
    VALUE_FORMAT_DECIMAL        = 0,
    VALUE_FORMAT_HEXADECIMAL    = 1,
} ValueFormat;

static void
generate_dither_blue(Noise noise, RandomSeries *rnd)
{
    f32 sigma = 1.5f;
    f32 sigma_squared = sigma * sigma;
    f32 sigma_squared2 = 2.0f * sigma_squared;

    s32 noise_size = noise.width * noise.height;
    s32 initial_sample_count = noise_size / 10;

    u8 *initial_binary_pattern = (u8 *) malloc(noise_size);

    for (s32 i = 0; i < noise_size; i += 1)
    {
        initial_binary_pattern[i] = 0;
    }

    // generate white noise samples

    for (s32 i = 0; i < initial_sample_count; i += 1)
    {
        s32 x = random_choice(rnd, noise.width);
        s32 y = random_choice(rnd, noise.height);

        initial_binary_pattern[y * noise.width + x] = 1;
    }

    // swap voids and clusters to get blue noise samples

    for (;;)
    {
        s32 cluster_x, cluster_y;
        f32 cluster_energy = 0.0f;

        for (s32 y = 0; y < noise.height; y += 1)
        {
            for (s32 x = 0; x < noise.width; x += 1)
            {
                if (initial_binary_pattern[y * noise.width + x])
                {
                    f32 energy = 0.0f;

                    for (s32 iy = 0; iy < noise.height; iy += 1)
                    {
                        for (s32 ix = 0; ix < noise.width; ix += 1)
                        {
                            if (initial_binary_pattern[iy * noise.width + ix])
                            {
                                s32 dx = abs(x - ix);
                                s32 dy = abs(y - iy);

                                if (dx > (noise.width / 2))
                                {
                                    dx = noise.width - dx;
                                }

                                if (dy > (noise.height / 2))
                                {
                                    dy = noise.height - dy;
                                }

                                f32 distance2 = (dx * dx) + (dy * dy);

                                energy += exp(-distance2 / sigma_squared2);
                            }
                        }
                    }

                    if (energy > cluster_energy)
                    {
                        cluster_energy = energy;
                        cluster_x = x;
                        cluster_y = y;
                    }
                }
            }
        }

        initial_binary_pattern[cluster_y * noise.width + cluster_x] = 0;

        s32 void_x, void_y;
        f32 void_energy = F32MAX;

        for (s32 y = 0; y < noise.height; y += 1)
        {
            for (s32 x = 0; x < noise.width; x += 1)
            {
                if (!initial_binary_pattern[y * noise.width + x])
                {
                    f32 energy = 0.0f;

                    for (s32 iy = 0; iy < noise.height; iy += 1)
                    {
                        for (s32 ix = 0; ix < noise.width; ix += 1)
                        {
                            if (initial_binary_pattern[iy * noise.width + ix])
                            {
                                s32 dx = abs(x - ix);
                                s32 dy = abs(y - iy);

                                if (dx > (noise.width / 2))
                                {
                                    dx = noise.width - dx;
                                }

                                if (dy > (noise.height / 2))
                                {
                                    dy = noise.height - dy;
                                }

                                f32 distance2 = (dx * dx) + (dy * dy);

                                energy += exp(-distance2 / sigma_squared2);
                            }
                        }
                    }

                    if (energy < void_energy)
                    {
                        void_energy = energy;
                        void_x = x;
                        void_y = y;
                    }
                }
            }
        }

        initial_binary_pattern[void_y * noise.width + void_x] = 1;

        if ((void_x == cluster_x) && (void_y == cluster_y))
        {
            break;
        }
    }

    // phase I

    s32 ones_count = 0;
    u8 *binary_pattern = (u8 *) malloc(noise_size);
    u32 *dither_array = (u32 *) malloc(sizeof(u32) * noise_size);

    for (s32 i = 0; i < noise_size; i += 1)
    {
        binary_pattern[i] = initial_binary_pattern[i];
        ones_count += initial_binary_pattern[i];
    }

    for (s32 rank = ones_count - 1; rank >= 0; rank -= 1)
    {
        s32 cluster_x, cluster_y;
        f32 cluster_energy = 0.0f;

        for (s32 y = 0; y < noise.height; y += 1)
        {
            for (s32 x = 0; x < noise.width; x += 1)
            {
                if (binary_pattern[y * noise.width + x])
                {
                    f32 energy = 0.0f;

                    for (s32 iy = 0; iy < noise.height; iy += 1)
                    {
                        for (s32 ix = 0; ix < noise.width; ix += 1)
                        {
                            if (binary_pattern[iy * noise.width + ix])
                            {
                                s32 dx = abs(x - ix);
                                s32 dy = abs(y - iy);

                                if (dx > (noise.width / 2))
                                {
                                    dx = noise.width - dx;
                                }

                                if (dy > (noise.height / 2))
                                {
                                    dy = noise.height - dy;
                                }

                                f32 distance2 = (dx * dx) + (dy * dy);

                                energy += exp(-distance2 / sigma_squared2);
                            }
                        }
                    }

                    if (energy > cluster_energy)
                    {
                        cluster_energy = energy;
                        cluster_x = x;
                        cluster_y = y;
                    }
                }
            }
        }

        binary_pattern[cluster_y * noise.width + cluster_x] = 0;
        dither_array[cluster_y * noise.width + cluster_x] = rank;
    }

    // phase II

    for (s32 i = 0; i < noise_size; i += 1)
    {
        binary_pattern[i] = initial_binary_pattern[i];
    }

    for (s32 rank = ones_count; rank < (noise_size / 2); rank += 1)
    {
        s32 void_x, void_y;
        f32 void_energy = F32MAX;

        for (s32 y = 0; y < noise.height; y += 1)
        {
            for (s32 x = 0; x < noise.width; x += 1)
            {
                if (!binary_pattern[y * noise.width + x])
                {
                    f32 energy = 0.0f;

                    for (s32 iy = 0; iy < noise.height; iy += 1)
                    {
                        for (s32 ix = 0; ix < noise.width; ix += 1)
                        {
                            if (binary_pattern[iy * noise.width + ix])
                            {
                                s32 dx = abs(x - ix);
                                s32 dy = abs(y - iy);

                                if (dx > (noise.width / 2))
                                {
                                    dx = noise.width - dx;
                                }

                                if (dy > (noise.height / 2))
                                {
                                    dy = noise.height - dy;
                                }

                                f32 distance2 = (dx * dx) + (dy * dy);

                                energy += exp(-distance2 / sigma_squared2);
                            }
                        }
                    }

                    if (energy < void_energy)
                    {
                        void_energy = energy;
                        void_x = x;
                        void_y = y;
                    }
                }
            }
        }

        binary_pattern[void_y * noise.width + void_x] = 1;
        dither_array[void_y * noise.width + void_x] = rank;
    }

    // phase III

    for (s32 rank = (noise_size / 2); rank < noise_size; rank += 1)
    {
        s32 cluster_x, cluster_y;
        f32 cluster_energy = 0.0f;

        for (s32 y = 0; y < noise.height; y += 1)
        {
            for (s32 x = 0; x < noise.width; x += 1)
            {
                if (!binary_pattern[y * noise.width + x])
                {
                    f32 energy = 0.0f;

                    for (s32 iy = 0; iy < noise.height; iy += 1)
                    {
                        for (s32 ix = 0; ix < noise.width; ix += 1)
                        {
                            if (!binary_pattern[iy * noise.width + ix])
                            {
                                s32 dx = abs(x - ix);
                                s32 dy = abs(y - iy);

                                if (dx > (noise.width / 2))
                                {
                                    dx = noise.width - dx;
                                }

                                if (dy > (noise.height / 2))
                                {
                                    dy = noise.height - dy;
                                }

                                f32 distance2 = (dx * dx) + (dy * dy);

                                energy += exp(-distance2 / sigma_squared2);
                            }
                        }
                    }

                    if (energy > cluster_energy)
                    {
                        cluster_energy = energy;
                        cluster_x = x;
                        cluster_y = y;
                    }
                }
            }
        }

        binary_pattern[cluster_y * noise.width + cluster_x] = 1;
        dither_array[cluster_y * noise.width + cluster_x] = rank;
    }

    // finalize

    for (s32 i = 0; i < noise_size; i += 1)
    {
        noise.data[i] = (dither_array[i] * 256) / noise_size;
    }

    free(dither_array);
    free(binary_pattern);
    free(initial_binary_pattern);
}

s32 main(s32 argument_count, char **arguments)
{
    u32 seed = 0;
    s32 width = 64;
    s32 height = 64;
    OutputFormat output_format = OUTPUT_FORMAT_RAW;
    ValueFormat value_format = VALUE_FORMAT_DECIMAL;
    NoiseType noise_type = NOISE_TYPE_NONE;

    char *output_filename = 0;

    char *text_start = "";
    char *text_sep = ",";
    char *text_end = "\n";

    s32 elements_per_line = 16;

    for (s32 i = 1; i < argument_count; i += 1)
    {
        char *argument = arguments[i];

        if (!strcmp_literal(argument, "-t"))
        {
            i += 1;

            if (i < argument_count)
            {
                if (!strcmp_literal(arguments[i], "dither-blue"))
                {
                    noise_type = NOISE_TYPE_DITHER_BLUE;
                }
            }
        }
        else if (!strcmp_literal(argument, "-s"))
        {
            i += 1;

            if (i < argument_count)
            {
                char *size = arguments[i];
                char *end;

                width = (s32) strtol(size, &end, 10);

                if ((end != 0) && (*end == 'x'))
                {
                    size = end + 1;
                    height = (s32) strtol(size, NULL, 10);
                }
                else
                {
                    height = width;
                }
            }
        }
        else if (!strcmp_literal(argument, "-o"))
        {
            i += 1;

            if (i < argument_count)
            {
                output_filename = arguments[i];
            }
        }
        else if (!strcmp_literal(argument, "-f"))
        {
            i += 1;

            if (i < argument_count)
            {
                if (!strcmp_literal(arguments[i], "raw"))
                {
                    output_format = OUTPUT_FORMAT_RAW;
                }
                else if (!strcmp_literal(arguments[i], "text") ||
                         !strcmp_literal(arguments[i], "text.dec"))
                {
                    output_format = OUTPUT_FORMAT_TEXT;
                    value_format = VALUE_FORMAT_DECIMAL;
                }
                else if (!strcmp_literal(arguments[i], "text.hex"))
                {
                    output_format = OUTPUT_FORMAT_TEXT;
                    value_format = VALUE_FORMAT_HEXADECIMAL;
                }
                else if (!strcmp_literal(arguments[i], "pgm"))
                {
                    output_format = OUTPUT_FORMAT_PGM;
                }
            }
        }
        else if (!strcmp_literal(argument, "--start"))
        {
            i += 1;

            if (i < argument_count)
            {
                text_start = arguments[i];
            }
        }
        else if (!strcmp_literal(argument, "--sep"))
        {
            i += 1;

            if (i < argument_count)
            {
                text_sep = arguments[i];
            }
        }
        else if (!strcmp_literal(argument, "--end"))
        {
            i += 1;

            if (i < argument_count)
            {
                text_end = arguments[i];
            }
        }
        else if (!strcmp_literal(argument, "--count"))
        {
            i += 1;

            if (i < argument_count)
            {
                elements_per_line = (s32) strtol(arguments[i], NULL, 10);

                if (elements_per_line <= 0)
                {
                    elements_per_line = 16;
                }
            }
        }
        else if (!strcmp_literal(argument, "-h") || !strcmp_literal(argument, "--help"))
        {
            fprintf(stderr, "USAGE: noise [options]\n");
            fprintf(stderr, "\n");
            fprintf(stderr, "OPTIONS:\n");
            fprintf(stderr, "  -h, --help              List all available options.\n");
            fprintf(stderr, "  -f <format>             Select the output format. Available: raw (default), text(.dec), text.hex, pgm.\n");
            fprintf(stderr, "  -s <width>[x<height]    Set the size of the generated noise. If only width is given, the height will be equal to width.\n");
            fprintf(stderr, "  --start <text>          For text output format this is outputted before the noise signal.\n");
            fprintf(stderr, "  --end <text>            For text output format this is outputted after the noise signal.\n");
            fprintf(stderr, "  --sep <text>            For text output format this is outputted between noise values.\n");
            fprintf(stderr, "  --count <num>           For text output format this is number of elements per line.\n");
            fprintf(stderr, "  -t <name>               Select a noise type to generate. A list of noise types is down below.\n");
            fprintf(stderr, "  -o <file>               Write the output to a file. By default the output is written to standard out.\n");
            fprintf(stderr, "\n");
            fprintf(stderr, "NOISE TYPES:\n");
            fprintf(stderr, "  dither-blue             Dither blue noise pattern\n");
            fprintf(stderr, "\n");

            // -d <datatype>            float, byte
            // --seed <seed>

            return 0;
        }
    }

    if (noise_type == NOISE_TYPE_NONE)
    {
        fprintf(stderr, "error: no valid noise type selected\n");
        return 1;
    }

    Noise noise = allocate_noise(width, height);
    RandomSeries rnd = { .state = hash(seed) };

    switch (noise_type)
    {
        case NOISE_TYPE_NONE: break;

        case NOISE_TYPE_DITHER_BLUE:
        {
            generate_dither_blue(noise, &rnd);
        } break;
    }

    FILE *output_file = stdout;

    if (output_filename)
    {
        output_file = fopen(output_filename, "wb");

        if (!output_file)
        {
            fprintf(stderr, "error: could not write to file '%s'.\n", output_filename);
            return 0;
        }
    }

    s32 noise_size = noise.width * noise.height;

    switch (output_format)
    {
        case OUTPUT_FORMAT_RAW:
        {
            fwrite(noise.data, 1, noise_size, output_file);
        } break;

        case OUTPUT_FORMAT_TEXT:
        {
            switch (value_format)
            {
                case VALUE_FORMAT_DECIMAL:
                {
                    fprintf(output_file, "%s%u", text_start, noise.data[0]);

                    for (s32 i = 1; i < noise_size; i += 1)
                    {
                        if ((i % elements_per_line) == 0)
                        {
                            fprintf(output_file, "%s\n%u", text_sep, noise.data[i]);
                        }
                        else
                        {
                            fprintf(output_file, "%s%u", text_sep, noise.data[i]);
                        }
                    }

                    fprintf(output_file, "%s", text_end);
                } break;

                case VALUE_FORMAT_HEXADECIMAL:
                {
                    fprintf(output_file, "%s0x%02X", text_start, noise.data[0]);

                    for (s32 i = 1; i < noise_size; i += 1)
                    {
                        if ((i % elements_per_line) == 0)
                        {
                            fprintf(output_file, "%s\n0x%02X", text_sep, noise.data[i]);
                        }
                        else
                        {
                            fprintf(output_file, "%s0x%02X", text_sep, noise.data[i]);
                        }
                    }

                    fprintf(output_file, "%s", text_end);
                } break;
            }
        } break;

        case OUTPUT_FORMAT_PGM:
        {
            fprintf(output_file, "P5\n%d %d\n255\n", noise.width, noise.height);
            fwrite(noise.data, 1, noise_size, output_file);
        } break;
    }

    if (output_filename)
    {
        fclose(output_file);
    }

    return 0;
}
