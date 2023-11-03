#include "Utils/Logger.h"
#include "OSDevice.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>

#include <unistd.h>
#include <errno.h>

#include "Storage/Slotmap.h"

using namespace BB;

typedef FreeListAllocator_t OSAllocator_t;
typedef LinearAllocator_t OSTempAllocator_t;

OSAllocator_t OSAllocator{ mbSize * 8 };
OSTempAllocator_t OSTempAllocator{ mbSize * 4 };

static OSDevice osDevice;

//The OS window for Windows.
class OSWindow
{
public:
	OSWindow(OS_WINDOW_STYLE a_style, int a_X, int a_Y, int a_width, int a_height, const char* a_window_name)
	{
		windowName = a_window_name;

		display = XOpenDisplay(NULL);
		BB_ASSERT(display != NULL, "Linux XOpenDisplay failed!");
		screen = DefaultScreen(display);

		uint64_t t_Foreground_color = WhitePixel(display, screen);
		uint64_t t_Background_color = BlackPixel(display, screen);

		//Initialize the window with a white foreground and a black background.
		window = XCreateSimpleWindow(display,
			DefaultRootWindow(display),
			a_X,
			a_Y,
			a_width,
			a_height,
			5,
			t_Foreground_color,
			t_Background_color);

		switch (a_style)
		{
		case BB::OS_WINDOW_STYLE::MAIN:
			// TO DO
			break;
		case BB::OS_WINDOW_STYLE::CHILD:
			//TO DO
			break;
		default:
			BB_ASSERT(false, "Tried to create a window with a OS_WINDOW_STYLE it does not accept.");
			break;
		}

		//Set window properties
		XSetStandardProperties(display, window, a_window_name, "EXAMPLE", None, NULL, 0, NULL);

		//The input rules on what is allowed in the input.
		XSelectInput(display, window, ExposureMask | ButtonPressMask | KeyPressMask);

		//Create the graphics context
		graphicContext = XCreateGC(display, window, 0, 0);

		//Set the foreground and background. Wtf is x11.
		XSetForeground(display, graphicContext, t_Foreground_color);
		XSetBackground(display, graphicContext, t_Background_color);

		/* clear the window and bring it on top of the other windows */
		XClearWindow(display, window);
		XMapRaised(display, window);

	}

	~OSWindow()
	{
		XFreeGC(display, graphicContext);
		XDestroyWindow(display, window);
		XCloseDisplay(display);
	}

	_XDisplay* display; //XDisplay, also known as Window.
	const char* windowName;
	int screen; //the current user window.
	XID window; //X11 window ID.
	_XGC* graphicContext; //Graphic context of X11
};

struct BB::OSDevice_o
{
	//Special array for all the windows. Stored seperately 
	Slotmap<OSWindow> OSWindows{ OSAllocator, 8 };	
};

OSDevice& BB::AppOSDevice()
{
	return osDevice;
}

OSDevice::OSDevice()
{
	m_OSDevice = BBnew<OSDevice_o>(OSAllocator);
}

OSDevice::~OSDevice()
{
	BBfree(OSAllocator, m_OSDevice);
}

const size_t BB::OSDevice::VirtualMemoryPageSize() const
{
	return sysconf(_SC_PAGE_SIZE);
}

const size_t BB::OSDevice::VirtualMemoryMinimumAllocation() const
{
	return sysconf(_SC_PAGE_SIZE);
}

const uint32_t OSDevice::LatestOSError() const
{
	return static_cast<uint32_t>(errno);
}

WindowHandle OSDevice::CreateOSWindow(OS_WINDOW_STYLE a_style, int a_X, int a_Y, int a_width, int a_height, const char* a_window_name)
{
	return WindowHandle(static_cast<uint32_t>(m_OSDevice->OSWindows.emplace(a_style, a_X, a_Y, a_width, a_height, a_window_name)));
}

void BB::OSDevice::DestroyOSWindow(WindowHandle a_handle)
{
	m_OSDevice->OSWindows.erase(a_handle.index);
}

void OSDevice::ExitApp() const
{
	exit(EXIT_FAILURE);
}

bool BB::OSDevice::ProcessMessages() const
{
	KeySym t_Key;
	char t_Text[255];

	for (auto it = m_OSDevice->OSWindows.begin(); it < m_OSDevice->OSWindows.end(); it++)
	{
		if (it->value.display == nullptr)
		{
			continue;
		}

		while (XPending(it->value.display))
		{
			XEvent event;
			XNextEvent(it->value.display, &event);

			switch (event.type)
			{
			case Expose:

				break;
			case KeyPress:

				break;
			}
		}
	}

	return true;
}