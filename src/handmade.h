#pragma once

#define PI 3.14159265359f

struct game_offscreen_buffer
{
    // NOTE(casey): Pixels are always 32-bits wide, Memory Order BB GG RR XX
    void *memory;
    int width;
    int height;
    int pitch;
};

struct game_sound_output_buffer {
    int samples_per_second;
    int sample_count;
    int16_t *samples;
};

static void GameUpdateAndRender(const game_offscreen_buffer *buffer, int BlueOffset, int GreenOffset,
    game_sound_output_buffer * sound_buffer);

