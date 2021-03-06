// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/net_error_diagnostics_dialog.h"

// Winsock.h must be included before ndfapi.h.
#include <winsock2.h>  // NOLINT
#include <ndfapi.h>    // NOLINT
#include <windows.h>   // NOLINT

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/native_library.h"
#include "base/scoped_native_library.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_runner.h"
#include "base/threading/thread.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/shell_dialogs/base_shell_dialog_win.h"
#include "ui/views/win/hwnd_util.h"

namespace {

class NetErrorDiagnosticsDialog : public ui::BaseShellDialogImpl {
 public:
  NetErrorDiagnosticsDialog() {}
  ~NetErrorDiagnosticsDialog() override {}

  // NetErrorDiagnosticsDialog implementation.
  void Show(content::WebContents* web_contents,
            const std::string& failed_url,
            const base::Closure& callback) {
    DCHECK(!callback.is_null());

    HWND parent =
        views::HWNDForNativeWindow(web_contents->GetTopLevelNativeWindow());
    if (IsRunningDialogForOwner(parent))
      return;

    RunState run_state = BeginRun(parent);
    run_state.dialog_thread->task_runner()->PostTaskAndReply(
        FROM_HERE,
        base::Bind(&NetErrorDiagnosticsDialog::ShowDialogOnPrivateThread,
                   base::Unretained(this), parent, failed_url),
        base::Bind(&NetErrorDiagnosticsDialog::DiagnosticsDone,
                   base::Unretained(this), run_state, callback));
  }

 private:
  void ShowDialogOnPrivateThread(HWND parent, const std::string& failed_url) {
    NDFHANDLE incident_handle;
    base::string16 failed_url_wide = base::UTF8ToUTF16(failed_url);
    if (!SUCCEEDED(NdfCreateWebIncident(failed_url_wide.c_str(),
                                        &incident_handle))) {
      return;
    }
    NdfExecuteDiagnosis(incident_handle, parent);
    NdfCloseIncident(incident_handle);
  }

  void DiagnosticsDone(RunState run_state, const base::Closure& callback) {
    EndRun(run_state);
    callback.Run();
  }

  DISALLOW_COPY_AND_ASSIGN(NetErrorDiagnosticsDialog);
};

}  // namespace

bool CanShowNetworkDiagnosticsDialog() {
  return true;
}

void ShowNetworkDiagnosticsDialog(content::WebContents* web_contents,
                                  const std::string& failed_url) {
  DCHECK(CanShowNetworkDiagnosticsDialog());

  NetErrorDiagnosticsDialog* dialog = new NetErrorDiagnosticsDialog();
  dialog->Show(
      web_contents, failed_url,
      base::Bind(&base::DeletePointer<NetErrorDiagnosticsDialog>, dialog));
}
