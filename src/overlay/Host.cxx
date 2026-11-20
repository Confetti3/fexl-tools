#include "overlay/Host.hxx"

#include "Detour.hxx"
#include "Log.hxx"

#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include <atomic>
#include <vector>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND window, UINT message, WPARAM w, LPARAM l);

namespace fexl::overlay {

namespace {

using PresentFn = HRESULT(WINAPI*)(IDXGISwapChain*, UINT, UINT);
PresentFn o_present;

// Render thread.
OverlayUi s_ui;
ID3D11Device* s_device;
ID3D11DeviceContext* s_context;
HWND s_window;
bool s_ready;
float s_scale;

// Shared with the game thread's window procedure.
std::atomic<WNDPROC> s_gameWndProc{nullptr};
std::atomic<bool> s_captured{false};
std::atomic<bool> s_enabled{false};
SRWLOCK s_queueLock = SRWLOCK_INIT;
struct Message {
  UINT message;
  WPARAM w;
  LPARAM l;
};
std::vector<Message> s_queue;

bool KeyboardMessage(UINT m) { return m == WM_KEYDOWN || m == WM_KEYUP || m == WM_SYSKEYDOWN || m == WM_SYSKEYUP || m == WM_CHAR; }
bool MouseMessage(UINT m) { return m >= WM_MOUSEFIRST && m <= WM_MOUSELAST; }
bool FocusMessage(UINT m) { return m == WM_SETFOCUS || m == WM_KILLFOCUS; }

// While the interface has the input (and still may: OverlayUi::mayTakeInput), the game keeps
// what it must not lose: every release (so no key or button stays held in the game), the system
// keys (Alt+F4, Alt+Enter) and the F keys.
bool KeptFromGame(UINT m, WPARAM w) {
  if (!s_captured.load() || !s_ui.mayTakeInput()) return false;
  if (m == WM_KEYDOWN) return w < VK_F1 || w > VK_F12;
  if (m == WM_CHAR) return true;
  if (MouseMessage(m)) return m != WM_LBUTTONUP && m != WM_RBUTTONUP && m != WM_MBUTTONUP && m != WM_XBUTTONUP;
  return false;
}

LRESULT CALLBACK WndProc(HWND window, UINT message, WPARAM w, LPARAM l) {
  if (KeyboardMessage(message) || MouseMessage(message) || FocusMessage(message)) {
    AcquireSRWLockExclusive(&s_queueLock);
    // Moves only matter as the last position: a run of them is kept as one, so a long frame
    // never pushes out a key or button release.
    if (message == WM_MOUSEMOVE && !s_queue.empty() && s_queue.back().message == WM_MOUSEMOVE) s_queue.back() = Message{message, w, l};
    else s_queue.push_back(Message{message, w, l});
    ReleaseSRWLockExclusive(&s_queueLock);
    if (KeptFromGame(message, w)) return 0;
  }
  return CallWindowProcW(s_gameWndProc.load(), window, message, w, l);
}

// The interface is laid out in the swap chain's pixels; Windows gives the mouse in the window's.
// They differ when the game renders below the window's size.
struct PixelScale {
  float x = 1, y = 1;
};

PixelScale WindowToBuffer(const DXGI_SWAP_CHAIN_DESC& desc) {
  RECT client{};
  if (!GetClientRect(s_window, &client) || client.right <= 0 || client.bottom <= 0) return {};
  return {float(desc.BufferDesc.Width) / float(client.right), float(desc.BufferDesc.Height) / float(client.bottom)};
}

void ReplayInput(PixelScale scale) {
  std::vector<Message> messages;
  AcquireSRWLockExclusive(&s_queueLock);
  messages.swap(s_queue);
  ReleaseSRWLockExclusive(&s_queueLock);
  for (Message m : messages) {
    if (m.message == WM_MOUSEMOVE) {
      const int x = int(float(short(LOWORD(m.l))) * scale.x), y = int(float(short(HIWORD(m.l))) * scale.y);
      m.l = MAKELPARAM(x, y);
    }
    ImGui_ImplWin32_WndProcHandler(s_window, m.message, m.w, m.l);
  }
  // The backend reads modifiers with GetKeyState, which only knows the calling thread's keys.
  ImGuiIO& io = ImGui::GetIO();
  io.AddKeyEvent(ImGuiMod_Ctrl, (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0);
  io.AddKeyEvent(ImGuiMod_Shift, (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0);
  io.AddKeyEvent(ImGuiMod_Alt, (GetAsyncKeyState(VK_MENU) & 0x8000) != 0);
}

bool Attach(IDXGISwapChain* swap) {
  DXGI_SWAP_CHAIN_DESC desc{};
  if (FAILED(swap->GetDesc(&desc)) || !desc.OutputWindow) return false;
  if (FAILED(swap->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&s_device)))) return false;
  s_device->GetImmediateContext(&s_context);
  s_window = desc.OutputWindow;
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.LogFilename = nullptr;
  ImGui_ImplWin32_Init(s_window);
  ImGui_ImplDX11_Init(s_device, s_context);
  s_gameWndProc.store(reinterpret_cast<WNDPROC>(GetWindowLongPtrW(s_window, GWLP_WNDPROC)));
  SetWindowLongPtrW(s_window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&WndProc));
  Log("overlay attached: window %p, %ux%u", static_cast<void*>(s_window), desc.BufferDesc.Width, desc.BufferDesc.Height);
  return true;
}

// A new picture size gives the interface its fonts and style at that size; the atlas is then
// uploaded again by the next frame.
float ApplyScale(const DXGI_SWAP_CHAIN_DESC& desc) {
  const float scale = float(desc.BufferDesc.Height) / 1080.0f;
  if (scale != s_scale) {
    s_scale = scale;
    s_ui.applyScale(scale);
    ImGui_ImplDX11_InvalidateDeviceObjects();
  }
  return scale;
}

void BeginFrame(const DXGI_SWAP_CHAIN_DESC& desc, PixelScale toBuffer) {
  ImGui_ImplDX11_NewFrame();
  ImGui_ImplWin32_NewFrame();
  ImGuiIO& io = ImGui::GetIO();
  io.DisplaySize = ImVec2(float(desc.BufferDesc.Width), float(desc.BufferDesc.Height));
  POINT cursor;
  if (GetCursorPos(&cursor) && ScreenToClient(s_window, &cursor))
    io.AddMousePosEvent(float(cursor.x) * toBuffer.x, float(cursor.y) * toBuffer.y);
}

void DrawFrame(IDXGISwapChain* swap) {
  if (!s_ready) s_ready = Attach(swap);
  if (!s_ready) return;
  DXGI_SWAP_CHAIN_DESC desc{};
  const bool described = SUCCEEDED(swap->GetDesc(&desc)) && desc.OutputWindow == s_window;
  const PixelScale toBuffer = described ? WindowToBuffer(desc) : PixelScale{};
  ReplayInput(toBuffer);  // always, so nothing queues up while a frame is skipped
  if (!described) return;
  const float scale = ApplyScale(desc);
  BeginFrame(desc, toBuffer);
  ImGui::NewFrame();
  const bool captured = s_ui.draw(scale, GetForegroundWindow() == s_window);
  s_captured.store(captured);
  ImGui::GetIO().MouseDrawCursor = captured;
  ImGui::Render();
  // Nothing drawn (any mode but Training): the game's device context is left alone.
  if (ImGui::GetDrawData()->TotalVtxCount == 0) return;
  ID3D11Texture2D* back = nullptr;
  if (FAILED(swap->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&back)))) return;
  ID3D11RenderTargetView* target = nullptr;
  const HRESULT made = s_device->CreateRenderTargetView(back, nullptr, &target);
  back->Release();
  if (FAILED(made)) return;
  ID3D11RenderTargetView* oldTarget = nullptr;
  ID3D11DepthStencilView* oldDepth = nullptr;
  s_context->OMGetRenderTargets(1, &oldTarget, &oldDepth);
  s_context->OMSetRenderTargets(1, &target, nullptr);
  ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
  s_context->OMSetRenderTargets(1, &oldTarget, oldDepth);
  if (oldTarget) oldTarget->Release();
  if (oldDepth) oldDepth->Release();
  target->Release();
}

