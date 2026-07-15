"""Environmental sensitivity figures (R1-3), computed directly from the sweep runs:
  sens2 = E2 depth distribution vs light attenuation Kd (V2M1..V2M5)
  sens3 = E3 source-distance distribution vs acoustic source level + frequency band
          (V3SPL140/153/160, V3FLO/V3FHI)

Reads the simulator's raw validation_positions.txt and filters to the sweep scenario labels
(single-timestamp, one config each) to avoid the mixed-provenance pooling in the append-only log.
Per-zone value = mean larvae per run (fraction of 250 x 100). Viridis heatmaps into Figures/final.
"""
import os, sys, csv
import numpy as np
import matplotlib; matplotlib.use("Agg")
import matplotlib.pyplot as plt
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from _plotstyle import apply_style, SEQ_CMAP
apply_style()
matplotlib.rcParams.update({"font.size": 12, "axes.titlesize": 13, "axes.labelsize": 12})  # one step bigger

from _valdata import positions_rows  # committed per-agent extracts (replaces raw positions.txt)
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUTP = os.path.join(REPO, "Plotting", "output", "plots")
FIN = os.path.join(REPO, "PLOS-Submission", "Manuscript",
                   "2025_Coral_Larvae_Simulation", "Figures", "final")

E2_LEVELS = [("V2M1", "0.025"), ("V2M2", "0.05"), ("V2M3", "0.1\n(base)"), ("V2M4", "0.2"), ("V2M5", "0.4")]
E3_LEVELS = [("V3SPL140", "140 dB"), ("V3SPL153", "153 dB\n(base)"), ("V3SPL160", "160 dB"),
             ("V3FLO", "0.1-1\nkHz"), ("V3FHI", "1-20\nkHz")]

# E2 vertical: final_z in cm; z~0 is the tube BOTTOM (larvae released at the top ~220 cm and
# descend). Zone index 0 = z<40 (bottom) ... 5 = z>200 (surface). Displayed top->bottom = surface->bottom.
E2_EDGES = [40, 80, 120, 160, 200]
E2_ZLAB = ["<40 cm\n(bottom)", "40-80", "80-120", "120-160", "160-200", ">200 cm\n(surface)"]
# E3 horizontal: final_x; source at the low-x end, so small x = nearest the source.
E3_EDGES = [20, 40, 60, 80]
E3_ZLAB = ["0-20 cm\n(nearest)", "20-40", "40-60", "60-80", ">80 cm\n(farthest)"]


def _euro(x):
    return float(x.replace(",", "."))


def collect(scen_labels, coord_col, edges):
    """Return {scenario: (per-zone mean larvae per run)} from the committed extracts."""
    want = set(scen_labels)
    # per scenario: total per zone, and set of run keys
    tot = {s: np.zeros(len(edges) + 1) for s in want}
    runs = {s: set() for s in want}
    for r in positions_rows():
        s = (r.get("scenario", "") or "").strip()
        if s not in want:
            continue
        try:
            v = _euro(r.get(coord_col, ""))
        except ValueError:
            continue
        z = int(np.searchsorted(edges, v, side="right"))
        tot[s][z] += 1
        runs[s].add((r.get("validation_genome_seed", ""), r.get("random_seed", "")))
    out = {}
    for s in want:
        nr = max(len(runs[s]), 1)
        out[s] = tot[s] / nr / 250.0 * 100.0  # mean % of the run's 250 larvae per zone
    return out


def heatmap(data, levels, zlab, title, xlabel, out_names, ylabel="zone"):
    scen = [s for s, _ in levels]
    if not any(data.get(s, np.zeros(1)).sum() > 0 for s in scen):
        print("SKIP", os.path.basename(out_names[0]), "- no data")
        return
    M = np.array([[data[s][i] for s, _ in levels] for i in range(len(zlab))])
    fig, ax = plt.subplots(figsize=(1.2 * len(levels) + 2.2, 0.62 * len(zlab) + 1.7))
    im = ax.imshow(M, cmap=SEQ_CMAP, aspect="auto")
    ax.set_xticks(range(len(levels))); ax.set_xticklabels([l for _, l in levels], fontsize=10)
    ax.set_yticks(range(len(zlab))); ax.set_yticklabels(zlab, fontsize=10)
    ax.set_xlabel(xlabel); ax.set_ylabel(ylabel); ax.set_title(title); ax.grid(False)
    vmax = M.max() if M.max() > 0 else 1.0
    for i in range(len(zlab)):
        for j in range(len(levels)):
            v = M[i, j]
            ax.text(j, i, f"{v:.0f}", ha="center", va="center", fontsize=10,
                    color="white" if v < 0.55 * vmax else "black")
    cb = fig.colorbar(im, ax=ax, fraction=0.046, pad=0.03); cb.set_label("% of larvae per run")
    fig.tight_layout()
    written = None
    for n in out_names:
        if not os.path.isdir(os.path.dirname(n)):
            continue  # skip targets whose folder is absent (e.g. the manuscript repo when run standalone)
        fig.savefig(n, dpi=200)
        written = n
    plt.close(fig)
    print("wrote", os.path.basename(written) if written else "(no target dir)")


e2 = collect([s for s, _ in E2_LEVELS], "final_z", E2_EDGES)
e2 = {s: v[::-1] for s, v in e2.items()}  # display surface (>200) at top, bottom (<40) at bottom
heatmap(e2, E2_LEVELS, E2_ZLAB[::-1],
        "E2 depth distribution vs light attenuation", "Light attenuation $K_d$ (m$^{-1}$)",
        [os.path.join(OUTP, "sens2.png"), os.path.join(FIN, "sens2.png")], ylabel="Height zone")

e3 = collect([s for s, _ in E3_LEVELS], "final_x", E3_EDGES)
heatmap(e3, E3_LEVELS, E3_ZLAB,
        "E3 source-distance distribution vs sound level / frequency", "acoustic condition",
        [os.path.join(OUTP, "sens3.png"), os.path.join(FIN, "sens3.png")], ylabel="Horizontal zone")
