"""Shared plot style for the manuscript figures (import from every Python plot script).

One scientific, colour-blind-safe scheme used across all figures:
  - sequential heatmaps / density  -> viridis (perceptually uniform)
  - categorical series             -> Okabe-Ito palette
  - single-series bars/lines/hist  -> ACCENT (teal)

Usage:
    from _plotstyle import apply_style, ACCENT, OKABE_ITO, SEQ_CMAP
    apply_style()
"""
import matplotlib as _mpl

# Okabe-Ito colour-blind-safe qualitative palette (Okabe & Ito 2008).
OKABE_ITO = [
    "#0072B2",  # blue
    "#E69F00",  # orange
    "#009E73",  # green
    "#CC79A7",  # reddish purple
    "#56B4E9",  # sky blue
    "#D55E00",  # vermillion
    "#F0E442",  # yellow
    "#000000",  # black
]

ACCENT = "#00798c"     # primary single-series colour (teal)
ACCENT_DARK = "#005766"
SEQ_CMAP = "viridis"   # sequential colormap for heatmaps / density


def apply_style():
    """Apply the shared rcParams. Call once at the top of each plot script."""
    _mpl.rcParams.update({
        "figure.dpi": 200,
        "savefig.dpi": 200,
        "savefig.bbox": "tight",
        "font.size": 10,
        "font.family": "sans-serif",
        "axes.titlesize": 11,
        "axes.labelsize": 10,
        "axes.edgecolor": "#333333",
        "axes.linewidth": 0.8,
        "axes.grid": True,
        "grid.color": "#cccccc",
        "grid.linewidth": 0.5,
        "grid.alpha": 0.5,
        "axes.axisbelow": True,
        "axes.prop_cycle": _mpl.cycler(color=OKABE_ITO),
        "image.cmap": SEQ_CMAP,
        "figure.facecolor": "white",
        "axes.facecolor": "white",
        "legend.frameon": False,
        "xtick.direction": "out",
        "ytick.direction": "out",
    })
