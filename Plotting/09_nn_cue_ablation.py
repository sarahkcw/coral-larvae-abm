#!/usr/bin/env python3
"""
NN cue-dominance / ablation analysis for the coral-larvae NE-ABM.

Answers reviewer R1's "open the black box" request functionally, not just
structurally: for each evolved controller we measure how much each sensed cue
(sensor input) changes the controller's action outputs (cue-on vs cue-off),
then aggregate across the successful controllers of each experiment.

This is a faithful offline re-implementation of the C++ runtime:
  - gene bit layout .......... GenomeFunctions.h (FGene)
  - index renumbering ........ AgentBrainComponent::MakeRenumberedConnectionList
  - unused-neuron pruning .... AgentBrainComponent::RemoveUnusedNeurons / CanReachAction
  - recurrent forward pass ... NeuralNetFunctions::FeedForward (tanh, prev-step neuron state)
  - action activation ........ NeuralNetFunctions::ActivateActions (tanh per action)
  - active sensors ........... LarvaAgent adds exactly the sensors wired in the net
                               (GetSensorClasses), so a genome's wired sensors ARE
                               its active sensors -> controller-only sensitivity.

It only reads the saved training genome files, so it is independent of the
validation seed issue and does not touch the Unreal simulator (safe to run
while E3 training is in progress).

Outputs (under Plotting/output/):
  tables/nn_cue_influence_<exp>.csv      per-sensor mean action influence + per-action
  tables/nn_cue_influence_ranked.csv     combined ranked cue dominance across experiments
  tables/nn_structure_summary_<exp>.csv  per-controller structural stats (final controllers)
  plots/nn_cue_influence_<exp>.png       ranked cue-influence bar chart
"""

import os
import glob
import math
import numpy as np

# ----------------------------------------------------------------------------
# Constants mirrored from the C++ source
# ----------------------------------------------------------------------------
NUM_SENSORS = 16          # ESensorType::NUM_SENSORS (active sensors before it)
NUM_ACTIONS = 5           # EActionType::NUM_ACTIONS
NUM_NEURONS = 12          # MaxInnerNeurons used in all final training runs
GENOME_LEN = 50

GSENSOR = 1               # bSourceType==1 -> sensor ; ==0 -> neuron (GNeuron)
GACTION = 1               # bTargetType==1 -> action ; ==0 -> neuron
GNEURON = 0

INITIAL_NEURON_OUTPUT = 0.5
WEIGHT_SCALE = 8192.0

SENSOR_NAMES = [
    "OSCILLATION", "AGE", "ENERGY", "ALTEROMONAS_BIOFILM", "CCA",
    "CCA_FORWARD_BACK", "CCA_UP_DOWN", "CCA_LEFT_RIGHT", "TEMPERATURE",
    "PRESSURE", "LIGHT_INTENSITY", "LIGHT_WAVELENGTH",
    "PARTICLE_MOTION_FORWARD_BACK", "PARTICLE_MOTION_UP_DOWN",
    "PARTICLE_MOTION_LEFT_RIGHT", "PARTICLE_MOTION",
]
ACTION_NAMES = ["FORWARD", "ROTATE_YAW", "ROTATE_PITCH", "SET_OSC", "SETTLE"]

# ----------------------------------------------------------------------------
# Analysis configuration
# ----------------------------------------------------------------------------
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

EXPERIMENTS = {
    # exp -> directory of the FINAL controller set actually used for validation
    # (reselected 2026-07-09/10 from the fresh UE5.8 rerun; SUS wins all three).
    "E1": os.path.join(REPO, "Training", "e1", "SUS"),
    "E2": os.path.join(REPO, "Training", "e2", "SUS"),
    "E3": os.path.join(REPO, "Training", "e3", "SUS_H3"),
}

# Final-config genome length per experiment (E3 winner uses the reduced genome = 25).
GENOME_LENS = {"E1": 50, "E2": 50, "E3": 25}

