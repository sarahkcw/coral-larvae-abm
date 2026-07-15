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
validation_workbook <- file.path(repo_root, "Validation", "E3-Validation.xlsx")
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
# per run/agent, multiple runs appended across timestamps/seeds), filtered to
# experiment == "e3".
#
# Fallback: the legacy manually-exported Validation/E3-Validation.xlsx
# workbook (Results/Enviro/PosNN sheets), used only if the sim logs are
# absent.
# ---------------------------------------------------------------------------

sim_results_txt <- file.path(sim_evolution_dir, "validation_results.txt")
sim_positions_txt <- file.path(sim_evolution_dir, "validation_positions.txt")
sim_environment_txt <- file.path(sim_evolution_dir, "validation_environment.txt")

use_sim_logs <- file.exists(sim_results_txt) &&
  file.exists(sim_positions_txt) &&
  file.exists(sim_environment_txt)

# The append-only sim logs accumulate every validation run ever executed; keep only rows at or after
# this cutoff to isolate the current final-configuration battery (see 05_import_validation_data.R).
validation_min_ts <- Sys.getenv("VALIDATION_MIN_TS", unset = "2026-07-11 00:00:00")

# European-decimal-comma numeric parsing: convert "1,2" -> "1.2" then parse.
parse_euro_number <- function(x) {
  if (is.numeric(x)) return(x)
  suppressWarnings(as.numeric(gsub(",", ".", x, fixed = TRUE)))
}

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
  if ("timestamp" %in% names(out)) {
    out <- out %>% filter(!is.na(timestamp), timestamp >= validation_min_ts)
  }
  out
}

read_sim_results <- function() {
  read_sim_log(sim_results_txt) %>%
    filter(tolower(experiment) == "e3") %>%
    mutate(
      experiment_id = "E3",
      workbook = normalizePath(sim_results_txt, winslash = "/", mustWork = TRUE),
      run_seed = as.integer(validation_genome_seed),
      .before = 1
    )
}

read_sim_enviro <- function() {
  read_sim_log(sim_environment_txt) %>%
    filter(tolower(experiment) == "e3") %>%
    mutate(
      experiment_id = "E3",
      workbook = normalizePath(sim_environment_txt, winslash = "/", mustWork = TRUE),
      run_seed = as.integer(validation_genome_seed),
      .before = 1
    )
}

read_sim_positions <- function() {
  read_sim_log(sim_positions_txt) %>%
    filter(tolower(experiment) == "e3") %>%
    mutate(
      experiment_id = "E3",
      workbook = normalizePath(sim_positions_txt, winslash = "/", mustWork = TRUE),
      position_sheet = NA_character_,
      sheet_seed = as.integer(validation_genome_seed),
      logged_validation_genome_seed = as.integer(validation_genome_seed),
      run_seed = as.integer(validation_genome_seed),
      .before = 1
    )
}

if (use_sim_logs) {
  message("Reading E3 validation data directly from simulator logs in: ", sim_evolution_dir)

  # Unique run_seed per (genome seed x RNG seed) so multi-RNG batteries keep 1:1 joins (see 05).
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
  stop("E3 validation simulator logs not found under ", sim_evolution_dir, ".\n",
       "The canonical data are validation_{results,positions,environment}.txt (deposited to the\n",
       "public data repository; see PLOS-Submission/data-availability-package.md). Obtain the\n",
       "deposited logs, or run the analysis from the committed extracts in Plotting/output/data/.")
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

write.csv(validation_results, file.path(data_dir, "validation_results_e3.csv"), row.names = FALSE)
write.csv(validation_enviro, file.path(data_dir, "validation_environment_e3.csv"), row.names = FALSE)
write.csv(validation_positions, file.path(data_dir, "validation_positions_e3.csv"), row.names = FALSE)
write.csv(scenario_run_check, file.path(tables_dir, "validation_import_run_counts_e3.csv"), row.names = FALSE)
write.csv(position_agent_counts, file.path(tables_dir, "validation_import_agent_count_issues_e3.csv"), row.names = FALSE)
write.csv(seed_label_mismatches, file.path(tables_dir, "validation_import_seed_label_mismatches_e3.csv"), row.names = FALSE)

message("Wrote E3 validation result data: ", file.path(data_dir, "validation_results_e3.csv"))
message("Wrote E3 validation position data: ", file.path(data_dir, "validation_positions_e3.csv"))
message("Wrote E3 validation import checks to: ", tables_dir)
