#include "MacAsyncFileDialog.h"

#ifdef TARGET_OSX

#import <AppKit/AppKit.h>

bool openJsonFileDialogAsync(const std::function<void(const std::string &)> & onComplete) {
	NSOpenPanel * panel = [NSOpenPanel openPanel];
	[panel setCanChooseFiles:YES];
	[panel setCanChooseDirectories:NO];
	[panel setAllowsMultipleSelection:NO];
	[panel setAllowedFileTypes:@[@"json"]];

	void (^completion)(NSModalResponse) = ^(NSModalResponse result) {
		if (result == NSModalResponseOK && panel.URL) {
			std::string path = [[panel.URL path] UTF8String];
			onComplete(path);
		} else {
			onComplete(std::string());
		}
	};

	// Prefer a document-style sheet attached to the active app window.
	// This is less disruptive than app-modal panels and keeps the app loop alive.
	NSWindow * hostWindow = [NSApp keyWindow];
	if (hostWindow) {
		[panel beginSheetModalForWindow:hostWindow completionHandler:completion];
	} else {
		[panel beginWithCompletionHandler:completion];
	}

	return true;
#endif

