// OpenSWOS Setup: the window over the swosdata library.
//
// The player names their own copy of SWOS -- the Amiga disks, the PC game
// folder, the Jaguar cartridge, a SWOS 2020 install for its pitches -- and
// SwosData_Run writes the game's data into
// the assets folder the game reads. The run happens on a worker thread; its log
// and progress reach the window through slint::invoke_from_event_loop.
//
//   openswos-setup [--res DIR] [--scan DIR]... [--out DIR] [--drawer DIR]
//   testing:       [--auto] [--snapshot PREFIX] [--quit] [--play]
//
// --auto starts the install once the window is up (and then fills --drawer, if
// one was given); --snapshot writes PREFIX-start.ppm and, with --auto,
// PREFIX-done.ppm; --quit closes the window when it has finished (exit status 0
// on success); --play presses Play OpenSWOS when the install is done.
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "app.h"
#include "platform.hpp"

extern "C" {
#include "swosdata.h"
}

#ifdef SD_WITH_DIALOGS
#include <nfd.h>
#endif

namespace {

slint::SharedString Str(const std::string& s) { return slint::SharedString(s); }

std::string Join(const std::string& a, const std::string& b) {
  if (a.empty()) return b;
  const char last = a.back();
  return (last == '/' || last == '\\') ? a + b : a + "/" + b;
}

// A path as the platform writes it. The library joins with '/', which Windows
// accepts but which reads oddly next to a picked "D:\Games\...".
std::string Shown(std::string p) {
#ifdef _WIN32
  for (char& c : p)
    if (c == '/') c = '\\';
#endif
  return p;
}

// --------------------------------------------------------------- pickers ---

#ifdef SD_WITH_DIALOGS
struct NfdSession {
  bool ok;
  NfdSession() : ok(NFD_Init() == NFD_OKAY) {}
  ~NfdSession() {
    if (ok) NFD_Quit();
  }
};
#endif

std::string PickFolder(const std::string& start) {
#ifdef SD_WITH_DIALOGS
  NfdSession s;
  if (!s.ok) return {};
  nfdu8char_t* out = nullptr;
  std::string picked;
  if (NFD_PickFolderU8(&out, start.empty() ? nullptr : start.c_str()) == NFD_OKAY && out) {
    picked = out;
    NFD_FreePathU8(out);
  }
  return picked;
#else
  (void)start;
  return {};
#endif
}

// One file, of the kinds `filterSpec` lists ("adf", "jag,j64"): "" when the
// player cancelled.
std::string PickFile(const char* filterName, const char* filterSpec) {
  std::string picked;
#ifdef SD_WITH_DIALOGS
  NfdSession s;
  if (!s.ok) return picked;
  const nfdu8filteritem_t filter[1] = {{filterName, filterSpec}};
  nfdu8char_t* p = nullptr;
  if (NFD_OpenDialogU8(&p, filter, 1, nullptr) == NFD_OKAY && p) {
    picked = p;
    NFD_FreePathU8(p);
  }
#else
  (void)filterName;
  (void)filterSpec;
#endif
  return picked;
}

// ------------------------------------------------------- the game's files ---

// The game's own files (setup/glyphs.txt, data/credits_icons.bmp, ...): beside
// the executable in an install; above it in OpenSWOS-Setup.zip; in a dev build,
// the repo's bin/ above it.
#if defined(__APPLE__)
// On macOS the setup is its own .app beside SWOS.app (SWOS-macOS-app.zip), and
// the game's files are inside that: SWOS.app/Contents/MacOS. exeDir is
// "OpenSWOS Setup.app/Contents/MacOS", three levels below the folder they share.
static std::string GameAppMacOSDir(const std::string& exeDir) {
  const std::string shared = sd::ParentDir(sd::ParentDir(sd::ParentDir(exeDir)));
  const std::string dir = Join(shared, "SWOS.app/Contents/MacOS");
  return sd::IsDir(dir) ? dir : std::string();
}
#endif

std::string FindResDir(const std::string& exeDir) {
  if (exeDir.empty()) return {};
  if (sd::IsFile(Join(exeDir, "setup/glyphs.txt"))) return exeDir;
#if defined(__APPLE__)
  {
    const std::string mac = GameAppMacOSDir(exeDir);
    if (!mac.empty() && sd::IsFile(Join(mac, "setup/glyphs.txt"))) return mac;
  }
#endif
  // Above it: OpenSWOS-Setup.zip keeps them above its Windows/Linux/macOS
  // folders; the repo keeps them in bin/.
  for (std::string d = exeDir; !d.empty();) {
    if (sd::IsFile(Join(d, "setup/glyphs.txt"))) return d;
    const std::string bin = Join(d, "bin");
    if (sd::IsFile(Join(bin, "setup/glyphs.txt"))) return bin;
    const std::string up = sd::ParentDir(d);
    if (up == d) break;
    d = up;
  }
  return {};
}

// The game itself, beside this program.
std::string FindGame(const std::string& exeDir) {
  if (exeDir.empty()) return {};
#if defined(_WIN32)
  const char* names[] = {"SWOS.exe"};
#elif defined(__APPLE__)
  const char* names[] = {"SWOS"};
#elif defined(__x86_64__)
  const char* names[] = {"SWOS.x86_64", "SWOS"};
#elif defined(__aarch64__)
  const char* names[] = {"SWOS.aarch64", "SWOS"};
#elif defined(__arm__)
  const char* names[] = {"SWOS.armhf", "SWOS"};
#else
  const char* names[] = {"SWOS"};
#endif
  for (const char* n : names) {
    const std::string p = Join(exeDir, n);
    if (sd::IsFile(p)) return p;
  }
#if defined(__APPLE__)
  {
    const std::string mac = GameAppMacOSDir(exeDir);
    if (!mac.empty() && sd::IsFile(Join(mac, "SWOS"))) return Join(mac, "SWOS");
  }
#endif
  return {};
}

// ------------------------------------------------------------------- app ---

// What the worker hands back. Lines are batched: the worker appends under the
// mutex and posts one flush at a time, so a run that logs thousands of files
// wakes the event loop a few times a frame rather than once a line.
struct Pipe {
  std::mutex mu;
  std::vector<std::string> lines;
  int32_t done = 0, total = 0;
  bool flushQueued = false;
  std::function<void()> flush;  // set by the App; runs on the UI thread
};

void PipeLog(void* user, const char* line) {
  Pipe* p = static_cast<Pipe*>(user);
  bool post = false;
  {
    std::lock_guard<std::mutex> lock(p->mu);
    p->lines.emplace_back(line ? line : "");
    if (!p->flushQueued) post = p->flushQueued = true;
  }
  if (post) slint::invoke_from_event_loop(p->flush);
}

void PipeProgress(void* user, int32_t done, int32_t total) {
  Pipe* p = static_cast<Pipe*>(user);
  bool post = false;
  {
    std::lock_guard<std::mutex> lock(p->mu);
    p->done = done;
    p->total = total;
    if (!p->flushQueued) post = p->flushQueued = true;
  }
  if (post) slint::invoke_from_event_loop(p->flush);
}

struct App {
  slint::ComponentHandle<MainWindow>* ui = nullptr;
  SdSources src{};
  std::string outDir, defaultOutDir, resDir, exeDir, game, drawer;
  std::shared_ptr<slint::VectorModel<slint::SharedString>> log =
      std::make_shared<slint::VectorModel<slint::SharedString>>();
  Pipe pipe;
  std::thread worker;
  bool busy = false;

