"""Shared per-agent validation loader.

Replaces the raw simulator log Content/Evolution/validation_positions.txt (transient
sim output, not kept in the repo) with the committed, git-tracked extracts under
Plotting/output/data/. Same column names as the raw log; standard CSV (comma-separated,
dot decimals), so every figure/analysis reproduces from tracked data alone.
"""
import os
import csv

_DATA = os.path.join(os.path.dirname(os.path.abspath(__file__)), "output", "data")

_CACHE = None


def positions_rows(which=("e1_e2", "e3")):
    """All per-agent validation rows (list of dict), pooled across the committed
    validation_positions_{e1_e2,e3}.csv extracts. Cached after first read."""
    global _CACHE
    if _CACHE is None:
        rows = []
        for w in which:
            path = os.path.join(_DATA, "validation_positions_%s.csv" % w)
            if os.path.exists(path):
                with open(path, newline="", encoding="utf-8", errors="ignore") as f:
                    rows.extend(csv.DictReader(f))
        _CACHE = rows
    return _CACHE
