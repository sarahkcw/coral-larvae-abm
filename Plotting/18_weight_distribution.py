"""Synaptic-weight distribution of the evolved controllers (reviewer #1, "open the
black box"). Complements 09_nn_cue_ablation.py (which measures cue *influence*):
here we report the distribution of the actual connection weights and how strongly
each *sensory channel class* is wired into the network, across all 30 seeds x
top-N elite genomes of the final controller set per experiment.

Standalone: reuses the same genome decode/prune logic as 09 (kept in sync).
Outputs:
  output/tables/weight_distribution_summary.csv   per-experiment weight stats
  output/tables/weight_by_sensor_class.csv        mean |w| of sensor->net wiring by cue class
  output/plots/weight_distribution.png            weight histograms E1/E2/E3
"""
import os, math, glob, sys
import numpy as np
import matplotlib; matplotlib.use("Agg")
import matplotlib.pyplot as plt
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from _plotstyle import apply_style, ACCENT
apply_style()

# --- constants (faithful to the C++ pipeline; mirror 09_nn_cue_ablation.py) ---
NUM_SENSORS = 16
NUM_ACTIONS = 5
NUM_NEURONS = 12
GSENSOR = 1; GACTION = 1; GNEURON = 0
WEIGHT_SCALE = 8192.0

SENSOR_NAMES = [
    "OSCILLATION", "AGE", "ENERGY", "ALTEROMONAS_BIOFILM", "CCA",
    "CCA_FORWARD_BACK", "CCA_UP_DOWN", "CCA_LEFT_RIGHT", "TEMPERATURE",
    "PRESSURE", "LIGHT_INTENSITY", "LIGHT_WAVELENGTH",
    "PARTICLE_MOTION_FORWARD_BACK", "PARTICLE_MOTION_UP_DOWN",
    "PARTICLE_MOTION_LEFT_RIGHT", "PARTICLE_MOTION",
]
CCA_CUES = {"CCA", "CCA_FORWARD_BACK", "CCA_UP_DOWN", "CCA_LEFT_RIGHT", "ALTEROMONAS_BIOFILM"}
PM_CUES = {"PARTICLE_MOTION", "PARTICLE_MOTION_FORWARD_BACK", "PARTICLE_MOTION_UP_DOWN",
           "PARTICLE_MOTION_LEFT_RIGHT"}
LIGHT_CUES = {"LIGHT_INTENSITY", "LIGHT_WAVELENGTH"}
INTERNAL = {"OSCILLATION", "AGE", "ENERGY"}
def sensor_class(name):
    if name in CCA_CUES: return "CCA/biofilm"
    if name in PM_CUES: return "particle-motion"
    if name in LIGHT_CUES: return "light"
    if name in INTERNAL: return "internal-state"
    return "temp/pressure"

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXPERIMENTS = {
    "E1": os.path.join(REPO, "Training", "e1", "SUS"),
    "E2": os.path.join(REPO, "Training", "e2", "SUS"),
    "E3": os.path.join(REPO, "Training", "e3", "SUS_H3"),
}
GENOME_LENS = {"E1": 50, "E2": 50, "E3": 25}
SEED_RANGE = range(1, 31)
TOP_N_PER_SEED = 10
OUT_TABLES = os.path.join(REPO, "Plotting", "output", "tables")
OUT_PLOTS = os.path.join(REPO, "Plotting", "output", "plots")
os.makedirs(OUT_TABLES, exist_ok=True); os.makedirs(OUT_PLOTS, exist_ok=True)


def decode_gene(hexstr):
    n = int(hexstr, 16) & 0xFFFFFFFF
    bf = n & 0xFFFF
    w_raw = (n >> 16) & 0xFFFF
    weight = (w_raw - 0x10000) if w_raw >= 0x8000 else w_raw
    return {"b_src": bf & 0x1, "s_idx": (bf >> 1) & 0x7F,
            "b_tgt": (bf >> 8) & 0x1, "t_idx": (bf >> 9) & 0x7F,
            "w": weight / WEIGHT_SCALE}


def renumber(genes):
    out = []
    for g in genes:
        s = g["s_idx"] % (NUM_NEURONS if g["b_src"] == GNEURON else NUM_SENSORS)
        t = g["t_idx"] % (NUM_ACTIONS if g["b_tgt"] == GACTION else NUM_NEURONS)
        out.append({"b_src": g["b_src"], "s_idx": s, "b_tgt": g["b_tgt"], "t_idx": t, "w": g["w"]})
    return out


def can_reach_action(neuron_idx, conns):
    stack, seen = [neuron_idx], set()
    while stack:
        cur = stack.pop()
        if cur in seen: continue
        seen.add(cur)
        for c in conns:
            if c["b_src"] == GNEURON and c["s_idx"] == cur:
                if c["b_tgt"] == GACTION: return True
                if c["b_tgt"] == GNEURON: stack.append(c["t_idx"])
    return False


