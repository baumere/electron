// Copyright (c) 2013 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "shell/browser/api/atom_api_app.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/system/sys_info.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/icon_manager.h"
#include "chrome/common/chrome_paths.h"
#include "content/browser/gpu/compositor_util.h"        // nogncheck
#include "content/browser/gpu/gpu_data_manager_impl.h"  // nogncheck
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_switches.h"
#include "gin/arguments.h"
#include "media/audio/audio_manager.h"
#include "native_mate/object_template_builder.h"
#include "net/ssl/client_cert_identity.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "services/service_manager/sandbox/switches.h"
#include "shell/browser/api/atom_api_menu.h"
#include "shell/browser/api/atom_api_session.h"
#include "shell/browser/api/atom_api_web_contents.h"
#include "shell/browser/api/gpuinfo_manager.h"
#include "shell/browser/atom_browser_context.h"
#include "shell/browser/atom_browser_main_parts.h"
#include "shell/browser/atom_paths.h"
#include "shell/browser/login_handler.h"
#include "shell/browser/relauncher.h"
#include "shell/common/application_info.h"
#include "shell/common/atom_command_line.h"
#include "shell/common/native_mate_converters/callback_converter_deprecated.h"
#include "shell/common/native_mate_converters/file_path_converter.h"
#include "shell/common/native_mate_converters/gurl_converter.h"
#include "shell/common/native_mate_converters/image_converter.h"
#include "shell/common/native_mate_converters/net_converter.h"
#include "shell/common/native_mate_converters/network_converter.h"
#include "shell/common/native_mate_converters/once_callback.h"
#include "shell/common/native_mate_converters/value_converter.h"
#include "shell/common/node_includes.h"
#include "shell/common/options_switches.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"

#if defined(OS_WIN)
#include "base/strings/utf_string_conversions.h"
#include "shell/browser/ui/win/jump_list.h"
#endif

#if defined(OS_MACOSX)
#include <CoreFoundation/CoreFoundation.h>
#include "shell/browser/ui/cocoa/atom_bundle_mover.h"
#endif

using electron::Browser;

namespace mate {

#if defined(OS_WIN)
template <>
struct Converter<electron::ProcessIntegrityLevel> {
  static v8::Local<v8::Value> ToV8(v8::Isolate* isolate,
                                   electron::ProcessIntegrityLevel value) {
    switch (value) {
      case electron::ProcessIntegrityLevel::Untrusted:
        return mate::StringToV8(isolate, "untrusted");
      case electron::ProcessIntegrityLevel::Low:
        return mate::StringToV8(isolate, "low");
      case electron::ProcessIntegrityLevel::Medium:
        return mate::StringToV8(isolate, "medium");
      case electron::ProcessIntegrityLevel::High:
        return mate::StringToV8(isolate, "high");
      default:
        return mate::StringToV8(isolate, "unknown");
    }
  }
};

template <>
struct Converter<Browser::UserTask> {
  static bool FromV8(v8::Isolate* isolate,
                     v8::Local<v8::Value> val,
                     Browser::UserTask* out) {
    mate::Dictionary dict;
    if (!ConvertFromV8(isolate, val, &dict))
      return false;
    if (!dict.Get("program", &(out->program)) ||
        !dict.Get("title", &(out->title)))
      return false;
    if (dict.Get("iconPath", &(out->icon_path)) &&
        !dict.Get("iconIndex", &(out->icon_index)))
      return false;
    dict.Get("arguments", &(out->arguments));
    dict.Get("description", &(out->description));
    dict.Get("workingDirectory", &(out->working_dir));
    return true;
  }
};

using electron::JumpListCategory;
using electron::JumpListItem;
using electron::JumpListResult;

template <>
struct Converter<JumpListItem::Type> {
  static bool FromV8(v8::Isolate* isolate,
                     v8::Local<v8::Value> val,
                     JumpListItem::Type* out) {
    std::string item_type;
    if (!ConvertFromV8(isolate, val, &item_type))
      return false;

    if (item_type == "task")
      *out = JumpListItem::Type::TASK;
    else if (item_type == "separator")
      *out = JumpListItem::Type::SEPARATOR;
    else if (item_type == "file")
      *out = JumpListItem::Type::FILE;
    else
      return false;

    return true;
  }

  static v8::Local<v8::Value> ToV8(v8::Isolate* isolate,
                                   JumpListItem::Type val) {
    std::string item_type;
    switch (val) {
      case JumpListItem::Type::TASK:
        item_type = "task";
        break;

      case JumpListItem::Type::SEPARATOR:
        item_type = "separator";
        break;

      case JumpListItem::Type::FILE:
        item_type = "file";
        break;
    }
    return mate::ConvertToV8(isolate, item_type);
  }
};

template <>
struct Converter<JumpListItem> {
  static bool FromV8(v8::Isolate* isolate,
                     v8::Local<v8::Value> val,
                     JumpListItem* out) {
    mate::Dictionary dict;
    if (!ConvertFromV8(isolate, val, &dict))
      return false;

    if (!dict.Get("type", &(out->type)))
      return false;

    switch (out->type) {
      case JumpListItem::Type::TASK:
        if (!dict.Get("program", &(out->path)) ||
            !dict.Get("title", &(out->title)))
          return false;

        if (dict.Get("iconPath", &(out->icon_path)) &&
            !dict.Get("iconIndex", &(out->icon_index)))
          return false;

        dict.Get("args", &(out->arguments));
        dict.Get("description", &(out->description));
        dict.Get("workingDirectory", &(out->working_dir));
        return true;

      case JumpListItem::Type::SEPARATOR:
        return true;

      case JumpListItem::Type::FILE:
        return dict.Get("path", &(out->path));
    }

    assert(false);
    return false;
  }

  static v8::Local<v8::Value> ToV8(v8::Isolate* isolate,
                                   const JumpListItem& val) {
    mate::Dictionary dict = mate::Dictionary::CreateEmpty(isolate);
    dict.Set("type", val.type);

    switch (val.type) {
      case JumpListItem::Type::TASK:
        dict.Set("program", val.path);
        dict.Set("args", val.arguments);
        dict.Set("title", val.title);
        dict.Set("iconPath", val.icon_path);
        dict.Set("iconIndex", val.icon_index);
        dict.Set("description", val.description);
        dict.Set("workingDirectory", val.working_dir);
        break;

      case JumpListItem::Type::SEPARATOR:
        break;

      case JumpListItem::Type::FILE:
        dict.Set("path", val.path);
        break;
    }
    return dict.GetHandle();
  }
};

template <>
struct Converter<JumpListCategory::Type> {
  static bool FromV8(v8::Isolate* isolate,
                     v8::Local<v8::Value> val,
                     JumpListCategory::Type* out) {
    std::string category_type;
    if (!ConvertFromV8(isolate, val, &category_type))
      return false;

    if (category_type == "tasks")
      *out = JumpListCategory::Type::TASKS;
    else if (category_type == "frequent")
      *out = JumpListCategory::Type::FREQUENT;
    else if (category_type == "recent")
      *out = JumpListCategory::Type::RECENT;
    else if (category_type == "custom")
      *out = JumpListCategory::Type::CUSTOM;
    else
      return false;

    return true;
  }

