#include <cstdint>
#include "handmade.h"
#include <cmath>

static void GameOutputSound(const game_sound_output_buffer *sound_buffer, int tone_hz)
{
    static float t_sine;
    int16_t tone_volume = 3000;
    int wave_period = sound_buffer->samples_per_second / tone_hz;

    int16_t *sample_out = sound_buffer->samples;
    for(int sample_index = 0; sample_index < sound_buffer->sample_count; ++sample_index)
    {
        float sine_value = sinf(t_sine);
        auto sample_value = static_cast<int16_t>(sine_value * tone_volume);
        *sample_out++ = sample_value;
        *sample_out++ = sample_value;

        t_sine += 2.0f * PI * 1.0f / static_cast<float>(wave_period);
    }
}

static void RenderWeirdGradient(const game_offscreen_buffer *buffer, int BlueOffset, int GreenOffset)
{
    auto *row = static_cast<uint8_t*>(buffer->memory);
    for(int y = 0; y < buffer->height; ++y)
    {
        auto *pixel = reinterpret_cast<uint32_t*>(row);
        for(int x = 0;
            x < buffer->width;
            ++x)
        {
            uint8_t blue = (x + BlueOffset);
            uint8_t green = (y + GreenOffset);

            *pixel++ = ((green << 8) | blue);
        }
        
        row += buffer->pitch;
    }
}

static void GameUpdateAndRender(const game_offscreen_buffer *buffer, int BlueOffset, int GreenOffset,
    game_sound_output_buffer * sound_buffer, int tone_hz)
{
    GameOutputSound(sound_buffer, tone_hz);
    RenderWeirdGradient(buffer, BlueOffset, GreenOffset);
}
