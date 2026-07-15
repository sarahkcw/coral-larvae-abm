# Analysis pipeline (training + validation + revision analyses)

This folder contains the R + Python workflow: training
analysis (`01`–`04`), validation analysis (`05`–`08`), and additional analyses (`09`–`17`). Scripts are numbered in run order; outputs
land in `output/{data,tables,plots}`.

R scripts use R 4.4.1; Python scripts need `numpy`, `matplotlib`, `openpyxl`.

## Scripts

- `01_import_training_data.R`
  - discovers and reads all per-seed batch-driver training CSVs
    (`train_<exp>_<method>_seed_<NN>.csv`, written by
    `ASimulationManager::WriteTrainingGenerationRow`) under
    `../Training/E{1,2,3}/<method>/`
  - writes long generation-level data and generation-999 run summaries
  - skips (with a message, not an error) any experiment/method combination
    that has no CSVs yet, so partial training progress still produces output
- `02_analyze_training_selection.R`
  - compares selection methods per experiment
  - writes descriptive summaries, Kruskal-Wallis tests, pairwise Wilcoxon tests, Cliff's delta effect sizes, and plots
- `03_import_hyperparameter_data.R`
  - discovers and reads the Phase 2 reduced mutation/genome/population
    per-seed CSVs from the `Reduced-Mutation`/`Reduced-Genome`/`Reduced-Pop`
    subfolders of each experiment's baseline-method folder (same CSV format
    as `01`)
  - combines them with the Phase 1 winner baselines
  - skips (with a message) any hyperparameter variant that has no CSVs yet
- `04_analyze_hyperparameter_sensitivity.R`
  - compares each hyperparameter reduction against the baseline winner setup
  - writes summaries, tests, effect sizes, and plots
- `05_import_validation_data.R`
  - reads the revised E1 and E2 validation workbooks from `../Validation`
  - writes run-level, environment, and agent-position CSV files plus import checks
- `06_analyze_validation_e1_e2.R`
  - computes E1/E2 validation summaries, nonparametric tests, E1 spatial aggregation statistics, E2 height-zone distributions, and validation plots
- `07_import_validation_e3.R`
  - reads the revised E3 validation workbook from `../Validation`
  - writes run-level, environment, and agent-position CSV files plus import checks
- `08_analyze_validation_e3.R`
  - computes E3 sound-attraction summaries, final distance-zone distributions, height-zone distributions, and validation plots
- `09_nn_cue_ablation.py` — cue dominance: realized (variance-weighted sensor
  influence) + intrinsic (single-cue sweeps) per experiment
- `10_convergence_reliability.py` — GA convergence + reliability across 30 seeds
  (fitness + diversity vs generation; success rates)
- `11_sim_vs_lab_gof.py` — E2 distributional goodness-of-fit vs Tay et al. 2011 time course.
  `11b_gof_e1_e3.py` — E1/E3 sim-vs-lab comparisons
- `12_baselines_noise.py` — simpler-controller baselines (random + reactive vs
  evolved) and sensory-noise robustness
- `13_e1_patchsize_figure.py` — E1 controlled patch-size sweep at fixed CCA cover.
- `17_effect_sizes.py` — Kendall's W effect sizes for the Friedman selection
  tests (E1/E2/E3, methods + hyperparameters)
- `18_weight_distribution.py` — distribution of the evolved controllers' active
  synaptic weights + mean incoming weight by sensor class
- `19_manuscript_distribution_figures.py` — E2 depth + E3 vertical/horizontal
  distribution heatmaps (`dis2`, `dis3vertical`, `dis3horizontal`) from the
  current validation counts





