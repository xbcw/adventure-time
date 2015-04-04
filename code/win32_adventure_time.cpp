#include <windows.h>
#include <stdint.h>
#include <xinput.h>
#include <dsound.h>
// TODO(brad): implement sine ourselves)
#include <math.h>

#define internal static
#define local_persist static
#define global_variable static

typedef int32_t bool32;

struct win32_offscreen_buffer
{
    BITMAPINFO Info;
    void *Memory;
    int Width;
    int Height;
    int Pitch;
    int BytesPerPixel;
};

// TODO(brad): This is a global for now
global_variable bool GlobalRunning;
global_variable win32_offscreen_buffer GlobalBackBuffer;
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;
global_variable bool SoundIsPlaying = false;

struct win32_window_dimension
{
    int Width;
    int Height;
};

// NOTE(brad); XInputGetState
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
    return(ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

// NOTE(brad): XInputSetState
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
    return(ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuiDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

#define Pi32 3.14159265359f

internal void Win32LoadXInput(void)
{
    // TODO(brad): Test this on Windows 8
    HMODULE XInputLibrary = LoadLibrary("xinput1_4.dll");
    if(!XInputLibrary)
    {
        // TODO(brad): Diagnostic
        HMODULE XInputLibrary = LoadLibrary("xinput1_3.dll");
    }
    if(XInputLibrary)
    {
        XInputGetState = (x_input_get_state *)GetProcAddress(XInputLibrary, "XInputGetState");
        XInputSetState = (x_input_set_state *)GetProcAddress(XInputLibrary, "XInputSetState");
    }
    else
    {
        // TODO(brad): Diagnostic
    }
}

internal void Win32InitDSound(HWND Window, int32_t SamplesPerSecond, int32_t BufferSize)
{
    // NOTE(brad): Load the library
    HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");

    if(DSoundLibrary)
    {
        // NOTE(brad): Get a DirectSound object
        direct_sound_create *DirectSoundCreate = (direct_sound_create *)GetProcAddress(DSoundLibrary, "DirectSoundCreate");

        LPDIRECTSOUND DirectSound;
        if(DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0)))
        {
            WAVEFORMATEX WaveFormat = {};
            WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
            WaveFormat.nChannels = 2;
            WaveFormat.nSamplesPerSec = SamplesPerSecond;
            WaveFormat.wBitsPerSample = 16;
            WaveFormat.nBlockAlign = (WaveFormat.nChannels * WaveFormat.wBitsPerSample) / 8;
            WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec * WaveFormat.nBlockAlign;
            WaveFormat.cbSize = 0;

            if(SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY)))
            {
                // NOTE(brad): Create a primary buffer
                DSBUFFERDESC BufferDescription = {};
                BufferDescription.dwSize = sizeof(BufferDescription);
                BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;
                BufferDescription.dwBufferBytes = BufferSize;

                LPDIRECTSOUNDBUFFER PrimaryBuffer;
                if(SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &PrimaryBuffer, 0)))
                {
                    if(SUCCEEDED(PrimaryBuffer->SetFormat(&WaveFormat)))
                    {
                        // NOTE(brad): We set the format
                    }
                    else
                    {
                        // TODO(brad): Diagnostic
                    }
                }
                else
                {
                    // TODO(brad): Diagnostic
                }
            }
            else
            {
                // TODO(brad): Diagnostic
            }

            // NOTE(brad): Create a secondary buffer
            DSBUFFERDESC BufferDescription = {};
            BufferDescription.dwSize = sizeof(BufferDescription);
            BufferDescription.dwFlags = 0;
            BufferDescription.dwBufferBytes = BufferSize;
            BufferDescription.lpwfxFormat = &WaveFormat;

            if(SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &GlobalSecondaryBuffer, 0)))
            {
            }           
            else
            {
                // TODO(brad): Diagnostic
            }
        }
        else
        {
            // TODO(brad): Diagnostic
        }
    }
    else
    {
        // TODO(brad): Diagnostic
    }
}