  static v8::Local<v8::Value> ToV8(v8::Isolate* isolate,
                                   JumpListCategory::Type val) {
    std::string category_type;
    switch (val) {
      case JumpListCategory::Type::TASKS:
        category_type = "tasks";
        break;

      case JumpListCategory::Type::FREQUENT:
        category_type = "frequent";
        break;

      case JumpListCategory::Type::RECENT:
        category_type = "recent";
        break;

      case JumpListCategory::Type::CUSTOM:
        category_type = "custom";
        break;
    }
    return mate::ConvertToV8(isolate, category_type);
  }
};

template <>
struct Converter<JumpListCategory> {
  static bool FromV8(v8::Isolate* isolate,
                     v8::Local<v8::Value> val,
                     JumpListCategory* out) {
    mate::Dictionary dict;
    if (!ConvertFromV8(isolate, val, &dict))
      return false;

    if (dict.Get("name", &(out->name)) && out->name.empty())
      return false;

    if (!dict.Get("type", &(out->type))) {
      if (out->name.empty())
        out->type = JumpListCategory::Type::TASKS;
      else
        out->type = JumpListCategory::Type::CUSTOM;
    }

    if ((out->type == JumpListCategory::Type::TASKS) ||
        (out->type == JumpListCategory::Type::CUSTOM)) {
      if (!dict.Get("items", &(out->items)))
        return false;
    }

    return true;
  }
};

// static
template <>
struct Converter<JumpListResult> {
  static v8::Local<v8::Value> ToV8(v8::Isolate* isolate, JumpListResult val) {
    std::string result_code;
    switch (val) {
      case JumpListResult::SUCCESS:
        result_code = "ok";
        break;

      case JumpListResult::ARGUMENT_ERROR:
        result_code = "argumentError";
        break;

      case JumpListResult::GENERIC_ERROR:
        result_code = "error";
        break;

      case JumpListResult::CUSTOM_CATEGORY_SEPARATOR_ERROR:
        result_code = "invalidSeparatorError";
        break;

      case JumpListResult::MISSING_FILE_TYPE_REGISTRATION_ERROR:
        result_code = "fileTypeRegistrationError";
        break;

      case JumpListResult::CUSTOM_CATEGORY_ACCESS_DENIED_ERROR:
        result_code = "customCategoryAccessDeniedError";
        break;
    }
    return ConvertToV8(isolate, result_code);
  }
};
#endif

template <>
struct Converter<Browser::LoginItemSettings> {
  static bool FromV8(v8::Isolate* isolate,
                     v8::Local<v8::Value> val,
                     Browser::LoginItemSettings* out) {
    mate::Dictionary dict;
    if (!ConvertFromV8(isolate, val, &dict))
      return false;

    dict.Get("openAtLogin", &(out->open_at_login));
    dict.Get("openAsHidden", &(out->open_as_hidden));
    dict.Get("path", &(out->path));
    dict.Get("args", &(out->args));
    return true;
  }

  static v8::Local<v8::Value> ToV8(v8::Isolate* isolate,
                                   Browser::LoginItemSettings val) {
    mate::Dictionary dict = mate::Dictionary::CreateEmpty(isolate);
    dict.Set("openAtLogin", val.open_at_login);
    dict.Set("openAsHidden", val.open_as_hidden);
    dict.Set("restoreState", val.restore_state);
    dict.Set("wasOpenedAtLogin", val.opened_at_login);
    dict.Set("wasOpenedAsHidden", val.opened_as_hidden);
    return dict.GetHandle();
  }
};

template <>
struct Converter<content::CertificateRequestResultType> {
  static bool FromV8(v8::Isolate* isolate,
                     v8::Local<v8::Value> val,
                     content::CertificateRequestResultType* out) {
    bool b;
    if (!ConvertFromV8(isolate, val, &b))
      return false;
    *out = b ? content::CERTIFICATE_REQUEST_RESULT_TYPE_CONTINUE
             : content::CERTIFICATE_REQUEST_RESULT_TYPE_CANCEL;
    return true;
  }
};

}  // namespace mate

