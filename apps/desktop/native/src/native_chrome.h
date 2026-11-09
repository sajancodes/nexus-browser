#pragma once

#include <string>

#include "include/views/cef_box_layout.h"
#include "include/views/cef_button_delegate.h"
#include "include/views/cef_label_button.h"
#include "include/views/cef_panel.h"
#include "include/views/cef_textfield.h"
#include "include/views/cef_textfield_delegate.h"
#include "include/views/cef_window.h"

namespace nexus {
class BrowserWindow;

// Native CEF Views toolbar. BrowserWindow owns browser state and lifecycle.
class NativeChrome final : public CefButtonDelegate, public CefTextfieldDelegate {
 public:
  explicit NativeChrome(BrowserWindow* owner);
  void Attach(CefRefPtr<CefWindow> window);
  void Layout(int width, int height);
  void Sync(const std::string& url, bool loading, bool back, bool forward);
  void FocusAddress();
  static constexpr int kHeight = 52;
  void OnButtonPressed(CefRefPtr<CefButton> button) override;
  bool OnKeyEvent(CefRefPtr<CefTextfield> textfield, const CefKeyEvent& event) override;
 private:
  CefRefPtr<CefLabelButton> Button(const char* text, int id);
  BrowserWindow* const owner_;
  CefRefPtr<CefPanel> panel_;
  CefRefPtr<CefBoxLayout> layout_;
  CefRefPtr<CefLabelButton> brand_, new_tab_, back_, forward_, reload_, dashboard_;
  CefRefPtr<CefTextfield> address_;
  IMPLEMENT_REFCOUNTING(NativeChrome);
  DISALLOW_COPY_AND_ASSIGN(NativeChrome);
};
}  // namespace nexus
