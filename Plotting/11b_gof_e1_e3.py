"""
Sim-vs-lab comparison for E1 (Sampayo) and E3 (Vermeij), companion to 11 (E2).

E1: Sampayo's design (settlement by species/patch) does NOT map to a single simulated
distribution, so a distributional equivalence test would be misleading. We report an
honest magnitude + direction comparison: simulated correct-settler rate (V1A, 30 seeds)
vs Sampayo's per-species settlement success (16.2/33.6/47.2 % ± SE). The *validated*
claim is the CCA dose-response (out-of-sample V1B/C), reported in the E1 doc.

E3: Vermeij published NO numeric per-zone proportions (only Fig-2 bars), so a zone-level
sim=lab GoF is not possible from the source. Instead we reproduce Vermeij's own statistical
SIGNATURE: chi-square of the 5 horizontal distance zones vs uniform, sound-ON vs silent.
Vermeij: sound chi^2=30.50 (p<1e-4), silent chi^2=0.05 (p=0.97). The stronger equivalence
evidence is the moved-source tracking (peak follows the source; a static pile-up cannot),
reported in the E3 doc.
"""
import csv, os, sys, statistics
from collections import defaultdict
from scipy import stats

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EVO = os.path.join(REPO, "coral-larvae-abm", "Content", "Evolution")
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from _valdata import positions_rows  # committed per-agent extracts (replaces raw positions.txt)

def rows(scen):
    return [r for r in positions_rows() if (r.get("scenario", "") or "").strip() == scen]

def truthy(v): return str(v).strip() in ("1", "true", "True")

# ---------- E1: magnitude + direction vs Sampayo ----------
print("=== E1 vs Sampayo (magnitude/direction; NOT a distributional equivalence) ===")
d = defaultdict(lambda: [0, 0])   # genome seed -> [correct, total]
for r in rows("V1A"):
    gs = r["validation_genome_seed"]; d[gs][1] += 1
    if truthy(r.get("correct_settler", "")): d[gs][0] += 1
rate = [100*c/t for c, t in d.values() if t]
LAB_E1 = {"A. valida": (16.2, 2.1), "A. digitifera": (33.6, 2.4), "A. spinosa": (47.2, 6.6)}
print(f"  sim V1A correct-settler %: median {statistics.median(rate):.1f} "
      f"[{min(rate):.1f}, {max(rate):.1f}] over {len(rate)} seeds")
for sp, (m, se) in LAB_E1.items():
    print(f"  Sampayo {sp:14}: {m:.1f} ± {se:.1f} %")
print(f"  -> sim reproduces CCA-driven settlement (dose-response validated out-of-sample V1B/C);")
print(f"     absolute magnitude is at/above the lab's upper species (over-prediction) -> stated as a limitation.\n")

# ---------- E3: Vermeij chi-square-vs-uniform signature (5 zones) ----------
print("=== E3 vs Vermeij (reproduce the chi^2-vs-uniform signature; 5 horizontal zones) ===")
def zone_chisq(scen, xmin=0.0, xmax=100.0, nz=5):
    xs = []
    for r in rows(scen):
        try: xs.append(float(str(r.get("final_x", "")).replace(",", ".")))
        except ValueError: pass
    if not xs: return None
    w = (xmax - xmin) / nz
    counts = [0]*nz
    for x in xs:
        k = min(nz-1, max(0, int((x - xmin)/w)))
        counts[k] += 1
    n = sum(counts); exp = n/nz
    chi2 = sum((c-exp)**2/exp for c in counts)
    p = 1 - stats.chi2.cdf(chi2, df=nz-1)
    return counts, chi2, p, n
for scen, lab in [("V3A", "sound ON  (Vermeij chi2=30.50, p<1e-4)"),
                  ("V3E", "silent    (Vermeij chi2=0.05,  p=0.97)")]:
    res = zone_chisq(scen)
    if res is None:
        print(f"  {scen}: no data"); continue
    counts, chi2, p, n = res
    print(f"  {scen} [{lab}]: zone counts {counts} (n={n})")
    print(f"        sim chi^2(df=4) = {chi2:.1f}, p = {p:.2e}  "
          f"-> {'rejects uniform (directional, like Vermeij)' if p<0.001 else 'consistent with uniform (like Vermeij silent)'}")
print("  Stronger equivalence: moved-source tracking (V3B/C: peak shifts toward the source,")
print("  meanX ~21 @src15 vs ~79 @src85) — a static concentration cannot track a moved source.")
