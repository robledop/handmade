#include <string>
#include <windows.h>

// TODO: this is global for now
static bool Running;
static BITMAPINFO BitmapInfo; // BitmapInfo will be used to create a DIB section
static void* BitmapMemory; // BitmapMemory will hold the pixel data for the DIB section
static HBITMAP BitmapHandle; // BitmapHandle will be used to manage the DIB section
static HDC BitmapDeviceContext; // BitmapDeviceContext will be used to draw the DIB section

// This function resizes the DIB section to the specified width and height
static void Win32ResizeDIBSection(const int width, const int height)
{
    // TODO: bulletproof this
    // maybe don't free first, free after, then free first if that fails

    if (BitmapHandle)
    {
        // DeleteObject will delete the bitmap handle and free the memory associated with it
        DeleteObject(BitmapHandle);
    }

    if (!BitmapDeviceContext)
    {
        // TODO: should we recreate these under certain special circumstances
        BitmapDeviceContext = CreateCompatibleDC(nullptr);
    }

    BitmapInfo.bmiHeader.biSize = sizeof(BitmapInfo.bmiHeader); // Size of the BITMAPINFOHEADER structure
    BitmapInfo.bmiHeader.biWidth = width;
    BitmapInfo.bmiHeader.biHeight = height;
    BitmapInfo.bmiHeader.biPlanes = 1; // Number of color planes must be 1
    BitmapInfo.bmiHeader.biBitCount = 32; // 32 bits per pixel (RGBA)
    BitmapInfo.bmiHeader.biCompression = BI_RGB;

    // A DIB section is a bitmap that can be used to draw directly to the screen.
    BitmapHandle = CreateDIBSection(BitmapDeviceContext,
                                    &BitmapInfo,
                                    DIB_RGB_COLORS,
                                    &BitmapMemory,
                                    nullptr,
                                    0);
}

// Updates the window with the pixel data from BitmapMemory
static void Win32UpdateWindow(
    HDC deviceContext,
    const int x,
    const int y,
    const int width,
    const int height)
{
    // StretchDIBits will copy the pixel data from BitmapMemory to the device context.
    // It will stretch the pixels to fit the specified width and height.
    StretchDIBits(deviceContext,
                  x,
                  y,
                  width,
                  height,
                  x,
                  y,
                  width,
                  height,
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

        Win32UpdateWindow(deviceContext, x, y, width, height);

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
            // Start handling messages
            Running = true;
            while (Running)
            {
                // Pull a message from the message queue
                MSG message;
                BOOL messageResult = GetMessageA(
                    &message,
                    nullptr,
                    0,
                    0);

                if (messageResult > 0) // 0 is WM_QUIT, -1 is an error
                {
                    // translates virtual key messages into character messages
                    TranslateMessage(&message);

                    // Sends the message to the window procedure.
                    // It will be handled by the function pointer at lpfnWndProc.
                    // Windows may send messages to the window without going through this (without putting it in the queue).
                    DispatchMessageA(&message);
                }
                else
                {
                    break;
                }
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
