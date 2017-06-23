#include <windows.h>
#include <stdint.h>
#include <xinput.h>
#include <dsound.h>
#include <math.h>

#define local_persist static
#define global_variable static
#define internal static
#define Pi32 3.14159265359f

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef float real32;
typedef double real64;

typedef int32 bool32;

struct win32_offscreen_buffer
{
	BITMAPINFO Info;
	void *Memory;
	int Width;
	int Height;
	int BytesPerPixel;
	int Pitch;
};

//if Running is true, entire game is running.
global_variable bool Running;
//holds the pixels in memory
global_variable win32_offscreen_buffer GlobalBackbuffer;
//sound buffer
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;


struct win32_window_dimension
{
	int Width;
	int Height;
};

#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_GET_STATE(x_input_get_state);
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{ 
	return (ERROR_DEVICE_NOT_CONNECTED);
}
X_INPUT_SET_STATE(XInputSetStateStub)
{
	return (ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;
global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputGetState XInputGetState_
#define XInputSetState XInputSetState_

#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);
//loads XInput library and imports functions XInputGetState and XInputSetState
internal void
Win32LoadXInput(void)
{
	HMODULE XInputLibrary = LoadLibraryA("xinput1_4.dll");
	if(!XInputLibrary)
	{
		HMODULE XInputLibrary = LoadLibraryA("xinput1_3.dll");
	}
	if (XInputLibrary)
	{
		XInputGetState = (x_input_get_state *)GetProcAddress(XInputLibrary, "XInputGetState");
		XInputSetState = (x_input_set_state *)GetProcAddress(XInputLibrary, "XInputSetState");
	}
}

//loads Direct Sound
internal void
Win32InitDSound(HWND Window, int32 SamplesPerSecond, int32 BufferSize)
{
	//load library
	HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");

	if(DSoundLibrary)
	{
		//get DirectSound Object
		direct_sound_create *DirectSoundCreate = (direct_sound_create *)GetProcAddress(DSoundLibrary, "DirectSoundCreate");
		LPDIRECTSOUND DirectSound;

		if(DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0,DirectSound,0)))
		{
			//create primary dummy buffer to set mode of dsound
			if(SUCCEEDED(DirectSound->SetCooperativeLevel(Window,DSSCL_PRIORITY)))
			{
				DSBUFFERDESC BufferDescription = {};
				BufferDescription.dwSize = sizeof(BufferDescription);
				BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;

				LPDIRECTSOUNDBUFFER PrimaryBuffer;
				if(SUCCEEDED(CreateSoundBuffer(&BufferDescription, &PrimaryBuffer, 0)))
				{
					WAVEFORMATEX WaveFormat = {};
					WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
					WaveFormat.nChannels = 2;
					WaveFormat.wBitsPerSample = 16;
					WaveFormat.nSamplesPerSecond = SamplesPerSecond;
					WaveFormat.nBlockAlign = (WaveFormat.nChannels*WaveFormat.wBitsPerSample) / 8;
					WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSecond*WaveFormat.nBlockAlign;
					WaveFormat.cbSize = 0;
					if(SUCCEEDED(PrimaryBuffer->SetFormat(&WaveFormat)))
					{
						OutputDebugStringA("Primary Sound Buffer Set");
					}
					else
					{

					}
				}
				else
				{

				}
			}
			else{

			}
			//create actual secondary buffer which is what we will put sound into
			DSBUFFERDESC BufferDescription = {};
			BufferDescription.dwSize = sizeof(BufferDescription);
			BufferDescription.dwFlags = 0;
			BufferDescription.dwBufferBytes = BufferSize;
			BufferDescription.lpwfxFormat = &WaveFormat;

			LPDIRECTSOUNDBUFFER SecondaryBuffer;
			if(SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &GlobalSecondaryBuffer, 0)))
			{
				//play sound
				OutputDebugStringA("Secondary Sound Buffer Created Successfully");
			}
			else{

			}
		}
		else{

		}
	}
	else{

	}
}

//returns width/height object of a window
internal win32_window_dimension Win32GetWindowDimension(HWND Window)
{
	win32_window_dimension Result;

	RECT ClientRect;
	GetClientRect(Window, &ClientRect);
	Result.Width = ClientRect.right - ClientRect.left;
	Result.Height = ClientRect.bottom - ClientRect.top;

	return Result;
}