internal win32_window_dimension Win32GetWindowDimension(HWND Window)
{
    win32_window_dimension Result;
    RECT ClientRect;
    GetClientRect(Window, &ClientRect);
    Result.Width = ClientRect.right - ClientRect.left;
    Result.Height = ClientRect.bottom - ClientRect.top;

    return(Result);
}

internal void RenderWeirdGradient(
        win32_offscreen_buffer Buffer, 
        int BlueOffset, int GreenOffset)
{
    uint8_t *Row = (uint8_t *)Buffer.Memory;
    for(int Y = 0;
            Y < Buffer.Height;
            ++Y)
    {
        uint32_t *Pixel = (uint32_t *)Row;
        for(int X = 0;
                X < Buffer.Width;
                ++X)
        {
            uint8_t Blue = (X + BlueOffset);
            uint8_t Green = (Y + GreenOffset);

            *Pixel++ = ((Green << 8) | Blue); 
        }

        Row += Buffer.Pitch;
    }
}

internal void Win32ResizeDIBSection(
        win32_offscreen_buffer *Buffer, 
        int Width, int Height)
{
    // TODO(brad): Bulletproof this.
    // Maybe don't free first, free after, then free first if that fails.

    if(Buffer->Memory)
    {
        VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
    }

    Buffer->Width = Width;
    Buffer->Height = Height;
    Buffer->BytesPerPixel = 4;

    Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
    Buffer->Info.bmiHeader.biWidth = Buffer->Width;
    Buffer->Info.bmiHeader.biHeight = -Buffer->Height;
    Buffer->Info.bmiHeader.biPlanes = 1;
    Buffer->Info.bmiHeader.biBitCount = 32;
    Buffer->Info.bmiHeader.biCompression = BI_RGB;

    int BitmapMemorySize = (Buffer->Width * Buffer->Height) * Buffer->BytesPerPixel;
    Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);

    Buffer->Pitch = Buffer->Width * Buffer->BytesPerPixel;
}

internal void Win32DisplayBufferInWindow(
        win32_offscreen_buffer Buffer, 
        HDC DeviceContext, 
        int WindowWidth, int WindowHeight, 
        int X, int Y, 
        int Width, int Height)
{
    // TODO(brad): Aspect ration correction
    StretchDIBits(
            DeviceContext,
            0, 0, WindowWidth, WindowHeight,
            0, 0, Buffer.Width, Buffer.Height,
            Buffer.Memory,
            &Buffer.Info,
            DIB_RGB_COLORS, SRCCOPY);
}