  // Testing hooks.
  bool autoRun = false, quitAfter = false, autoPlay = false;
  std::string snapshot;
  int exitCode = 0;

  MainWindow& w() { return **ui; }

  void Notice(const std::string& text, bool error) {
    w().set_notice(Str(text));
    w().set_notice_error(error);
  }

  // ------------------------------------------------------------ the rows ---

  // The five sources, each a path the player can type, paste, browse for or
  // have found by "Choose a folder". A path that is not the thing its row asks
  // for stays in the box (so it can be corrected), marked, and is not used.
  enum Row { ROW_PC, ROW_DISK1, ROW_DISK2, ROW_JAG, ROW_2020, ROW_COUNT };
  std::string unusable[ROW_COUNT];
  std::shared_ptr<slint::VectorModel<SourceRow>> rows;

  char* RowPath(int r) {
    switch (r) {
      case ROW_PC: return src.dosDir;
      case ROW_DISK1: return src.adf1;
      case ROW_DISK2: return src.adf2;
      case ROW_JAG: return src.jagRom;
      default: return src.s2020Dir;
    }
  }

  SourceRow MakeRow(int r) {
    static const char* const kLabels[ROW_COUNT] = {
        "PC game folder", "Amiga disk 1", "Amiga disk 2", "Jaguar cartridge (optional)", "SWOS 2020 (optional)"};
    static const char* const kHints[ROW_COUNT] = {
        "the folder with SPRITE.DAT and DATA in it", "disk 1 image (.adf)", "disk 2 image (.adf)",
        "Sensible Soccer International Edition (.jag)", "the SWOS 2020 install folder"};
    SourceRow row;
    row.label = kLabels[r];
    row.placeholder = kHints[r];
    const char* path = RowPath(r);
    row.value = Str(!unusable[r].empty() ? unusable[r] : path[0] ? Shown(path) : std::string());
    row.state = !unusable[r].empty() ? 2 : path[0] ? 1 : 0;
    return row;
  }

