"""E1 patch-size (beta_patch) figure: correct-settler rate vs maximum tile area, controlled sweep
(fixed CCA cover 0.41). Reads the V1P05/15/30/57 scenarios from validation_positions.txt."""
import csv, statistics, os, sys
from collections import defaultdict
import matplotlib; matplotlib.use("Agg")
import matplotlib.pyplot as plt
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from _plotstyle import apply_style, ACCENT
apply_style()

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from _valdata import positions_rows  # committed per-agent extracts (replaces raw positions.txt)
OUTP = os.path.join(REPO, "Plotting", "output", "plots")
os.makedirs(OUTP, exist_ok=True)
# Also drop a copy into the manuscript figure folder when it is present (i.e. run inside the paper tree).
FIN = os.path.join(REPO, "PLOS-Submission", "Manuscript", "2025_Coral_Larvae_Simulation", "Figures", "final")
SCN = {"V1P05": 5, "V1P15": 15, "V1P30": 30, "V1P57": 57}

runs = defaultdict(lambda: defaultdict(lambda: [0, 0]))  # scen -> (gseed,rng) -> [correct,total]
# The committed extracts are already de-duplicated to the current battery, so no timestamp cutoff is needed.
for r in positions_rows():
    s = (r.get("scenario", "") or "").strip()
    if s not in SCN: continue
    key = (r["validation_genome_seed"], r.get("random_seed", ""))
    runs[s][key][1] += 1
    if str(r.get("correct_settler", "")).strip() in ("1", "true", "True"): runs[s][key][0] += 1

xs, ys, es = [], [], []
print("MaxTile  correct%  (mean +/- SE, n runs)")
for s in ["V1P05", "V1P15", "V1P30", "V1P57"]:
    rates = [100 * c / t for c, t in runs[s].values() if t]
    m = statistics.mean(rates); se = statistics.pstdev(rates) / len(rates) ** 0.5
    xs.append(SCN[s]); ys.append(m); es.append(se)
    print(f"{SCN[s]:>7}  {m:5.1f}  (+/-{se:.1f}, n={len(rates)})")

fig, ax = plt.subplots(figsize=(5, 3.6))
ax.errorbar(xs, ys, yerr=es, fmt="o-", color=ACCENT, capsize=4, linewidth=1.6)
ax.set_xlabel("maximum tile area (cm$^2$)")
ax.set_ylabel("correct settlement (%)")
ax.set_ylim(0, 100); ax.grid(alpha=0.3)
ax.set_title("E1: settlement vs patch size (CCA cover 0.41)")
fig.tight_layout()
out = os.path.join(OUTP, "e1_patchsize.png")
fig.savefig(out, dpi=200)
if os.path.isdir(FIN):
    fig.savefig(os.path.join(FIN, "e1_patchsize.png"), dpi=200)
print("wrote", out)