namespace electron {

namespace api {

namespace {

IconLoader::IconSize GetIconSizeByString(const std::string& size) {
  if (size == "small") {
    return IconLoader::IconSize::SMALL;
  } else if (size == "large") {
    return IconLoader::IconSize::LARGE;
  }
  return IconLoader::IconSize::NORMAL;
}

// Return the path constant from string.
int GetPathConstant(const std::string& name) {
  if (name == "appData")
    return DIR_APP_DATA;
  else if (name == "userData")
    return DIR_USER_DATA;
  else if (name == "cache")
    return DIR_CACHE;
  else if (name == "userCache")
    return DIR_USER_CACHE;
  else if (name == "logs")
    return DIR_APP_LOGS;
  else if (name == "home")
    return base::DIR_HOME;
  else if (name == "temp")
    return base::DIR_TEMP;
  else if (name == "userDesktop" || name == "desktop")
    return base::DIR_USER_DESKTOP;
  else if (name == "exe")
    return base::FILE_EXE;
  else if (name == "module")
    return base::FILE_MODULE;
  else if (name == "documents")
    return chrome::DIR_USER_DOCUMENTS;
  else if (name == "downloads")
    return chrome::DIR_DEFAULT_DOWNLOADS;
  else if (name == "music")
    return chrome::DIR_USER_MUSIC;
  else if (name == "pictures")
    return chrome::DIR_USER_PICTURES;
  else if (name == "videos")
    return chrome::DIR_USER_VIDEOS;
  else if (name == "pepperFlashSystemPlugin")
    return chrome::FILE_PEPPER_FLASH_SYSTEM_PLUGIN;
  else
    return -1;
}

bool NotificationCallbackWrapper(
    const base::RepeatingCallback<
        void(const base::CommandLine::StringVector& command_line,
             const base::FilePath& current_directory)>& callback,
    const base::CommandLine::StringVector& cmd,
    const base::FilePath& cwd) {
  // Make sure the callback is called after app gets ready.
  if (Browser::Get()->is_ready()) {
    callback.Run(cmd, cwd);
  } else {
    scoped_refptr<base::SingleThreadTaskRunner> task_runner(
        base::ThreadTaskRunnerHandle::Get());
    task_runner->PostTask(
        FROM_HERE, base::BindOnce(base::IgnoreResult(callback), cmd, cwd));
  }
  // ProcessSingleton needs to know whether current process is quiting.
  return !Browser::Get()->is_shutting_down();
}

void GotPrivateKey(std::shared_ptr<content::ClientCertificateDelegate> delegate,
                   scoped_refptr<net::X509Certificate> cert,
                   scoped_refptr<net::SSLPrivateKey> private_key) {
  delegate->ContinueWithCertificate(cert, private_key);
}

void OnClientCertificateSelected(
    v8::Isolate* isolate,
    std::shared_ptr<content::ClientCertificateDelegate> delegate,
    std::shared_ptr<net::ClientCertIdentityList> identities,
    mate::Arguments* args) {
  if (args->Length() == 2) {
    delegate->ContinueWithCertificate(nullptr, nullptr);
    return;
  }

  v8::Local<v8::Value> val;
  args->GetNext(&val);
  if (val->IsNull()) {
    delegate->ContinueWithCertificate(nullptr, nullptr);
    return;
  }

  mate::Dictionary cert_data;
  if (!mate::ConvertFromV8(isolate, val, &cert_data)) {
    args->ThrowError("Must pass valid certificate object.");
    return;
  }

  std::string data;
  if (!cert_data.Get("data", &data))
    return;

  auto certs = net::X509Certificate::CreateCertificateListFromBytes(
      data.c_str(), data.length(), net::X509Certificate::FORMAT_AUTO);
  if (!certs.empty()) {
    scoped_refptr<net::X509Certificate> cert(certs[0].get());
    for (size_t i = 0; i < identities->size(); ++i) {
      if (cert->EqualsExcludingChain((*identities)[i]->certificate())) {
        net::ClientCertIdentity::SelfOwningAcquirePrivateKey(
            std::move((*identities)[i]),
            base::BindRepeating(&GotPrivateKey, delegate, std::move(cert)));
        break;
      }
    }
  }
}

void PassLoginInformation(scoped_refptr<LoginHandler> login_handler,
                          mate::Arguments* args) {
  base::string16 username, password;
  if (args->GetNext(&username) && args->GetNext(&password))
    login_handler->Login(username, password);
  else
    login_handler->CancelAuth();
}

#if defined(USE_NSS_CERTS)
int ImportIntoCertStore(CertificateManagerModel* model,
                        const base::DictionaryValue& options) {
  std::string file_data, cert_path;
  base::string16 password;
  net::ScopedCERTCertificateList imported_certs;
  int rv = -1;
  options.GetString("certificate", &cert_path);
  options.GetString("password", &password);

  if (!cert_path.empty()) {
    if (base::ReadFileToString(base::FilePath(cert_path), &file_data)) {
      auto module = model->cert_db()->GetPrivateSlot();
      rv = model->ImportFromPKCS12(module.get(), file_data, password, true,
                                   &imported_certs);
      if (imported_certs.size() > 1) {
        auto it = imported_certs.begin();
        ++it;  // skip first which would  be the client certificate.
        for (; it != imported_certs.end(); ++it)
          rv &= model->SetCertTrust(it->get(), net::CA_CERT,
                                    net::NSSCertDatabase::TRUSTED_SSL);
      }
    }
  }
  return rv;
}
#endif

void OnIconDataAvailable(util::Promise<gfx::Image> promise, gfx::Image icon) {
  if (!icon.IsEmpty()) {
    promise.Resolve(icon);
  } else {
    promise.RejectWithErrorMessage("Failed to get file icon.");
  }
}

}  // namespace

App::App(v8::Isolate* isolate) {
  static_cast<AtomBrowserClient*>(AtomBrowserClient::Get())->set_delegate(this);
  Browser::Get()->AddObserver(this);
  content::GpuDataManager::GetInstance()->AddObserver(this);

  base::ProcessId pid = base::GetCurrentProcId();
  auto process_metric = std::make_unique<electron::ProcessMetric>(
      content::PROCESS_TYPE_BROWSER, base::GetCurrentProcessHandle(),
      base::ProcessMetrics::CreateCurrentProcessMetrics());
  app_metrics_[pid] = std::move(process_metric);
  Init(isolate);
}

App::~App() {
  static_cast<AtomBrowserClient*>(AtomBrowserClient::Get())
      ->set_delegate(nullptr);
  Browser::Get()->RemoveObserver(this);
  content::GpuDataManager::GetInstance()->RemoveObserver(this);
  content::BrowserChildProcessObserver::Remove(this);
}

void App::OnBeforeQuit(bool* prevent_default) {
  if (Emit("before-quit")) {
    *prevent_default = true;
  }
}

void App::OnWillQuit(bool* prevent_default) {
  if (Emit("will-quit")) {
    *prevent_default = true;
  }
}

void App::OnWindowAllClosed() {
  Emit("window-all-closed");
}

void App::OnQuit() {
  int exitCode = AtomBrowserMainParts::Get()->GetExitCode();
  Emit("quit", exitCode);

  if (process_singleton_) {
    process_singleton_->Cleanup();
    process_singleton_.reset();
  }
}

void App::OnOpenFile(bool* prevent_default, const std::string& file_path) {
  if (Emit("open-file", file_path)) {
    *prevent_default = true;
  }
}

void App::OnOpenURL(const std::string& url) {
  Emit("open-url", url);
}

void App::OnActivate(bool has_visible_windows) {
  Emit("activate", has_visible_windows);
}

void App::OnWillFinishLaunching() {
  Emit("will-finish-launching");
}

void App::OnFinishLaunching(const base::DictionaryValue& launch_info) {
#if defined(OS_LINUX)
  // Set the application name for audio streams shown in external
  // applications. Only affects pulseaudio currently.
  media::AudioManager::SetGlobalAppName(Browser::Get()->GetName());
#endif
  Emit("ready", launch_info);
}

void App::OnPreMainMessageLoopRun() {
  content::BrowserChildProcessObserver::Add(this);
  if (process_singleton_) {
    process_singleton_->OnBrowserReady();
  }
}

void App::OnAccessibilitySupportChanged() {
  Emit("accessibility-support-changed", IsAccessibilitySupportEnabled());
}

#if defined(OS_MACOSX)
void App::OnWillContinueUserActivity(bool* prevent_default,
                                     const std::string& type) {
  if (Emit("will-continue-activity", type)) {
    *prevent_default = true;
  }
}

void App::OnDidFailToContinueUserActivity(const std::string& type,
                                          const std::string& error) {
  Emit("continue-activity-error", type, error);
}

void App::OnContinueUserActivity(bool* prevent_default,
                                 const std::string& type,
                                 const base::DictionaryValue& user_info) {
  if (Emit("continue-activity", type, user_info)) {
    *prevent_default = true;
  }
}

void App::OnUserActivityWasContinued(const std::string& type,
                                     const base::DictionaryValue& user_info) {
  Emit("activity-was-continued", type, user_info);
}

void App::OnUpdateUserActivityState(bool* prevent_default,
                                    const std::string& type,
                                    const base::DictionaryValue& user_info) {
  if (Emit("update-activity-state", type, user_info)) {
    *prevent_default = true;
  }
}

void App::OnNewWindowForTab() {
  Emit("new-window-for-tab");
}
#endif

void App::OnLogin(scoped_refptr<LoginHandler> login_handler,
                  const base::DictionaryValue& request_details) {
  v8::Locker locker(isolate());
  v8::HandleScope handle_scope(isolate());
  bool prevent_default = false;
  content::WebContents* web_contents = login_handler->GetWebContents();
  if (web_contents) {
    prevent_default =
        Emit("login", WebContents::FromOrCreate(isolate(), web_contents),
             request_details, *login_handler->auth_info(),
             base::BindOnce(&PassLoginInformation,
                            base::RetainedRef(login_handler)));
  }

  // Default behavior is to always cancel the auth.
  if (!prevent_default)
    login_handler->CancelAuth();
}

bool App::CanCreateWindow(
    content::RenderFrameHost* opener,
    const GURL& opener_url,
    const GURL& opener_top_level_frame_url,
    const url::Origin& source_origin,
    content::mojom::WindowContainerType container_type,
    const GURL& target_url,
    const content::Referrer& referrer,
    const std::string& frame_name,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& features,
    const std::vector<std::string>& additional_features,
    const scoped_refptr<network::ResourceRequestBody>& body,
    bool user_gesture,
    bool opener_suppressed,
    bool* no_javascript_access) {
  v8::Locker locker(isolate());
  v8::HandleScope handle_scope(isolate());
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(opener);
  if (web_contents) {
    auto api_web_contents = WebContents::From(isolate(), web_contents);
    // No need to emit any event if the WebContents is not available in JS.
    if (!api_web_contents.IsEmpty()) {
      api_web_contents->OnCreateWindow(target_url, referrer, frame_name,
                                       disposition, additional_features, body);
    }
  }

  return false;
}

void App::AllowCertificateError(
    content::WebContents* web_contents,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    bool is_main_frame_request,
    bool strict_enforcement,
    const base::RepeatingCallback<void(content::CertificateRequestResultType)>&
        callback) {
  v8::Locker locker(isolate());
  v8::HandleScope handle_scope(isolate());
  bool prevent_default = Emit(
      "certificate-error", WebContents::FromOrCreate(isolate(), web_contents),
      request_url, net::ErrorToString(cert_error), ssl_info.cert, callback);

  // Deny the certificate by default.
  if (!prevent_default)
    callback.Run(content::CERTIFICATE_REQUEST_RESULT_TYPE_DENY);
}

base::OnceClosure App::SelectClientCertificate(
    content::WebContents* web_contents,
    net::SSLCertRequestInfo* cert_request_info,
    net::ClientCertIdentityList identities,
    std::unique_ptr<content::ClientCertificateDelegate> delegate) {
  std::shared_ptr<content::ClientCertificateDelegate> shared_delegate(
      delegate.release());

  // Convert the ClientCertIdentityList to a CertificateList
  // to avoid changes in the API.
  auto client_certs = net::CertificateList();
  for (const std::unique_ptr<net::ClientCertIdentity>& identity : identities)
    client_certs.push_back(identity->certificate());

  auto shared_identities =
      std::make_shared<net::ClientCertIdentityList>(std::move(identities));

  bool prevent_default =
      Emit("select-client-certificate",
           WebContents::FromOrCreate(isolate(), web_contents),
           cert_request_info->host_and_port.ToString(), std::move(client_certs),
           base::BindOnce(&OnClientCertificateSelected, isolate(),
                          shared_delegate, shared_identities));

  // Default to first certificate from the platform store.
  if (!prevent_default) {
    scoped_refptr<net::X509Certificate> cert =
        (*shared_identities)[0]->certificate();
    net::ClientCertIdentity::SelfOwningAcquirePrivateKey(
        std::move((*shared_identities)[0]),
        base::BindRepeating(&GotPrivateKey, shared_delegate, std::move(cert)));
  }
  return base::OnceClosure();
}

void App::OnGpuInfoUpdate() {
  Emit("gpu-info-update");
}

void App::OnGpuProcessCrashed(base::TerminationStatus status) {
  Emit("gpu-process-crashed",
       status == base::TERMINATION_STATUS_PROCESS_WAS_KILLED);
}

void App::BrowserChildProcessLaunchedAndConnected(
    const content::ChildProcessData& data) {
  ChildProcessLaunched(data.process_type, data.GetProcess().Handle());
}

void App::BrowserChildProcessHostDisconnected(
    const content::ChildProcessData& data) {
  ChildProcessDisconnected(base::GetProcId(data.GetProcess().Handle()));
}

void App::BrowserChildProcessCrashed(
    const content::ChildProcessData& data,
    const content::ChildProcessTerminationInfo& info) {
  ChildProcessDisconnected(base::GetProcId(data.GetProcess().Handle()));
}

void App::BrowserChildProcessKilled(
    const content::ChildProcessData& data,
    const content::ChildProcessTerminationInfo& info) {
  ChildProcessDisconnected(base::GetProcId(data.GetProcess().Handle()));
}

void App::RenderProcessReady(content::RenderProcessHost* host) {
  ChildProcessLaunched(content::PROCESS_TYPE_RENDERER,
                       host->GetProcess().Handle());

  // TODO(jeremy): this isn't really the right place to be creating
  // `WebContents` instances, but this was implicitly happening before in
  // `RenderProcessPreferences`, so this is at least more explicit...
  content::WebContents* web_contents =
      AtomBrowserClient::Get()->GetWebContentsFromProcessID(host->GetID());
  if (web_contents)
    WebContents::FromOrCreate(v8::Isolate::GetCurrent(), web_contents);
}

void App::RenderProcessDisconnected(base::ProcessId host_pid) {
  ChildProcessDisconnected(host_pid);
}

void App::ChildProcessLaunched(int process_type, base::ProcessHandle handle) {
  auto pid = base::GetProcId(handle);

#if defined(OS_MACOSX)
  auto metrics = base::ProcessMetrics::CreateProcessMetrics(
      handle, content::BrowserChildProcessHost::GetPortProvider());
#else
  auto metrics = base::ProcessMetrics::CreateProcessMetrics(handle);
#endif
  app_metrics_[pid] = std::make_unique<electron::ProcessMetric>(
      process_type, handle, std::move(metrics));
}

void App::ChildProcessDisconnected(base::ProcessId pid) {
  app_metrics_.erase(pid);
}

base::FilePath App::GetAppPath() const {
  return app_path_;
}

void App::SetAppPath(const base::FilePath& app_path) {
  app_path_ = app_path;
}

#if !defined(OS_MACOSX)
void App::SetAppLogsPath(gin_helper::ErrorThrower thrower,
                         base::Optional<base::FilePath> custom_path) {
  if (custom_path.has_value()) {
    if (!custom_path->IsAbsolute()) {
      thrower.ThrowError("Path must be absolute");
      return;
    }
    base::PathService::Override(DIR_APP_LOGS, custom_path.value());
  } else {
    base::FilePath path;
    if (base::PathService::Get(DIR_USER_DATA, &path)) {
      path = path.Append(base::FilePath::FromUTF8Unsafe(GetApplicationName()));
      path = path.Append(base::FilePath::FromUTF8Unsafe("logs"));
      base::PathService::Override(DIR_APP_LOGS, path);
    }
  }
}
#endif

base::FilePath App::GetPath(gin_helper::ErrorThrower thrower,
                            const std::string& name) {
  bool succeed = false;
  base::FilePath path;

  int key = GetPathConstant(name);
  if (key >= 0) {
    succeed = base::PathService::Get(key, &path);
    // If users try to get the logs path before setting a logs path,
    // set the path to a sensible default and then try to get it again
    if (!succeed && name == "logs") {
      base::ThreadRestrictions::ScopedAllowIO allow_io;
      SetAppLogsPath(thrower, base::Optional<base::FilePath>());
      succeed = base::PathService::Get(key, &path);
    }
  }

  if (!succeed)
    thrower.ThrowError("Failed to get '" + name + "' path");

  return path;
}

void App::SetPath(gin_helper::ErrorThrower thrower,
                  const std::string& name,
                  const base::FilePath& path) {
  if (!path.IsAbsolute()) {
    thrower.ThrowError("Path must be absolute");
    return;
  }

  bool succeed = false;
  int key = GetPathConstant(name);
  if (key >= 0)
    succeed =
        base::PathService::OverrideAndCreateIfNeeded(key, path, true, false);
  if (!succeed)
    thrower.ThrowError("Failed to set path");
}

void App::SetDesktopName(const std::string& desktop_name) {
#if defined(OS_LINUX)
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  env->SetVar("CHROME_DESKTOP", desktop_name);
#endif
}

std::string App::GetLocale() {
  return g_browser_process->GetApplicationLocale();
}

std::string App::GetLocaleCountryCode() {
  std::string region;
#if defined(OS_WIN)
  WCHAR locale_name[LOCALE_NAME_MAX_LENGTH] = {0};

  if (GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_SISO3166CTRYNAME,
                      (LPWSTR)&locale_name,
                      sizeof(locale_name) / sizeof(WCHAR)) ||
      GetLocaleInfoEx(LOCALE_NAME_SYSTEM_DEFAULT, LOCALE_SISO3166CTRYNAME,
                      (LPWSTR)&locale_name,
                      sizeof(locale_name) / sizeof(WCHAR))) {
    base::WideToUTF8(locale_name, wcslen(locale_name), &region);
  }
#elif defined(OS_MACOSX)
  CFLocaleRef locale = CFLocaleCopyCurrent();
  CFStringRef value = CFStringRef(
      static_cast<CFTypeRef>(CFLocaleGetValue(locale, kCFLocaleCountryCode)));
  const CFIndex kCStringSize = 128;
  char temporaryCString[kCStringSize] = {0};
  CFStringGetCString(value, temporaryCString, kCStringSize,
                     kCFStringEncodingUTF8);
  region = temporaryCString;
#else
  const char* locale_ptr = setlocale(LC_TIME, NULL);
  if (!locale_ptr)
    locale_ptr = setlocale(LC_NUMERIC, NULL);
  if (locale_ptr) {
    std::string locale = locale_ptr;
    std::string::size_type rpos = locale.find('.');
    if (rpos != std::string::npos)
      locale = locale.substr(0, rpos);
    rpos = locale.find('_');
    if (rpos != std::string::npos && rpos + 1 < locale.size())
      region = locale.substr(rpos + 1);
  }
#endif
  return region.size() == 2 ? region : std::string();
}

void App::OnSecondInstance(const base::CommandLine::StringVector& cmd,
                           const base::FilePath& cwd) {
  Emit("second-instance", cmd, cwd);
}

bool App::HasSingleInstanceLock() const {
  if (process_singleton_)
    return true;
  return false;
}

bool App::RequestSingleInstanceLock() {
  if (HasSingleInstanceLock())
    return true;

  base::FilePath user_dir;
  base::PathService::Get(DIR_USER_DATA, &user_dir);

  auto cb = base::BindRepeating(&App::OnSecondInstance, base::Unretained(this));

  process_singleton_.reset(new ProcessSingleton(
      user_dir, base::BindRepeating(NotificationCallbackWrapper, cb)));

  switch (process_singleton_->NotifyOtherProcessOrCreate()) {
    case ProcessSingleton::NotifyResult::LOCK_ERROR:
    case ProcessSingleton::NotifyResult::PROFILE_IN_USE:
    case ProcessSingleton::NotifyResult::PROCESS_NOTIFIED: {
      process_singleton_.reset();
      return false;
    }
    case ProcessSingleton::NotifyResult::PROCESS_NONE:
    default:  // Shouldn't be needed, but VS warns if it is not there.
      return true;
  }
}

void App::ReleaseSingleInstanceLock() {
  if (process_singleton_) {
    process_singleton_->Cleanup();
    process_singleton_.reset();
  }
}

bool App::Relaunch(mate::Arguments* js_args) {
  // Parse parameters.
  bool override_argv = false;
  base::FilePath exec_path;
  relauncher::StringVector args;

  mate::Dictionary options;
  if (js_args->GetNext(&options)) {
    if (options.Get("execPath", &exec_path) | options.Get("args", &args))
      override_argv = true;
  }

  if (!override_argv) {
    const relauncher::StringVector& argv = electron::AtomCommandLine::argv();
    return relauncher::RelaunchApp(argv);
  }

  relauncher::StringVector argv;
  argv.reserve(1 + args.size());

  if (exec_path.empty()) {
    base::FilePath current_exe_path;
    base::PathService::Get(base::FILE_EXE, &current_exe_path);
    argv.push_back(current_exe_path.value());
  } else {
    argv.push_back(exec_path.value());
  }

  argv.insert(argv.end(), args.begin(), args.end());

  return relauncher::RelaunchApp(argv);
}

void App::DisableHardwareAcceleration(gin_helper::ErrorThrower thrower) {
  if (Browser::Get()->is_ready()) {
    thrower.ThrowError(
        "app.disableHardwareAcceleration() can only be called "
        "before app is ready");
    return;
  }
  content::GpuDataManager::GetInstance()->DisableHardwareAcceleration();
}

void App::DisableDomainBlockingFor3DAPIs(gin_helper::ErrorThrower thrower) {
  if (Browser::Get()->is_ready()) {
    thrower.ThrowError(
        "app.disableDomainBlockingFor3DAPIs() can only be called "
        "before app is ready");
    return;
  }
  content::GpuDataManagerImpl::GetInstance()
      ->DisableDomainBlockingFor3DAPIsForTesting();
}

bool App::IsAccessibilitySupportEnabled() {
  auto* ax_state = content::BrowserAccessibilityState::GetInstance();
  return ax_state->IsAccessibleBrowser();
}

void App::SetAccessibilitySupportEnabled(gin_helper::ErrorThrower thrower,
                                         bool enabled) {
  if (!Browser::Get()->is_ready()) {
    thrower.ThrowError(
        "app.setAccessibilitySupportEnabled() can only be called "
        "after app is ready");
    return;
  }

  auto* ax_state = content::BrowserAccessibilityState::GetInstance();
  if (enabled) {
    ax_state->OnScreenReaderDetected();
  } else {
    ax_state->DisableAccessibility();
  }
  Browser::Get()->OnAccessibilitySupportChanged();
}

Browser::LoginItemSettings App::GetLoginItemSettings(mate::Arguments* args) {
  Browser::LoginItemSettings options;
  args->GetNext(&options);
  return Browser::Get()->GetLoginItemSettings(options);
}

#if defined(USE_NSS_CERTS)
void App::ImportCertificate(const base::DictionaryValue& options,
                            net::CompletionRepeatingCallback callback) {
  auto browser_context = AtomBrowserContext::From("", false);
  if (!certificate_manager_model_) {
    auto copy = base::DictionaryValue::From(
        base::Value::ToUniquePtrValue(options.Clone()));
    CertificateManagerModel::Create(
        browser_context.get(),
        base::BindRepeating(&App::OnCertificateManagerModelCreated,
                            base::Unretained(this), base::Passed(&copy),
                            callback));
    return;
  }

  int rv = ImportIntoCertStore(certificate_manager_model_.get(), options);
  std::move(callback).Run(rv);
}

void App::OnCertificateManagerModelCreated(
    std::unique_ptr<base::DictionaryValue> options,
    net::CompletionOnceCallback callback,
    std::unique_ptr<CertificateManagerModel> model) {
  certificate_manager_model_ = std::move(model);
  int rv =
      ImportIntoCertStore(certificate_manager_model_.get(), *(options.get()));
  std::move(callback).Run(rv);
}
#endif

#if defined(OS_WIN)
v8::Local<v8::Value> App::GetJumpListSettings() {
  JumpList jump_list(Browser::Get()->GetAppUserModelID());

  int min_items = 10;
  std::vector<JumpListItem> removed_items;
  if (jump_list.Begin(&min_items, &removed_items)) {
    // We don't actually want to change anything, so abort the transaction.
    jump_list.Abort();
  } else {
    LOG(ERROR) << "Failed to begin Jump List transaction.";
  }

  auto dict = mate::Dictionary::CreateEmpty(isolate());
  dict.Set("minItems", min_items);
  dict.Set("removedItems", mate::ConvertToV8(isolate(), removed_items));
  return dict.GetHandle();
}

JumpListResult App::SetJumpList(v8::Local<v8::Value> val,
                                mate::Arguments* args) {
  std::vector<JumpListCategory> categories;
  bool delete_jump_list = val->IsNull();
  if (!delete_jump_list &&
      !mate::ConvertFromV8(args->isolate(), val, &categories)) {
    args->ThrowError("Argument must be null or an array of categories");
    return JumpListResult::ARGUMENT_ERROR;
  }

  JumpList jump_list(Browser::Get()->GetAppUserModelID());

  if (delete_jump_list) {
    return jump_list.Delete() ? JumpListResult::SUCCESS
                              : JumpListResult::GENERIC_ERROR;
  }

  // Start a transaction that updates the JumpList of this application.
  if (!jump_list.Begin())
    return JumpListResult::GENERIC_ERROR;

  JumpListResult result = jump_list.AppendCategories(categories);
  // AppendCategories may have failed to add some categories, but it's better
  // to have something than nothing so try to commit the changes anyway.
  if (!jump_list.Commit()) {
    LOG(ERROR) << "Failed to commit changes to custom Jump List.";
    // It's more useful to return the earlier error code that might give
    // some indication as to why the transaction actually failed, so don't
    // overwrite it with a "generic error" code here.
    if (result == JumpListResult::SUCCESS)
      result = JumpListResult::GENERIC_ERROR;
  }

  return result;
}
#endif  // defined(OS_WIN)

v8::Local<v8::Promise> App::GetFileIcon(const base::FilePath& path,
                                        mate::Arguments* args) {
  util::Promise<gfx::Image> promise(isolate());
  v8::Local<v8::Promise> handle = promise.GetHandle();
  base::FilePath normalized_path = path.NormalizePathSeparators();

  IconLoader::IconSize icon_size;
  mate::Dictionary options;
  if (!args->GetNext(&options)) {
    icon_size = IconLoader::IconSize::NORMAL;
  } else {
    std::string icon_size_string;
    options.Get("size", &icon_size_string);
    icon_size = GetIconSizeByString(icon_size_string);
  }

  auto* icon_manager = AtomBrowserMainParts::Get()->GetIconManager();
  gfx::Image* icon =
      icon_manager->LookupIconFromFilepath(normalized_path, icon_size);
  if (icon) {
    promise.Resolve(*icon);
  } else {
    icon_manager->LoadIcon(
        normalized_path, icon_size,
        base::BindOnce(&OnIconDataAvailable, std::move(promise)),
        &cancelable_task_tracker_);
  }
  return handle;
}

std::vector<mate::Dictionary> App::GetAppMetrics(v8::Isolate* isolate) {
  std::vector<mate::Dictionary> result;
  result.reserve(app_metrics_.size());
  int processor_count = base::SysInfo::NumberOfProcessors();

  for (const auto& process_metric : app_metrics_) {
    mate::Dictionary pid_dict = mate::Dictionary::CreateEmpty(isolate);
    mate::Dictionary cpu_dict = mate::Dictionary::CreateEmpty(isolate);

    pid_dict.SetHidden("simple", true);
    cpu_dict.SetHidden("simple", true);

    cpu_dict.Set(
        "percentCPUUsage",
        process_metric.second->metrics->GetPlatformIndependentCPUUsage() /
            processor_count);

#if !defined(OS_WIN)
    cpu_dict.Set("idleWakeupsPerSecond",
                 process_metric.second->metrics->GetIdleWakeupsPerSecond());
#else
    // Chrome's underlying process_metrics.cc will throw a non-fatal warning
    // that this method isn't implemented on Windows, so set it to 0 instead
    // of calling it
    cpu_dict.Set("idleWakeupsPerSecond", 0);
#endif

    pid_dict.Set("cpu", cpu_dict);
    pid_dict.Set("pid", process_metric.second->process.Pid());
    pid_dict.Set("type", content::GetProcessTypeNameInEnglish(
                             process_metric.second->type));
    pid_dict.Set("creationTime",
                 process_metric.second->process.CreationTime().ToJsTime());

#if !defined(OS_LINUX)
    auto memory_info = process_metric.second->GetMemoryInfo();

    mate::Dictionary memory_dict = mate::Dictionary::CreateEmpty(isolate);
    memory_dict.SetHidden("simple", true);
    memory_dict.Set("workingSetSize",
                    static_cast<double>(memory_info.working_set_size >> 10));
    memory_dict.Set(
        "peakWorkingSetSize",
        static_cast<double>(memory_info.peak_working_set_size >> 10));

#if defined(OS_WIN)
    memory_dict.Set("privateBytes",
                    static_cast<double>(memory_info.private_bytes >> 10));
#endif

    pid_dict.Set("memory", memory_dict);
#endif

#if defined(OS_MACOSX)
    pid_dict.Set("sandboxed", process_metric.second->IsSandboxed());
#elif defined(OS_WIN)
    auto integrity_level = process_metric.second->GetIntegrityLevel();
    auto sandboxed = ProcessMetric::IsSandboxed(integrity_level);
    pid_dict.Set("integrityLevel", integrity_level);
    pid_dict.Set("sandboxed", sandboxed);
#endif

    result.push_back(pid_dict);
  }

  return result;
}

v8::Local<v8::Value> App::GetGPUFeatureStatus(v8::Isolate* isolate) {
  auto status = content::GetFeatureStatus();
  base::DictionaryValue temp;
  return mate::ConvertToV8(isolate, status ? *status : temp);
}

v8::Local<v8::Promise> App::GetGPUInfo(v8::Isolate* isolate,
                                       const std::string& info_type) {
  auto* const gpu_data_manager = content::GpuDataManagerImpl::GetInstance();
  util::Promise<base::DictionaryValue> promise(isolate);
  v8::Local<v8::Promise> handle = promise.GetHandle();
  if (info_type != "basic" && info_type != "complete") {
    promise.RejectWithErrorMessage(
        "Invalid info type. Use 'basic' or 'complete'");
    return handle;
  }
  std::string reason;
  if (!gpu_data_manager->GpuAccessAllowed(&reason)) {
    promise.RejectWithErrorMessage("GPU access not allowed. Reason: " + reason);
    return handle;
  }

  auto* const info_mgr = GPUInfoManager::GetInstance();
  if (info_type == "complete") {
#if defined(OS_WIN) || defined(OS_MACOSX)
    info_mgr->FetchCompleteInfo(std::move(promise));
#else
    info_mgr->FetchBasicInfo(std::move(promise));
#endif
  } else /* (info_type == "basic") */ {
    info_mgr->FetchBasicInfo(std::move(promise));
  }
  return handle;
}

static void RemoveNoSandboxSwitch(base::CommandLine* command_line) {
  if (command_line->HasSwitch(service_manager::switches::kNoSandbox)) {
    const base::CommandLine::CharType* noSandboxArg =
        FILE_PATH_LITERAL("--no-sandbox");
    base::CommandLine::StringVector modified_command_line;
    for (auto& arg : command_line->argv()) {
      if (arg.compare(noSandboxArg) != 0) {
        modified_command_line.push_back(arg);
      }
    }
    command_line->InitFromArgv(modified_command_line);
  }
}

void App::EnableSandbox(gin_helper::ErrorThrower thrower) {
  if (Browser::Get()->is_ready()) {
    thrower.ThrowError(
        "app.enableSandbox() can only be called "
        "before app is ready");
    return;
  }

  auto* command_line = base::CommandLine::ForCurrentProcess();
  RemoveNoSandboxSwitch(command_line);
  command_line->AppendSwitch(switches::kEnableSandbox);
}

void App::SetUserAgentFallback(const std::string& user_agent) {
  AtomBrowserClient::Get()->SetUserAgent(user_agent);
}

std::string App::GetUserAgentFallback() {
  return AtomBrowserClient::Get()->GetUserAgent();
}

void App::SetBrowserClientCanUseCustomSiteInstance(bool should_disable) {
  AtomBrowserClient::Get()->SetCanUseCustomSiteInstance(should_disable);
}
bool App::CanBrowserClientUseCustomSiteInstance() {
  return AtomBrowserClient::Get()->CanUseCustomSiteInstance();
}

#if defined(OS_MACOSX)
bool App::MoveToApplicationsFolder(gin_helper::ErrorThrower thrower,
                                   mate::Arguments* args) {
  gin::Arguments gin_args(args->info());
  return AtomBundleMover::Move(thrower, &gin_args);
}

bool App::IsInApplicationsFolder() {
  return AtomBundleMover::IsCurrentAppInApplicationsFolder();
}

int DockBounce(mate::Arguments* args) {
  int request_id = -1;
  std::string type = "informational";
  args->GetNext(&type);

  if (type == "critical")
    request_id = Browser::Get()->DockBounce(Browser::BounceType::CRITICAL);
  else if (type == "informational")
    request_id = Browser::Get()->DockBounce(Browser::BounceType::INFORMATIONAL);
  return request_id;
}

void DockSetMenu(electron::api::Menu* menu) {
  Browser::Get()->DockSetMenu(menu->model());
}

v8::Local<v8::Value> App::GetDockAPI(v8::Isolate* isolate) {
  if (dock_.IsEmpty()) {
    // Initialize the Dock API, the methods are bound to "dock" which exists
    // for the lifetime of "app"
    auto browser = base::Unretained(Browser::Get());
    mate::Dictionary dock_obj = mate::Dictionary::CreateEmpty(isolate);
    dock_obj.SetMethod("bounce", &DockBounce);
    dock_obj.SetMethod(
        "cancelBounce",
        base::BindRepeating(&Browser::DockCancelBounce, browser));
    dock_obj.SetMethod(
        "downloadFinished",
        base::BindRepeating(&Browser::DockDownloadFinished, browser));
    dock_obj.SetMethod(
        "setBadge", base::BindRepeating(&Browser::DockSetBadgeText, browser));
    dock_obj.SetMethod(
        "getBadge", base::BindRepeating(&Browser::DockGetBadgeText, browser));
    dock_obj.SetMethod("hide",
                       base::BindRepeating(&Browser::DockHide, browser));
    dock_obj.SetMethod("show",
                       base::BindRepeating(&Browser::DockShow, browser));
    dock_obj.SetMethod("isVisible",
                       base::BindRepeating(&Browser::DockIsVisible, browser));
    dock_obj.SetMethod("setMenu", &DockSetMenu);
    dock_obj.SetMethod("setIcon",
                       base::BindRepeating(&Browser::DockSetIcon, browser));

    dock_.Reset(isolate, dock_obj.GetHandle());
  }
  return v8::Local<v8::Value>::New(isolate, dock_);
}
#endif

// static
mate::Handle<App> App::Create(v8::Isolate* isolate) {
  return mate::CreateHandle(isolate, new App(isolate));
}

// static
void App::BuildPrototype(v8::Isolate* isolate,
                         v8::Local<v8::FunctionTemplate> prototype) {
  prototype->SetClassName(mate::StringToV8(isolate, "App"));
  auto browser = base::Unretained(Browser::Get());
  mate::ObjectTemplateBuilder(isolate, prototype->PrototypeTemplate())
      .SetMethod("quit", base::BindRepeating(&Browser::Quit, browser))
      .SetMethod("exit", base::BindRepeating(&Browser::Exit, browser))
      .SetMethod("focus", base::BindRepeating(&Browser::Focus, browser))
      .SetMethod("getVersion",
                 base::BindRepeating(&Browser::GetVersion, browser))
      .SetMethod("setVersion",
                 base::BindRepeating(&Browser::SetVersion, browser))
      .SetMethod("_getName", base::BindRepeating(&Browser::GetName, browser))
      .SetMethod("_setName", base::BindRepeating(&Browser::SetName, browser))
      .SetMethod("isReady", base::BindRepeating(&Browser::is_ready, browser))
      .SetMethod("whenReady", base::BindRepeating(&Browser::WhenReady, browser))
      .SetMethod("addRecentDocument",
                 base::BindRepeating(&Browser::AddRecentDocument, browser))
      .SetMethod("clearRecentDocuments",
                 base::BindRepeating(&Browser::ClearRecentDocuments, browser))
      .SetMethod("setAppUserModelId",
                 base::BindRepeating(&Browser::SetAppUserModelID, browser))
      .SetMethod(
          "isDefaultProtocolClient",
          base::BindRepeating(&Browser::IsDefaultProtocolClient, browser))
      .SetMethod(
          "setAsDefaultProtocolClient",
          base::BindRepeating(&Browser::SetAsDefaultProtocolClient, browser))
      .SetMethod(
          "removeAsDefaultProtocolClient",
          base::BindRepeating(&Browser::RemoveAsDefaultProtocolClient, browser))
      .SetMethod("_setBadgeCount",
                 base::BindRepeating(&Browser::SetBadgeCount, browser))
      .SetMethod("_getBadgeCount",
                 base::BindRepeating(&Browser::GetBadgeCount, browser))
      .SetMethod("getLoginItemSettings", &App::GetLoginItemSettings)
      .SetMethod("setLoginItemSettings",
                 base::BindRepeating(&Browser::SetLoginItemSettings, browser))
      .SetMethod("isEmojiPanelSupported",
                 base::BindRepeating(&Browser::IsEmojiPanelSupported, browser))
      .SetProperty("badgeCount",
                   base::BindRepeating(&Browser::GetBadgeCount, browser),
                   base::BindRepeating(&Browser::SetBadgeCount, browser))
      .SetProperty("name", base::BindRepeating(&Browser::GetName, browser),
                   base::BindRepeating(&Browser::SetName, browser))
#if defined(OS_MACOSX)
      .SetMethod("hide", base::BindRepeating(&Browser::Hide, browser))
      .SetMethod("show", base::BindRepeating(&Browser::Show, browser))
      .SetMethod("setUserActivity",
                 base::BindRepeating(&Browser::SetUserActivity, browser))
      .SetMethod("getCurrentActivityType",
                 base::BindRepeating(&Browser::GetCurrentActivityType, browser))
      .SetMethod(
          "invalidateCurrentActivity",
          base::BindRepeating(&Browser::InvalidateCurrentActivity, browser))
      .SetMethod("resignCurrentActivity",
                 base::BindRepeating(&Browser::ResignCurrentActivity, browser))
      .SetMethod("updateCurrentActivity",
                 base::BindRepeating(&Browser::UpdateCurrentActivity, browser))
      .SetMethod("moveToApplicationsFolder", &App::MoveToApplicationsFolder)
      .SetMethod("isInApplicationsFolder", &App::IsInApplicationsFolder)
#endif
      .SetMethod("setAboutPanelOptions",
                 base::BindRepeating(&Browser::SetAboutPanelOptions, browser))
      .SetMethod("showAboutPanel",
                 base::BindRepeating(&Browser::ShowAboutPanel, browser))
#if defined(OS_MACOSX) || defined(OS_WIN)
      .SetMethod("showEmojiPanel",
                 base::BindRepeating(&Browser::ShowEmojiPanel, browser))
      .SetProperty("accessibilitySupportEnabled",
                   &App::IsAccessibilitySupportEnabled,
                   &App::SetAccessibilitySupportEnabled)
#endif
#if defined(OS_WIN)
      .SetMethod("setUserTasks",
                 base::BindRepeating(&Browser::SetUserTasks, browser))
      .SetMethod("getJumpListSettings", &App::GetJumpListSettings)
      .SetMethod("setJumpList", &App::SetJumpList)
#endif
#if defined(OS_LINUX)
      .SetMethod("isUnityRunning",
                 base::BindRepeating(&Browser::IsUnityRunning, browser))
#endif
      .SetMethod("setAppPath", &App::SetAppPath)
      .SetMethod("getAppPath", &App::GetAppPath)
      .SetMethod("setPath", &App::SetPath)
      .SetMethod("getPath", &App::GetPath)
      .SetMethod("setAppLogsPath", &App::SetAppLogsPath)
      .SetMethod("setDesktopName", &App::SetDesktopName)
      .SetMethod("getLocale", &App::GetLocale)
      .SetMethod("getLocaleCountryCode", &App::GetLocaleCountryCode)
#if defined(USE_NSS_CERTS)
      .SetMethod("importCertificate", &App::ImportCertificate)
#endif
      .SetMethod("hasSingleInstanceLock", &App::HasSingleInstanceLock)
      .SetMethod("requestSingleInstanceLock", &App::RequestSingleInstanceLock)
      .SetMethod("releaseSingleInstanceLock", &App::ReleaseSingleInstanceLock)
      .SetMethod("relaunch", &App::Relaunch)
      .SetMethod("_isAccessibilitySupportEnabled",
                 &App::IsAccessibilitySupportEnabled)
      .SetMethod("_setAccessibilitySupportEnabled",
                 &App::SetAccessibilitySupportEnabled)
      .SetMethod("disableHardwareAcceleration",
                 &App::DisableHardwareAcceleration)
      .SetMethod("disableDomainBlockingFor3DAPIs",
                 &App::DisableDomainBlockingFor3DAPIs)
      .SetMethod("getFileIcon", &App::GetFileIcon)
      .SetMethod("getAppMetrics", &App::GetAppMetrics)
      .SetMethod("getGPUFeatureStatus", &App::GetGPUFeatureStatus)
      .SetMethod("getGPUInfo", &App::GetGPUInfo)
#if defined(MAS_BUILD)
      .SetMethod("startAccessingSecurityScopedResource",
                 &App::StartAccessingSecurityScopedResource)
#endif
#if defined(OS_MACOSX)
      .SetProperty("dock", &App::GetDockAPI)
#endif
      .SetProperty("userAgentFallback", &App::GetUserAgentFallback,
                   &App::SetUserAgentFallback)
      .SetMethod("enableSandbox", &App::EnableSandbox)
      .SetProperty("allowRendererProcessReuse",
                   &App::CanBrowserClientUseCustomSiteInstance,
                   &App::SetBrowserClientCanUseCustomSiteInstance);
}

}  // namespace api

}  // namespace electron

namespace {

void Initialize(v8::Local<v8::Object> exports,
                v8::Local<v8::Value> unused,
                v8::Local<v8::Context> context,
                void* priv) {
  v8::Isolate* isolate = context->GetIsolate();
  mate::Dictionary dict(isolate, exports);
  dict.Set("App", electron::api::App::GetConstructor(isolate)
                      ->GetFunction(context)
                      .ToLocalChecked());
  dict.Set("app", electron::api::App::Create(isolate));
}

}  // namespace

NODE_LINKED_MODULE_CONTEXT_AWARE(atom_browser_app, Initialize)
