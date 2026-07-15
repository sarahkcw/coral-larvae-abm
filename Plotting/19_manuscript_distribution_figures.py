"""Manuscript distribution heatmaps (E2 depth, E3 vertical + horizontal), regenerated
from the CURRENT validation data in the shared scientific style (viridis).

Replaces the stale pre-rerun figures dis2/dis3vertical/dis3horizontal in the
manuscript Figures/final folder. Cells are annotated with mean larvae per run
(out of 250); colour = viridis by the same value.

Reads:
  output/tables/e2_depth_zone_counts_by_scenario.csv
  output/tables/e3_height_zone_counts_by_scenario.csv
  output/tables/e3_horizontal_zone_counts_by_scenario.csv
"""
import os, sys, csv
import numpy as np
import matplotlib; matplotlib.use("Agg")
import matplotlib.pyplot as plt
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from _plotstyle import apply_style, SEQ_CMAP
apply_style()

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TAB = os.path.join(REPO, "Plotting", "output", "tables")
OUTP = os.path.join(REPO, "Plotting", "output", "plots")
FIN = os.path.join(REPO, "PLOS-Submission", "Manuscript",
                   "2025_Coral_Larvae_Simulation", "Figures", "final")
os.makedirs(OUTP, exist_ok=True)


def load(path, zone_col):
    # Use mean_proportion (fraction of the run's larvae in each zone), which is robust to the
    # zero-fill run-count dilution in mean_larvae_per_run. Returned as a percentage.
    rows = {}
    with open(path, newline="") as f:
        for r in csv.DictReader(f):
            sc = r["scenario"].strip().strip('"')
            z = r[zone_col].strip().strip('"')
            try:
                rows[(sc, z)] = 100.0 * float(r["mean_proportion"])
            except (ValueError, KeyError):
                rows[(sc, z)] = 0.0
    return rows


def heatmap(rows, scenarios, zones, zone_labels, title, xlabel, out_names,
            ylabel="zone", row_note_top=None, row_note_bottom=None):
    M = np.array([[rows.get((sc, z), 0.0) for sc in scenarios] for z in zones])
    fig, ax = plt.subplots(figsize=(1.05 * len(scenarios) + 2.2, 0.62 * len(zones) + 1.6))
    im = ax.imshow(M, cmap=SEQ_CMAP, aspect="auto")
    _disp = {"V2BASE": "V2A (base)", "V2CUR": "V2D (current)"}
    ax.set_xticks(range(len(scenarios))); ax.set_xticklabels([_disp.get(s, s) for s in scenarios])
    ax.set_yticks(range(len(zones))); ax.set_yticklabels(zone_labels)
    ax.set_xlabel(xlabel); ax.set_ylabel(ylabel)
    ax.set_title(title)
    ax.grid(False)
    vmax = M.max() if M.max() > 0 else 1.0
    for i in range(len(zones)):
        for j in range(len(scenarios)):
            v = M[i, j]
            ax.text(j, i, f"{v:.0f}", ha="center", va="center", fontsize=8,
                    color="white" if v < 0.55 * vmax else "black")
    cb = fig.colorbar(im, ax=ax, fraction=0.046, pad=0.03)
    cb.set_label("% of larvae per run")
    if row_note_top:
        ax.text(-0.6, -0.75, row_note_top, fontsize=7, color="#555", ha="left")
    if row_note_bottom:
        ax.text(-0.6, len(zones) - 0.15, row_note_bottom, fontsize=7, color="#555", ha="left")
    fig.tight_layout()
    written = None
    for name in out_names:
        if not os.path.isdir(os.path.dirname(name)):
            continue  # skip targets whose folder is absent (e.g. the manuscript repo when run standalone)
        fig.savefig(name, dpi=200)
        written = name
    plt.close(fig)
    print("wrote", os.path.basename(written) if written else "(no target dir)")


# --- E2 depth distribution (dis2). final_z ~0 is the tube BOTTOM (larvae descend from the top);
# display surface (>200 cm) at top, bottom (<40 cm) at the bottom row. ---
e2 = load(os.path.join(TAB, "e2_depth_zone_counts_by_scenario.csv"), "depth_zone")
e2_zones = [">200 cm", "160-200 cm", "120-160 cm", "80-120 cm", "40-80 cm", "<40 cm"]
heatmap(e2, ["V2BASE", "V2CUR"], e2_zones, e2_zones,
        "E2 vertical distribution", "Verification round",
        [os.path.join(OUTP, "dis2.png"), os.path.join(FIN, "dis2.png")],
        ylabel="Depth zone")

# --- E3 vertical distribution (dis3vertical): tube bottom at bottom ---
e3h = load(os.path.join(TAB, "e3_height_zone_counts_by_scenario.csv"), "zone")
e3_hz = [">8 cm", "6-8 cm", "4-6 cm", "2-4 cm", "<2 cm"]
heatmap(e3h, ["V3A", "V3B", "V3C", "V3D"], e3_hz, e3_hz,
        "E3 vertical distribution", "Verification round",
        [os.path.join(OUTP, "dis3vertical.png"), os.path.join(FIN, "dis3vertical.png")],
        ylabel="Height zone")

# --- E3 horizontal distribution (dis3horizontal): nearest source at top ---
e3d = load(os.path.join(TAB, "e3_horizontal_zone_counts_by_scenario.csv"), "zone")
e3_dz = ["zone1 (nearest)", "zone2", "zone3", "zone4", "zone5 (farthest)"]  # CSV keys
# Display distance-to-source in cm (matching the sens3 figure) rather than zone indices.
e3_dz_labels = ["0-20 cm\n(nearest)", "20-40", "40-60", "60-80", ">80 cm\n(farthest)"]
heatmap(e3d, ["V3A", "V3B", "V3C", "V3D"], e3_dz, e3_dz_labels,
        "E3 horizontal distribution", "Verification round",
        [os.path.join(OUTP, "dis3horizontal.png"), os.path.join(FIN, "dis3horizontal.png")],
        ylabel="Horizontal zone")
