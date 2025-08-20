#include <cstdint>
#include <string>
#include <windows.h>
#include <xinput.h>
#include <dsound.h>
#include <cmath>

#define PI 3.14159265359f

struct win32_offscreen_buffer {
    // NOTE: Pixels are always 32-bits wide, Memory Order 0x BB GG RR xx
    BITMAPINFO Info;
    void* Memory;
    int Width;
    int Height;
    int Pitch;
    int BytesPerPixel;
};

struct win32_window_dimensions {
    int Width;
    int Height;
};
struct win32_sound_output {
    int samples_per_second;
    int tone_hz;
    int16_t tone_volume;
    uint32_t running_sample_index;
    int wave_period;
    int bytes_per_sample;
    int secondary_buffer_size;
    float t_sine;
    int latency_sample_count;
};

// NOTE: We are declaring these functions ourselves because we don't want to link with xinput directly
#define X_INPUT_GET_STATE(name) DWORD WINAPI name([[maybe_unused]] DWORD dwUserIndex, [[maybe_unused]] XINPUT_STATE* pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
    return ERROR_DEVICE_NOT_CONNECTED;
}

// NOTE: If for some reason we are unable to load XInput, we will use this stub function that does nothing
static x_input_get_state* XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

#define X_INPUT_SET_STATE(name) DWORD WINAPI name([[maybe_unused]] DWORD dwUserIndex,[[maybe_unused]] XINPUT_VIBRATION* pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
    return ERROR_DEVICE_NOT_CONNECTED;
}

// NOTE: If for some reason we are unable to load XInput, we will use this stub function that does nothing
static x_input_set_state* XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

static void Win32LoadXInput()
{
    // NOTE: If we are able to load XInput, replace the stubs with the real functions
    HMODULE x_input_library = LoadLibraryA("xinput1_4.dll");
    if (!x_input_library)
    {
        x_input_library = LoadLibraryA("xinput1_3.dll");
        if (!x_input_library)
        {
            x_input_library = LoadLibraryA("xinput9_1_0.dll");
        }
    }

    if (x_input_library)
    {
        XInputGetState_ = reinterpret_cast<x_input_get_state*>(GetProcAddress(x_input_library, "XInputGetState"));
        XInputSetState_ = reinterpret_cast<x_input_set_state*>(GetProcAddress(x_input_library, "XInputSetState"));
    }
}

#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name([[maybe_unused]] LPCGUID pcGuidDevice, [[maybe_unused]] LPDIRECTSOUND* ppDS, [[maybe_unused]] LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

static bool GlobalRunning;
static win32_offscreen_buffer GlobalBackBuffer;
static LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer = { };

win32_window_dimensions Win32GetWindowDimensions(HWND window)
{
    RECT clientRect;
    GetClientRect(window, &clientRect);
    const int width = clientRect.right - clientRect.left;
    const int height = clientRect.bottom - clientRect.top;

    return { width, height };
}

static void Win32InitDSound(HWND windowHandle, int32_t samples_per_second, int32_t buffer_size)
{
    HMODULE dsound_library = LoadLibraryA("dsound.dll");
    if (!dsound_library)
    {
        // TODO: Diagnostic
        return;
    }

    // NOTE: Get a DirectSound object - cooperative
    auto* DirectSoundCreate = reinterpret_cast<direct_sound_create*>(GetProcAddress(
        dsound_library, "DirectSoundCreate"));

    IDirectSound* direct_sound;
    if (DirectSoundCreate && SUCCEEDED(DirectSoundCreate(nullptr, &direct_sound, nullptr)))
    {
        WAVEFORMATEX wave_format = { };
        wave_format.wFormatTag = WAVE_FORMAT_PCM;
        wave_format.nChannels = 2;
        wave_format.nSamplesPerSec = samples_per_second;
        wave_format.wBitsPerSample = 16;
        wave_format.nBlockAlign = (wave_format.nChannels * wave_format.wBitsPerSample) / 8;
        wave_format.nAvgBytesPerSec = wave_format.nSamplesPerSec * wave_format.nBlockAlign;
        wave_format.cbSize = 0;

        if (SUCCEEDED(direct_sound->SetCooperativeLevel(windowHandle, DSSCL_PRIORITY)))
        {
            // NOTE: Create a primary buffer
            DSBUFFERDESC buffer_description = { };
            buffer_description.dwSize = sizeof(buffer_description);
            buffer_description.dwFlags = DSBCAPS_PRIMARYBUFFER;

            LPDIRECTSOUNDBUFFER primary_buffer = { };
            if (SUCCEEDED(direct_sound->CreateSoundBuffer(&buffer_description, &primary_buffer, nullptr)))
            {
                if (SUCCEEDED(primary_buffer->SetFormat(&wave_format)))
                {
                    // NOTE: We have finally set the format
                    OutputDebugStringA("Primary buffer SetFormat succeeded\n");
                }
                else
                {
                    // TODO: Diagnostic
                }
            }
            else
            {
                // TODO: Diagnostic
            }
        }
        else
        {
            // TODO: Diagnostic
        }

        // NOTE: Create a secondary buffer
        DSBUFFERDESC buffer_description = { };
        buffer_description.dwSize = sizeof(buffer_description);
        buffer_description.dwFlags = 0;
        buffer_description.dwBufferBytes = buffer_size;
        buffer_description.lpwfxFormat = &wave_format;

        if (SUCCEEDED(direct_sound->CreateSoundBuffer(&buffer_description, &GlobalSecondaryBuffer, nullptr)))
        {
            OutputDebugStringA("Secondary buffer created\n");
        }
        else
        {
            // TODO: Diagnostic
        }
    }
}

