#include <cstdint>
#include <string>
#include <windows.h>
#include <cstdint>

struct win32_offscreen_buffer {
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
        MEM_COMMIT,
        PAGE_READWRITE);

    buffer->Pitch = buffer->Width * buffer->BytesPerPixel; // the number of bytes in a row of the bitmap
}

// Updates the window with the pixel data from BitmapMemory
static void Win32DisplayBufferInWindow(
    HDC deviceContext,
    int windowWidth,
    int windowHeight,
    const win32_offscreen_buffer& buffer,
    const int x,
    const int y,
    const int width,
    const int height)
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
    case WM_KEYDOWN:
    {
        OutputDebugStringA(("WM_KEYDOWN " +
            std::to_string(static_cast<int>(wParam)) + "\n").c_str());
    }
    break;
    case WM_SIZE: // Window size changed
    {
    }
    break;

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

    case WM_PAINT: // Window needs to be repainted
    {
        OutputDebugStringA("WM_PAINT\n");
        PAINTSTRUCT paint;
        // Prepare the window for painting and returns a device context
        HDC deviceContext = BeginPaint(window, &paint);

        const int x = paint.rcPaint.left;
        const int y = paint.rcPaint.top;
        const int height = paint.rcPaint.bottom - paint.rcPaint.top;
        const int width = paint.rcPaint.right - paint.rcPaint.left;

        win32_window_dimensions dimensions = Win32GetWindowDimensions(window);
        Win32DisplayBufferInWindow(deviceContext, dimensions.Width, dimensions.Height, GlobalBackBuffer, x, y,
                                       width, height);

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
    WNDCLASS WindowClass = { };

    Win32ResizeDIBSection(&GlobalBackBuffer, 1280, 720);
    OutputDebugStringA("WM_SIZE\n");


    // Win32MainWindowCallback will handle the messages sent to the window
    WindowClass.style = CS_HREDRAW | CS_VREDRAW; // Redraw the whole window on horizontal or vertical resize
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
            int xOffset = 0;
            int yOffset = 0;
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
                RenderWeirdGradient(GlobalBackBuffer, xOffset, yOffset);

                HDC deviceContext = GetDC(windowHandle);
                // RECT clientRect;
                // GetClientRect(windowHandle, &clientRect);
                // int windowWidth = clientRect.right - clientRect.left;
                // int windowHeight = clientRect.bottom - clientRect.top;
                win32_window_dimensions dimensions = Win32GetWindowDimensions(windowHandle);
                Win32DisplayBufferInWindow(deviceContext, dimensions.Width, dimensions.Height, GlobalBackBuffer, 0, 0, dimensions.Width,
                                               dimensions.Height);

                ReleaseDC(windowHandle, deviceContext);

                ++xOffset;
                ++yOffset;
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
