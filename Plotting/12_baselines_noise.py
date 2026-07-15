"""
Gap 2 (simpler-controller baselines) + Gap 3 (sensory-noise robustness).

Reads validation_positions.txt. Primary metric per experiment:
  E1: correct-settler rate (%)            -> higher = better
  E2: bottom-zone fraction (final_z<73)   -> Tay target ~70%
  E3: aggregation toward source = mean final_x (V3A source at low-X end -> lower = toward source;
      chance/no-response ~ tank centre 50)
Baselines: evolved (V?A) vs RandomBrain (V?A_rand) vs ReactiveRule (V?A_react), Mann-Whitney
evolved-vs-baseline. Noise: metric vs sigma {0,5,10,20,40}% (0 = evolved V?A).
"""
import csv, os, sys, statistics
from collections import defaultdict
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from _valdata import positions_rows  # committed per-agent extracts (replaces raw positions.txt)
import matplotlib; matplotlib.use("Agg")
import matplotlib.pyplot as plt
matplotlib.rcParams.update({"font.size": 13, "axes.titlesize": 14, "axes.labelsize": 13,
                            "xtick.labelsize": 11, "ytick.labelsize": 11})  # one step bigger
from scipy import stats

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_P = os.path.join(REPO, "Plotting", "output", "plots")
OUT_T = os.path.join(REPO, "Plotting", "output", "tables")

def load(scen):
    return [r for r in positions_rows() if (r.get("scenario", "") or "").strip() == scen]
def truthy(v): return str(v).strip() in ("1","true","True")
def fnum(v):
    try: return float(str(v).replace(",", "."))
    except ValueError: return None

def _metric(rows, metric):
    if metric == "correct":
        n = len(rows); c = sum(1 for r in rows if truthy(r.get("correct_settler","")))
        return 100*c/n if n else 0
    if metric == "bottomz":
        zs = [fnum(r.get("final_z")) for r in rows]; zs = [z for z in zs if z is not None]
        return 100*sum(1 for z in zs if z < 73)/len(zs) if zs else 0
    xs = [fnum(r.get("final_x")) for r in rows]; xs = [x for x in xs if x is not None]
    return statistics.mean(xs) if xs else 0

# per-genome-seed samples (evolved V?A + noise runs use GenomeSeeds 1-30 -> 30 groups)
def per_run(scen, metric):
    g = defaultdict(list)
    for r in load(scen):
        g[r["validation_genome_seed"]].append(r)
    return [_metric(rows, metric) for rows in g.values()]

# RandomBrain/ReactiveRule used GenomeSeeds=1-1 x RngSeeds=101-130 and positions.txt does not log the
# RNG seed, so the 30 runs collapse to one genome-seed key. Pool all their agents and bootstrap a CI.
def pooled_ci(scen, metric, nboot=2000):
    rows = load(scen)
    if not rows: return None
    point = _metric(rows, metric)
    import random; random.seed(1)
    by = defaultdict(list)
    for r in rows: by[r["agent_index"]].append(r)   # resample agents
    agents = list(by.values())
    boots = []
    for _ in range(nboot):
        samp = [random.choice(agents) for _ in agents]
        flat = [r for a in samp for r in a]
        boots.append(_metric(flat, metric))
    boots.sort()
    return point, boots[int(0.025*nboot)], boots[int(0.975*nboot)], len(rows)

EXP = {
 "E1": dict(evolved="V1A", metric="correct", unit="correct-settler %", better="high"),
 "E2": dict(evolved="V2A", metric="bottomz", unit="bottom-zone %",     better="target70"),
 "E3": dict(evolved="V3A", metric="meanx",   unit="mean final X",      better="low_toward_src"),
}

# E2 evolved final-config bottom fraction comes from its trajectory file (positions has no V2A row
# for the evolved run; the E2 battery logged trajectories). Per genome seed, z<73 at the last step.
def e2_evolved_bottom():
    import collections
    p = os.path.join(REPO,"coral-larvae-abm","Content","Evolution","validation_trajectory_e2_E2Hb_SUS_H4b.csv")
    if not os.path.exists(p): return []
    seeds = collections.defaultdict(lambda: collections.defaultdict(list))
    with open(p) as f:
        for r in csv.DictReader(f):
            seeds[r["validation_genome_seed"]][r["agent_index"]].append((int(r["sim_step"]),float(r["pos_z"])))
    out=[]
    for ag in seeds.values():
        fz=[sorted(pts)[-1][1] for pts in ag.values()]
        out.append(100*sum(1 for z in fz if z<73)/len(fz))
    return out