static void RenderWeirdGradient(const win32_offscreen_buffer& buffer, int blueOffset, int greenOffset)
{
    auto row = static_cast<uint8_t*>(buffer.Memory);

    for (int y = 0; y < buffer.Height; ++y)
    {
        auto pixel = reinterpret_cast<uint32_t*>(row);
        for (int x = 0; x < buffer.Width; ++x)
        {
            uint8_t blue = x + blueOffset;
            uint8_t green = y + greenOffset;

            *pixel++ = green << 8 | blue;
        }

        row += buffer.Pitch;
    }
}

static void Win32ResizeDIBSection(win32_offscreen_buffer* buffer, const int width, const int height)
{
    if (buffer->Memory)
    {
        VirtualFree(buffer->Memory, 0, MEM_RELEASE);
    }

    buffer->Width = width;
    buffer->Height = height;
    buffer->BytesPerPixel = 4; // 4 bytes per pixel (RGBA)

    buffer->Info.bmiHeader.biSize = sizeof(buffer->Info.bmiHeader); // Size of the BITMAPINFOHEADER structure
    buffer->Info.bmiHeader.biWidth = buffer->Width;
    buffer->Info.bmiHeader.biHeight = -buffer->Height; // Negative height indicates a top-down bitmap
    buffer->Info.bmiHeader.biPlanes = 1; // Number of color planes must be 1
    buffer->Info.bmiHeader.biBitCount = 32; // 32 bits per pixel (RGBA)
    buffer->Info.bmiHeader.biCompression = BI_RGB;

    int bitmapMemorySize = buffer->Width * buffer->Height * buffer->BytesPerPixel;

    // Allocate memory for the bitmap with read/write permissions
    buffer->Memory = VirtualAlloc(
        nullptr,
        bitmapMemorySize,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE);

    buffer->Pitch = buffer->Width * buffer->BytesPerPixel; // the number of bytes in a row of the bitmap
}

// Updates the window with the pixel data from BitmapMemory
static void Win32DisplayBufferInWindow(
    HDC deviceContext,
    int windowWidth,
    int windowHeight,
    const win32_offscreen_buffer& buffer)
{
    // TODO: Aspect ratio correction
    // StretchDIBits will copy the pixel data from BitmapMemory to the device context.
    // It will stretch the pixels to fit the specified width and height.
    StretchDIBits(deviceContext,
                  0,
                  0,
                  windowWidth,
                  windowHeight,
                  0,
                  0,
                  buffer.Width,
                  buffer.Height,
                  buffer.Memory,
                  &buffer.Info,
                  DIB_RGB_COLORS,
                  SRCCOPY);
}