  // In place: rebuilding the model would take the focus from the box the
  // player is typing in.
  void RefreshRows() {
    if (!rows) {
      rows = std::make_shared<slint::VectorModel<SourceRow>>();
      for (int r = 0; r < ROW_COUNT; r++) rows->push_back(MakeRow(r));
      w().set_sources(rows);
      return;
    }
    for (int r = 0; r < ROW_COUNT; r++) rows->set_row_data(r, MakeRow(r));
  }

  // A pasted path: spaces around it and the quotes Windows' "Copy as path"
  // puts round it go; ~/ is the home folder.
  static std::string CleanPath(std::string p) {
    while (!p.empty() && (p.back() == ' ' || p.back() == '\t' || p.back() == '\r' || p.back() == '\n')) p.pop_back();
    size_t b = 0;
    while (b < p.size() && (p[b] == ' ' || p[b] == '\t')) b++;
    p = p.substr(b);
    if (p.size() >= 2 && ((p.front() == '"' && p.back() == '"') || (p.front() == '\'' && p.back() == '\'')))
      p = p.substr(1, p.size() - 2);
#ifndef _WIN32
    if (p.rfind("~/", 0) == 0) {
      const char* home = std::getenv("HOME");
      if (home) p = std::string(home) + p.substr(1);
    }
#endif
    return p;
  }

