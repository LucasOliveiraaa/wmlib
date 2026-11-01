#pragma once

#include "wmlib/wm.h"
namespace wmlib {

struct Manager;
struct Screen;
struct Client;
struct Monitor;

using Color = unsigned long;

using NetAtom = ::WMNetAtom;
using Atom = ::WMAtom;
using ClientNode = ::WMClientNode;

using Rect = ::WMRect;
using Extents = ::WMExtents;

using Strut = ::WMStrut;

#define None__ None
#undef None

enum class WindowType : int {
  Normal = WM_WINDOW_NORMAL,
  Dialog = WM_WINDOW_DIALOG,
  Dock = WM_WINDOW_DOCK,
  Desktop = WM_WINDOW_DESKTOP,
  Splash = WM_WINDOW_SPLASH,
  Utility = WM_WINDOW_UTILITY,
  Menu = WM_WINDOW_MENU,
};

enum class EventType : int {
  KeyPressEvent = 2,
  KeyReleaseEvent = 3,
  ButtonPressEvent = 4,
  ButtonReleaseEvent = 5,
  MotionNotifyEvent = 6,
  EnterNotifyEvent = 7,
  LeaveNotifyEvent = 8,
  FocusInEvent = 9,
  FocusOutEvent = 10,
  KeymapNotifyEvent = 11,
  ExposeEvent = 12,
  GraphicsExposeEvent = 13,
  NoExposeEvent = 14,
  VisibilityNotifyEvent = 15,
  CreateNotifyEvent = 16,
  DestroyNotifyEvent = 17,
  UnmapNotifyEvent = 18,
  MapNotifyEvent = 19,
  MapRequestEvent = 20,
  ReparentNotifyEvent = 21,
  ConfigureNotifyEvent = 22,
  ConfigureRequestEvent = 23,
  GravityNotifyEvent = 24,
  ResizeRequestEvent = 25,
  CirculateNotifyEvent = 26,
  CirculateRequestEvent = 27,
  PropertyNotifyEvent = 28,
  SelectionClearEvent = 29,
  SelectionRequestEvent = 30,
  SelectionNotifyEvent = 31,
  ColormapNotifyEvent = 32,
  ClientMessageEvent = 33,
  MappingNotifyEvent = 34,
  GenericEventType = 35,
};


enum class Source : int {
  Application = WM_SOURCE_APPLICATION,
  Pager = WM_SOURCE_PAGER,
  None = WM_SOURCE_NONE,
};

enum class MoveResizeDirection : int {
  SizeTop = WM_MOVERESIZE_SIZE_TOP,
  SizeBottom = WM_MOVERESIZE_SIZE_BOTTOM,
  SizeLeft = WM_MOVERESIZE_SIZE_LEFT,
  SizeRight = WM_MOVERESIZE_SIZE_RIGHT,
  SizeTopLeft = WM_MOVERESIZE_SIZE_TOPLEFT,
  SizeTopRight = WM_MOVERESIZE_SIZE_TOPRIGHT,
  SizeBottomLeft = WM_MOVERESIZE_SIZE_BOTTOMLEFT,
  SizeBottomRight = WM_MOVERESIZE_SIZE_BOTTOMRIGHT,
  Move = WM_MOVERESIZE_MOVE,
  SizeKeyboard = WM_MOVERESIZE_SIZE_KEYBOARD,
  MoveKeyboard = WM_MOVERESIZE_MOVE_KEYBOARD,
  Cancel = WM_MOVERESIZE_CANCEL,
};

#define None None__
#undef None__

} // namespace wmlib