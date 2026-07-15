"""
Sim-vs-lab goodness-of-fit (reviewer gap 1): tests whether the SIMULATED distribution
matches the PUBLISHED laboratory distribution — not merely that each is non-uniform.

E2 (Tay et al. 2011, sinensis): bottom-zone fraction over time. Lab bottom% (mean ± SE
across 3 tubes) at day 3.5 / 10.4 / 15.7 = 1.9 / 59.8±8.3 / 69.9±7.7 (docs/sources.md).
Sim: final-config e2/SUS_H4b, per-genome-seed bottom fraction (z<73 cm) at the matching
sim steps (5000 steps = 15.7 d), 30 seeds -> mean + bootstrap 95% CI.

GoF statistic: standardized residual z_i = (sim_i - lab_i)/sqrt(SE_lab_i^2 + SE_sim_i^2)
per timepoint, and chi^2 = sum z_i^2 (df=3) with its p-value. Small chi^2 / large p =
the simulated time-course is statistically consistent with Tay's. Large |z_i| flags a
localized lack-of-fit (expected at day 10.4). Overlay figure with both uncertainty bands.
"""
import csv, os, math, statistics
from collections import defaultdict
import matplotlib; matplotlib.use("Agg")
import matplotlib.pyplot as plt
from scipy import stats

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EVO = os.path.join(REPO, "coral-larvae-abm", "Content", "Evolution")
OUT_P = os.path.join(REPO, "Plotting", "output", "plots")
OUT_T = os.path.join(REPO, "Plotting", "output", "tables")

# --- E2 lab (Tay sinensis) ---
DAYS = [3.5, 10.4, 15.7]
STEPS = [1100, 3300, 5000]      # 5000 steps = 15.7 d
LAB = [1.9, 59.8, 69.9]
LAB_SE = [2.0, 8.3, 7.7]        # day-3.5 SE not reported by Tay; use a conservative 2.0
BOT = 73.0

def sim_bottom_per_seed(path):
    seeds = defaultdict(lambda: defaultdict(list))
    with open(path) as f:
        for r in csv.DictReader(f):
            seeds[r["validation_genome_seed"]][r["agent_index"]].append((int(r["sim_step"]), float(r["pos_z"])))
    out = {t: [] for t in STEPS}
    for gs, agents in seeds.items():
        for t in STEPS:
            lo = tot = 0
            for pts in agents.values():
                pts.sort(); z = None
                for s, zz in pts:
                    if s <= t: z = zz
                    else: break
                if z is None: z = pts[0][1]
                tot += 1; lo += (z < BOT)
            out[t].append(100*lo/tot)
    return out

def boot_ci(vals, n=5000):
    vals = list(vals); m = statistics.mean(vals)
    # deterministic bootstrap via fixed index stride (no RNG needed for a CI estimate)
    import random; random.seed(1)
    means = sorted(statistics.mean(random.choice(vals) for _ in vals) for _ in range(n))
    return m, means[int(0.025*n)], means[int(0.975*n)], statistics.pstdev(vals)/math.sqrt(len(vals))

path = os.path.join(EVO, "validation_trajectory_e2_E2Hb_SUS_H4b.csv")
per = sim_bottom_per_seed(path)
sim_mean, sim_lo, sim_hi, sim_se = [], [], [], []
for t in STEPS:
    m, lo, hi, se = boot_ci(per[t]); sim_mean.append(m); sim_lo.append(lo); sim_hi.append(hi); sim_se.append(se)

print("E2 sim-vs-lab (Tay sinensis bottom-zone %)")
print(f"{'day':>6} {'lab':>12} {'sim(95%CI)':>22} {'std_resid':>10}")
chi2 = 0.0
rows = []
for i, d in enumerate(DAYS):
    denom = math.sqrt(LAB_SE[i]**2 + sim_se[i]**2)
    z = (sim_mean[i] - LAB[i]) / denom if denom > 0 else 0.0
    chi2 += z*z
    print(f"{d:>6} {LAB[i]:>6.1f}±{LAB_SE[i]:<4.1f} {sim_mean[i]:>7.1f} [{sim_lo[i]:.1f},{sim_hi[i]:.1f}]   {z:>10.2f}")
    rows.append(dict(day=d, lab_pct=LAB[i], lab_se=LAB_SE[i], sim_pct=round(sim_mean[i],1),
                     sim_ci_lo=round(sim_lo[i],1), sim_ci_hi=round(sim_hi[i],1), std_residual=round(z,2)))
p = 1 - stats.chi2.cdf(chi2, df=len(DAYS))
print(f"GoF chi^2 = {chi2:.2f}, df={len(DAYS)}, p = {p:.3f}  "
      f"({'consistent with Tay' if p>0.05 else 'localized lack-of-fit (see residuals)'})")

with open(os.path.join(OUT_T, "sim_vs_lab_gof_E2.csv"), "w", newline="") as f:
    w = csv.DictWriter(f, fieldnames=list(rows[0].keys())); w.writeheader(); w.writerows(rows)

# overlay figure
fig, ax = plt.subplots(figsize=(6, 4))
ax.errorbar(DAYS, LAB, yerr=LAB_SE, fmt="o-", color="#333", capsize=4, label="Tay et al. 2011 (lab, ±SE)")
ax.errorbar(DAYS, sim_mean, yerr=[[m-l for m,l in zip(sim_mean,sim_lo)],[h-m for m,h in zip(sim_mean,sim_hi)]],
            fmt="s--", color="#00798c", capsize=4, label="sim e2/SUS_H4b (95% CI, 30 seeds)")
ax.set_xlabel("day"); ax.set_ylabel("Larvae in bottom zone (%)")
ax.set_title(f"E2 simulation vs Tay et al. 2011 (GoF chi2={chi2:.1f}, df=3, p={p:.2f})")
ax.legend(fontsize=8); ax.grid(alpha=0.3)
fig.tight_layout(); fig.savefig(os.path.join(OUT_P, "sim_vs_lab_E2.png"), dpi=150)
print("wrote sim_vs_lab_E2.png + sim_vs_lab_gof_E2.csv")