  // Put `text` in row r. Returns what to tell the player ("" for nothing).
  std::string SetRow(int r, const std::string& text) {
    const std::string path = CleanPath(text);
    char* dst = RowPath(r);
    unusable[r].clear();
    if (path.empty()) {
      dst[0] = '\0';
      return "";
    }
    SdSources found{};
    SdSources_AddFile(&found, path.c_str());
    auto take = [](char* to, const char* from) { std::snprintf(to, 1024, "%s", from); };
    switch (r) {
      case ROW_PC:
        if (found.dosDir[0]) { take(dst, found.dosDir); return ""; }
        dst[0] = '\0';
        unusable[r] = path;
        return found.s2020Dir[0] ? "That is SWOS 2020, not the PC game: it goes in the SWOS 2020 row."
                                 : "Not the PC game's folder: it is the one with SPRITE.DAT and DATA in it.";
      case ROW_DISK1:
      case ROW_DISK2: {
        const int other = r == ROW_DISK1 ? ROW_DISK2 : ROW_DISK1;
        const char* mine = r == ROW_DISK1 ? found.adf1 : found.adf2;
        const char* theirs = r == ROW_DISK1 ? found.adf2 : found.adf1;
        if (mine[0]) { take(dst, mine); return ""; }
        dst[0] = '\0';
        if (theirs[0]) {  // the other disk: put it where it belongs
          take(RowPath(other), theirs);
          unusable[other].clear();
          return std::string("That is disk ") + (other == ROW_DISK1 ? "1" : "2") + ", so it went in that row.";
        }
        unusable[r] = path;
        return "Not a Sensible World of Soccer disk image (.adf). The two disks are named SWOS and SWOS2.";
      }
      case ROW_JAG:
        if (found.jagRom[0]) { take(dst, found.jagRom); return ""; }
        dst[0] = '\0';
        unusable[r] = path;
        return "Not the Sensible Soccer Jaguar cartridge (a 2 MB .jag, .j64, .rom or .bin).";
      default:
        if (found.s2020Dir[0]) { take(dst, found.s2020Dir); return ""; }
        dst[0] = '\0';
        unusable[r] = path;
        return "Not a SWOS 2020 install: it is the folder with gameLauncher.exe and the swos folder in it.";
    }
  }

  // While typing: only the mark and the plan follow, nothing is said yet.
  void RowEdited(int r, const std::string& text) {
    SetRow(r, text);
    SourceRow row = MakeRow(r);
    row.value = Str(text);
    rows->set_row_data(r, row);
    RefreshPlan();
  }

  // Enter, or the box loses focus: the cleaned path, and what is wrong with it.
  void RowCommitted(int r, const std::string& text) {
    const std::string msg = SetRow(r, text);
    Notice(msg, !msg.empty() && msg.rfind("That is disk", 0) != 0);
    RefreshRows();
    RefreshPlan();
  }

  void BrowseRow(int r) {
    std::string picked;
    const char* current = RowPath(r);
    switch (r) {
      case ROW_PC:
      case ROW_2020: picked = PickFolder(current[0] ? current : std::string()); break;
      case ROW_DISK1:
      case ROW_DISK2: picked = PickFile("Amiga disk image", "adf"); break;
      default: picked = PickFile("Jaguar cartridge", "jag,j64,rom,bin"); break;
    }
    if (picked.empty()) return;
    RowCommitted(r, picked);
  }

  void Refresh() {
    RefreshRows();
    RefreshPlan();
  }

  // ------------------------------------------------------- installed ---

  // What the assets folder already has: read at launch, after an install, and
  // when the folder is changed.
  SdInstalled installed[SD_STEP_COUNT] = {};

  int InstalledState(int s) const {
    const SdInstalled& n = installed[s];
    return n.total > 0 && n.present == n.total ? 1 : n.present > 0 ? 2 : 0;
  }

  // Enough to play: the Amiga files, or the PC version's (its sound plus the
  // AGA graphics made from it); and the teams either way.
  bool Playable() const {
    return (InstalledState(SD_STEP_AMIGA_FILES) == 1 ||
            (InstalledState(SD_STEP_DOS_FILES) == 1 && InstalledState(SD_STEP_AGA_PC) == 1)) &&
           InstalledState(SD_STEP_TEAMS) == 1;
  }

  void CheckInstalled() {
    SwosData_Installed(outDir.c_str(), installed);
    if (busy) return;
    const bool playable = Playable();
    w().set_done(playable);
    w().set_status_error(false);
    w().set_progress(playable ? 1.0f : 0.0f);
    game = FindGame(exeDir);
    w().set_can_play(playable && !game.empty());
    bool any = false;
    for (int s = 0; s < SD_STEP_COUNT; s++) any = any || installed[s].present > 0;
    if (playable)
      w().set_status(Str("The game's data is already installed in " + Shown(outDir) +
                         (game.empty() ? ". Install again from your copy to add to it."
                                       : ". Play, or install again from your copy to add to it.")));
    else if (any)
      w().set_status(Str("Some of the game's data is in " + Shown(outDir) +
                         ", but not enough to play: install from your copy."));
    else
      w().set_status(Str("Ready."));
  }

