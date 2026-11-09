#include "src/native_chrome.h"

#include <algorithm>
#include <fstream>
#include <vector>

#include "core/data_paths.h"
#include "include/cef_image.h"
#include "include/views/cef_textfield.h"
#include "include/views/cef_window.h"
#include "src/browser_window.h"

namespace nexus {
namespace {
constexpr cef_color_t kBackground = 0xFF111315; constexpr cef_color_t kText = 0xFFF1F3F5;
CefRefPtr<CefImage> BrandImage() {
  std::ifstream stream(core::ExecutableDirectory() / L"ui" / L"newtab" / L"nexus-logo-20.png", std::ios::binary);
  if (!stream) {
    stream.open(core::ExecutableDirectory() / L"ui" / L"newtab" / L"nexus-logo.png", std::ios::binary);
  }
  if (!stream) return nullptr;
  const std::vector<char> bytes((std::istreambuf_iterator<char>(stream)), {});
  auto image = CefImage::CreateImage();
  return !bytes.empty() && image->AddPNG(1.0F, bytes.data(), bytes.size()) ? image : nullptr;
}
}
NativeChrome::NativeChrome(BrowserWindow* owner) : owner_(owner) {}
CefRefPtr<CefLabelButton> NativeChrome::Button(const char* text, int id) {
  auto button = CefLabelButton::CreateLabelButton(this, text);
  button->SetID(id); button->SetFontList("Segoe UI, Bold 13px");
  button->SetEnabledTextColors(kText); button->SetBackgroundColor(kBackground); return button;
}
void NativeChrome::Attach(CefRefPtr<CefWindow> window) {
  panel_ = CefPanel::CreatePanel(nullptr);
  panel_->SetBackgroundColor(kBackground);
  window->AddChildView(panel_);

  CefBoxLayoutSettings settings;
  settings.horizontal = 1;
  settings.inside_border_horizontal_spacing = 8;
  settings.inside_border_vertical_spacing = 8;
  settings.between_child_spacing = 6;
  layout_ = panel_->SetToBoxLayout(settings);

  brand_ = Button("NEXUS", 1);
  if (auto logo = BrandImage()) brand_->SetImage(CEF_BUTTON_STATE_NORMAL, logo);
  new_tab_ = Button("+", 2);
  back_ = Button("‹", 3);
  forward_ = Button("›", 4);
  reload_ = Button("↻", 5);
  dashboard_ = Button("Dashboard", 6);

  address_ = CefTextfield::CreateTextfield(this);
  address_->SetPlaceholderText("Search Google or enter address");
  address_->SetAccessibleName("Address bar");
  address_->SetBackgroundColor(0xFF202429);
  address_->SetFontList("Segoe UI, 14px");

  panel_->AddChildView(brand_);
  panel_->AddChildView(new_tab_);
  panel_->AddChildView(back_);
  panel_->AddChildView(forward_);
  panel_->AddChildView(reload_);
  panel_->AddChildView(address_);
  panel_->AddChildView(dashboard_);

  if (layout_) {
    layout_->SetFlexForView(address_, 1);
  }
}
void NativeChrome::Layout(int width, int height) {
  if (!panel_) return;
  panel_->SetBounds(CefRect(0, 0, std::max(1, width), std::min(kHeight, std::max(1, height))));
  panel_->Layout();
}
void NativeChrome::Sync(const std::string& url, bool loading, bool back, bool forward) {
  if (!address_) return; if (!address_->HasFocus()) address_->SetText(url); back_->SetEnabled(back); forward_->SetEnabled(forward); reload_->SetText(loading ? "■" : "↻");
}
void NativeChrome::FocusAddress() { if (address_) { address_->RequestFocus(); address_->SelectAll(false); } }
void NativeChrome::OnButtonPressed(CefRefPtr<CefButton> button) {
  if (!owner_ || !button) return; switch (button->GetID()) {
    case 1: case 2: owner_->CreateTab("nexus://newtab/", true); owner_->FocusAddressBar(); break;
    case 3: owner_->GoBack(); break; case 4: owner_->GoForward(); break; case 5: owner_->ReloadOrStop(); break;
    case 6: owner_->SetDashboardOpen(true); break; default: break; }
}
bool NativeChrome::OnKeyEvent(CefRefPtr<CefTextfield>, const CefKeyEvent& event) {
  if (event.type == KEYEVENT_RAWKEYDOWN && event.windows_key_code == VK_RETURN) { owner_->NavigateFromAddressBar(address_->GetText().ToString()); return true; } return false;
}
}  // namespace nexus