# ----------------------------------------------------------------------------
# AP1 trajectory-based cue analysis (gold standard).
#
# When Content/Evolution/validation_trajectory_<exp>_<scenario>.csv files exist
# (bLogPerStepTrajectory on during the validation rerun; see docs/findings.md
# "Validation rerun infrastructure"), we can measure REALIZED cue influence
# from real per-step sensor traces instead of synthetic on/off sweeps: for
# each wired sensor, how much of the variance in each action output is
# explained by that sensor's actual observed values during validation runs.
# This complements (does not replace) the synthetic-sweep analysis above,
# which measures a controller's structural sensitivity regardless of whether
# that sensor's field is ever actually varying in a given experiment's
# geometry.
#
# CSV schema (SimulationManager::FlushPerStepTrajectoryLog):
#   timestamp,experiment,scenario,validation_genome_seed,random_seed,
#   agent_index,sim_step,pos_x,pos_y,pos_z,
#   sensor_<ShortName> x NUM_SENSORS (enum order == SENSOR_NAMES order),
#   action_<ShortName> x NUM_ACTIONS (enum order == ACTION_NAMES order)
# Decimals are DOT-separated (FString::SanitizeFloat), comma-separated columns.
# ----------------------------------------------------------------------------
_EVO = os.path.join(REPO, "coral-larvae-abm", "Content", "Evolution")
TRAJECTORY_GLOB = {
    # Realized (variance-weighted) cue influence from real per-step sensor traces of the FINAL
    # controller sets only. E2: the winner's (SUS_H2) baseline + perturbation runs (excludes the
    # calibration probes SW_*/G5_*/RT_*/CAL and the 1-agent Pop-bug junk file). E1/E3 have no clean
    # stride-sampled trajectory yet (would need a dedicated -Traj V1A/V3A run); realized analysis is
    # skipped for them and the controller-intrinsic (synthetic-sweep) analysis stands alone.
    "E1": os.path.join(_EVO, "validation_trajectory_e1_V1Atraj.csv"),
    "E2": os.path.join(_EVO, "validation_trajectory_e2_E2Hb_SUS_H4b.csv"),
    "E3": os.path.join(_EVO, "validation_trajectory_e3_V3Atraj.csv"),
}

# ----------------------------------------------------------------------------
# Per-experiment cue classification (AP2).
#
# This is a FACTUAL classification of which sensory channels carry a non-trivial
# environmental field in each experiment's setup, NOT a fabricated variance. A
# sensor whose field does not exist by design (e.g. acoustic particle motion in
# the CCA-only E1 bowl, or light in the dark E3 tubes) is definitionally
# uninformative and is excluded from the cue-dominance ranking. Internal-state
# inputs (oscillator, age, energy) are not environmental cues and are reported
# separately. Realized (variance-weighted) influence from real per-step sensor
# traces is deferred to the trajectory-based analysis on the validation rerun
# (see validation-rerun-plan.md, "AP1").
# ----------------------------------------------------------------------------
INTERNAL_SENSORS = {"OSCILLATION", "AGE", "ENERGY"}

CCA_CUES = {"CCA", "CCA_FORWARD_BACK", "CCA_UP_DOWN", "CCA_LEFT_RIGHT",
            "ALTEROMONAS_BIOFILM"}
PM_CUES = {"PARTICLE_MOTION", "PARTICLE_MOTION_FORWARD_BACK",
           "PARTICLE_MOTION_UP_DOWN", "PARTICLE_MOTION_LEFT_RIGHT"}
LIGHT_CUES = {"LIGHT_INTENSITY", "LIGHT_WAVELENGTH"}