  // Everything below the sources: where it goes and what will be made.
  void RefreshPlan() {
    w().set_out_dir(Str(Shown(outDir)));
    w().set_out_note(Shown(outDir) == Shown(defaultOutDir)
                         ? Str("This is the folder the game reads its data from.")
                         : Str("The game reads " + Shown(defaultOutDir) +
                               " — to use this folder instead, start the game with SWOS_ASSETS set to it."));

    SdJob job{};
    job.src = src;
    SdStepInfo info[SD_STEP_COUNT];
    SwosData_Plan(&job, info);
    auto left = std::make_shared<slint::VectorModel<PlanStep>>();
    auto right = std::make_shared<slint::VectorModel<PlanStep>>();
    bool any = false;
    for (int i = 0; i < SD_STEP_COUNT; i++) {
      (i < (SD_STEP_COUNT + 1) / 2 ? left : right)
          ->push_back({Str(info[i].name), bool(info[i].ready), Str(info[i].needs), InstalledState(i)});
      any = any || info[i].ready;
    }
    w().set_plan_left(left);
    w().set_plan_right(right);
    w().set_can_install(any && !outDir.empty());
    w().set_drawer(Str(Shown(drawer)));
  }

  void Log(const std::string& line) { log->push_back(Str(line)); }

  // Runs on the UI thread: move what the worker produced into the window.
  void Flush() {
    std::vector<std::string> lines;
    int32_t done, total;
    {
      std::lock_guard<std::mutex> lock(pipe.mu);
      lines.swap(pipe.lines);
      done = pipe.done;
      total = pipe.total;
      pipe.flushQueued = false;
    }
    for (const std::string& l : lines) Log(l);
    if (total > 0) w().set_progress(float(done) / float(total));
  }

  // ----------------------------------------------------------- sources ---

  // The player chooses folders; each is searched, with every folder inside it,
  // on the worker (a big folder takes a while), and what it finds is added to
  // what earlier folders found. Stop keeps what was found so far.
  std::atomic<bool> stopScan{false};

  static int32_t CountFound(const SdSources& s) {
    return (s.adf1[0] != 0) + (s.adf2[0] != 0) + (s.dosDir[0] != 0) + (s.jagRom[0] != 0) +
           (s.s2020Dir[0] != 0);
  }

  struct ScanProgress {
    App* app;
    std::chrono::steady_clock::time_point last;
  };

  static int32_t OnScanFolder(void* user, int32_t folders, const char* dir) {
    ScanProgress* p = static_cast<ScanProgress*>(user);
    const auto now = std::chrono::steady_clock::now();
    if (now - p->last >= std::chrono::milliseconds(100)) {
      p->last = now;
      App* app = p->app;
      const std::string text = "Searching... " + std::to_string(folders) + " folders: " + Shown(dir);
      slint::invoke_from_event_loop([app, text] { app->w().set_scan_status(Str(text)); });
    }
    return p->app->stopScan.load() ? 1 : 0;
  }

  void StartScan(const std::string& dir) {
    if (busy) return;
    if (worker.joinable()) worker.join();
    SetBusy(true);
    stopScan = false;
    w().set_scanning(true);
    w().set_scan_status(Str("Searching " + Shown(dir) + " ..."));
    Notice("", false);
    const SdSources start = src;
    worker = std::thread([this, dir, start] {
      SdSources found = start;
      ScanProgress progress{this, std::chrono::steady_clock::now()};
      const int32_t rc = SdSources_ScanEx(&found, dir.c_str(), OnScanFolder, &progress);
      slint::invoke_from_event_loop([this, dir, found, rc, start] { FinishScan(dir, found, rc < 0, start); });
    });
  }

