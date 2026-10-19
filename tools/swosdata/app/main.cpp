// OpenSWOS Setup: the window over the swosdata library.
//
// The player names their own copy of SWOS -- the Amiga disks, the PC game
// folder, the Jaguar cartridge -- and SwosData_Run writes the game's data into
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

std::string BaseName(const std::string& p) {
  const size_t cut = p.find_last_of("/\\");
  return cut == std::string::npos ? p : p.substr(cut + 1);
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

std::vector<std::string> PickFiles(const char* filterName, const char* filterSpec, bool multiple) {
  std::vector<std::string> picked;
#ifdef SD_WITH_DIALOGS
  NfdSession s;
  if (!s.ok) return picked;
  const nfdu8filteritem_t filter[1] = {{filterName, filterSpec}};
  if (multiple) {
    const nfdpathset_t* set = nullptr;
    if (NFD_OpenDialogMultipleU8(&set, filter, 1, nullptr) == NFD_OKAY && set) {
      nfdpathsetsize_t n = 0;
      NFD_PathSet_GetCount(set, &n);
      for (nfdpathsetsize_t i = 0; i < n; i++) {
        nfdu8char_t* p = nullptr;
        if (NFD_PathSet_GetPathU8(set, i, &p) == NFD_OKAY && p) {
          picked.emplace_back(p);
          NFD_PathSet_FreePathU8(p);
        }
      }
      NFD_PathSet_Free(set);
    }
  } else {
    nfdu8char_t* p = nullptr;
    if (NFD_OpenDialogU8(&p, filter, 1, nullptr) == NFD_OKAY && p) {
      picked.emplace_back(p);
      NFD_FreePathU8(p);
    }
  }
#else
  (void)filterName;
  (void)filterSpec;
  (void)multiple;
#endif
  return picked;
}

// ------------------------------------------------------- the game's files ---

// The game's own files (setup/glyphs.txt, data/credits_icons.bmp, ...): beside
// the executable in an install; in a dev build, the repo's bin/ above it.
std::string FindResDir(const std::string& exeDir) {
  if (exeDir.empty()) return {};
  if (sd::IsFile(Join(exeDir, "setup/glyphs.txt"))) return exeDir;
  for (std::string d = exeDir; !d.empty();) {
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

  void Refresh() {
    auto rows = std::make_shared<slint::VectorModel<SourceRow>>();

    std::string amiga;
    if (src.adf1[0] && src.adf2[0])
      amiga = Shown(src.adf1) + "   +   " + BaseName(src.adf2);
    else if (src.adf1[0])
      amiga = Shown(src.adf1) + "   (disk 2 not found)";
    else if (src.adf2[0])
      amiga = Shown(src.adf2) + "   (disk 1 not found)";
    else
      amiga = "not found";
    rows->push_back({"Amiga disks (2 .adf)", Str(amiga), bool(SdSources_HaveAmiga(&src))});
    rows->push_back({"PC game folder", Str(src.dosDir[0] ? Shown(src.dosDir) : "not found"),
                     bool(SdSources_HaveDos(&src))});
    rows->push_back({"Jaguar cartridge (optional)", Str(src.jagRom[0] ? Shown(src.jagRom) : "not found"),
                     bool(SdSources_HaveJaguar(&src))});
    w().set_sources(rows);

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
          ->push_back({Str(info[i].name), bool(info[i].ready), Str(info[i].needs)});
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

  void BrowseAmiga() {
    const auto files = PickFiles("Amiga disk images", "adf", true);
    if (files.empty()) return;
    // A fresh set, so a second Browse replaces the disks rather than being
    // refused because a disk is already set.
    SdSources fresh{};
    std::vector<std::string> bad;
    for (const auto& f : files)
      if (SdSources_AddFile(&fresh, f.c_str()) != 0) bad.push_back(BaseName(f));
    if (fresh.adf1[0]) std::memcpy(src.adf1, fresh.adf1, sizeof src.adf1);
    if (fresh.adf2[0]) std::memcpy(src.adf2, fresh.adf2, sizeof src.adf2);
    if (!bad.empty()) {
      std::string names;
      for (const auto& b : bad) names += (names.empty() ? "" : ", ") + b;
      Notice("Not a Sensible World of Soccer disk: " + names +
                 ". The two disks are the ones named SWOS and SWOS2.",
             true);
    } else if (!SdSources_HaveAmiga(&src)) {
      Notice(std::string("Found disk ") + (src.adf1[0] ? "1" : "2") +
                 ". Pick both .adf files (disk 1 and disk 2) together.",
             false);
    } else {
      Notice("Both Amiga disks found.", false);
    }
    Refresh();
  }

  void BrowsePc() {
    const std::string dir = PickFolder(src.dosDir);
    if (dir.empty()) return;
    SdSources fresh{};
    if (SdSources_AddFile(&fresh, dir.c_str()) != 0) {
      Notice("Not the PC game's folder: " + dir +
                 ". It is the folder with SPRITE.DAT and DATA in it (for GOG, the game's install folder).",
             true);
    } else {
      std::memcpy(src.dosDir, fresh.dosDir, sizeof src.dosDir);
      Notice("PC game found.", false);
    }
    Refresh();
  }

  void BrowseJaguar() {
    const auto files = PickFiles("Jaguar cartridge", "jag,j64,rom,bin", false);
    if (files.empty()) return;
    SdSources fresh{};
    if (SdSources_AddFile(&fresh, files[0].c_str()) != 0 || !fresh.jagRom[0]) {
      Notice("Not the Sensible Soccer Jaguar cartridge: " + BaseName(files[0]) +
                 " (a 2 MB .jag, .j64, .rom or .bin).",
             true);
    } else {
      std::memcpy(src.jagRom, fresh.jagRom, sizeof src.jagRom);
      Notice("Jaguar cartridge found.", false);
    }
    Refresh();
  }

  void Scan(const std::string& dir) {
    const int32_t before = (src.adf1[0] != 0) + (src.adf2[0] != 0) + (src.dosDir[0] != 0) +
                           (src.jagRom[0] != 0);
    const int32_t after = SdSources_Scan(&src, dir.c_str());
    if (after > before)
      Notice("Found " + std::to_string(after - before) + " new in " + dir + ".", false);
    else
      Notice("Nothing new found in " + dir + " (it and the folders inside it).", true);
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
      "  --scan DIR   look for the game's sources in DIR (repeatable)\n"
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
    app.Notice("This build has no file picker (built without dbus); start it with --scan DIR.", true);
#endif
  ui->set_status(Str("Ready."));

  ui->on_browse_source([&app](int i) {
    if (i == 0) app.BrowseAmiga();
    else if (i == 1) app.BrowsePc();
    else app.BrowseJaguar();
  });
  ui->on_search_folder([&app] {
    const std::string dir = PickFolder({});
    if (!dir.empty()) app.Scan(dir);
  });
  ui->on_change_out([&app] {
    const std::string dir = PickFolder(sd::IsDir(app.outDir) ? app.outDir : std::string());
    if (dir.empty()) return;
    app.outDir = dir;
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
