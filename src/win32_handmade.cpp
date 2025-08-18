#include <cstdint>
#include <string>
#include <windows.h>
#include <xinput.h>
#include <dsound.h>

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
    // NOTE: Load the library
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

        LPDIRECTSOUNDBUFFER secondary_buffer = { };
        if (SUCCEEDED(direct_sound->CreateSoundBuffer(&buffer_description, &secondary_buffer, nullptr)))
        {
            OutputDebugStringA("Secondary buffer created\n");
        }

        // NOTE: Start it playing
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
        // OutputDebugStringA("default\n");
        result = DefWindowProc(window, message, wParam, lParam);
        break;
    }

    return result;
}

int CALLBACK WinMain(HINSTANCE hInstance,
    [[maybe_unused]] HINSTANCE hPrevInstance,
    [[maybe_unused]] LPSTR lpCmdLine,
    [[maybe_unused]] int nShowCmd)
{
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

            int xOffset = 0;
            int yOffset = 0;

            Win32InitDSound(windowHandle, 48000, 48000 * sizeof(int16_t) * 2);

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

                        xOffset += left_thumb_x >> 14;
                        yOffset += left_thumb_y >> 14;

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

                win32_window_dimensions dimensions = Win32GetWindowDimensions(windowHandle);
                Win32DisplayBufferInWindow(deviceContext, dimensions.Width, dimensions.Height, GlobalBackBuffer);

                ReleaseDC(windowHandle, deviceContext);
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