  void FinishScan(const std::string& dir, const SdSources& found, bool stopped, const SdSources& before) {
    SetBusy(false);
    w().set_scanning(false);
    w().set_scan_status("");
    src = found;
    for (int r = 0; r < ROW_COUNT; r++)
      if (RowPath(r)[0]) unusable[r].clear();
    const int32_t added = CountFound(found) - CountFound(before);
    std::string text;
    bool error = false;
    if (added > 0)
      text = "Found " + std::to_string(added) + " in " + Shown(dir) + ".";
    else {
      text = "Nothing new in " + Shown(dir) + " or the folders inside it.";
      error = true;
    }
    if (stopped) text += " (Stopped before the end.)";
    if (!SdSources_HaveDos(&src) && !SdSources_HaveAmiga(&src))
      text += " The game needs the PC version's folder or both Amiga disks.";
    else if (src.adf1[0] != 0 && src.adf2[0] == 0)
      text += " Disk 2 of the Amiga version is still missing.";
    else if (src.adf2[0] != 0 && src.adf1[0] == 0)
      text += " Disk 1 of the Amiga version is still missing.";
    Notice(text, error);
    Refresh();
  }

  void ClearSources() {
    if (busy) return;
    src = SdSources{};
    for (auto& u : unusable) u.clear();
    Notice("", false);
    Refresh();
  }

  // ------------------------------------------------------------ install ---

  void SetBusy(bool b) {
    busy = b;
    w().set_busy(b);
  }

  void Install() {
    if (busy) return;
    if (worker.joinable()) worker.join();
    SetBusy(true);
    w().set_done(false);
    w().set_status_error(false);
    w().set_progress(0.0f);
    w().set_status(Str("Installing to " + Shown(outDir) + " ..."));
    log->clear();
    if (resDir.empty())
      Log("warning: the game's own files (setup/glyphs.txt) were not found beside this program; "
          "start it with --res DIR");

    auto job = std::make_shared<SdJob>();
    std::memset(job.get(), 0, sizeof *job);
    job->src = src;
    job->src.disk1 = job->src.disk2 = nullptr;
    std::snprintf(job->outDir, sizeof job->outDir, "%s", outDir.c_str());
    std::snprintf(job->resDir, sizeof job->resDir, "%s", resDir.c_str());
    job->log = PipeLog;
    job->progress = PipeProgress;
    job->user = &pipe;

    worker = std::thread([this, job] {
      char err[256] = {0};
      const int32_t rc = SwosData_Run(job.get(), err);
      SdSources_Close(&job->src);
      const std::string msg(err);
      slint::invoke_from_event_loop([this, rc, msg] { FinishInstall(rc, msg); });
    });
  }

  void FinishInstall(int32_t rc, const std::string& err) {
    Flush();
    SetBusy(false);
    SwosData_Installed(outDir.c_str(), installed);
    RefreshPlan();
    if (rc == 0) {
      w().set_progress(1.0f);
      w().set_done(true);
      game = FindGame(exeDir);
      w().set_can_play(!game.empty());
      w().set_status(Str("Done. The game's data is in " + Shown(outDir) + "."));
      exitCode = 0;
    } else {
      w().set_status_error(true);
      w().set_status(Str("Failed: " + err));
      Log("failed: " + err);
      exitCode = 1;
    }
    if (autoRun && rc == 0 && !drawer.empty()) {
      FillDrawer();  // AutoFinished runs when it has
      return;
    }
    AutoFinished();
  }

  // Testing: what --snapshot / --play / --quit do once the work is done.
  void AutoFinished() {
    if (!autoRun) return;
    // Give the window a moment to draw the final state first.
    slint::Timer::single_shot(std::chrono::milliseconds(400), [this] {
      if (!snapshot.empty()) Snapshot(snapshot + "-done.ppm");
      if (autoPlay && w().get_can_play()) Play();
      else if (quitAfter) slint::quit_event_loop();
    });
  }