def prune(conns):
    conns = list(conns)
    while True:
        neurons = set()
        for c in conns:
            if c["b_tgt"] == GNEURON: neurons.add(c["t_idx"])
            if c["b_src"] == GNEURON: neurons.add(c["s_idx"])
        dead = {n for n in neurons if not can_reach_action(n, conns)}
        if not dead: break
        conns = [c for c in conns if not (c["b_tgt"] == GNEURON and c["t_idx"] in dead)]
    return conns


def active_connections(genes):
    """Return list of pruned/active connections (dicts) for one genome."""
    return prune(renumber(genes))


def load_genomes(path, genome_len):
    out = []
    with open(path, "r") as fh:
        lines = [ln.strip() for ln in fh if ln.strip()]
    i = 0
    while i < len(lines):
        gene_line = lines[i]
        fit = math.nan
        if i + 1 < len(lines) and lines[i + 1].startswith(";"):
            parts = lines[i + 1].split()
            if len(parts) >= 2:
                try: fit = float(parts[1])
                except ValueError: fit = math.nan
            i += 2
        else:
            i += 1
        toks = gene_line.split()
        if len(toks) < genome_len: continue
        out.append(([decode_gene(t) for t in toks], fit))
    return out


def analyse(exp, directory):
    glen = GENOME_LENS[exp]
    all_w = []                       # active connection weights
    class_w = {}                     # sensor-class -> list of |w| of sensor->(neuron/action) edges
    s2n = s2a = n2n = n2a = 0
    for seed in SEED_RANGE:
        files = glob.glob(os.path.join(directory, f"best_genomes_*seed_{seed:02d}.txt"))
        for path in files:
            genomes = load_genomes(path, glen)
            genomes = sorted(genomes, key=lambda gf: (-gf[1] if not math.isnan(gf[1]) else 0))
            for genes, _fit in genomes[:TOP_N_PER_SEED]:
                for c in active_connections(genes):
                    all_w.append(c["w"])
                    if c["b_src"] == GSENSOR:
                        cls = sensor_class(SENSOR_NAMES[c["s_idx"] % NUM_SENSORS])
                        class_w.setdefault(cls, []).append(abs(c["w"]))
                        if c["b_tgt"] == GACTION: s2a += 1
                        else: s2n += 1
                    else:
                        if c["b_tgt"] == GACTION: n2a += 1
                        else: n2n += 1
    w = np.array(all_w, dtype=float)
    summary = dict(
        experiment=exp, n_weights=len(w),
        mean=float(np.mean(w)), sd=float(np.std(w)),
        median=float(np.median(w)), min=float(np.min(w)), max=float(np.max(w)),
        pct_excitatory=100.0 * float(np.mean(w > 0)),
        pct_inhibitory=100.0 * float(np.mean(w < 0)),
        mean_abs=float(np.mean(np.abs(w))),
        sensor_to_neuron=s2n, sensor_to_action=s2a,
        neuron_to_neuron=n2n, neuron_to_action=n2a,
    )
    return w, summary, class_w


def main():
    weights, summaries, class_rows = {}, [], []
    for exp, d in EXPERIMENTS.items():
        w, s, cw = analyse(exp, d)
        weights[exp] = w
        summaries.append(s)
        for cls, vals in sorted(cw.items()):
            class_rows.append(dict(experiment=exp, sensor_class=cls,
                                   n_edges=len(vals), mean_abs_weight=float(np.mean(vals))))
        print(f"{exp}: {s['n_weights']} active weights, mean={s['mean']:+.3f} sd={s['sd']:.3f}, "
              f"{s['pct_excitatory']:.0f}% excit / {s['pct_inhibitory']:.0f}% inhib")

    # write tables
    def write_csv(path, rows, cols):
        with open(path, "w", newline="") as f:
            f.write(",".join(cols) + "\n")
            for r in rows:
                f.write(",".join(f"{r[c]:.4f}" if isinstance(r[c], float) else str(r[c]) for c in cols) + "\n")
    write_csv(os.path.join(OUT_TABLES, "weight_distribution_summary.csv"), summaries,
              list(summaries[0].keys()))
    write_csv(os.path.join(OUT_TABLES, "weight_by_sensor_class.csv"), class_rows,
              ["experiment", "sensor_class", "n_edges", "mean_abs_weight"])

    # figure: weight histograms
    fig, axes = plt.subplots(1, 3, figsize=(10.5, 3.2), sharey=True)
    for ax, exp in zip(axes, EXPERIMENTS):
        ax.hist(weights[exp], bins=40, color=ACCENT, edgecolor="white", linewidth=0.3)
        ax.axvline(0, color="#333", lw=0.8, ls="--")
        ax.set_title(f"{exp}  (n={len(weights[exp])})")
        ax.set_xlabel("connection weight")
    axes[0].set_ylabel("count")
    fig.suptitle("Distribution of active synaptic weights in the evolved controllers")
    fig.tight_layout()
    out = os.path.join(OUT_PLOTS, "weight_distribution.png")
    fig.savefig(out, dpi=200)
    print("wrote", out)


if __name__ == "__main__":
    main()
