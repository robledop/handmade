#include <windows.h>

#define internal static
#define local_persist static
#define global_variable static

// TODO: this is global for now
global_variable bool Running;
global_variable BITMAPINFO BitmapInfo;
global_variable void* BitmapMemory;
global_variable HBITMAP BitmapHandle;
global_variable HDC BitmapDeviceContext;

internal void Win32ResizeDIBSection(int width, int height) {
  // TODO: bulletproof this
  // maybe don't free first, free after, then free first if that fails

  if (BitmapHandle) {
    DeleteObject(BitmapHandle);
  }
  if (!BitmapDeviceContext) {
    // TODO: should we recreate these under certain special circumstances
    BitmapDeviceContext = CreateCompatibleDC(0);
  }

  BitmapInfo.bmiHeader.biSize = sizeof(BitmapInfo.bmiHeader);
  BitmapInfo.bmiHeader.biWidth = width;
  BitmapInfo.bmiHeader.biHeight = height;
  BitmapInfo.bmiHeader.biPlanes = 1;
  BitmapInfo.bmiHeader.biBitCount = 32;
  BitmapInfo.bmiHeader.biCompression = BI_RGB;

  BitmapHandle = CreateDIBSection(BitmapDeviceContext, &BitmapInfo,
                                  DIB_RGB_COLORS, &BitmapMemory, 0, 0);
}

internal void Win32UpdateWindow(HDC deviceContext,
                                int x,
                                int y,
                                int width,
                                int height) {
  StretchDIBits(deviceContext, x, y, width, height, x, y, width, height,
                BitmapMemory, &BitmapInfo, DIB_RGB_COLORS, SRCCOPY);
}

LRESULT CALLBACK Win32MainWindowCallback(HWND window,
                                         UINT message,
                                         WPARAM wParam,
                                         LPARAM lParam) {
  LRESULT result = 0;
  switch (message) {
    case WM_SIZE: {
      RECT clientRect;
      GetClientRect(window, &clientRect);
      int width = clientRect.right - clientRect.left;
      int height = clientRect.bottom - clientRect.top;
      Win32ResizeDIBSection(width, height);
      OutputDebugStringA("WM_SIZE\n");
    } break;

    case WM_DESTROY: {
      // TODO: handle this as an error - recreate window?
      Running = false;
      OutputDebugStringA("WM_DESTROY\n");
    } break;

    case WM_CLOSE: {
      // TODO: handle this with a message to the user?
      Running = false;
      OutputDebugStringA("WM_CLOSE\n");
    } break;

    case WM_ACTIVATEAPP: {
      OutputDebugStringA("WM_ACTIVATEAPP\n");
    } break;

    case WM_PAINT: {
      OutputDebugStringA("WM_PAINT\n");
      PAINTSTRUCT paint;
      HDC deviceContext = BeginPaint(window, &paint);

      int x = paint.rcPaint.left;
      int y = paint.rcPaint.top;
      int height = paint.rcPaint.bottom - paint.rcPaint.top;
      int width = paint.rcPaint.right - paint.rcPaint.left;

      Win32UpdateWindow(deviceContext, x, y, width, height);

      EndPaint(window, &paint);
    } break;

    default:
      // OutputDebugStringA("default\n");
      result = DefWindowProc(window, message, wParam, lParam);
      break;
  }

  return result;
}

int CALLBACK WinMain(HINSTANCE instance,
                     HINSTANCE prevInstance,
                     LPSTR commandLine,
                     int showCode) {
  WNDCLASS WindowClass = {};

  WindowClass.lpfnWndProc = Win32MainWindowCallback;
  WindowClass.hInstance = instance;
  WindowClass.lpszClassName = "HandmadeHeroWindowClass";

  if (RegisterClassA(&WindowClass)) {
    HWND windowHandle = CreateWindowExA(
        0, WindowClass.lpszClassName, "Handmade Hero",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
        CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, instance, 0);

    if (windowHandle) {
      Running = true;
      while (Running) {
        MSG message;
        BOOL messageResult = GetMessage(&message, 0, 0, 0);
        if (messageResult > 0) {
          TranslateMessage(&message);
          DispatchMessageA(&message);
        } else {
          break;
        }
      }
    } else {
      // TODO: logging
    }
  } else {
    // TODO: logging
  }

  return 0;
}