#include "platform.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#else
#include <cerrno>
#include <climits>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#endif

namespace sd {

#ifdef _WIN32
static std::wstring Widen(const std::string& s) {
  if (s.empty()) return {};
  const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), nullptr, 0);
  std::wstring w(size_t(n), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), w.data(), n);
  return w;
}

static std::string Narrow(const std::wstring& w) {
  if (w.empty()) return {};
  const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), int(w.size()), nullptr, 0, nullptr, nullptr);
  std::string s(size_t(n), '\0');
  WideCharToMultiByte(CP_UTF8, 0, w.data(), int(w.size()), s.data(), n, nullptr, nullptr);
  return s;
}
#endif

std::string ParentDir(const std::string& path) {
  size_t end = path.size();
  while (end > 0 && (path[end - 1] == '/' || path[end - 1] == '\\')) --end;
  if (end == 0) return {};
  const size_t cut = path.find_last_of("/\\", end - 1);
  if (cut == std::string::npos) return {};
  if (cut == 0) return path.substr(0, 1);                    // "/usr" -> "/"
  if (cut == 2 && path[1] == ':') return path.substr(0, 3);  // "C:\x" -> "C:\"
  return path.substr(0, cut);
}

std::string ExeDir() {
#ifdef _WIN32
  std::wstring buf(32768, L'\0');
  const DWORD n = GetModuleFileNameW(nullptr, buf.data(), DWORD(buf.size()));
  if (n == 0 || n >= buf.size()) return {};
  buf.resize(n);
  return ParentDir(Narrow(buf));
#elif defined(__APPLE__)
  uint32_t size = 0;
  _NSGetExecutablePath(nullptr, &size);
  std::vector<char> raw(size + 1, '\0');
  if (_NSGetExecutablePath(raw.data(), &size) != 0) return {};
  char real[PATH_MAX];
  if (!realpath(raw.data(), real)) return ParentDir(raw.data());
  return ParentDir(real);
#else
  char buf[PATH_MAX];
  const ssize_t n = readlink("/proc/self/exe", buf, sizeof buf - 1);
  if (n <= 0) return {};
  buf[n] = '\0';
  return ParentDir(buf);
#endif
}

bool IsFile(const std::string& path) {
#ifdef _WIN32
  const DWORD a = GetFileAttributesW(Widen(path).c_str());
  return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
#else
  struct stat st;
  return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
#endif
}

bool IsDir(const std::string& path) {
#ifdef _WIN32
  const DWORD a = GetFileAttributesW(Widen(path).c_str());
  return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
#else
  struct stat st;
  return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
#endif
}

#ifndef _WIN32
// fork twice so the child is not ours to reap, exec without a shell (paths
// have spaces and quotes in them).
static bool RunDetached(const std::vector<const char*>& argv, const char* cwd) {
  const pid_t pid = fork();
  if (pid < 0) return false;
  if (pid == 0) {
    if (fork() == 0) {
      const int devnull = ::open("/dev/null", O_RDWR);
      if (devnull >= 0) {
        dup2(devnull, 0);
        dup2(devnull, 1);
        dup2(devnull, 2);
        if (devnull > 2) close(devnull);
      }
      setsid();
      if (cwd && *cwd && chdir(cwd) != 0) _exit(127);
      execvp(argv[0], const_cast<char* const*>(argv.data()));
      _exit(127);
    }
    _exit(0);
  }
  int status = 0;
  while (waitpid(pid, &status, 0) < 0) {
    if (errno != EINTR) break;
  }
  return true;
}
#endif

bool OpenUrl(const std::string& url) {
#ifdef _WIN32
  return reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", Widen(url).c_str(), nullptr,
                                                 nullptr, SW_SHOWNORMAL)) > 32;
#elif defined(__APPLE__)
  return RunDetached({"open", url.c_str(), nullptr}, nullptr);
#else
  return RunDetached({"xdg-open", url.c_str(), nullptr}, nullptr);
#endif
}

bool Launch(const std::string& exe, const std::string& cwd) {
#ifdef _WIN32
  SHELLEXECUTEINFOW info{};
  info.cbSize = sizeof info;
  info.fMask = SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
  info.lpVerb = L"open";
  const std::wstring file = Widen(exe), dir = Widen(cwd);
  info.lpFile = file.c_str();
  info.lpDirectory = dir.empty() ? nullptr : dir.c_str();
  info.nShow = SW_SHOWNORMAL;
  return ShellExecuteExW(&info) != FALSE;
#else
  if (access(exe.c_str(), X_OK) != 0) return false;
  return RunDetached({exe.c_str(), nullptr}, cwd.c_str());
#endif
}

bool ApplyNativeWindowIcon() {
#ifdef _WIN32
  // Slint exposes no HWND. The largest visible unowned top-level window on this
  // thread is ours (winit also keeps a tiny helper window, hence the floor).
  struct Candidate {
    HWND window = nullptr;
    long long area = 0;
  };
  Candidate best;
  EnumThreadWindows(
      GetCurrentThreadId(),
      [](HWND window, LPARAM out) -> BOOL {
        if (GetWindow(window, GW_OWNER) != nullptr || !IsWindowVisible(window)) return TRUE;
        RECT r{};
        if (!GetWindowRect(window, &r)) return TRUE;
        const long long w = r.right - r.left, h = r.bottom - r.top;
        if (w < 100 || h < 100) return TRUE;
        Candidate* c = reinterpret_cast<Candidate*>(out);
        if (w * h > c->area) {
          c->area = w * h;
          c->window = window;
        }
        return TRUE;
      },
      reinterpret_cast<LPARAM>(&best));
  if (!best.window) return false;
  const HINSTANCE self = GetModuleHandleW(nullptr);
  const auto icon_of = [self](int metric) {
    const int size = GetSystemMetrics(metric);
    return static_cast<HICON>(
        LoadImageW(self, MAKEINTRESOURCEW(1), IMAGE_ICON, size, size, LR_DEFAULTCOLOR));
  };
  if (const HICON s = icon_of(SM_CXSMICON))
    SendMessageW(best.window, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(s));
  if (const HICON b = icon_of(SM_CXICON))
    SendMessageW(best.window, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(b));
  return true;
#else
  return true;
#endif
}

void AttachParentConsole() {
#ifdef _WIN32
  if (AttachConsole(ATTACH_PARENT_PROCESS)) {
    FILE* f = nullptr;
    freopen_s(&f, "CONOUT$", "w", stdout);
    freopen_s(&f, "CONOUT$", "w", stderr);
  }
#endif
}

}  // namespace sd
