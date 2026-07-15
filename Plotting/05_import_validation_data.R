suppressPackageStartupMessages({
  library(readxl)
  library(dplyr)
  library(purrr)
  library(stringr)
  library(tidyr)
  library(readr)
})

find_repo_root <- function(start = getwd()) {
  current <- normalizePath(start, winslash = "/", mustWork = TRUE)
  repeat {
    if (dir.exists(file.path(current, "Validation")) &&
        dir.exists(file.path(current, "PLOS-Submission"))) {
      return(current)
    }
    parent <- dirname(current)
    if (identical(parent, current)) {
      stop("Could not locate repository root containing Validation and PLOS-Submission.")
    }
    current <- parent
  }
}

repo_root <- find_repo_root()
validation_dir <- file.path(repo_root, "Validation")
sim_evolution_dir <- file.path(repo_root, "coral-larvae-abm", "Content", "Evolution")
plotting_dir <- file.path(repo_root, "Plotting")
data_dir <- file.path(plotting_dir, "output", "data")
tables_dir <- file.path(plotting_dir, "output", "tables")
dir.create(data_dir, recursive = TRUE, showWarnings = FALSE)
dir.create(tables_dir, recursive = TRUE, showWarnings = FALSE)

# ---------------------------------------------------------------------------
# Primary source: the simulator's own appended validation logs under
# coral-larvae-abm/Content/Evolution/{validation_results,validation_positions,
# validation_environment}.txt (TAB-separated, European-comma decimals, one row
# per run/agent, multiple runs appended across timestamps/seeds).
#
# Fallback: the legacy manually-exported Validation/E1-Validation.xlsx and
# Validation/E2-Validation.xlsx workbooks (Results/Enviro/PosNN sheets), used
# only if the sim logs are absent, so older checkouts / manual re-exports keep
# working.
# ---------------------------------------------------------------------------

sim_results_txt <- file.path(sim_evolution_dir, "validation_results.txt")
sim_positions_txt <- file.path(sim_evolution_dir, "validation_positions.txt")
sim_environment_txt <- file.path(sim_evolution_dir, "validation_environment.txt")

use_sim_logs <- file.exists(sim_results_txt) &&
  file.exists(sim_positions_txt) &&
  file.exists(sim_environment_txt)

exp_id_map <- c(e1 = "E1", e2 = "E2", e3 = "E3")

# The sim logs are append-only and accumulate every validation run ever executed, so the same
# canonical scenario (e.g. V1A) can appear multiple times from different controller configurations
# across the project. To analyse only the current final-configuration battery, keep rows at or after
# this cutoff, which isolates the most recent (canonical-label) validation block. Override via the
# VALIDATION_MIN_TS environment variable if the batteries are re-run on another day.
validation_min_ts <- Sys.getenv("VALIDATION_MIN_TS", unset = "2026-07-11 00:00:00")

# European-decimal-comma numeric parsing: convert "1,2" -> "1.2" then parse.
# Leaves already-dotted / non-numeric-looking strings (e.g. genome_source
# paths, tile_geometry strings) untouched.
parse_euro_number <- function(x) {
  if (is.numeric(x)) return(x)
  suppressWarnings(as.numeric(gsub(",", ".", x, fixed = TRUE)))
}

# Read a TAB-separated sim log with comma-decimal numerics. Numeric-looking
# columns are converted; everything else (timestamp text, experiment,
# scenario, genome_source paths, tile_geometry, sound_source_positions) is
# left as character.
read_sim_log <- function(path) {
  raw <- readr::read_delim(
    path,
    delim = "\t",
    col_types = readr::cols(.default = readr::col_character()),
    trim_ws = TRUE,
    progress = FALSE
  )

  non_numeric_cols <- c(
    "timestamp", "experiment", "scenario", "genome_source",
    "tile_geometry", "sound_source_positions"
  )

  numeric_candidate_cols <- setdiff(names(raw), non_numeric_cols)

  out <- raw %>%
    mutate(across(all_of(numeric_candidate_cols), parse_euro_number))

  # Keep only the current final-configuration battery (see validation_min_ts above): drop older
  # appended runs of the same scenario labels from earlier controller configurations.
  if ("timestamp" %in% names(out)) {
    out <- out %>% filter(!is.na(timestamp), timestamp >= validation_min_ts)
  }
  out
}

# --- sim-log readers ---------------------------------------------------

read_sim_results <- function() {
  read_sim_log(sim_results_txt) %>%
    filter(tolower(experiment) %in% c("e1", "e2")) %>%
    mutate(
      experiment_id = unname(exp_id_map[tolower(experiment)]),
      workbook = normalizePath(sim_results_txt, winslash = "/", mustWork = TRUE),
      run_seed = as.integer(validation_genome_seed),
      .before = 1
    )
}