# primary = strong manipulated/spatially varying field; weak = field defined but
# near-constant in that geometry (kept, but flagged).
EXP_FIELDS = {
    "E1": {"primary": set(CCA_CUES),
           "weak": {"TEMPERATURE", "PRESSURE"}},                 # shallow bowl -> flat T/P
    "E2": {"primary": set(CCA_CUES) | set(LIGHT_CUES) | {"TEMPERATURE", "PRESSURE"},
           "weak": set()},                                       # 2.2 m tube + diel cycle
    "E3": {"primary": set(PM_CUES),
           "weak": {"TEMPERATURE", "PRESSURE"}},
}


def sensor_role(exp, sensor_name):
    if sensor_name in INTERNAL_SENSORS:
        return "internal"
    fields = EXP_FIELDS.get(exp, {"primary": set(), "weak": set()})
    if sensor_name in fields["primary"]:
        return "environmental_primary"
    if sensor_name in fields.get("weak", set()):
        return "environmental_weak"
    return "inactive_no_field"

TOP_N_PER_SEED = 10       # analyse the top-N (elite / "successful") genomes per seed
T_STEPS = 80              # recurrent steps to reach steady controller state
MC_SAMPLES = 48           # random background input vectors per controller
SEED_RANGE = range(1, 31)
RNG = np.random.default_rng(20260706)  # fixed for reproducibility (no Date/rand)

OUT_TABLES = os.path.join(REPO, "Plotting", "output", "tables")
OUT_PLOTS = os.path.join(REPO, "Plotting", "output", "plots")
os.makedirs(OUT_TABLES, exist_ok=True)
os.makedirs(OUT_PLOTS, exist_ok=True)


# ----------------------------------------------------------------------------
# Genome decoding + wiring (faithful to the C++ pipeline)
# ----------------------------------------------------------------------------
def decode_gene(hexstr):
    n = int(hexstr, 16) & 0xFFFFFFFF
    bf = n & 0xFFFF
    w_raw = (n >> 16) & 0xFFFF
    weight = (w_raw - 0x10000) if w_raw >= 0x8000 else w_raw
    b_src = bf & 0x1
    s_idx = (bf >> 1) & 0x7F
    b_tgt = (bf >> 8) & 0x1
    t_idx = (bf >> 9) & 0x7F
    return {
        "b_src": b_src, "s_idx": s_idx,
        "b_tgt": b_tgt, "t_idx": t_idx,
        "w": weight / WEIGHT_SCALE,
    }


def renumber(genes):
    """MakeRenumberedConnectionList: mod indices by their node-type counts."""
    conns = []
    for g in genes:
        s_idx = g["s_idx"] % (NUM_NEURONS if g["b_src"] == GNEURON else NUM_SENSORS)
        t_idx = g["t_idx"] % (NUM_ACTIONS if g["b_tgt"] == GACTION else NUM_NEURONS)
        conns.append({"b_src": g["b_src"], "s_idx": s_idx,
                      "b_tgt": g["b_tgt"], "t_idx": t_idx, "w": g["w"]})
    return conns


def can_reach_action(neuron_idx, conns):
    """DFS over neuron->neuron/action edges (CanReachAction)."""
    stack = [neuron_idx]
    visited = set()
    while stack:
        cur = stack.pop()
        if cur in visited:
            continue
        visited.add(cur)
        for c in conns:
            if c["b_src"] == GNEURON and c["s_idx"] == cur:
                if c["b_tgt"] == GACTION:
                    return True
                if c["b_tgt"] == GNEURON:
                    stack.append(c["t_idx"])
    return False


def prune(conns):
    """RemoveUnusedNeurons: drop neurons that cannot reach an action, and the
    connections feeding them; repeat until stable."""
    conns = list(conns)
    while True:
        neurons = set()
        for c in conns:
            if c["b_tgt"] == GNEURON:
                neurons.add(c["t_idx"])
            if c["b_src"] == GNEURON:
                neurons.add(c["s_idx"])
        dead = {n for n in neurons if not can_reach_action(n, conns)}
        if not dead:
            break
        # remove connections whose TARGET is a dead neuron (RemoveConnectionsToNeuron)
        conns = [c for c in conns
                 if not (c["b_tgt"] == GNEURON and c["t_idx"] in dead)]
    return conns


