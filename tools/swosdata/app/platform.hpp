// The few things OpenSWOS Setup needs from the operating system, kept out of
// main.cpp so windows.h never meets the generated Slint header.
//
// Every string is UTF-8.
#pragma once

#include <string>

namespace sd {

// The folder the executable is in ("" if it cannot be found).
std::string ExeDir();

// The parent of a path, or "" at the top.
std::string ParentDir(const std::string& path);

bool IsFile(const std::string& path);
bool IsDir(const std::string& path);

// Open a URL in the browser.
bool OpenUrl(const std::string& url);

// Start a program, detached, in `cwd`. False when it could not be started.
bool Launch(const std::string& exe, const std::string& cwd);

// Windows: set the title-bar / taskbar icon from the .exe's icon resource at
// the sizes Windows asks for (Slint hands Windows one bitmap for both). False
// while the window does not exist yet; true elsewhere.
bool ApplyNativeWindowIcon();

// Windows: a GUI-subsystem program has no console; attach the parent's so
// --help prints somewhere. No-op elsewhere.
void AttachParentConsole();

}  // namespace sd
