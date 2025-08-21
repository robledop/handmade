#pragma once

struct game_offscreen_buffer
{
    // NOTE(casey): Pixels are always 32-bits wide, Memory Order BB GG RR XX
    void *Memory;
    int Width;
    int Height;
    int Pitch;
};

static void GameUpdateAndRender(const game_offscreen_buffer *buffer, int BlueOffset, int GreenOffset);