def build_net(genes):
    """CreateWiringForGenome -> compact recurrent net.

    Returns (connections, n_neurons, driven_mask, wired_sensors).
    Connections use remapped 0..k-1 neuron ids.
    """
    conns = prune(renumber(genes))
    # surviving neurons = neuron ids still present as a source-neuron or target-neuron
    surviving = []
    seen = set()
    for c in conns:
        if c["b_src"] == GNEURON and c["s_idx"] not in seen:
            seen.add(c["s_idx"]); surviving.append(c["s_idx"])
        if c["b_tgt"] == GNEURON and c["t_idx"] not in seen:
            seen.add(c["t_idx"]); surviving.append(c["t_idx"])
    remap = {old: new for new, old in enumerate(surviving)}
    k = len(surviving)

    net = []
    for c in conns:
        s_idx = remap[c["s_idx"]] if c["b_src"] == GNEURON else c["s_idx"]
        t_idx = remap[c["t_idx"]] if c["b_tgt"] == GNEURON else c["t_idx"]
        net.append((c["b_src"], s_idx, c["b_tgt"], t_idx, c["w"]))

    # bDriven: neuron has >=1 non-self input (from sensor or other neuron)
    driven = np.zeros(k, dtype=bool)
    for (b_src, s_idx, b_tgt, t_idx, w) in net:
        if b_tgt == GNEURON:
            is_self = (b_src == GNEURON and s_idx == t_idx)
            if not is_self:
                driven[t_idx] = True

    wired_sensors = sorted({s_idx for (b_src, s_idx, _, _, _) in net
                            if b_src == GSENSOR})
    return net, k, driven, wired_sensors


# ----------------------------------------------------------------------------
# Vectorised recurrent forward pass (batch over MC samples)
# ----------------------------------------------------------------------------
def forward_batch(net, k, driven, sensor_in):
    """sensor_in: (B, NUM_SENSORS). Returns actions (B, NUM_ACTIONS) after tanh.

    Runs T_STEPS recurrent updates with constant inputs, then one action pass
    from the steady neuron state.  Mirrors FeedForward + ActivateActions.
    """
    B = sensor_in.shape[0]
    neuron_out = np.full((B, k), INITIAL_NEURON_OUTPUT, dtype=np.float64)

    for _ in range(T_STEPS):
        neuron_acc = np.zeros((B, k), dtype=np.float64)
        for (b_src, s_idx, b_tgt, t_idx, w) in net:
            if b_tgt != GNEURON:
                continue  # action targets don't affect neuron state
            src = sensor_in[:, s_idx] if b_src == GSENSOR else neuron_out[:, s_idx]
            neuron_acc[:, t_idx] += src * w
        upd = np.tanh(neuron_acc)
        if k:
            neuron_out[:, driven] = upd[:, driven]

    action_lvl = np.zeros((B, NUM_ACTIONS), dtype=np.float64)
    for (b_src, s_idx, b_tgt, t_idx, w) in net:
        if b_tgt != GACTION:
            continue
        src = sensor_in[:, s_idx] if b_src == GSENSOR else neuron_out[:, s_idx]
        action_lvl[:, t_idx] += src * w
    return np.tanh(action_lvl)


