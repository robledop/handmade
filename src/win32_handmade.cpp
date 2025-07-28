#include <cstdint>
#include <string>
#include <windows.h>

// TODO: this is global for now
static bool Running;
static BITMAPINFO BitmapInfo;
static void* BitmapMemory;
static int BitmapWidth;
static int BitmapHeight;
static int BytesPerPixel = 4;

static void RenderWeirdGradient(int xOffset, int yOffset)
{
    int pitch = BitmapWidth * BytesPerPixel; // the number of bytes in a row of the bitmap
    auto row = static_cast<uint8_t*>(BitmapMemory);

    for (int y = 0; y < BitmapHeight; ++y)
    {
        auto pixel = reinterpret_cast<uint32_t*>(row);
        for (int x = 0; x < BitmapWidth; ++x)
        {
            uint8_t blue = x + xOffset;
            uint8_t green = y + yOffset;

            *pixel++ = green << 8 | blue;
        }

        row += pitch;
    }
}

// This function resizes the DIB section to the specified width and height
static void Win32ResizeDIBSection(const int width, const int height)
{
    if (BitmapMemory)
    {
        VirtualFree(BitmapMemory, 0, MEM_RELEASE);
    }

    BitmapWidth = width;
    BitmapHeight = height;

    BitmapInfo.bmiHeader.biSize = sizeof(BitmapInfo.bmiHeader); // Size of the BITMAPINFOHEADER structure
    BitmapInfo.bmiHeader.biWidth = BitmapWidth;
    BitmapInfo.bmiHeader.biHeight = -BitmapHeight; // Negative height indicates a top-down bitmap
    BitmapInfo.bmiHeader.biPlanes = 1; // Number of color planes must be 1
    BitmapInfo.bmiHeader.biBitCount = 32; // 32 bits per pixel (RGBA)
    BitmapInfo.bmiHeader.biCompression = BI_RGB;

    int bitmapMemorySize = BitmapWidth * BitmapHeight * BytesPerPixel;

    // Allocate memory for the bitmap with read/write permissions
    BitmapMemory = VirtualAlloc(
        nullptr,
        bitmapMemorySize,
        MEM_COMMIT,
        PAGE_READWRITE);
}

// Updates the window with the pixel data from BitmapMemory
static void Win32UpdateWindow(
    HDC deviceContext,
    const RECT* windowRect,
    const int x,
    const int y,
    const int width,
    const int height)
{
    int windowWidth = windowRect->right - windowRect->left;
    int windowHeight = windowRect->bottom - windowRect->top;

    // StretchDIBits will copy the pixel data from BitmapMemory to the device context.
    // It will stretch the pixels to fit the specified width and height.
    StretchDIBits(deviceContext,
                  0,
                  0,
                  BitmapWidth,
                  BitmapHeight,
                  0,
                  0,
                  BitmapWidth,
                  BitmapHeight,
                  BitmapMemory,
                  &BitmapInfo,
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
        RECT clientRect;
        GetClientRect(window, &clientRect);
        const int width = clientRect.right - clientRect.left;
        const int height = clientRect.bottom - clientRect.top;
        Win32ResizeDIBSection(width, height);
        OutputDebugStringA("WM_SIZE\n");
    }
    break;

    case WM_DESTROY:
    {
        // TODO: handle this as an error - recreate window?
        Running = false;
        OutputDebugStringA("WM_DESTROY\n");
    }
    break;

    case WM_CLOSE:
    {
        // TODO: handle this with a message to the user?
        Running = false;
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

        RECT clientRect;
        GetClientRect(window, &clientRect);

        Win32UpdateWindow(deviceContext, &clientRect, x, y, width, height);

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

    // Win32MainWindowCallback will handle the messages sent to the window
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
            Running = true;
            while (Running)
            {
                // Pull a message from the message queue
                MSG message;
                while (PeekMessageA(&message, nullptr, 0, 0, PM_REMOVE))
                {
                    if (message.message == WM_QUIT)
                    {
                        Running = false;
                    }

                    // translates virtual key messages into character messages
                    TranslateMessage(&message);

                    // Sends the message to the window procedure.
                    // It will be handled by the function pointer at lpfnWndProc.
                    // Windows may send messages to the window without going through this (without putting it in the queue).
                    DispatchMessageA(&message);
                }
                RenderWeirdGradient(xOffset, yOffset);

                HDC deviceContext = GetDC(windowHandle);
                RECT clientRect;
                GetClientRect(windowHandle, &clientRect);
                int windowWidth = clientRect.right - clientRect.left;
                int windowHeight = clientRect.bottom - clientRect.top;
                Win32UpdateWindow(deviceContext, &clientRect, 0, 0, windowWidth, windowHeight);

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