//fills in pixels of a buffer in a specific way
internal void
RenderWeirdGradient(win32_offscreen_buffer *Buffer, int XOffset, int YOffset)
{
	//int Pitch = Width*Buffer->BytesPerPixel;
	uint8 *Row = (uint8 *)Buffer->Memory;
	for (int Y = 0; Y < Buffer->Height; ++Y) {
		uint32 *Pixel = (uint32 *)Row;
		for (int X = 0; X < Buffer->Width; ++X) {
			uint8 Red = 0;
			uint8 Green = uint8(X + XOffset);
			uint8 Blue = uint8(Y + YOffset);

			*Pixel++ = ((Red << 16) | (Green << 8) | Blue);
		}
		Row += Buffer->Pitch;
	}
}

//initializes buffer fields
internal void
Win32ResizeDIBSection(win32_offscreen_buffer *Buffer, int Width, int Height)
{
	if (Buffer->Memory)
	{
		VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
	}

	Buffer->Width = Width;
	Buffer->Height = Height;
	Buffer->BytesPerPixel = 4;

	Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
	Buffer->Info.bmiHeader.biWidth = Width;
	Buffer->Info.bmiHeader.biHeight = -Height;
	Buffer->Info.bmiHeader.biPlanes = 1;
	Buffer->Info.bmiHeader.biBitCount = 32;
	Buffer->Info.bmiHeader.biCompression = BI_RGB;

	int BitmapMemorySize = Buffer->BytesPerPixel*Buffer->Width*Buffer->Height;
	Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);
	Buffer->Pitch = Width*Buffer->BytesPerPixel;
}

//takes a buffer and puts it in a window
internal void
Win32DisplayBufferInWindow(HDC DeviceContext, int WindowWidth,int WindowHeight,
							win32_offscreen_buffer *Buffer)
{
	StretchDIBits(
		DeviceContext,
		0,0,WindowWidth,WindowHeight,
		0,0,Buffer->Width,Buffer->Height,
		Buffer->Memory,
		&Buffer->Info,
		DIB_RGB_COLORS,
		SRCCOPY);
}

//interprets interaction with a window
LRESULT CALLBACK
Win32MainWindowCallback(
	HWND Window,
	UINT Message,
	WPARAM WParam,
	LPARAM LParam)
{
	LRESULT Result = 0;
	switch (Message)
	{
		case WM_SIZE:
		{
			OutputDebugString("WM_SIZE\n");
		}break;

		case WM_CLOSE:
		{
			Running = false;
		}break;

		case WM_DESTROY:
		{
			Running = false;
		}break;

		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_KEYDOWN:
		case WM_KEYUP:
		{
			uint32 VKCode = WParam;
			bool WasDown = ((LParam & (1 << 30)) != 0);
			bool IsDown = ((LParam & (1 << 31)) == 0);
			if(WasDown != IsDown){
				if(VKCode == 'W')
				{
				}
				else if(VKCode == 'A')
				{
				}
				else if(VKCode == 'S')
				{
				}
				else if(VKCode == 'D')
				{
				}
				else if(VKCode == 'Q')
				{
				}
				else if(VKCode == 'E')
				{
				}
				else if(VKCode == VK_UP)
				{
				}
				else if(VKCode == VK_LEFT)
				{
				}
				else if(VKCode == VK_DOWN)
				{
				}
				else if(VKCode == VK_RIGHT)
				{
				}
				else if(VKCode == VK_ESCAPE)
				{
				}
				else if(VKCode == VK_SPACE)
				{
				}
			}

			bool32 AltKeyWasDown = (LParam & (1 << 29));
			if(VKCode == VK_F4 && AltKeyWasDown)
			{
				Running = false;
			}
		}break;

		case WM_ACTIVATEAPP:
		{
			OutputDebugString("WM_ACTIVATEAPP\n");
		}break;

		case WM_PAINT:
		{
			PAINTSTRUCT Paint;
			HDC DeviceContext = BeginPaint(Window, &Paint);
			int X = Paint.rcPaint.left;
			int Y = Paint.rcPaint.top;
			int Height = Paint.rcPaint.bottom - Paint.rcPaint.top;
			int Width = Paint.rcPaint.right - Paint.rcPaint.left;

			win32_window_dimension Dimension = Win32GetWindowDimension(Window);
			Win32DisplayBufferInWindow(DeviceContext, Dimension.Width,Dimension.Height, &GlobalBackbuffer);
			EndPaint(Window, &Paint);
		}break;

		default:
		{
			//OutputDebugString("DEFAULT\n");
			Result = DefWindowProc(Window, Message, WParam, LParam);
		}break;
	}
	return Result;
}