def cue_influence(net, k, driven, wired_sensors):
    """For each wired sensor: mean |action(on) - action(off)| over MC random
    backgrounds. Returns (n_wired,) x (NUM_ACTIONS) influence matrix keyed by sensor idx."""
    infl = {}  # sensor_idx -> (NUM_ACTIONS,) mean abs delta
    if not wired_sensors:
        return infl
    # random background: wired sensors ~ U[0,1], others 0
    base = np.zeros((MC_SAMPLES, NUM_SENSORS), dtype=np.float64)
    base[:, wired_sensors] = RNG.random((MC_SAMPLES, len(wired_sensors)))
    for s in wired_sensors:
        on = base.copy();  on[:, s] = 1.0
        off = base.copy(); off[:, s] = 0.0
        a_on = forward_batch(net, k, driven, on)
        a_off = forward_batch(net, k, driven, off)
        infl[s] = np.abs(a_on - a_off).mean(axis=0)  # (NUM_ACTIONS,)
    return infl


# ----------------------------------------------------------------------------
# Genome file parsing
# ----------------------------------------------------------------------------
def load_genomes(path, genome_len=GENOME_LEN):
    """Returns list of (genes, fitness), genes = list of decoded genes."""
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
                try:
                    fit = float(parts[1])
                except ValueError:
                    fit = math.nan
            i += 2
        else:
            i += 1
        toks = gene_line.split()
        if len(toks) < genome_len:
            continue
        genes = [decode_gene(t) for t in toks]
        out.append((genes, fit))
    return out


# ----------------------------------------------------------------------------
# Structural summary (reproduce nn-structure-summary on final controllers)
# ----------------------------------------------------------------------------
def structural_stats(net, k):
    n_conn = len(net)
    if n_conn == 0:
        return dict(active_neurons=0, eff_connections=0, mean_abs_weight=0.0,
                    s2n=0, s2a=0, n2n=0, n2a=0)
    s2n = s2a = n2n = n2a = 0
    wsum = 0.0
    for (b_src, s_idx, b_tgt, t_idx, w) in net:
        wsum += abs(w)
        if b_src == GSENSOR and b_tgt == GNEURON: s2n += 1
        elif b_src == GSENSOR and b_tgt == GACTION: s2a += 1
        elif b_src == GNEURON and b_tgt == GNEURON: n2n += 1
        elif b_src == GNEURON and b_tgt == GACTION: n2a += 1
    return dict(active_neurons=k, eff_connections=n_conn,
                mean_abs_weight=wsum / n_conn,
                s2n=s2n, s2a=s2a, n2n=n2n, n2a=n2a)


