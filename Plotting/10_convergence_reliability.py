"""
Convergence + reliability of the GA (addresses reviewer R1-2).

Reads per-generation training CSVs (Generation, AvgFitness, MaxFitness,
GeneticDiversity, TotalSettlers, CorrectSettlers, BoundaryContacts) for the
Phase-1 method comparison (Elitism / Truncation / SUS) per experiment, and reports:

  1. Fitness convergence  : median + IQR of AvgFitness & MaxFitness over generations,
                            with the 30 individual seeds faint behind (shows convergence
                            AND run-to-run spread, not a cherry-picked mean).
  2. Genetic diversity    : median diversity over generations (shows the population did
                            not collapse prematurely).
  3. Success rate         : k/30 seeds reaching a competence criterion, reported explicitly
                            rather than hidden in an IQR (the [0, x] tails are real).

Outputs: Plotting/output/plots/convergence_<exp>.png,
         Plotting/output/tables/ga_success_rate.csv
"""
import csv, glob, os, statistics
from collections import defaultdict
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
matplotlib.rcParams.update({"font.size": 13, "axes.titlesize": 14, "axes.labelsize": 13,
                            "xtick.labelsize": 11, "ytick.labelsize": 11, "legend.fontsize": 11})  # one step bigger

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TR = os.path.join(REPO, "Training")
OUT_P = os.path.join(REPO, "Plotting", "output", "plots")
OUT_T = os.path.join(REPO, "Plotting", "output", "tables")
os.makedirs(OUT_P, exist_ok=True); os.makedirs(OUT_T, exist_ok=True)

EXPS = ["e1", "e2", "e3"]
METHODS = ["Elitism", "Truncation", "SUS"]
COLORS = {"Elitism": "#d1495b", "Truncation": "#edae49", "SUS": "#00798c"}
POP = {"e1": 250, "e2": 250, "e3": 250}   # Phase-1 population per experiment

def load_seed(path):
    gens, avg, mx, div, corr, tot = [], [], [], [], [], []
    with open(path) as f:
        for r in csv.DictReader(f):
            gens.append(int(r["Generation"])); avg.append(float(r["AvgFitness"]))
            mx.append(float(r["MaxFitness"])); div.append(float(r["GeneticDiversity"]))
            corr.append(int(r["CorrectSettlers"])); tot.append(int(r["TotalSettlers"]))
    return dict(gen=gens, avg=avg, mx=mx, div=div, corr=corr, tot=tot)

def band(seeds_series):
    """median + IQR across seeds at each generation index."""
    n = min(len(s) for s in seeds_series)
    med, q1, q3 = [], [], []
    for i in range(n):
        vals = sorted(s[i] for s in seeds_series)
        med.append(statistics.median(vals))
        q1.append(vals[len(vals)//4]); q3.append(vals[(3*len(vals))//4])
    return list(range(n)), med, q1, q3

success_rows = []
for exp in EXPS:
    fig, axes = plt.subplots(1, 3, figsize=(15, 4.2))
    for method in METHODS:
        files = sorted(glob.glob(os.path.join(TR, exp, method, f"train_{exp}_{method}_seed_*.csv")))
        if not files:
            continue
        seeds = [load_seed(f) for f in files]
        c = COLORS[method]
        # --- panel 0: AvgFitness convergence (population mean; the GA's optimisation target) ---
        g, med, q1, q3 = band([s["avg"] for s in seeds])
        for s in seeds:
            axes[0].plot(s["gen"][:len(g)], s["avg"][:len(g)], color=c, alpha=0.06, linewidth=0.4)
        axes[0].fill_between(g, q1, q3, color=c, alpha=0.15)
        axes[0].plot(g, med, color=c, linewidth=1.6, label=method)
        # --- panel 1: MaxFitness convergence (best-of-generation = the deployed controller) ---
        gm, medm, q1m, q3m = band([s["mx"] for s in seeds])
        axes[1].fill_between(gm, q1m, q3m, color=c, alpha=0.15)
        axes[1].plot(gm, medm, color=c, linewidth=1.6, label=method)
        # --- panel 2: genetic diversity (did the population collapse prematurely?) ---
        gd, medd, q1d, q3d = band([s["div"] for s in seeds])
        axes[2].fill_between(gd, q1d, q3d, color=c, alpha=0.15)
        axes[2].plot(gd, medd, color=c, linewidth=1.6, label=method)
        # --- reliability (per-experiment criterion) ---
        # E1 biological metric = correct-settler rate; E2/E3 biological outcomes (depth/tracking)
        # are validation-time, not logged per generation, so reliability there is fitness-based:
        # did the GA improve the population mean fitness over its gen-0 (random) baseline?
        avg0 = statistics.median(s["avg"][0] for s in seeds)
        avgF = [s["avg"][-1] for s in seeds]
        improved = sum(1 for v in avgF if v > avg0)
        row = dict(experiment=exp, method=method, n_seeds=len(seeds),
                   gen0_med_avgfit=round(avg0,1), final_med_avgfit=round(statistics.median(avgF),1),
                   seeds_improved_over_gen0=improved)
        if exp == "e1":
            rate = [100*s["corr"][-1]/POP[exp] for s in seeds]
            row["E1_median_correct_pct"] = round(statistics.median(rate),1)
            row["E1_seeds_gt10pct"] = sum(1 for v in rate if v > 10)
            row["E1_seeds_zero"] = sum(1 for v in rate if v == 0)
        success_rows.append(row)
    axes[0].set_title(f"{exp.upper()}  Average fitness (30 seeds faint)"); axes[0].set_xlabel("Generation"); axes[0].legend()
    axes[1].set_title("Maximum fitness (deployed best)"); axes[1].set_xlabel("Generation")
    axes[2].set_title("Genetic diversity"); axes[2].set_xlabel("Generation")
    fig.tight_layout()
    fig.savefig(os.path.join(OUT_P, f"convergence_{exp}.png"), dpi=150)
    plt.close(fig)
    print(f"wrote convergence_{exp}.png")

allkeys = []
for r in success_rows:
    for k in r:
        if k not in allkeys: allkeys.append(k)
with open(os.path.join(OUT_T, "ga_success_rate.csv"), "w", newline="") as f:
    w = csv.DictWriter(f, fieldnames=allkeys)
    w.writeheader(); w.writerows(success_rows)
print("\n=== GA convergence + reliability (Phase-1 methods) ===")
print(f"{'exp':4} {'method':11} {'n':3} {'gen0_avgfit':>11} {'final_avgfit':>12} {'improved/30':>11} {'E1_correct%':>11} {'E1_zero':>7}")
for r in success_rows:
    print(f"{r['experiment']:4} {r['method']:11} {r['n_seeds']:3} {r['gen0_med_avgfit']:>11} "
          f"{r['final_med_avgfit']:>12} {r['seeds_improved_over_gen0']:>11} "
          f"{r.get('E1_median_correct_pct',''):>11} {r.get('E1_seeds_zero',''):>7}")