//vars for outputting a simple sine wave to the sound buffer
struct win32_sound_output
{
	int WaveFreq;
	int SamplesPerSecond;
	int WavePeriod;
	int ToneVolume;
	int BytesPerSample;
	uint32 RunningSampleIndex;
	int SecondaryBufferSize;
};

//Similar to RenderWeirdGradient for the pixel buffer, fills and plays the direct sound buffer using a test sine wave
internal void 
Win32FillSoundBuffer(win32_sound_output *SoundOutput, DWORD ByteToLock, DWORD BytesToWrite)
{
	VOID *Region1;
	DWORD Region1Size;
	VOID *Region2;
	DWORD Region2Size;
	if(SUCCEEDED(GlobalSecondaryBuffer->Lock(
		ByteToLock,
		BytesToWrite,
		&Region1,&Region1Size,
		&Region2,&Region2Size,
		0)))
	{
		int16 *SampleOut = (int16 *)Region1;
		DWORD Region1SampleCount = Region1Size / SoundOutput->BytesPerSample;
		for(DWORD SampleIndex = 0; SampleIndex < Region1SampleCount; ++SampleIndex)
		{
			real32 t = 2.0f*Pi32*(real32)SoundOutput->RunningSampleIndex / SoundOutput->WavePeriod;
			real32 SineValue = sinf(t);
			int16 SampleValue = (int16)(SineValue*SoundOutput->ToneVolume);
			*SampleOut++ = SampleValue;
			*SampleOut++ = SampleValue;
			++SoundOutput->RunningSampleIndex;
		}
		SampleOut = (int16 *)Region2;
		DWORD Region2SampleCount = Region2Size / SoundOutput->BytesPerSample;
		for(DWORD SampleIndex = 0; SampleIndex < Region2SampleCount; ++SampleIndex)
		{
			real32 t = 2.0f*Pi32*(real32)SoundOutput->RunningSampleIndex / SoundOutput->WavePeriod;
			real32 SineValue = sinf(t);
			int16 SampleValue = (int16)(SineValue*SoundOutput->ToneVolume);
			*SampleOut++ = SampleValue;
			*SampleOut++ = SampleValue;
			++SoundOutput->RunningSampleIndex;
		}
		GlobalSecondaryBuffer->Unlock(Region1, Region1Size,Region2,Region2Size);
	}
}