LRESULT CALLBACK Win32MainWindowCallback(
        HWND Window,
        UINT Message,
        WPARAM WParam,
        LPARAM LParam)
{
    LRESULT Result = 0;

    switch(Message)
    {
        case WM_SIZE:
            {
            } break;

        case WM_DESTROY:
            {
                // TODO(brad): Handle this as an error - recreate window?
                GlobalRunning = false;
            } break;

        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP:
            {
                uint32_t VKCode = WParam; 
                bool WasDown = ((LParam & (1 << 30)) != 0);
                bool IsDown = ((LParam & (1 << 31)) == 0);

                if(WasDown != IsDown)
                {
                    if(VKCode == 'W')
                    {
                        OutputDebugStringA("W\n");
                    }
                    else if(VKCode == 'A')
                    {
                        OutputDebugStringA("A\n");
                    } 
                    else if(VKCode == 'S')
                    {
                        OutputDebugStringA("S\n");
                    }
                    else if(VKCode == 'D')
                    {
                        OutputDebugStringA("D\n");
                    }
                    else if(VKCode == 'F')
                    {
                        OutputDebugStringA("F\n");
                    }
                    else if(VKCode == 'Q')
                    {
                        OutputDebugStringA("Q\n");
                    }
                    else if(VKCode == 'E')
                    {
                        OutputDebugStringA("E\n");
                    }
                    else if(VKCode == VK_ESCAPE)
                    {
                        OutputDebugStringA("ESCAPE: ");
                        if(IsDown)
                        {
                            OutputDebugStringA("IsDown");
                        }
                        if(WasDown)
                        {
                            OutputDebugStringA("WasDown");
                        }
                        OutputDebugStringA("\n");

                        OutputDebugStringA("VK_ESCAPE\n");
                    }
                    else if(VKCode == VK_SPACE)
                    {
                        OutputDebugStringA("VK_SPACE\n");
                    }
                    else if(VKCode == VK_UP)
                    {
                        OutputDebugStringA("VK_UP\n");
                    }
                    else if(VKCode == VK_DOWN)
                    {
                        OutputDebugStringA("VK_DOWN\n");
                    }
                    else if(VKCode == VK_RIGHT)
                    {
                        OutputDebugStringA("VK_RIGHT\n");
                    }
                    else if(VKCode == VK_LEFT)
                    {
                        OutputDebugStringA("VK_LEFT\n");
                    }
                    else
                    {}
                }

                bool32 AltKeyWasDown = LParam & (1 << 29);
                if((VKCode == VK_F4) && AltKeyWasDown)
                {
                    GlobalRunning = false;
                }
            }break;

        case WM_CLOSE:
            {
                // TODO(brad): Handle this with a message to the user?
                GlobalRunning = false;
            } break;

        case WM_ACTIVATEAPP:
            {
                OutputDebugStringA("WM_ACTIVATEAPP\n");
            } break;

        case WM_PAINT:
            {
                PAINTSTRUCT Paint;
                HDC DeviceContext = BeginPaint(Window, &Paint);
                int X = Paint.rcPaint.left;
                int Y = Paint.rcPaint.top;
                int Height = Paint.rcPaint.bottom - Paint.rcPaint.top;
                int Width = Paint.rcPaint.right - Paint.rcPaint.left;

                win32_window_dimension Dimension = Win32GetWindowDimension(Window);
                Win32DisplayBufferInWindow(GlobalBackBuffer, DeviceContext, Dimension.Width, Dimension.Height, X, Y, Width, Height);
                EndPaint(Window, &Paint);
            } break;

        default:
            {
                //			OutputDebugStringA("default\n");
                Result = DefWindowProc(Window, Message, WParam, LParam);
            } break;
    }

    return(Result);
}

struct win32_sound_output
{
    int SamplesPerSecond;
    int ToneHz;
    int ToneVolume;
    int RunningSampleIndex;
    int WavePeriod;
    int HalfWavePeriod;
    int BytesPerSample;
    int SecondaryBufferSize;
};

internal void Win32FillSoundBuffer(win32_sound_output *SoundOutput, DWORD ByteToLock, DWORD BytesToWrite)
{
    VOID *Region1;
    DWORD Region1Size;
    VOID *Region2;
    DWORD Region2Size;
    if(SUCCEEDED(GlobalSecondaryBuffer->Lock(
                    ByteToLock, BytesToWrite,
                    &Region1, &Region1Size,
                    &Region2, &Region2Size,
                    0)))
    {

        // TODO(brad): assert that Region1Size/Region2Size is valid
        int16_t *SampleOut = (int16_t *)Region1;
        DWORD Region1SampleCount = Region1Size / SoundOutput->BytesPerSample;
        for(DWORD SampleIndex = 0; SampleIndex < Region1SampleCount; ++SampleIndex)
        {
            float t = 2.0f*Pi32*(float)SoundOutput->RunningSampleIndex / (float)SoundOutput->WavePeriod;
            float SineValue = sinf(t);
            int16_t SampleValue = (int16_t)(SineValue * SoundOutput->ToneVolume); 
            //int16_t SampleValue = ((RunningSampleIndex++ / HalfWavePeriod) % 2) ? ToneVolume : -ToneVolume;
            *SampleOut++ = SampleValue;
            *SampleOut++ = SampleValue;

            ++SoundOutput->RunningSampleIndex;
        }

        DWORD Region2SampleCount = Region2Size / SoundOutput->BytesPerSample;
        SampleOut = (int16_t *)Region2;
        for(DWORD SampleIndex = 0; SampleIndex < Region2SampleCount; ++SampleIndex)
        {
            float t = 2.0f*Pi32*(float)SoundOutput->RunningSampleIndex / (float)SoundOutput->WavePeriod;
            float SineValue = sinf(t);
            int16_t SampleValue = (int16_t)(SineValue * SoundOutput->ToneVolume); 
            //int16_t SampleValue = ((RunningSampleIndex++ / HalfWavePeriod) % 2) ? ToneVolume : -ToneVolume;
            *SampleOut++ = SampleValue;
            *SampleOut++ = SampleValue;

            ++SoundOutput->RunningSampleIndex;
        }
        GlobalSecondaryBuffer->Unlock(
                Region1, Region1Size,
                Region2, Region2Size);
    }
}

