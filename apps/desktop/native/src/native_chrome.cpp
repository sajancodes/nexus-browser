#include "src/native_chrome.h"

#include <algorithm>
#include <fstream>
#include <vector>

#include "core/data_paths.h"
#include "include/cef_image.h"
#include "include/views/cef_box_layout.h"
#include "include/views/cef_textfield.h"
#include "include/views/cef_window.h"
#include "src/browser_window.h"

namespace nexus {
namespace {

constexpr cef_color_t kTabStripBg = 0xFF111315;
constexpr cef_color_t kActiveTabBg = 0xFF24272B;
constexpr cef_color_t kInactiveTabBg = 0xFF181B1F;
constexpr cef_color_t kToolbarBg = 0xFF24272B;
constexpr cef_color_t kAddressBg = 0xFF15171A;
constexpr cef_color_t kTextWhite = 0xFFFFFFFF;
constexpr cef_color_t kTextMuted = 0xFF9AA0A6;
constexpr cef_color_t kTextDim = 0xFF71767B;
constexpr cef_color_t kShieldColor = 0xFF38BDF8;

constexpr int kIdBrand = 1;
constexpr int kIdNewTab = 2;
constexpr int kIdBack = 3;
constexpr int kIdForward = 4;
constexpr int kIdReload = 5;
constexpr int kIdBookmark = 6;
constexpr int kIdDashboard = 7;
constexpr int kIdMenu = 8;
constexpr int kIdTabBase = 1000;
constexpr int kIdCloseBase = 2000;

CefRefPtr<CefImage> LoadBrandImage() {
  std::ifstream stream(core::ExecutableDirectory() / L"ui" / L"newtab" / L"nexus-logo-20.png", std::ios::binary);
  if (!stream) {
    stream.open(core::ExecutableDirectory() / L"ui" / L"newtab" / L"nexus-logo.png", std::ios::binary);
  }
  if (!stream) {
    return nullptr;
  }
  const std::vector<char> bytes((std::istreambuf_iterator<char>(stream)), {});
  if (bytes.empty()) {
    return nullptr;
  }
  auto image = CefImage::CreateImage();
  return image->AddPNG(1.0F, bytes.data(), bytes.size()) ? image : nullptr;
}

std::string TruncateTitle(const std::string& title, size_t max_len = 20) {
  if (title.empty()) {
    return "New Tab";
  }
  if (title.length() <= max_len) {
    return title;
  }
  return title.substr(0, max_len - 1) + "…";
}

CefBoxLayoutSettings MakeBoxLayoutSettings(bool horizontal,
                                           int h_spacing,
                                           int v_spacing,
                                           int child_spacing) {
  CefBoxLayoutSettings s;
  std::memset(&s, 0, sizeof(s));
  s.size = sizeof(cef_box_layout_settings_t);
  s.horizontal = horizontal ? 1 : 0;
  s.inside_border_horizontal_spacing = h_spacing;
  s.inside_border_vertical_spacing = v_spacing;
  s.inside_border_insets = {0, 0, 0, 0};
  s.between_child_spacing = child_spacing;
  s.main_axis_alignment = CEF_AXIS_ALIGNMENT_START;
  s.cross_axis_alignment = CEF_AXIS_ALIGNMENT_CENTER;
  s.minimum_cross_axis_size = 0;
  s.default_flex = 0;
  return s;
}

}  // namespace

NativeChrome::NativeChrome(BrowserWindow* owner) : owner_(owner) {}

CefRefPtr<CefLabelButton> NativeChrome::CreateButton(const char* text, int id, const char* tooltip, int width) {
  auto btn = CefLabelButton::CreateLabelButton(this, text);
  btn->SetID(id);
  // btn->SetFontList("Segoe UI, 13px");
  btn->SetEnabledTextColors(kTextWhite);
  btn->SetBackgroundColor(kToolbarBg);
  if (tooltip) {
    btn->SetTooltipText(tooltip);
  }
  if (width > 0) {
    btn->SetMinimumSize(CefSize(width, 28));
  }
  return btn;
}

void NativeChrome::Attach(CefRefPtr<CefWindow> window) {
  // Row 1: Tab Strip
  tab_strip_row_ = CefPanel::CreatePanel(nullptr);
  tab_strip_row_->SetBackgroundColor(kTabStripBg);
  window->AddChildView(tab_strip_row_);

  tab_strip_layout_ = tab_strip_row_->SetToBoxLayout(MakeBoxLayoutSettings(true, 8, 4, 4));

  brand_btn_ = CreateButton(" NEXUS", kIdBrand, "NEXUS Browser");
  brand_btn_->SetBackgroundColor(kTabStripBg);
  brand_btn_->SetEnabledTextColors(kTextWhite);
  if (auto logo = LoadBrandImage()) {
    brand_btn_->SetImage(CEF_BUTTON_STATE_NORMAL, logo);
  }
  tab_strip_row_->AddChildView(brand_btn_);

  tabs_container_ = CefPanel::CreatePanel(nullptr);
  tabs_container_->SetBackgroundColor(kTabStripBg);
  tabs_layout_ = tabs_container_->SetToBoxLayout(MakeBoxLayoutSettings(true, 0, 0, 2));
  tab_strip_row_->AddChildView(tabs_container_);
  if (tab_strip_layout_) {
    tab_strip_layout_->SetFlexForView(tabs_container_, 1);
  }

  new_tab_btn_ = CreateButton("+", kIdNewTab, "New Tab (Ctrl+T)", 28);
  new_tab_btn_->SetBackgroundColor(kTabStripBg);
  new_tab_btn_->SetEnabledTextColors(kTextMuted);
  tab_strip_row_->AddChildView(new_tab_btn_);

  // Row 2: Navigation & Address Bar
  toolbar_row_ = CefPanel::CreatePanel(nullptr);
  toolbar_row_->SetBackgroundColor(kToolbarBg);
  window->AddChildView(toolbar_row_);

  toolbar_layout_ = toolbar_row_->SetToBoxLayout(MakeBoxLayoutSettings(true, 10, 4, 6));

  back_btn_ = CreateButton("‹", kIdBack, "Back (Alt+Left)", 30);
  forward_btn_ = CreateButton("›", kIdForward, "Forward (Alt+Right)", 30);
  reload_btn_ = CreateButton("↻", kIdReload, "Reload (Ctrl+R)", 30);
  bookmark_btn_ = CreateButton("★", kIdBookmark, "Bookmark this tab (Ctrl+D)", 30);

  toolbar_row_->AddChildView(back_btn_);
  toolbar_row_->AddChildView(forward_btn_);
  toolbar_row_->AddChildView(reload_btn_);
  toolbar_row_->AddChildView(bookmark_btn_);

  // Address Bar Omnibox Pill
  address_pill_ = CefPanel::CreatePanel(nullptr);
  address_pill_->SetBackgroundColor(kAddressBg);
  address_layout_ = address_pill_->SetToBoxLayout(MakeBoxLayoutSettings(true, 8, 2, 6));

  lock_icon_ = CreateButton("🔒", 0, "Secure connection", 22);
  lock_icon_->SetBackgroundColor(kAddressBg);
  lock_icon_->SetEnabledTextColors(kTextMuted);
  address_pill_->AddChildView(lock_icon_);

  address_input_ = CefTextfield::CreateTextfield(this);
  address_input_->SetPlaceholderText("Search Google or enter address");
  address_input_->SetAccessibleName("Address and search bar");
  address_input_->SetBackgroundColor(kAddressBg);
  address_pill_->AddChildView(address_input_);

  shield_badge_ = CreateButton("🛡", kIdDashboard, "NEXUS Privacy & Tracker Protection", 24);
  shield_badge_->SetBackgroundColor(kAddressBg);
  shield_badge_->SetEnabledTextColors(kShieldColor);
  address_pill_->AddChildView(shield_badge_);

  if (address_layout_) {
    address_layout_->SetFlexForView(address_input_, 1);
  }

  toolbar_row_->AddChildView(address_pill_);
  if (toolbar_layout_) {
    toolbar_layout_->SetFlexForView(address_pill_, 1);
  }

  dashboard_btn_ = CreateButton("🛡", kIdDashboard, "Privacy Dashboard", 30);
  dashboard_btn_->SetEnabledTextColors(kShieldColor);
  toolbar_row_->AddChildView(dashboard_btn_);

  menu_btn_ = CreateButton("☰", kIdMenu, "Settings & Menu", 30);
  toolbar_row_->AddChildView(menu_btn_);
}

void NativeChrome::Layout(int width, int /*height*/) {
  const int safe_width = std::max(1, width);

  if (tab_strip_row_) {
    tab_strip_row_->SetBounds(CefRect(0, 0, safe_width, kTabStripHeight));
    tab_strip_row_->Layout();
  }
  if (toolbar_row_) {
    toolbar_row_->SetBounds(CefRect(0, kTabStripHeight, safe_width, kToolbarHeight));
    toolbar_row_->Layout();
  }
}

void NativeChrome::Sync(const std::vector<TabSummary>& tabs,
                        const std::string& active_url,
                        bool loading,
                        bool can_go_back,
                        bool can_go_forward) {
  if (back_btn_) back_btn_->SetEnabled(can_go_back);
  if (forward_btn_) forward_btn_->SetEnabled(can_go_forward);
  if (reload_btn_) reload_btn_->SetText(loading ? "✕" : "↻");

  if (address_input_ && !address_input_->HasFocus()) {
    if (active_url.empty() || active_url == "nexus://newtab/" || active_url == "nexus://newtab") {
      if (!address_input_->GetText().empty()) {
        address_input_->SetText("");
      }
    } else if (address_input_->GetText().ToString() != active_url) {
      address_input_->SetText(active_url);
    }
  }

  if (!tabs_container_) return;

  bool structure_changed = (tabs.size() != rendered_tabs_.size());
  if (!structure_changed) {
    for (size_t i = 0; i < tabs.size(); ++i) {
      if (tabs[i].id != rendered_tabs_[i].tab_id) {
        structure_changed = true;
        break;
      }
    }
  }

  if (structure_changed) {
    tabs_container_->RemoveAllChildViews();
    rendered_tabs_.clear();

    for (const TabSummary& tab : tabs) {
      RenderedTab rt;
      rt.tab_id = tab.id;

      std::string display_title = (tab.is_loading ? "⏳ " : "") + TruncateTitle(tab.title);
      rt.title_btn = CefLabelButton::CreateLabelButton(this, display_title);
      rt.title_btn->SetID(kIdTabBase + tab.id);
      rt.title_btn->SetTooltipText(tab.title.empty() ? tab.url : tab.title + " — " + tab.url);
      rt.title_btn->SetMinimumSize(CefSize(60, 28));
      rt.title_btn->SetMaximumSize(CefSize(200, 28));

      rt.close_btn = CefLabelButton::CreateLabelButton(this, "×");
      rt.close_btn->SetID(kIdCloseBase + tab.id);
      rt.close_btn->SetTooltipText("Close tab");
      rt.close_btn->SetMinimumSize(CefSize(22, 28));

      if (tab.is_active) {
        rt.title_btn->SetEnabledTextColors(kTextWhite);
        rt.title_btn->SetBackgroundColor(kActiveTabBg);
        rt.close_btn->SetEnabledTextColors(kTextMuted);
        rt.close_btn->SetBackgroundColor(kActiveTabBg);
      } else {
        rt.title_btn->SetEnabledTextColors(kTextMuted);
        rt.title_btn->SetBackgroundColor(kInactiveTabBg);
        rt.close_btn->SetEnabledTextColors(kTextDim);
        rt.close_btn->SetBackgroundColor(kInactiveTabBg);
      }

      tabs_container_->AddChildView(rt.title_btn);
      tabs_container_->AddChildView(rt.close_btn);
      rendered_tabs_.push_back(rt);
    }

    if (tab_strip_row_) {
      tab_strip_row_->Layout();
    }
  } else {
    for (size_t i = 0; i < tabs.size(); ++i) {
      const TabSummary& tab = tabs[i];
      RenderedTab& rt = rendered_tabs_[i];

      std::string display_title = (tab.is_loading ? "⏳ " : "") + TruncateTitle(tab.title);
      rt.title_btn->SetText(display_title);
      rt.title_btn->SetTooltipText(tab.title.empty() ? tab.url : tab.title + " — " + tab.url);

      if (tab.is_active) {
        rt.title_btn->SetEnabledTextColors(kTextWhite);
        rt.title_btn->SetBackgroundColor(kActiveTabBg);
        rt.close_btn->SetEnabledTextColors(kTextMuted);
        rt.close_btn->SetBackgroundColor(kActiveTabBg);
      } else {
        rt.title_btn->SetEnabledTextColors(kTextMuted);
        rt.title_btn->SetBackgroundColor(kInactiveTabBg);
        rt.close_btn->SetEnabledTextColors(kTextDim);
        rt.close_btn->SetBackgroundColor(kInactiveTabBg);
      }
    }
  }
}

void NativeChrome::FocusAddress() {
  if (address_input_) {
    address_input_->RequestFocus();
    address_input_->SelectAll(false);
  }
}

void NativeChrome::OnButtonPressed(CefRefPtr<CefButton> button) {
  if (!owner_ || !button) return;
  const int id = button->GetID();

  if (id == kIdBrand) {
    owner_->CreateTab("nexus://newtab/", true);
    FocusAddress();
  } else if (id == kIdNewTab) {
    owner_->CreateTab("nexus://newtab/", true);
    FocusAddress();
  } else if (id == kIdBack) {
    owner_->GoBack();
  } else if (id == kIdForward) {
    owner_->GoForward();
  } else if (id == kIdReload) {
    owner_->ReloadOrStop();
  } else if (id == kIdBookmark) {
    // Navigate or bookmark
  } else if (id == kIdDashboard) {
    owner_->OpenUrlInNewTab("nexus://privacy/", true);
  } else if (id == kIdMenu) {
    owner_->OpenUrlInNewTab("nexus://settings/", true);
  } else if (id >= kIdCloseBase) {
    const int tab_id = id - kIdCloseBase;
    owner_->CloseTab(tab_id);
  } else if (id >= kIdTabBase) {
    const int tab_id = id - kIdTabBase;
    owner_->ActivateTab(tab_id);
  }
}

bool NativeChrome::OnKeyEvent(CefRefPtr<CefTextfield>, const CefKeyEvent& event) {
  if (event.type == KEYEVENT_RAWKEYDOWN && event.windows_key_code == VK_RETURN) {
    if (address_input_) {
      const std::string text = address_input_->GetText().ToString();
      if (!text.empty()) {
        owner_->NavigateFromAddressBar(text);
      }
    }
    return true;
  }
  return false;
}

}  // namespace nexus