# The per-agent rows for the noise sweep (V?A_n0*) and baselines (V?A_rand/_react) live only in
# the simulator's transient positions log, which is not retained in the repo. Their results are
# preserved as the committed figure (noise_robustness.png) + summary CSVs (noise_robustness.csv,
# baselines.csv) and the run-level extracts (validation_results_*.csv). If the per-agent rows are
# absent, do NOT run — that would overwrite the committed artifacts with empty output. Re-running
# requires regenerating the sweeps in-sim (-SensorNoise / -RandomBrain / -ReactiveRule).
if not any(load(EXP[e]["evolved"] + suf) for e in EXP for suf in ("_n005", "_rand", "_react")):
    print("SKIP 12: noise/baseline per-agent rows not present in the committed extracts.")
    print("  Committed record stands: noise_robustness.png + noise_robustness.csv + baselines.csv")
    print("  + run-level validation_results_*.csv. Regenerate the sweeps in-sim to rebuild raw data.")
    sys.exit(0)

print("=== Gap 2: simpler-controller baselines ===")
print("    evolved = median[min,max] over 30 genome seeds; random/reactive = pooled point (95% boot CI)")
base_rows = []
for e, cfg in EXP.items():
    m = cfg["metric"]
    ev = e2_evolved_bottom() if e == "E2" else per_run(cfg["evolved"], m)
    if not ev:
        print(f"{e}: no evolved data, skipping"); continue
    rd = pooled_ci(cfg["evolved"]+"_rand", m)
    rc = pooled_ci(cfg["evolved"]+"_react", m)
    evm = statistics.median(ev) if ev else float('nan')
    def d(ci): return f"{ci[0]:.1f} [{ci[1]:.1f},{ci[2]:.1f}]" if ci else "NO DATA"
    print(f"{e} ({cfg['unit']}): evolved {evm:.1f} [{min(ev):.1f},{max(ev):.1f}] (n={len(ev)}) | "
          f"random {d(rd)} | reactive {d(rc)}")
    base_rows.append(dict(exp=e, unit=cfg["unit"], evolved_median=round(evm,1),
        random_pooled=round(rd[0],1) if rd else None, random_ci=f"[{rd[1]:.1f},{rd[2]:.1f}]" if rd else "",
        reactive_pooled=round(rc[0],1) if rc else None, reactive_ci=f"[{rc[1]:.1f},{rc[2]:.1f}]" if rc else ""))

print("\n=== Gap 3: sensory-noise robustness (median primary metric vs sigma) ===")
noise_rows = []
SIG = [("0",""),("0.05","_n005"),("0.1","_n01"),("0.2","_n02"),("0.4","_n04")]
fig, axes = plt.subplots(1,3, figsize=(14,4))
for ax,(e,cfg) in zip(axes, EXP.items()):
    m = cfg["metric"]; xs=[]; ys=[]
    for sig,suf in SIG:
        scen = cfg["evolved"] + suf
        v = e2_evolved_bottom() if (e=="E2" and sig=="0") else per_run(scen, m)
        if not v: continue
        xs.append(float(sig)*100); ys.append(statistics.median(v))
        noise_rows.append(dict(exp=e, sigma_pct=float(sig)*100, median=round(statistics.median(v),1), n=len(v)))
    ax.plot(xs, ys, "o-", color="#00798c"); ax.set_title(f"{e}: {cfg['unit']} vs noise")
    ax.set_xlabel("sensor noise sigma (% of range)"); ax.grid(alpha=0.3)
    print(f"{e}: " + ", ".join(f"s{int(x)}%={y:.1f}" for x,y in zip(xs,ys)))
fig.tight_layout(); fig.savefig(os.path.join(OUT_P,"noise_robustness.png"), dpi=150)

with open(os.path.join(OUT_T,"baselines.csv"),"w",newline="") as f:
    w=csv.DictWriter(f,fieldnames=list(base_rows[0].keys())); w.writeheader(); w.writerows(base_rows)
with open(os.path.join(OUT_T,"noise_robustness.csv"),"w",newline="") as f:
    w=csv.DictWriter(f,fieldnames=list(noise_rows[0].keys())); w.writeheader(); w.writerows(noise_rows)
print("\nwrote baselines.csv, noise_robustness.csv, noise_robustness.png")