//PROGRAM ACTUALLY STARTS RUNNING HERE
/*     Pseudo so far:
Load XInput functions we need
Make a window which refers to Win32MainWindowCallback as its procedure
Allocate memory for pixel buffer
Initialize some vars for graphics and sound test
Load Direct Sound stuff we need
Write stuff into sound ring buffer
Actually play sound (asynchronously with everything else)
set Running to true

While Running is true:
	Get window message and dispatch it according to Win32MainWindowCallback
	Get controller input / do some testing stuff to see if that works
	Render pixels into pixel buffer (right now it's just the test pattern from RenderWeirdGradient)
	Write stuff into sound ring buffer if necessary
	Actually put the pixel buffer pixels into the window
	Print out beginning and ending times of the loop

*/
int CALLBACK
WinMain(HINSTANCE Instance,
	HINSTANCE PrevInstance,
	LPSTR     CommandLine,
	int       ShowCode)
{
	Win32LoadXInput();

	LARGE_INTEGER PerformanceFrequencyResult;
	QueryPerformanceFrequency(&PerformanceFrequencyResult);
	int64 PerformanceFrequency = PerformanceFrequencyResult.QuadPart;

	WNDCLASSA WindowClass = {};

	Win32ResizeDIBSection(&GlobalBackbuffer, 1280, 720);

	WindowClass.style = CS_HREDRAW|CS_VREDRAW;
	WindowClass.lpfnWndProc = Win32MainWindowCallback;
	WindowClass.hInstance = Instance;
	// WindowClass.hIcon;
	WindowClass.lpszClassName = "HandmadeHeroWindowClass";

	if (RegisterClass(&WindowClass))
	{
		HWND Window = 
		CreateWindowExA(
			0,
			WindowClass.lpszClassName,
			"Handmade Hero",
			WS_OVERLAPPEDWINDOW|WS_VISIBLE,
			//Casey
			// CW_USEDEFAULT,
			// CW_USEDEFAULT,
			// CW_USEDEFAULT,
			// CW_USEDEFAULT,
			//Me
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			1280,
			720,
			0,
			0,
			Instance,
			0);
		if (Window)
		{
			HDC DeviceContext = GetDC(Window);
			//graphics test vars
			int XOffset = 0;
			int YOffset = 0;
			//sound test vars
			win32_sound_output SoundOutput = {};
			SoundOutput.WaveFreq = 256;
			SoundOutput.SamplesPerSecond = 48000;
			SoundOutput.WavePeriod = SoundOutput.SamplesPerSecond / SoundOutput.WaveFreq;
			SoundOutput.ToneVolume = 5000;
			SoundOutput.BytesPerSample = sizeof(int16)*2;
			SoundOutput.RunningSampleIndex = 0;
			SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond*SoundOutput.BytesPerSample;

			Win32InitDSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
			Win32FillSoundBuffer(&SoundOutput,0,SoundOutput.SecondaryBufferSize);
			Running = true;
			LARGE_INTEGER LastCounter;
			QueryPerformanceCounter(&LastCounter);
			while(Running)
			{
				// LARGE_INTEGER BeginCounter;
				// QueryPerformanceCounter(&BeginCounter);
				MSG Message;
				while (PeekMessageA(&Message, 0, 0, 0, PM_REMOVE))
				{
					if (Message.message == WM_QUIT)
					{
						Running = false;
					}

					TranslateMessage(&Message);
					DispatchMessageA(&Message);
				}

				//get controller input 
				for(DWORD ControllerIndex = 0;ControllerIndex < XUSER_MAX_COUNT;++ControllerIndex)
				{
					XINPUT_STATE ControllerState;
					if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
					{
						XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;

						bool Up = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
						bool Down = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
						bool Left = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
						bool Right = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
						bool Start = (Pad->wButtons & XINPUT_GAMEPAD_START);
						bool Back = (Pad->wButtons & XINPUT_GAMEPAD_BACK);
						bool LeftShoulder = (Pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
						bool RightShoulder = (Pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
						bool AButton = (Pad->wButtons & XINPUT_GAMEPAD_A);
						bool BButton = (Pad->wButtons & XINPUT_GAMEPAD_B);
						bool XButton = (Pad->wButtons & XINPUT_GAMEPAD_X);
						bool YButton = (Pad->wButtons & XINPUT_GAMEPAD_Y);
						int16 StickX = Pad->sThumbLX;
						int16 StickY = Pad->sThumbLY;

						if (AButton)
						{
							YOffset += 2;
						}

						XOffset += StickX >> 12;
						YOffset += StickY >> 12;
					}
					else
					{
						//controller is not available to get input from
					}
				}
				XINPUT_VIBRATION Vibration;
				Vibration.wLeftMotorSpeed = 60000;
				Vibration.wRightMotorSpeed = 60000;
				XInputSetState(0, &Vibration);

				RenderWeirdGradient(&GlobalBackbuffer, XOffset, YOffset);

				DWORD PlayCursor;
				DWORD WriteCursor;
				if(!SoundIsPlaying &&
					SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor,&WriteCursor)))
				{
					DWORD ByteToLock = (SoundOutput.RunningSampleIndex*SoundOutput.BytesPerSample) % SoundOutput.SecondaryBufferSize;
					DWORD BytesToWrite;
					if(ByteToLock == PlayCursor)
					{
						BytesToWrite = 0;
					}
					else if(ByteToLock > PlayCursor)
					{
						BytesToWrite = (SoundOutput.SecondaryBufferSize - ByteToLock);
						BytesToWrite += PlayCursor;
					}
					else
					{
						BytesToWrite = PlayCursor - ByteToLock;
					}
					Win32FillSoundBuffer(&SoundOutput,ByteToLock, BytesToWrite);
				}

				win32_window_dimension Dimension = Win32GetWindowDimension(Window);
				Win32DisplayBufferInWindow(DeviceContext, Dimension.Width, Dimension.Height, &GlobalBackbuffer);

				LARGE_INTEGER EndCounter;
				QueryPerformanceCounter(&EndCounter);

				int64 CounterElapsed = EndCounter.QuadPart - LastCounter.QuadPart;
				int32 MillisecondsElapsed = (int32)((1000*CounterElapsed) / PerformanceFrequency));

				char Buffer[256];
				wsprintf(Buffer, "Milliseconds per frame: %d \n",MillisecondsElapsed);
				OutputDebugStringA(Buffer);

				LastCounter = EndCounter;
			}
		}
		else
		{
			//failed to create WindowHandle
		}
	}
	else {
		//failed to RegisterClass(window)
	}

	return 0;
}