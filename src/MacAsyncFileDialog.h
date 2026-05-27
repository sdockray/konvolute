#pragma once

#include <functional>
#include <string>

// Opens a non-blocking file picker on macOS and invokes callback with the selected path.
// Callback receives an empty string when cancelled.
bool openJsonFileDialogAsync(const std::function<void(const std::string &)> & onComplete);
