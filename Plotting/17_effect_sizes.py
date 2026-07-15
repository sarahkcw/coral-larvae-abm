"""Effect sizes (Kendall's W) for the Friedman selection tests, from the reported chi-square
statistics. W = chi2 / (N * (k-1)); N = 30 seeds; k = 3 methods (selection) or 4 configs
(hyperparameter). Interpretation: <0.3 small, 0.3-0.5 medium, >=0.5 large."""
from scipy import stats
N = 30
def W(chi2, k): return chi2 / (N * (k - 1))
def interp(w): return "small" if w < 0.3 else ("medium" if w < 0.5 else "large")
rows = [
    ("E1 selection (3 methods)", 43.3, 3),
    ("E2 selection (3 methods)", 9.87, 3),
    ("E3 selection (3 methods)", 7.40, 3),
    ("E1 hyperparameter (4 cfg)", stats.chi2.ppf(1 - 9e-7, 3), 4),
    ("E2 hyperparameter (4 cfg)", 45.7, 4),
    ("E3 hyperparameter (4 cfg)", 38.5, 4),
]
print(f"{'comparison':28} {'chi2':>7} {'k':>2} {'W':>6}  effect")
for name, chi2, k in rows:
    w = W(chi2, k)
    print(f"{name:28} {chi2:7.1f} {k:>2} {w:6.3f}  {interp(w)}")