# ----------------------------------------------------------------------------
# Main analysis per experiment
# ----------------------------------------------------------------------------
def analyse_experiment(exp, directory):
    print(f"\n=== {exp}  ({directory}) ===")
    influence_rows = []   # per (genome) -> per sensor
    struct_rows = []
    n_controllers = 0
    # accumulator: sensor_idx -> list of total influence (summed over actions)
    per_sensor_total = {s: [] for s in range(NUM_SENSORS)}
    per_sensor_byaction = {s: [] for s in range(NUM_SENSORS)}  # list of (NUM_ACTIONS,)
    wired_count = {s: 0 for s in range(NUM_SENSORS)}

    for seed in SEED_RANGE:
        matches = glob.glob(os.path.join(
            directory, f"best_genomes_*seed_{seed:02d}.txt"))
        if not matches:
            print(f"  seed {seed:02d}: MISSING")
            continue
        genomes = load_genomes(matches[0], GENOME_LENS.get(exp, GENOME_LEN))
        genomes.sort(key=lambda gf: (-gf[1] if not math.isnan(gf[1]) else 0.0))
        elite = genomes[:TOP_N_PER_SEED]
        for genes, fit in elite:
            net, k, driven, wired = build_net(genes)
            n_controllers += 1
            st = structural_stats(net, k)
            st.update(experiment=exp, seed=seed, fitness=fit)
            struct_rows.append(st)
            infl = cue_influence(net, k, driven, wired)
            for s in wired:
                wired_count[s] += 1
            for s, vec in infl.items():
                per_sensor_total[s].append(float(vec.sum()))
                per_sensor_byaction[s].append(vec)

    # aggregate cue influence
    infl_rows = []
    for s in range(NUM_SENSORS):
        vals = per_sensor_total[s]
        if not vals:
            continue
        arr = np.array(vals)
        by = np.array(per_sensor_byaction[s])  # (n, NUM_ACTIONS)
        row = dict(
            experiment=exp,
            sensor=SENSOR_NAMES[s],
            role=sensor_role(exp, SENSOR_NAMES[s]),
            n_controllers_wiring=len(vals),
            frac_controllers=len(vals) / max(1, n_controllers),
            mean_total_influence=float(arr.mean()),
            se_total_influence=float(arr.std(ddof=1) / math.sqrt(len(arr))) if len(arr) > 1 else 0.0,
        )
        for ai, an in enumerate(ACTION_NAMES):
            row[f"infl_{an}"] = float(by[:, ai].mean())
        infl_rows.append(row)

    infl_rows.sort(key=lambda r: -r["mean_total_influence"])
    env_rows = [r for r in infl_rows
                if r["role"] in ("environmental_primary", "environmental_weak")]
    internal_rows = [r for r in infl_rows if r["role"] == "internal"]

    # write tables (full table keeps every wired sensor + role; environmental
    # table is the headline cue-dominance ranking used for interpretation)
    import csv
    infl_path = os.path.join(OUT_TABLES, f"nn_cue_influence_{exp}.csv")
    if infl_rows:
        with open(infl_path, "w", newline="") as fh:
            w = csv.DictWriter(fh, fieldnames=list(infl_rows[0].keys()))
            w.writeheader(); w.writerows(infl_rows)
    env_path = os.path.join(OUT_TABLES, f"nn_cue_influence_{exp}_environmental.csv")
    if env_rows:
        with open(env_path, "w", newline="") as fh:
            w = csv.DictWriter(fh, fieldnames=list(env_rows[0].keys()))
            w.writeheader(); w.writerows(env_rows)
    struct_path = os.path.join(OUT_TABLES, f"nn_structure_summary_{exp}.csv")
    if struct_rows:
        with open(struct_path, "w", newline="") as fh:
            w = csv.DictWriter(fh, fieldnames=list(struct_rows[0].keys()))
            w.writeheader(); w.writerows(struct_rows)

    # console ranking (headline = environmental cues only)
    print(f"  controllers analysed: {n_controllers}")
    print(f"  ENVIRONMENTAL CUE DOMINANCE (informative fields only):")
    print(f"  {'sensor':<28}{'role':<24}{'mean_infl':>10}{'frac':>7}  top action")
    for r in env_rows:
        top_act = max(ACTION_NAMES, key=lambda a: r[f"infl_{a}"])
        print(f"  {r['sensor']:<28}{r['role']:<24}{r['mean_total_influence']:>10.4f}"
              f"{r['frac_controllers']:>7.2f}  {top_act}")
    if internal_rows:
        print(f"  internal-state inputs (not environmental cues):")
        for r in internal_rows:
            print(f"    {r['sensor']:<26}{r['mean_total_influence']:>10.4f}")

    # plot
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
        if env_rows:
            names = [r["sensor"] for r in env_rows]
            vals = [r["mean_total_influence"] for r in env_rows]
            ses = [r["se_total_influence"] for r in env_rows]
            colors = ["#3b7a9e" if r["role"] == "environmental_primary" else "#b0b0b0"
                      for r in env_rows]
            fig, ax = plt.subplots(figsize=(7, 0.4 * len(names) + 1.6))
            ax.barh(range(len(names)), vals, xerr=ses, color=colors)
            ax.set_yticks(range(len(names))); ax.set_yticklabels(names, fontsize=8)
            ax.invert_yaxis()
            ax.set_xlabel("Mean |action change| (cue on vs off), summed over actions")
            ax.set_title(f"{exp}: environmental cue influence on evolved controllers\n"
                         f"(top {TOP_N_PER_SEED}/seed, n={n_controllers})")
            from matplotlib.patches import Patch
            ax.legend(handles=[Patch(color="#3b7a9e", label="primary field"),
                               Patch(color="#b0b0b0", label="weak field")],
                      loc="lower right", fontsize=8)
            fig.tight_layout()
            fig.savefig(os.path.join(OUT_PLOTS, f"nn_cue_influence_{exp}.png"), dpi=150)
            plt.close(fig)
    except Exception as e:
        print(f"  (plot skipped: {e})")

    return env_rows