// This is the callback function that will handle messages sent to the window
LRESULT CALLBACK Win32MainWindowCallback(
    HWND window,
    const UINT message,
    const WPARAM wParam,
    const LPARAM lParam)
{
    LRESULT result = 0;
    switch (message)
    {
    case WM_DESTROY:
    {
        // TODO: handle this as an error - recreate window?
        GlobalRunning = false;
        OutputDebugStringA("WM_DESTROY\n");
    }
    break;

    case WM_CLOSE:
    {
        // TODO: handle this with a message to the user?
        GlobalRunning = false;
        OutputDebugStringA("WM_CLOSE\n");
    }
    break;

    case WM_ACTIVATEAPP:
    {
        OutputDebugStringA("WM_ACTIVATEAPP\n");
    }
    break;

    case WM_SYSKEYUP:
    case WM_SYSKEYDOWN:
    case WM_KEYDOWN:
    case WM_KEYUP:
    {
#define KeyMessageWasDownBit (1 << 30)
#define KeyMessageWasUpBit (1 << 31)

        uint32_t vk_code = wParam;

        bool was_down = (lParam & KeyMessageWasDownBit) != 0;
        bool is_down = (lParam & KeyMessageWasUpBit) == 0;

        // Avoid key repeats
        if (was_down == is_down)
        {
            break;
        }

        switch (vk_code)
        {
        case VK_ESCAPE:
        {
            GlobalRunning = false;
        }
        break;

        case 0x57: // W
        {
            OutputDebugStringA("W\n");
        }
        break;

        case 0x41: // A
        {
            OutputDebugStringA("A\n");
        }
        break;

        case 0x53: // S
        {
            OutputDebugStringA("S\n");
        }
        break;

        case 0x44: // D
        {
            OutputDebugStringA("D\n");
        }
        break;

        case 0x45: // E
        {
            OutputDebugStringA("E\n");
        }
        break;

        case 0x46: // F
        {
            OutputDebugStringA("F\n");
        }
        break;

        case 0x47: // G
        {
            OutputDebugStringA("G\n");
        }
        break;

        case VK_SPACE: // Space
        {
            OutputDebugStringA("SPACE\n");
        }
        break;

        case VK_RETURN: // Enter
        {
            OutputDebugStringA("ENTER\n");
        }
        break;

        case VK_TAB: // Tab
        {
            OutputDebugStringA("TAB: ");
            if (is_down)
            {
                OutputDebugStringA("IS DOWN\n");
            }

            if (was_down)
            {
                OutputDebugStringA("WAS DOWN\n");
            }
        }
        break;

        case VK_BACK: // Backspace
        {
            OutputDebugStringA("BACKSPACE\n");
        }
        break;

        case VK_DELETE: // Delete
        {
            OutputDebugStringA("DELETE\n");
        }
        break;

        case VK_LEFT: // Left arrow
        {
            OutputDebugStringA("LEFT\n");
        }
        break;

        case VK_RIGHT: // Right arrow
        {
            OutputDebugStringA("RIGHT\n");
        }
        break;

        case VK_UP: // Up arrow
        {
            OutputDebugStringA("UP\n");
        }
        break;

        case VK_DOWN: // Down arrow
        {
            OutputDebugStringA("DOWN\n");
        }
        break;
        case VK_F4:
        {
            bool alt_key_was_down = (lParam & (1 << 29)) != 0;
            if (alt_key_was_down)
            {
                GlobalRunning = false; // ALT + F4
            }
        }
        break;
        default:
            // Unhandled key
            break;
        }
    }
    break;

    case WM_PAINT: // Window needs to be repainted
    {
        OutputDebugStringA("WM_PAINT\n");
        PAINTSTRUCT paint;
        // Prepare the window for painting and returns a device context
        HDC deviceContext = BeginPaint(window, &paint);

        auto [Width, Height] = Win32GetWindowDimensions(window);
        Win32DisplayBufferInWindow(deviceContext,
                                   Width,
                                   Height,
                                   GlobalBackBuffer);

        EndPaint(window, &paint);
    }
    break;

    default:
        result = DefWindowProcA(window, message, wParam, lParam);
        break;
    }

    return result;
}

