import sys, traceback
sys.path.insert(0, "scripts")
from swos_plugin import SwosHarness

def log(*a): print(*a, flush=True)
s = SwosHarness(); h = s.h
def iv(e):
    try: return h.int_value(e)
    except Exception: return -999

try:
    log("== boot =="); s.boot()
    s.select("PresetCompetitionMenu")
    s.fire_ordinal(11)
    s.fire_text("PLAY TOURNAMENT", timeout=40)
    log("entered comp")

    last_rc = None
    exit_streak = 0
    for i in range(300):
        init = s._current_init() or ""
        rc = iv("_diyFileBufferCopy.header.roundComplete")
        lb = {o: t for o, t in s.scan_text()}
        e1 = lb.get(1); e10 = lb.get(10)
        vis1 = 1 in lb

        if rc != last_rc or init not in ("DIYLeagueTournamentInit", "DIYCupInit"):
            log(f"[{i}] init={init} rc(slot)={rc} amp={iv('_diyAllMatchesPlayed')} "
                f"activeRC={iv('_diyCompetitionStart.base.roundComplete')} "
                f"mState={iv('_diyCompetitionStart.base.menuState')} "
                f"compFin={iv('_diyCompetitionStart.base.competitionFinished')} "
                f"hg={iv('_diyCompetitionStart.base.hasGroups')} "
                f"isAct={iv('_diyFileBufferCopy.header.isActive')} "
                f"curRIdx={iv('_diyFileBufferCopy.header.currentRoundIndex')} | e1={e1!r} vis1={vis1}")
            last_rc = rc

        if init not in ("DIYLeagueTournamentInit", "DIYCupInit") and i > 0:
            log(">>> LEFT DIY menu; competition ended. init=", init)
            break

        if vis1:
            exit_streak = 0
            s.fire_ordinal(1, timeout=30)
            l2 = {o: t for o, t in s.scan_text()}
            if 8 in l2 and (l2.get(8) or "").upper().startswith("VIEW"):
                s.fire_ordinal(8, timeout=30)
        else:
            # entry 1 hidden -> menuState=6 style, only EXIT (10)
            log(f"[{i}] entry1 HIDDEN (menuState=6?). firing EXIT(10). e10={e10!r}")
            s.fire_ordinal(10, timeout=30)
            exit_streak += 1
            if exit_streak >= 4:
                log(">>> EXIT fired 4x without leaving; stopping"); break

    log("FINAL rc(slot)=", iv("_diyFileBufferCopy.header.roundComplete"),
        "init=", s._current_init())
except Exception as e:
    log("EXC:", e); traceback.print_exc()
