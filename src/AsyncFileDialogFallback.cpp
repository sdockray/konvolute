#include "MacAsyncFileDialog.h"

#ifndef TARGET_OSX

bool openJsonFileDialogAsync(const std::function<void(const std::string &)> & onComplete) {
	onComplete(std::string());
	return false;
}

#endif