static void Win32FillSoundBuffer(win32_sound_output* sound_output, DWORD byte_to_lock, DWORD bytes_to_write)
{
    VOID* region1;
    DWORD region1_size;
    VOID* region2;
    DWORD region2_size;

    if (SUCCEEDED(GlobalSecondaryBuffer->Lock(
        byte_to_lock,
        bytes_to_write,
        &region1,
        &region1_size,
        &region2,
        &region2_size,
        0)))
    {
        DWORD region1_sample_count = region1_size / sound_output->bytes_per_sample;
        auto* sample_out = static_cast<int16_t*>(region1);
        for (DWORD sample_index = 0; sample_index < region1_sample_count; ++sample_index)
        {
            float t = 2.0f * PI * (static_cast<float>(sound_output->running_sample_index) / static_cast<float>(
                sound_output->wave_period));
            float sine_value = sinf(t);
            auto sample_value = static_cast<int16_t>(sine_value * static_cast<float>(sound_output->tone_volume));
            *sample_out++ = sample_value;
            *sample_out++ = sample_value;
            ++sound_output->running_sample_index;
        }

        DWORD region2_sample_count = region2_size / sound_output->bytes_per_sample;
        sample_out = static_cast<int16_t*>(region2);
        for (DWORD sample_index = 0; sample_index < region2_sample_count; ++sample_index)
        {
            float t = 2.0f * PI * (static_cast<float>(sound_output->running_sample_index) / static_cast<float>(
                sound_output->wave_period));
            float sine_value = sinf(t);
            auto sample_value = static_cast<int16_t>(sine_value * static_cast<float>(sound_output->tone_volume));
            *sample_out++ = sample_value;
            *sample_out++ = sample_value;
            ++sound_output->running_sample_index;
        }

        SUCCEEDED(GlobalSecondaryBuffer->Unlock(region1, region1_size, region2, region2_size));
    }
}