read_sim_enviro <- function() {
  read_sim_log(sim_environment_txt) %>%
    filter(tolower(experiment) %in% c("e1", "e2")) %>%
    mutate(
      experiment_id = unname(exp_id_map[tolower(experiment)]),
      workbook = normalizePath(sim_environment_txt, winslash = "/", mustWork = TRUE),
      run_seed = as.integer(validation_genome_seed),
      .before = 1
    )
}

read_sim_positions <- function() {
  read_sim_log(sim_positions_txt) %>%
    filter(tolower(experiment) %in% c("e1", "e2")) %>%
    mutate(
      experiment_id = unname(exp_id_map[tolower(experiment)]),
      workbook = normalizePath(sim_positions_txt, winslash = "/", mustWork = TRUE),
      position_sheet = NA_character_,
      sheet_seed = as.integer(validation_genome_seed),
      logged_validation_genome_seed = as.integer(validation_genome_seed),
      # No per-seed "sheet" concept in the appended log; run_seed is the
      # genome seed actually logged for that run.
      run_seed = as.integer(validation_genome_seed),
      .before = 1
    )
}

if (use_sim_logs) {
  message("Reading E1/E2 validation data directly from simulator logs in: ", sim_evolution_dir)

  # A validation "run" is one (genome seed x RNG seed) evaluation. The logs record both, so make
  # run_seed unique per run (genome*1000 + rng); this keeps the results<->positions<->enviro joins
  # 1:1 when several RNG seeds are evaluated per genome seed, and lets the per-run summaries average
  # over environment realizations (e.g. the E1 CCA dose-response, which is a per-cell-probability
  # effect that only resolves across multiple environments).
  composite_run_seed <- function(df) {
    if (all(c("validation_genome_seed", "random_seed") %in% names(df))) {
      df %>% mutate(run_seed = as.integer(validation_genome_seed) * 1000L + as.integer(random_seed))
    } else df
  }
  validation_results <- read_sim_results() %>% composite_run_seed() %>%
    arrange(experiment_id, scenario, run_seed)
  validation_enviro <- read_sim_enviro() %>% composite_run_seed() %>%
    arrange(experiment_id, scenario, run_seed)
  validation_positions <- read_sim_positions() %>% composite_run_seed() %>%
    arrange(experiment_id, scenario, run_seed, agent_index)
} else {
  stop("E1/E2 validation simulator logs not found under ", sim_evolution_dir, ".\n",
       "The canonical data are validation_{results,positions,environment}.txt (deposited to the\n",
       "public data repository; see PLOS-Submission/data-availability-package.md). The legacy\n",
       ".xlsx fallback was removed. Obtain the deposited logs, or run the analysis from the committed\n",
       "extracts in Plotting/output/data/ (scripts 06/08 read those directly).")
}

result_counts <- validation_results %>%
  count(experiment_id, scenario, name = "n_result_runs")
position_counts <- validation_positions %>%
  distinct(experiment_id, scenario, run_seed) %>%
  count(experiment_id, scenario, name = "n_position_runs")
position_agent_counts <- validation_positions %>%
  count(experiment_id, scenario, run_seed, name = "n_agents") %>%
  filter(n_agents != 250)

seed_label_mismatches <- validation_positions %>%
  distinct(experiment_id, scenario, position_sheet, run_seed, logged_validation_genome_seed, genome_source) %>%
  filter(!is.na(logged_validation_genome_seed), run_seed != logged_validation_genome_seed) %>%
  arrange(experiment_id, scenario, run_seed)

scenario_run_check <- full_join(result_counts, position_counts, by = c("experiment_id", "scenario")) %>%
  mutate(
    n_result_runs = replace_na(n_result_runs, 0L),
    n_position_runs = replace_na(n_position_runs, 0L),
    complete_30_results = n_result_runs == 30,
    complete_30_positions = n_position_runs == 30
  )

write.csv(validation_results, file.path(data_dir, "validation_results_e1_e2.csv"), row.names = FALSE)
write.csv(validation_enviro, file.path(data_dir, "validation_environment_e1_e2.csv"), row.names = FALSE)
write.csv(validation_positions, file.path(data_dir, "validation_positions_e1_e2.csv"), row.names = FALSE)
write.csv(scenario_run_check, file.path(tables_dir, "validation_import_run_counts_e1_e2.csv"), row.names = FALSE)
write.csv(position_agent_counts, file.path(tables_dir, "validation_import_agent_count_issues_e1_e2.csv"), row.names = FALSE)
write.csv(seed_label_mismatches, file.path(tables_dir, "validation_import_seed_label_mismatches_e1_e2.csv"), row.names = FALSE)

message("Wrote validation result data: ", file.path(data_dir, "validation_results_e1_e2.csv"))
message("Wrote validation position data: ", file.path(data_dir, "validation_positions_e1_e2.csv"))
message("Wrote validation import checks to: ", tables_dir)