  void Play() {
    if (game.empty()) return;
    if (sd::Launch(game, exeDir)) {
      slint::quit_event_loop();
    } else {
      w().set_status_error(true);
      w().set_status(Str("Could not start " + game + "."));
    }
  }

  // ------------------------------------------------------- amiga drawer ---

  void FillDrawer() {
    if (busy || drawer.empty()) return;
    if (worker.joinable()) worker.join();
    SetBusy(true);
    w().set_drawer_status(Str("Copying..."));
    w().set_drawer_error(false);
    const std::string assets = outDir, dir = drawer;
    worker = std::thread([this, assets, dir] {
      char err[256] = {0};
      const int32_t rc = SwosData_FillAmigaDrawer(assets.c_str(), dir.c_str(), PipeLog, &pipe, err);
      const std::string msg(err);
      slint::invoke_from_event_loop([this, rc, msg] {
        Flush();
        SetBusy(false);
        w().set_drawer_error(rc != 0);
        w().set_drawer_status(rc == 0 ? Str("Done: the drawer has the game's data. Copy it to the Amiga.")
                                      : Str("Failed: " + msg));
        if (rc != 0) exitCode = 1;
        AutoFinished();
      });
    });
  }

  // ------------------------------------------------------------ testing ---

  void Snapshot(const std::string& path) {
    auto shot = w().window().take_snapshot();
    if (!shot) {
      std::fprintf(stderr, "snapshot: the window could not be captured\n");
      return;
    }
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return;
    const uint32_t width = shot->width(), height = shot->height();
    std::fprintf(f, "P6\n%u %u\n255\n", width, height);
    const slint::Rgba8Pixel* px = shot->begin();
    std::vector<uint8_t> row(size_t(width) * 3);
    for (uint32_t y = 0; y < height; y++) {
      for (uint32_t x = 0; x < width; x++) {
        const slint::Rgba8Pixel& p = px[size_t(y) * width + x];
        row[x * 3 + 0] = p.r;
        row[x * 3 + 1] = p.g;
        row[x * 3 + 2] = p.b;
      }
      std::fwrite(row.data(), 1, row.size(), f);
    }
    std::fclose(f);
    std::printf("snapshot: %s (%ux%u)\n", path.c_str(), width, height);
    std::fflush(stdout);
  }
};

int Usage() {
  sd::AttachParentConsole();
  std::printf(
      "OpenSWOS Setup %s\n"
      "usage: openswos-setup [--res DIR] [--scan DIR]... [--out DIR] [--drawer DIR]\n"
      "  --res DIR    the game's own files (with setup/glyphs.txt); default: beside\n"
      "               this program, else the nearest bin/ above it\n"
      "  --scan DIR   look for the game's sources in DIR and every folder inside\n"
      "               it (repeatable)\n"
      "  --out DIR    the assets folder to write (default: the one the game reads)\n"
      "  --drawer DIR an unpacked SWOS-Amiga.zip drawer to add the game data to\n"
      "testing: --auto (install, then fill --drawer)  --snapshot PREFIX  --quit  --play\n",
      SD_VERSION);
  std::fflush(stdout);
  return 2;
}

}  // namespace