int CALLBACK WinMain(HINSTANCE hInstance,
    [[maybe_unused]] HINSTANCE hPrevInstance,
    [[maybe_unused]] LPSTR lpCmdLine,
    [[maybe_unused]] int nShowCmd)
{
    LARGE_INTEGER perf_counter_frequency_result;
    QueryPerformanceFrequency(&perf_counter_frequency_result);
    int64_t perf_counter_frequency = perf_counter_frequency_result.QuadPart;

    Win32LoadXInput();
    WNDCLASSA WindowClass = { };

    Win32ResizeDIBSection(&GlobalBackBuffer, 1280, 720);
    OutputDebugStringA("WM_SIZE\n");

    // Win32MainWindowCallback will handle the messages sent to the window
    WindowClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC; // Redraw the whole window on horizontal or vertical resize
    WindowClass.lpfnWndProc = Win32MainWindowCallback;
    WindowClass.hInstance = hInstance;
    WindowClass.lpszClassName = "HandmadeHeroWindowClass";

    // Register the window class
    if (RegisterClassA(&WindowClass))
    {
        // Create the window with the class we just registered
        HWND windowHandle = CreateWindowExA(
            0,
            WindowClass.lpszClassName,
            "Handmade Hero",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            nullptr,
            nullptr,
            hInstance,
            nullptr);

        if (windowHandle)
        {
            // NOTE: Since we specified CS_OWNDC, we can just
            // get one device context and use it forever because we
            // are not sharing it with anyone.
            HDC deviceContext = GetDC(windowHandle);

            // NOTE: Graphics test
            int xOffset = 0;
            int yOffset = 0;

            // NOTE: Sound test
            win32_sound_output sound_output{ };
            sound_output.samples_per_second = 48000;
            sound_output.tone_hz = 256;
            sound_output.tone_volume = 500;
            sound_output.running_sample_index = 0;
            sound_output.wave_period = sound_output.samples_per_second / sound_output.tone_hz;
            sound_output.bytes_per_sample = sizeof(int16_t) * 2;
            sound_output.secondary_buffer_size = sound_output.samples_per_second * sound_output.bytes_per_sample;
            sound_output.latency_sample_count = sound_output.samples_per_second / 15;

            Win32InitDSound(windowHandle, sound_output.samples_per_second, sound_output.secondary_buffer_size);
            Win32FillSoundBuffer(&sound_output, 0, sound_output.latency_sample_count * sound_output.bytes_per_sample);

            SUCCEEDED(GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING));

            LARGE_INTEGER last_counter;
            QueryPerformanceCounter(&last_counter);

            DWORD64 last_cycle_count = __rdtsc();

            // Start handling messages
            GlobalRunning = true;
            while (GlobalRunning)
            {
                // Pull a message from the message queue
                MSG message;
                while (PeekMessageA(&message, nullptr, 0, 0, PM_REMOVE))
                {
                    if (message.message == WM_QUIT)
                    {
                        GlobalRunning = false;
                    }

                    // translates virtual key messages into character messages
                    TranslateMessage(&message);

                    // Sends the message to the window procedure.
                    // It will be handled by the function pointer at lpfnWndProc.
                    // Windows may send messages to the window without going through this (without putting it in the queue).
                    DispatchMessageA(&message);
                }

                XINPUT_VIBRATION vibration;
                // TODO: Should we poll this more frequently?
                for (DWORD controller_index = 0; controller_index < XUSER_MAX_COUNT; ++controller_index)
                {
                    XINPUT_STATE controller_state;
                    if (XInputGetState(controller_index, &controller_state) == ERROR_SUCCESS)
                    {
                        // NOTE: This controller is plugged in.
                        XINPUT_GAMEPAD* pad = &controller_state.Gamepad;
                        bool up = pad->wButtons & XINPUT_GAMEPAD_DPAD_UP;
                        bool down = pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
                        bool left = pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
                        bool right = pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;
                        bool start = pad->wButtons & XINPUT_GAMEPAD_START;
                        bool back = pad->wButtons & XINPUT_GAMEPAD_BACK;
                        bool left_shoulder = pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER;
                        bool right_shoulder = pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER;
                        bool left_trigger = pad->bLeftTrigger > 0;
                        bool right_trigger = pad->bRightTrigger > 0;
                        bool a = pad->wButtons & XINPUT_GAMEPAD_A;
                        bool b = pad->wButtons & XINPUT_GAMEPAD_B;
                        bool x = pad->wButtons & XINPUT_GAMEPAD_X;
                        bool y = pad->wButtons & XINPUT_GAMEPAD_Y;
                        int16_t left_thumb_x = pad->sThumbLX;
                        int16_t left_thumb_y = pad->sThumbLY;
                        int16_t right_thumb_x = pad->sThumbRX;
                        int16_t right_thumb_y = pad->sThumbRY;

                        xOffset += left_thumb_x / 4096;
                        yOffset += left_thumb_y / 4096;

                        if (a)
                        {
                            OutputDebugStringA("A\n");
                            yOffset += 2;

                            vibration.wLeftMotorSpeed = 60000;
                            vibration.wRightMotorSpeed = 60000;
                            XInputSetState(0, &vibration);
                        }
                        else
                        {
                            vibration.wLeftMotorSpeed = 0;
                            vibration.wRightMotorSpeed = 0;
                            XInputSetState(0, &vibration);
                        }
                    }
                    else
                    {
                        // NOTE: The controller is not available.
                    }
                }

                RenderWeirdGradient(GlobalBackBuffer, xOffset, yOffset);

                // NOTE: DirectSound output test
                DWORD play_cursor;
                DWORD write_cursor;
                if (SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition(&play_cursor, &write_cursor)))
                {
                    DWORD byte_to_lock = (sound_output.running_sample_index * sound_output.bytes_per_sample) %
                            sound_output.secondary_buffer_size;
                    DWORD target_cursor = (play_cursor + (
                                sound_output.latency_sample_count * sound_output.bytes_per_sample)) % sound_output.
                            secondary_buffer_size;

                    DWORD bytes_to_write;

                    if (byte_to_lock > target_cursor)
                    {
                        bytes_to_write = sound_output.secondary_buffer_size - byte_to_lock;
                        bytes_to_write += target_cursor;
                    }
                    else
                    {
                        bytes_to_write = target_cursor - byte_to_lock;
                    }

                    Win32FillSoundBuffer(&sound_output, byte_to_lock, bytes_to_write);
                }

                win32_window_dimensions dimensions = Win32GetWindowDimensions(windowHandle);
                Win32DisplayBufferInWindow(deviceContext, dimensions.Width, dimensions.Height, GlobalBackBuffer);

                uint64_t end_cycle_count = __rdtsc();

                LARGE_INTEGER end_counter;
                QueryPerformanceCounter(&end_counter);

                uint64_t cycles_elapsed = end_cycle_count - last_cycle_count;
                int64_t counter_elapsed = end_counter.QuadPart - last_counter.QuadPart;
                auto ms_per_frame = static_cast<float>(counter_elapsed) * 1000.0f / static_cast<
                    float>(perf_counter_frequency);
                float fps = 1000 / ms_per_frame;
                auto mega_cycles_per_frame = static_cast<float>(cycles_elapsed) / (1000.0f *
                    1000.0f);
                char buffer[256];
                wsprintf(buffer, "ms/frame: %f (FPS: %f, Mega Cycles: %f)\n", ms_per_frame, fps,
                          mega_cycles_per_frame);
                OutputDebugStringA(buffer);

                last_counter = end_counter;
                last_cycle_count = end_cycle_count;
            }
        }
        else
        {
            // TODO: logging
        }
    }
    else
    {
        // TODO: logging
    }

    return 0;
}
