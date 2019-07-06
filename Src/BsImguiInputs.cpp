#include "imgui.h"
#include "ImGuizmo.h"
#include "./BsImgui.h"

#include "BsPrerequisites.h"
#include "Input/BsInputFwd.h"
#include "Platform/BsPlatform.h"
#include "Platform/BsCursor.h"
#include "Input/BsInput.h"
#include "BsApplication.h"


// utility operator for osstream debugging.
std::ostream& operator<<(std::ostream& s, const ImVec2& vec) {
	s << vec.x << " " << vec.y;
	return s;
}

namespace bs {

// forward declare
void updateImguiInputs();

enum BsfClientApi {
  BsfClientApi_Unknown,
  BsfClientApi_OpenGL,
  BsfClientApi_Vulkan
};

// static bool g_MousePressed[3] = {false, false, false};
static CursorType g_MouseCursors[ImGuiMouseCursor_COUNT] = {};
static float g_Time = 0.0;

// kinda a hack to copy our local mouswheel to the io at a specific time
// instead of as part of event since it keeps getting reset between frames...
// if other inputs are failing to be captured, we'll likely need to store
// local static variable, and then copy over during the imguiUpdateInputs
// which is called before the start of each frame. *shrug*.
static float gMouseWheel = 0.f;

static INT32 displayTop{0};
static INT32 displayLeft{0};

static const char* ImGui_ImplBsf_GetClipboardText(void* user_data) {
  return Platform::copyFromClipboard().c_str();
}

static void ImGui_ImplBsf_SetClipboardText(void* user_data, const char* text) {
  Platform::copyToClipboard(text);
}

void connectInputs();

static void initCursorMap() {
  g_MouseCursors[ImGuiMouseCursor_Arrow] = CursorType::Arrow;
  g_MouseCursors[ImGuiMouseCursor_TextInput] = CursorType::IBeam;
  g_MouseCursors[ImGuiMouseCursor_ResizeAll] = CursorType::ArrowDrag;
  g_MouseCursors[ImGuiMouseCursor_ResizeNS] = CursorType::SizeNS;
  g_MouseCursors[ImGuiMouseCursor_ResizeEW] = CursorType::SizeWE;
  g_MouseCursors[ImGuiMouseCursor_ResizeNESW] = CursorType::SizeNESW;
  g_MouseCursors[ImGuiMouseCursor_ResizeNWSE] = CursorType::SizeNWSE;
  g_MouseCursors[ImGuiMouseCursor_Hand] = CursorType::Deny;
}

void initInputs() {	
  // Setup back-end capabilities flags
  ImGuiIO& io = ImGui::GetIO();
  io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;  // We can honor
                                                         // GetMouseCursor()
                                                         // values (optional)
  io.BackendFlags |=
      ImGuiBackendFlags_HasSetMousePos;  // We can honor io.WantSetMousePos
                                         // requests (optional, rarely used)
  io.BackendPlatformName = "imgui_impl_bsf";

  // Keyboard mapping. ImGui will use those indices to peek into the
  // io.KeysDown[] array.
  io.KeyMap[ImGuiKey_Tab] = BC_TAB;
  io.KeyMap[ImGuiKey_LeftArrow] = BC_LEFT;
  io.KeyMap[ImGuiKey_RightArrow] = BC_RIGHT;
  io.KeyMap[ImGuiKey_UpArrow] = BC_UP;
  io.KeyMap[ImGuiKey_DownArrow] = BC_DOWN;
  io.KeyMap[ImGuiKey_PageUp] = BC_PGUP;
  io.KeyMap[ImGuiKey_PageDown] = BC_PGDOWN;
  io.KeyMap[ImGuiKey_Home] = BC_HOME;
  io.KeyMap[ImGuiKey_End] = BC_END;
  io.KeyMap[ImGuiKey_Insert] = BC_INSERT;
  io.KeyMap[ImGuiKey_Delete] = BC_DELETE;
  io.KeyMap[ImGuiKey_Backspace] = BC_BACK;
  io.KeyMap[ImGuiKey_Space] = BC_SPACE;
  io.KeyMap[ImGuiKey_Enter] = BC_RETURN;
  io.KeyMap[ImGuiKey_Escape] = BC_ESCAPE;
  io.KeyMap[ImGuiKey_A] = BC_A;
  io.KeyMap[ImGuiKey_C] = BC_C;
  io.KeyMap[ImGuiKey_V] = BC_V;
  io.KeyMap[ImGuiKey_X] = BC_X;
  io.KeyMap[ImGuiKey_Y] = BC_Y;
  io.KeyMap[ImGuiKey_Z] = BC_Z;

  io.SetClipboardTextFn = ImGui_ImplBsf_SetClipboardText;
  io.GetClipboardTextFn = ImGui_ImplBsf_GetClipboardText;

  initCursorMap();
  connectInputs();
}

bool initImgui() {
	initInputs();
  updateImguiInputs();


  ImGui::NewFrame();
  // make empty render.
  ImGui::Render();

  ImGui::NewFrame();
  ImGuizmo::BeginFrame();


  return true;
}

static void ImGui_ImplBsf_UpdateMouseCursor() {
  ImGuiIO& io = ImGui::GetIO();
  if ((io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange)) return;

  ImGuiMouseCursor imgui_cursor = ImGui::GetMouseCursor();
  if (imgui_cursor == ImGuiMouseCursor_None || io.MouseDrawCursor) {
    // Hide OS mouse cursor if imgui is drawing it or if it wants no cursor
    gCursor().hide();
  } else {
    // Show OS mouse cursor
    gCursor().setCursor(g_MouseCursors[imgui_cursor]);
    gCursor().show();
  }

  // kinda a hack to copy our local mouswheel to the io
  // at a specific time instead of as part of event
  // since it keeps getting reset between frames...
  io.MouseWheel = gMouseWheel;
  gMouseWheel = 0.f;
}

static HEvent g_OnPointerMovedConn;
static HEvent g_OnPointerPressedConn;
static HEvent g_OnPointerReleasedConn;
static HEvent g_OnPointerDoubleClick;
static HEvent g_OnTextInputConn;
static HEvent g_OnButtonUp;
static HEvent g_OnButtonDown;


void onPointerMoved(const PointerEvent& event) {
  ImGuiIO& io = ImGui::GetIO();
  io.MousePos.x = event.screenPos.x - displayLeft;
  io.MousePos.y = event.screenPos.y - displayTop;
  if (event.mouseWheelScrollAmount > 0) {
    gMouseWheel += 1;
  } else if (event.mouseWheelScrollAmount < 0) {
    gMouseWheel -= 1;
  }
}

void onPointerPressed(const PointerEvent& event) {
  ImGuiIO& io = ImGui::GetIO();
  io.MouseDown[0] = event.buttonStates[0];
  io.MouseDown[1] = event.buttonStates[1];
  io.MouseDown[2] = event.buttonStates[2];
}
void onPointerReleased(const PointerEvent& event) {
  ImGuiIO& io = ImGui::GetIO();
  io.MouseDown[0] = event.buttonStates[0];
  io.MouseDown[1] = event.buttonStates[1];
  io.MouseDown[2] = event.buttonStates[2];
}
void onPointerDoubleClick(const PointerEvent& event) {
	// doesn't seem to be necessary to track, since imgui has its own logic for
	// identifying double-clicks I think.
}

void onCharInput(const TextInputEvent& event) {
  // io.KeysDown[]
  ImGuiIO& io = ImGui::GetIO();
  io.AddInputCharacter(event.textChar);
}

void onButtonUp(const ButtonEvent& event) {
  ImGuiIO& io = ImGui::GetIO();
  if (!event.isKeyboard()) return;
  if (event.isUsed()) return;
  ButtonCode key = event.buttonCode;
  IM_ASSERT(key >= 0 && key < IM_ARRAYSIZE(io.KeysDown));
  io.KeysDown[key] = false;

	if(event.buttonCode == BC_LSHIFT || event.buttonCode == BC_RSHIFT) {
		io.KeyShift = false;
	}
	if(event.buttonCode == BC_LCONTROL || event.buttonCode == BC_RCONTROL) {
		io.KeyCtrl = false;
	}
}

void onButtonDown(const ButtonEvent& event) {
  ImGuiIO& io = ImGui::GetIO();
  if (!event.isKeyboard()) return;
  if (event.isUsed()) return;
  ButtonCode key = event.buttonCode;
  IM_ASSERT(key >= 0 && key < IM_ARRAYSIZE(io.KeysDown));
  io.KeysDown[key] = true;


	if(event.buttonCode == BC_LSHIFT || event.buttonCode == BC_RSHIFT) {
		io.KeyShift = true;
	}
	if(event.buttonCode == BC_LCONTROL || event.buttonCode == BC_RCONTROL) {
		io.KeyCtrl = true;
	}
}

void disconnectImgui() {
	g_OnPointerMovedConn.disconnect();
	g_OnPointerPressedConn.disconnect();
	g_OnPointerReleasedConn.disconnect();
	g_OnPointerDoubleClick.disconnect();
	g_OnTextInputConn.disconnect();
	g_OnButtonUp.disconnect();
	g_OnButtonDown.disconnect();
}

void connectInputs() {
  g_OnPointerMovedConn = gInput().onPointerMoved.connect(&onPointerMoved);
  g_OnPointerPressedConn = gInput().onPointerPressed.connect(&onPointerPressed);
  g_OnPointerReleasedConn =
      gInput().onPointerReleased.connect(&onPointerReleased);
  g_OnPointerDoubleClick =
      gInput().onPointerDoubleClick.connect(&onPointerDoubleClick);
  g_OnTextInputConn = gInput().onCharInput.connect(&onCharInput);
  g_OnButtonUp = gInput().onButtonUp.connect(&onButtonUp);
  g_OnButtonDown = gInput().onButtonDown.connect(&onButtonDown);
}


void updateImguiInputs() {
  ImGuiIO& io = ImGui::GetIO();
  IM_ASSERT(io.Fonts->IsBuilt() &&
            "Font atlas not built! It is generally built by the renderer "
            "back-end. Missing call to renderer _NewFrame() function? e.g. "
            "ImGui_ImplOpenGL3_NewFrame().");

  int w, h;
  int display_w, display_h;

  SPtr<RenderWindow> primaryWindow = gCoreApplication().getPrimaryWindow();
  const auto& properties = primaryWindow->getProperties();
  w = properties.width;
  h = properties.height;
  // get frame buffer size ?
  display_w = properties.width;
  display_h = properties.height;

  displayLeft = properties.left;
  displayTop = properties.top;

  io.DisplaySize = ImVec2((float)w, (float)h);

  // update imguizmo's rectangle reference.
	ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);

  if (w > 0 && h > 0)
    io.DisplayFramebufferScale =
        ImVec2((float)display_w / w, (float)display_h / h);
  float current_time = gTime().getTime();
  // std::cout << "current time " << current_time << " " << g_Time << std::endl;
  io.DeltaTime =
      g_Time > 0.0 ? (current_time - g_Time) : (1.0f / 60.0f);
  if (io.DeltaTime == 0.0) io.DeltaTime = 1.f / 60.f;
  g_Time = current_time;

  ImGui_ImplBsf_UpdateMouseCursor();
}

}  // namespace bs