# ----------------------------------------------------------------------------
# Trajectory-based (gold-standard) cue analysis using real per-step sensor
# and action traces from the AP1 validation-rerun logging.
# ----------------------------------------------------------------------------
def load_trajectory_csv(path):
    """Parse one validation_trajectory_<exp>_<scenario>.csv (comma-separated,
    dot decimals). Returns dict of column name -> list, or None if the file
    is empty/malformed."""
    import csv as csv_mod
    rows = []
    with open(path, "r", newline="") as fh:
        reader = csv_mod.DictReader(fh)
        for row in reader:
            rows.append(row)
    if not rows:
        return None

    # Header uses mnemonic short names, not positional indices; recover the
    # sensor/action column order directly from the header instead (enum
    # order is preserved by the C++ writer, but don't assume the names).
    fieldnames = reader.fieldnames or []
    sensor_cols = [c for c in fieldnames if c.startswith("sensor_")]
    action_cols = [c for c in fieldnames if c.startswith("action_")]
    if len(sensor_cols) != NUM_SENSORS or len(action_cols) != NUM_ACTIONS:
        print(f"  WARNING: {os.path.basename(path)} has {len(sensor_cols)} sensor "
              f"cols / {len(action_cols)} action cols, expected {NUM_SENSORS}/{NUM_ACTIONS} "
              f"-> skipping")
        return None

    def to_f(x):
        try:
            return float(x)
        except (TypeError, ValueError):
            return math.nan

    sensors = np.array([[to_f(r[c]) for c in sensor_cols] for r in rows])
    actions = np.array([[to_f(r[c]) for c in action_cols] for r in rows])
    return {
        "sensor_cols": sensor_cols,
        "action_cols": action_cols,
        "sensors": sensors,   # (n_steps, NUM_SENSORS), header order
        "actions": actions,   # (n_steps, NUM_ACTIONS), header order
        "scenario": rows[0].get("scenario", ""),
        "agent_index": [r.get("agent_index") for r in rows],
    }


