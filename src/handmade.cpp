#include <cstdint>
#include "handmade.h"

static void RenderWeirdGradient(const game_offscreen_buffer *buffer, int BlueOffset, int GreenOffset)
{
    auto *row = static_cast<uint8_t*>(buffer->Memory);
    for(int y = 0; y < buffer->Height; ++y)
    {
        auto *pixel = reinterpret_cast<uint32_t*>(row);
        for(int x = 0;
            x < buffer->Width;
            ++x)
        {
            uint8_t blue = (x + BlueOffset);
            uint8_t green = (y + GreenOffset);

            *pixel++ = ((green << 8) | blue);
        }
        
        row += buffer->Pitch;
    }
}

static void GameUpdateAndRender(const game_offscreen_buffer *buffer, int BlueOffset, int GreenOffset)
{
    RenderWeirdGradient(buffer, BlueOffset, GreenOffset);
}