int CALLBACK WinMain(
        HINSTANCE Instance,
        HINSTANCE PrevInstance,
        LPSTR CommandLine,
        int ShowCode)
{
    LARGE_INTEGER PerfCounterFrequencyResult;
    QueryPerformanceFrequency(&PerfCounterFrequencyResult);
    int64_t PerfCounterFrequency = PerfCounterFrequencyResult.QuadPart;

    Win32LoadXInput();

    WNDCLASS WindowClass = {};

    Win32ResizeDIBSection(&GlobalBackBuffer, 1280, 720);

    WindowClass.style = CS_HREDRAW|CS_VREDRAW;
    WindowClass.lpfnWndProc = Win32MainWindowCallback;
    WindowClass.hInstance = Instance;
    // WindowClass.hIcon;
    WindowClass.lpszClassName = "AdventureTimeWindowClass";

    if(RegisterClass(&WindowClass))
    {
        HWND Window = 
            CreateWindowEx(
                    0,
                    WindowClass.lpszClassName,
                    "Adventure Time",
                    WS_OVERLAPPEDWINDOW|WS_VISIBLE,
                    CW_USEDEFAULT,
                    CW_USEDEFAULT,
                    CW_USEDEFAULT,
                    CW_USEDEFAULT,
                    0,
                    0,
                    Instance,
                    0);
        if(Window)
        {
            HDC DeviceContext = GetDC(Window);

            // NOTE(brad): Graphics Test
            int XOffset = 0;
            int YOffset = 0;

            // NOTE(brad): Sount Test
            win32_sound_output SoundOutput = {};

            SoundOutput.SamplesPerSecond = 48000;
            SoundOutput.ToneHz = 256;
            SoundOutput.ToneVolume = 500;
            SoundOutput.RunningSampleIndex = 0;
            SoundOutput.WavePeriod = SoundOutput.SamplesPerSecond / SoundOutput.ToneHz;
            SoundOutput.HalfWavePeriod = SoundOutput.WavePeriod/2;
            SoundOutput.BytesPerSample = sizeof(int16_t)*2;
            SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond*SoundOutput.BytesPerSample;

            Win32InitDSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
            Win32FillSoundBuffer(&SoundOutput, 0, SoundOutput.SecondaryBufferSize);
            GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

            GlobalRunning = true;

            LARGE_INTEGER LastCounter;
            QueryPerformanceCounter(&LastCounter);
            
            int64_t LastCycleCount = __rdtsc();

            while(GlobalRunning)
            {

                MSG Message;
                while(PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
                {
                    if(Message.message == WM_QUIT)
                    {
                        GlobalRunning = false;
                    }

                    TranslateMessage(&Message);
                    DispatchMessage(&Message);
                }

                // TODO(brad): Should we poll this more frequently?
                for(DWORD ControllerIndex=0; ControllerIndex<XUSER_MAX_COUNT; ControllerIndex++)
                {
                    XINPUT_STATE ControllerState;
                    if(XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
                    {
                        // NOTE(brad): This controller is plugged in
                        XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;

                        bool PadDpadUp = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
                        bool PadDpadDown = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
                        bool PadDpadLeft = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
                        bool PadDpadRight = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
                        bool PadStart = (Pad->wButtons & XINPUT_GAMEPAD_START);
                        bool PadBack = (Pad->wButtons & XINPUT_GAMEPAD_BACK);
                        bool PadLeftThumb = (Pad->wButtons & XINPUT_GAMEPAD_LEFT_THUMB);
                        bool PadRightThumb = (Pad->wButtons & XINPUT_GAMEPAD_RIGHT_THUMB);
                        bool PadLeftShoulder = (Pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
                        bool PadRightShoulder = (Pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
                        bool PadA = (Pad->wButtons & XINPUT_GAMEPAD_A);
                        bool PadB = (Pad->wButtons & XINPUT_GAMEPAD_B);
                        bool PadX = (Pad->wButtons & XINPUT_GAMEPAD_X);
                        bool PadY = (Pad->wButtons & XINPUT_GAMEPAD_Y);

                        int8_t TriggerLeft = Pad->bLeftTrigger;
                        int8_t TriggerRight = Pad->bRightTrigger;

                        int16_t StickLeftX = Pad->sThumbLX;
                        int16_t StickLeftY = Pad->sThumbLY;
                        int16_t StickRightX = Pad->sThumbRX;
                        int16_t StickRightY = Pad->sThumbRY;

                        XINPUT_VIBRATION Vibration;
                        Vibration.wLeftMotorSpeed = 60000;
                        Vibration.wRightMotorSpeed = 60000;
                        if(PadDpadUp)
                        {
                            YOffset += 3;
                            XInputSetState(0, &Vibration);
                        }
                        if(PadDpadDown)
                        {
                            YOffset -= 3;
                        }
                        if(PadDpadLeft)
                        {
                            XOffset += 3;
                        }
                        if(PadDpadRight)
                        {
                            XOffset -= 3;
                        }
                    }
                    else
                    {
                        // NOTE(brad): This controller is not communicating
                    }
                }

                RenderWeirdGradient(GlobalBackBuffer, XOffset, YOffset);

                // NOTE(brad): DirectSound output test
                DWORD PlayCursor;
                DWORD WriteCursor; 

                if(SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor)))
                {
                    DWORD ByteToLock = (SoundOutput.RunningSampleIndex*SoundOutput.BytesPerSample) % SoundOutput.SecondaryBufferSize;
                    DWORD BytesToWrite;
                    // TODO(brad): Change this to a lower latency offset from the playcursor
                    if(ByteToLock == PlayCursor)
                    {
                        BytesToWrite = 0;
                    }
                    else if (ByteToLock > PlayCursor)
                    {
                        BytesToWrite = (SoundOutput.SecondaryBufferSize - ByteToLock);
                        BytesToWrite += PlayCursor;
                    }
                    else
                    {
                        BytesToWrite = PlayCursor - ByteToLock;
                    }

                    Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite);
                }

                win32_window_dimension Dimension = Win32GetWindowDimension(Window);
                Win32DisplayBufferInWindow(GlobalBackBuffer, DeviceContext, Dimension.Width, Dimension.Height, 0, 0, Dimension.Width, Dimension.Height);

                int64_t EndCycleCount = __rdtsc();

                LARGE_INTEGER EndCounter;
                QueryPerformanceCounter(&EndCounter);

                int64_t CyclesElapsed = EndCycleCount - LastCycleCount;
                int64_t CounterElapsed = EndCounter.QuadPart - LastCounter.QuadPart;
                int32_t MSPerFrame = (int32_t)((1000*CounterElapsed) / PerfCounterFrequency);
                //                int32_t FPS = 1000/MSPerFrame;
                int32_t FPS = PerfCounterFrequency / CounterElapsed;
                int32_t MCPF = (int32_t)(CyclesElapsed / 1000 / 1000);

                char Buffer[256];
                wsprintf(Buffer, "Performance: %dms / %dFPS / %d Megacycles\n", MSPerFrame, FPS, MCPF);
                OutputDebugStringA(Buffer);

                LastCounter = EndCounter;
                LastCycleCount = EndCycleCount;
            }
        }
        else
        {
            // TODO(brad): Diagnostic) 
        }
    }
    else
    {
        // TODO(brad): Diagnostic) 
    }

    return(0);
}
