#pragma once

#include <string>
#include <vector>

#include "include/views/cef_box_layout.h"
#include "include/views/cef_button_delegate.h"
#include "include/views/cef_label_button.h"
#include "include/views/cef_panel.h"
#include "include/views/cef_textfield.h"
#include "include/views/cef_textfield_delegate.h"
#include "include/views/cef_window.h"

namespace nexus {
class BrowserWindow;

// Native CEF Views browser chrome implementing a modern tab strip and toolbar.
class NativeChrome final : public CefButtonDelegate, public CefTextfieldDelegate {
 public:
  struct TabSummary {
    int id = 0;
    std::string title;
    std::string url;
    bool is_active = false;
    bool is_loading = false;
  };

  static constexpr int kTabStripHeight = 36;
  static constexpr int kToolbarHeight = 40;
  static constexpr int kHeight = kTabStripHeight + kToolbarHeight;

  explicit NativeChrome(BrowserWindow* owner);

  void Attach(CefRefPtr<CefWindow> window);
  void Layout(int width, int height);
  void Sync(const std::vector<TabSummary>& tabs,
            const std::string& active_url,
            bool loading,
            bool can_go_back,
            bool can_go_forward);
  void FocusAddress();

  // CefButtonDelegate
  void OnButtonPressed(CefRefPtr<CefButton> button) override;

  // CefTextfieldDelegate
  bool OnKeyEvent(CefRefPtr<CefTextfield> textfield, const CefKeyEvent& event) override;

 private:
  CefRefPtr<CefLabelButton> CreateButton(const char* text, int id, const char* tooltip = nullptr, int width = 0);

  BrowserWindow* const owner_;
  CefRefPtr<CefPanel> tab_strip_row_;
  CefRefPtr<CefBoxLayout> tab_strip_layout_;
  CefRefPtr<CefPanel> tabs_container_;
  CefRefPtr<CefBoxLayout> tabs_layout_;
  CefRefPtr<CefPanel> toolbar_row_;
  CefRefPtr<CefBoxLayout> toolbar_layout_;
  CefRefPtr<CefPanel> address_pill_;
  CefRefPtr<CefBoxLayout> address_layout_;

  CefRefPtr<CefLabelButton> brand_btn_;
  CefRefPtr<CefLabelButton> new_tab_btn_;
  CefRefPtr<CefLabelButton> back_btn_;
  CefRefPtr<CefLabelButton> forward_btn_;
  CefRefPtr<CefLabelButton> reload_btn_;
  CefRefPtr<CefLabelButton> bookmark_btn_;
  CefRefPtr<CefLabelButton> lock_icon_;
  CefRefPtr<CefTextfield> address_input_;
  CefRefPtr<CefLabelButton> shield_badge_;
  CefRefPtr<CefLabelButton> dashboard_btn_;
  CefRefPtr<CefLabelButton> menu_btn_;

  struct RenderedTab {
    int tab_id = 0;
    CefRefPtr<CefLabelButton> title_btn;
    CefRefPtr<CefLabelButton> close_btn;
  };
  std::vector<RenderedTab> rendered_tabs_;

  IMPLEMENT_REFCOUNTING(NativeChrome);
  DISALLOW_COPY_AND_ASSIGN(NativeChrome);
};

}  // namespace nexus
