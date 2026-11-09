#include "src/render_process_handler.h"

#include <cstdio>
#include <string>
#include <string_view>

#include "core/url_utils.h"
#include "include/cef_browser.h"
#include "include/cef_frame.h"
#include "include/cef_process_message.h"
#include "include/cef_values.h"

namespace nexus {
namespace {

constexpr size_t kMaxOutgoingRequestBytes = 64 * 1024;
constexpr size_t kMaxIncomingMessageBytes = 256 * 1024;

class NativePostMessageHandler final : public CefV8Handler {
 public:
  NativePostMessageHandler() = default;

  bool Execute(const CefString& /*name*/,
               CefRefPtr<CefV8Value> /*object*/,
               const CefV8ValueList& arguments,
               CefRefPtr<CefV8Value>& retval,
               CefString& exception) override {
    if (arguments.size() != 1 || !arguments[0]->IsString()) {
      exception = "NEXUS IPC expects one JSON string argument.";
      return true;
    }

    CefRefPtr<CefV8Context> context = CefV8Context::GetCurrentContext();
    if (!context) {
      exception = "NEXUS IPC is unavailable outside a document context.";
      return true;
    }

    CefRefPtr<CefFrame> frame = context->GetFrame();
    if (!frame || !frame->IsMain() ||
        !core::IsInternalAppUrl(frame->GetURL().ToString())) {
      exception = "NEXUS IPC is only available to the local application shell.";
      return true;
    }

    const std::string json = arguments[0]->GetStringValue().ToString();
    if (json.size() > kMaxOutgoingRequestBytes) {
      exception = "NEXUS IPC request is too large.";
      return true;
    }

    CefRefPtr<CefProcessMessage> message =
        CefProcessMessage::Create("nexus.ipc.message");
    message->GetArgumentList()->SetString(0, CefString(json));
    frame->SendProcessMessage(PID_BROWSER, message);

    retval = CefV8Value::CreateUndefined();
    return true;
  }

 private:
  IMPLEMENT_REFCOUNTING(NativePostMessageHandler);
  DISALLOW_COPY_AND_ASSIGN(NativePostMessageHandler);
};

std::string JsStringLiteral(std::string_view value) {
  std::string literal;
  literal.reserve(value.size() + 2);
  literal.push_back('"');

  for (const unsigned char c : value) {
    switch (c) {
      case '"':
        literal += "\\\"";
        break;
      case '\\':
        literal += "\\\\";
        break;
      case '\b':
        literal += "\\b";
        break;
      case '\f':
        literal += "\\f";
        break;
      case '\n':
        literal += "\\n";
        break;
      case '\r':
        literal += "\\r";
        break;
      case '\t':
        literal += "\\t";
        break;
      default:
        if (c < 0x20) {
          char buffer[8];
          std::snprintf(buffer, sizeof(buffer), "\\u%04X",
                        static_cast<unsigned int>(c));
          literal += buffer;
        } else {
          literal.push_back(static_cast<char>(c));
        }
        break;
    }
  }

  literal.push_back('"');
  return literal;
}

void DispatchToOnMessage(CefRefPtr<CefFrame> frame, const std::string& json) {
  const std::string script =
      "(function(){try{var d=JSON.parse(" + JsStringLiteral(json) +
      ");var n=window.__nexusNative;if(n&&typeof n.onMessage==='function'){"
      "n.onMessage(d);}}catch(e){}})();";
  frame->ExecuteJavaScript(CefString(script),
                           CefString("nexus://app/ipc-dispatch.js"), 0);
}

}  // namespace

void NexusRenderProcessHandler::OnBrowserCreated(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefDictionaryValue> extra_info) {
  if (!browser || !extra_info || !extra_info->HasKey("role")) {
    return;
  }
  if (extra_info->GetString("role") == "shell") {
    shell_browser_ids_.insert(browser->GetIdentifier());
  }
}

void NexusRenderProcessHandler::OnBrowserDestroyed(
    CefRefPtr<CefBrowser> browser) {
  if (browser) {
    shell_browser_ids_.erase(browser->GetIdentifier());
  }
}

void NexusRenderProcessHandler::OnContextCreated(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefRefPtr<CefV8Context> context) {
  if (!browser || !frame || !context || !frame->IsMain()) {
    return;
  }
  if (shell_browser_ids_.find(browser->GetIdentifier()) ==
      shell_browser_ids_.end()) {
    return;
  }
  if (!core::IsInternalAppUrl(frame->GetURL().ToString())) {
    return;
  }

  CefRefPtr<CefV8Value> global = context->GetGlobal();
  if (!global) {
    return;
  }

  CefRefPtr<CefV8Value> bridge =
      CefV8Value::CreateObject(nullptr, nullptr);
  if (!bridge) {
    return;
  }

  CefRefPtr<CefV8Value> post_message =
      CefV8Value::CreateFunction("postMessage",
                                 new NativePostMessageHandler());
  if (!post_message) {
    return;
  }

  constexpr cef_v8_propertyattribute_t kProtected =
      static_cast<cef_v8_propertyattribute_t>(
          V8_PROPERTY_ATTRIBUTE_READONLY | V8_PROPERTY_ATTRIBUTE_DONTENUM |
          V8_PROPERTY_ATTRIBUTE_DONTDELETE);

  bridge->SetValue("postMessage", post_message, kProtected);
  bridge->SetValue("onMessage", CefV8Value::CreateNull(),
                   V8_PROPERTY_ATTRIBUTE_NONE);
  global->SetValue("__nexusNative", bridge, kProtected);
}

bool NexusRenderProcessHandler::OnProcessMessageReceived(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefProcessId source_process,
    CefRefPtr<CefProcessMessage> message) {
  if (source_process != PID_BROWSER || !browser || !frame || !message) {
    return false;
  }
  if (message->GetName() != "nexus.ipc.message" || !frame->IsMain()) {
    return false;
  }
  if (shell_browser_ids_.find(browser->GetIdentifier()) ==
      shell_browser_ids_.end()) {
    return false;
  }

  CefRefPtr<CefListValue> arguments = message->GetArgumentList();
  if (!arguments || arguments->GetSize() != 1) {
    return false;
  }
  CefRefPtr<CefValue> argument = arguments->GetValue(0);
  if (!argument || argument->GetType() != VTYPE_STRING) {
    return false;
  }

  const std::string json = argument->GetString().ToString();
  if (json.size() > kMaxIncomingMessageBytes) {
    return false;
  }

  DispatchToOnMessage(frame, json);
  return true;
}

}  // namespace nexus