def realized_cue_influence(exp, glob_pattern):
    """For each sensor column actually present in the trajectory logs: realized
    influence = |corr(sensor, action)| averaged over actions, weighted by the
    number of steps contributing (i.e. pooled across all matching files),
    using the ACTUAL observed sensor variance (not a synthetic sweep). This
    directly answers "does this cue's real variation in the validation run
    move the controller's outputs", complementing the structural on/off sweep.
    """
    files = sorted(glob.glob(glob_pattern))
    if not files:
        print(f"  no trajectory files found for {exp}: {glob_pattern}")
        return []

    print(f"  found {len(files)} trajectory file(s) for {exp}")
    pooled_sensors = []
    pooled_actions = []
    sensor_cols = action_cols = None
    for f in files:
        parsed = load_trajectory_csv(f)
        if parsed is None:
            continue
        if sensor_cols is None:
            sensor_cols, action_cols = parsed["sensor_cols"], parsed["action_cols"]
        elif parsed["sensor_cols"] != sensor_cols or parsed["action_cols"] != action_cols:
            print(f"  WARNING: column order mismatch in {f}, skipping")
            continue
        pooled_sensors.append(parsed["sensors"])
        pooled_actions.append(parsed["actions"])

    if not pooled_sensors:
        return []

    sensors = np.concatenate(pooled_sensors, axis=0)
    actions = np.concatenate(pooled_actions, axis=0)
    n_steps = sensors.shape[0]

    # Map header short names back to SENSOR_NAMES/ACTION_NAMES via the enum
    # order the C++ writer uses (SensorShortName/ActionShortName iterate
    # ESensorType/EActionType in declaration order == SENSOR_NAMES/ACTION_NAMES).
    if len(sensor_cols) != len(SENSOR_NAMES) or len(action_cols) != len(ACTION_NAMES):
        print("  WARNING: trajectory column count does not match SENSOR_NAMES/ACTION_NAMES")

    rows = []
    for s_idx, s_name in enumerate(SENSOR_NAMES):
        if s_idx >= sensors.shape[1]:
            continue
        s_vals = sensors[:, s_idx]
        if np.all(np.isnan(s_vals)) or np.nanstd(s_vals) == 0:
            continue  # constant/missing sensor -> no realized influence, not an error
        corrs = []
        for a_idx in range(actions.shape[1]):
            a_vals = actions[:, a_idx]
            mask = ~np.isnan(s_vals) & ~np.isnan(a_vals)
            if mask.sum() < 2 or np.nanstd(a_vals[mask]) == 0:
                corrs.append(0.0)
                continue
            c = np.corrcoef(s_vals[mask], a_vals[mask])[0, 1]
            corrs.append(0.0 if np.isnan(c) else abs(c))
        row = dict(
            experiment=exp,
            sensor=s_name,
            role=sensor_role(exp, s_name),
            n_steps=int(n_steps),
            observed_sd=float(np.nanstd(s_vals)),
            mean_abs_corr=float(np.mean(corrs)),
        )
        for ai, an in enumerate(ACTION_NAMES):
            row[f"abs_corr_{an}"] = corrs[ai] if ai < len(corrs) else float("nan")
        rows.append(row)

    rows.sort(key=lambda r: -r["mean_abs_corr"])
    return rows


def analyse_trajectory(exp, glob_pattern):
    print(f"\n=== {exp} trajectory-based (gold-standard) cue analysis ===")
    rows = realized_cue_influence(exp, glob_pattern)
    if not rows:
        return []

    import csv as csv_mod
    out_path = os.path.join(OUT_TABLES, f"nn_cue_influence_trajectory_{exp}.csv")
    with open(out_path, "w", newline="") as fh:
        w = csv_mod.DictWriter(fh, fieldnames=list(rows[0].keys()))
        w.writeheader(); w.writerows(rows)
    print(f"  wrote {out_path}")

    env_rows = [r for r in rows if r["role"] in ("environmental_primary", "environmental_weak")]
    print(f"  REALIZED ENVIRONMENTAL CUE INFLUENCE (from actual validation-run sensor traces):")
    print(f"  {'sensor':<28}{'role':<24}{'mean|corr|':>10}{'observed_sd':>13}")
    for r in env_rows:
        print(f"  {r['sensor']:<28}{r['role']:<24}{r['mean_abs_corr']:>10.4f}{r['observed_sd']:>13.4f}")
    return rows


def main():
    all_rows = []
    for exp, directory in EXPERIMENTS.items():
        if not os.path.isdir(directory):
            print(f"skip {exp}: {directory} not found")
            continue
        all_rows.extend(analyse_experiment(exp, directory))

    if all_rows:
        import csv
        combined = os.path.join(OUT_TABLES, "nn_cue_influence_ranked.csv")
        with open(combined, "w", newline="") as fh:
            w = csv.DictWriter(fh, fieldnames=list(all_rows[0].keys()))
            w.writeheader(); w.writerows(all_rows)
        print(f"\nwrote {combined}")

    # Gold-standard trajectory-based analysis, where AP1 logs are available.
    # Additive: does not affect the synthetic-sweep tables/plots above.
    for exp, glob_pattern in TRAJECTORY_GLOB.items():
        analyse_trajectory(exp, glob_pattern)


if __name__ == "__main__":
    main()