int main(int argc, char** argv) {
  App app;
  app.exeDir = sd::ExeDir();
  std::vector<std::string> scans;
  std::string resArg, outArg;

  for (int i = 1; i < argc; i++) {
    const std::string a = argv[i];
    const char* v = i + 1 < argc ? argv[i + 1] : nullptr;
    if (a == "--auto") {
      app.autoRun = true;
    } else if (a == "--quit") {
      app.quitAfter = true;
    } else if (a == "--play") {
      app.autoPlay = true;
    } else if (a == "--help" || a == "-h") {
      Usage();
      return 0;
    } else if ((a == "--res" || a == "--scan" || a == "--out" || a == "--snapshot" ||
                a == "--drawer") && v) {
      if (a == "--res") resArg = v;
      else if (a == "--drawer") app.drawer = v;
      else if (a == "--scan") scans.emplace_back(v);
      else if (a == "--out") outArg = v;
      else app.snapshot = v;
      i++;
    } else {
      return Usage();
    }
  }

  app.resDir = !resArg.empty() ? resArg : FindResDir(app.exeDir);
  char def[1024];
  SwosData_DefaultAssetsDir(def, sizeof def);
  app.defaultOutDir = def;
  app.outDir = !outArg.empty() ? outArg : app.defaultOutDir;

  // Wayland matches this against a .desktop file for the icon; X11 and
  // Windows/macOS use the Window's own icon.
  slint::set_xdg_app_id("openswos-setup");

  auto ui = MainWindow::create();
  app.ui = &ui;
  app.pipe.flush = [&app] { app.Flush(); };
  ui->set_log(app.log);
#ifndef SD_WITH_DIALOGS
  ui->set_can_pick(false);
#endif

  for (const auto& s : scans) SdSources_Scan(&app.src, s.c_str());
  app.Refresh();
  if (app.resDir.empty())
    app.Notice("The game's own files (setup/glyphs.txt) were not found beside this program. "
               "Start it with --res DIR, or install the game first.",
               true);
#ifndef SD_WITH_DIALOGS
  else
    app.Notice("This build has no folder picker (built without dbus); start it with --scan DIR.", true);
#endif
  // What an earlier run left: Play is there at once if the game can be played.
  app.CheckInstalled();
  app.RefreshPlan();

  ui->on_choose_folder([&app] {
    const std::string dir = PickFolder({});
    if (!dir.empty()) app.StartScan(dir);
  });
  ui->on_stop_scan([&app] { app.stopScan = true; });
  ui->on_browse_source([&app](int r) { app.BrowseRow(r); });
  ui->on_source_edited([&app](int r, const slint::SharedString& t) { app.RowEdited(r, std::string(t)); });
  ui->on_source_committed([&app](int r, const slint::SharedString& t) { app.RowCommitted(r, std::string(t)); });
  ui->on_clear_sources([&app] { app.ClearSources(); });
  ui->on_change_out([&app] {
    const std::string dir = PickFolder(sd::IsDir(app.outDir) ? app.outDir : std::string());
    if (dir.empty()) return;
    app.outDir = dir;
    app.CheckInstalled();
    app.Refresh();
  });
  ui->on_install([&app] { app.Install(); });
  ui->on_play([&app] { app.Play(); });
  ui->on_open_url([](const slint::SharedString& url) { sd::OpenUrl(std::string(url)); });
  ui->on_browse_drawer([&app] {
    const std::string dir = PickFolder(app.drawer);
    if (dir.empty()) return;
    app.drawer = dir;
    app.w().set_drawer_status("");
    app.Refresh();
  });
  ui->on_fill_drawer([&app] { app.FillDrawer(); });

  // Windows only: the title-bar icon from the resource, once the window exists.
  slint::Timer icon_timer;
  int icon_tries = 0;
  icon_timer.start(slint::TimerMode::Repeated, std::chrono::milliseconds(100),
                   [&icon_timer, &icon_tries] {
                     if (sd::ApplyNativeWindowIcon() || ++icon_tries >= 30) icon_timer.stop();
                   });

  if (!app.snapshot.empty())
    slint::Timer::single_shot(std::chrono::milliseconds(1200),
                              [&app] { app.Snapshot(app.snapshot + "-start.ppm"); });
  if (app.autoRun)
    slint::Timer::single_shot(std::chrono::milliseconds(1500), [&app] { app.Install(); });

  ui->run();

  // Closed mid-install: let the worker finish writing rather than cut a file
  // in half. Its posts to the stopped event loop are dropped.
  if (app.worker.joinable()) app.worker.join();
  return app.exitCode;
}