HRESULT WINAPI Present(IDXGISwapChain* swap, UINT syncInterval, UINT flags) {
  if (!(flags & DXGI_PRESENT_TEST) && s_enabled.load()) DrawFrame(swap);
  return o_present(swap, syncInterval, flags);
}

// Present is the same code for every swap chain: its address read from a throwaway one.
void* FindPresent() {
  HWND window = CreateWindowExW(0, L"STATIC", L"fexl-probe", WS_OVERLAPPED, 0, 0, 8, 8, nullptr, nullptr, nullptr, nullptr);
  if (!window) return nullptr;
  DXGI_SWAP_CHAIN_DESC desc{};
  desc.BufferCount = 1;
  desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  desc.OutputWindow = window;
  desc.SampleDesc.Count = 1;
  desc.Windowed = TRUE;
  desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
  IDXGISwapChain* swap = nullptr;
  ID3D11Device* device = nullptr;
  ID3D11DeviceContext* context = nullptr;
  void* present = nullptr;
  if (SUCCEEDED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION,
                                              &desc, &swap, &device, nullptr, &context)))
    present = (*reinterpret_cast<void***>(swap))[8];  // IDXGISwapChain::Present
  if (swap) swap->Release();
  if (context) context->Release();
  if (device) device->Release();
  DestroyWindow(window);
  return present;
}

}  // namespace

bool InstallHost(OverlayUi ui) {
  s_ui = std::move(ui);
  for (int attempt = 0; attempt < 5; ++attempt) {
    if (attempt > 0) Sleep(2000);
    if (void* present = FindPresent())
      return HookFunction("present", present, reinterpret_cast<void*>(&Present), reinterpret_cast<void**>(&o_present));
  }
  Log("present: no Direct3D 11 swap chain");
  return false;
}

void EnableHost() { s_enabled.store(true); }

}  // namespace fexl::overlay
