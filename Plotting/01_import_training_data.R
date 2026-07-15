suppressPackageStartupMessages({
  library(dplyr)
  library(purrr)
  library(stringr)
  library(tidyr)
})

find_repo_root <- function(start = getwd()) {
  current <- normalizePath(start, winslash = "/", mustWork = TRUE)
  repeat {
    if (dir.exists(file.path(current, "Training")) &&
        dir.exists(file.path(current, "PLOS-Submission"))) {
      return(current)
    }
    parent <- dirname(current)
    if (identical(parent, current)) {
      stop("Could not locate repository root containing Training and PLOS-Submission.")
    }
    current <- parent
  }
}

repo_root <- find_repo_root()
training_dir <- file.path(repo_root, "Training")
plotting_dir <- file.path(repo_root, "Plotting")
data_dir <- file.path(plotting_dir, "output", "data")
dir.create(data_dir, recursive = TRUE, showWarnings = FALSE)

# ---------------------------------------------------------------------------
# Discover per-seed training CSVs written by the batch driver
# (ASimulationManager::WriteTrainingGenerationRow, SimulationManager.cpp).
#
# Files are named train_<exp>_<method>_seed_<NN>.csv, e.g.
#   Training/E1/E1-Elitism/train_e1_Elitism_seed_01.csv
#   Training/E3/Elitism/train_e3_Elitism_seed_07.csv
#
# Header written by the batch driver:
#   Generation,AvgFitness,MaxFitness,GeneticDiversity,TotalSettlers,CorrectSettlers,BoundaryContacts
#
# We only look under Training/E1, Training/E2, Training/E3 (not E3_depr or
# hyperparameter subfolders like Reduced-Mutation/Reduced-Pop/Reduced-Genome,
# which are handled by 03_import_hyperparameter_data.R).
# ---------------------------------------------------------------------------

experiments <- c("E1", "E2", "E3")

filename_pattern <- "^train_([A-Za-z0-9]+)_(.+)_seed_([0-9]+)\\.csv$"

discover_training_csvs <- function(experiment) {
  exp_dir <- file.path(training_dir, experiment)
  if (!dir.exists(exp_dir)) {
    return(character(0))
  }
  all_files <- list.files(exp_dir, pattern = "^train_.*_seed_[0-9]+\\.csv$",
                           recursive = TRUE, full.names = TRUE)
  # Exclude hyperparameter sensitivity subfolders; those are imported by
  # 03_import_hyperparameter_data.R against the Phase 1 winner only.
  all_files[!grepl("Reduced-(Mutation|Pop|Genome)", all_files, ignore.case = TRUE)]
}

training_files <- map(experiments, discover_training_csvs) %>%
  set_names(experiments)

# Report what is missing so runs with only some experiments/methods trained
# so far still produce partial output instead of aborting.
for (experiment in experiments) {
  if (length(training_files[[experiment]]) == 0) {
    message("No training CSVs found yet for ", experiment,
            " under ", file.path(training_dir, experiment),
            " (skipping).")
  }
}

all_training_files <- unlist(training_files, use.names = FALSE)

if (length(all_training_files) == 0) {
  stop("No train_*_seed_*.csv files found under any of Training/E1, Training/E2, Training/E3. ",
       "Run the batch training driver first (see docs/workflows.md).")
}

# ---------------------------------------------------------------------------
# Robust numeric parsing: batch-driver CSVs use '.' decimals (FString::
# SanitizeFloat is locale-independent), but handle ',' decimals defensively
# in case of legacy/manually-edited exports (precedent: 08_analyze_validation_e3.R).
# ---------------------------------------------------------------------------
parse_number <- function(x) {
  if (is.numeric(x)) return(x)
  as.numeric(gsub(",", ".", as.character(x), fixed = TRUE))
}

expected_cols <- c(
  "Generation", "AvgFitness", "MaxFitness", "GeneticDiversity",
  "TotalSettlers", "CorrectSettlers", "BoundaryContacts"
)

read_training_csv <- function(path) {
  fname <- basename(path)
  m <- str_match(fname, filename_pattern)
  if (is.na(m[1, 1])) {
    warning("Skipping file with unexpected name pattern: ", path)
    return(NULL)
  }
  experiment_raw <- m[1, 2]
  method <- m[1, 3]
  seed <- as.integer(m[1, 4])

  # Filenames use lowercase experiment labels (e1/e2/e3, see
  # ASimulationManager::GetExperimentLabel); normalise to E1/E2/E3.
  experiment <- toupper(experiment_raw)

  raw <- tryCatch(
    read.csv(path, stringsAsFactors = FALSE, colClasses = "character"),
    error = function(e) {
      warning("Failed to read ", path, ": ", conditionMessage(e))
      NULL
    }
  )
  if (is.null(raw) || nrow(raw) == 0) {
    warning("No rows in ", path, "; skipping.")
    return(NULL)
  }

  missing_cols <- setdiff(expected_cols, names(raw))
  if (length(missing_cols) > 0) {
    warning("File ", path, " is missing expected column(s): ",
            paste(missing_cols, collapse = ", "), "; skipping.")
    return(NULL)
  }

  raw %>%
    transmute(
      Generation = as.integer(parse_number(.data$Generation)),
      AvgFitness = parse_number(.data$AvgFitness),
      MaxFitness = parse_number(.data$MaxFitness),
      GeneticDiversity = parse_number(.data$GeneticDiversity),
      TotalSettlers = as.integer(parse_number(.data$TotalSettlers)),
      CorrectSettlers = as.integer(parse_number(.data$CorrectSettlers)),
      BoundaryContacts = as.integer(parse_number(.data$BoundaryContacts))
    ) %>%
    filter(!is.na(Generation)) %>%
    mutate(
      experiment = experiment,
      method = method,
      seed = seed,
      workbook = normalizePath(path, winslash = "/", mustWork = TRUE),
      .before = 1
    ) %>%
    filter(Generation >= 0, Generation <= 999)
}

training_long <- map_dfr(all_training_files, read_training_csv) %>%
  arrange(experiment, method, seed, desc(Generation))

if (nrow(training_long) == 0) {
  stop("Training CSVs were found but none could be parsed. Check file format against ",
       "ASimulationManager::WriteTrainingGenerationRow in SimulationManager.cpp.")
}

generation_check <- training_long %>%
  count(experiment, method, seed, name = "n_generations") %>%
  filter(n_generations != 1000)

if (nrow(generation_check) > 0) {
  warning("Some runs do not contain exactly 1000 generations after filtering 0:999 ",
          "(this is expected for in-progress or short training runs).")
  print(generation_check)
}

run_summary <- training_long %>%
  group_by(experiment, method, seed) %>%
  summarise(
    final_generation = max(Generation, na.rm = TRUE),
    final_avg_fitness = AvgFitness[Generation == 999][1],
    final_max_fitness = MaxFitness[Generation == 999][1],
    final_genetic_diversity = GeneticDiversity[Generation == 999][1],
    final_total_settlers = TotalSettlers[Generation == 999][1],
    final_correct_settlers = CorrectSettlers[Generation == 999][1],
    final_boundary_contacts = BoundaryContacts[Generation == 999][1],
    peak_avg_fitness = max(AvgFitness, na.rm = TRUE),
    peak_max_fitness = max(MaxFitness, na.rm = TRUE),
    min_avg_fitness = min(AvgFitness, na.rm = TRUE),
    n_generations = n(),
    .groups = "drop"
  ) %>%
  mutate(
    correct_settlement_rate = final_correct_settlers / 250,
    total_settlement_rate = final_total_settlers / 250,
    incorrect_settlers = final_total_settlers - final_correct_settlers
  ) %>%
  arrange(experiment, method, seed)

write.csv(
  training_long,
  file.path(data_dir, "training_generation_long.csv"),
  row.names = FALSE
)
write.csv(
  run_summary,
  file.path(data_dir, "training_run_summary_generation999.csv"),
  row.names = FALSE
)

message("Wrote generation data: ", file.path(data_dir, "training_generation_long.csv"))
message("Wrote run summary: ", file.path(data_dir, "training_run_summary_generation999.csv"))

present_combos <- run_summary %>% distinct(experiment, method) %>% arrange(experiment, method)
all_combos <- expand.grid(experiment = experiments,
                           method = c("Elitism", "Truncation", "SUS"),
                           stringsAsFactors = FALSE)
missing_combos <- anti_join(all_combos, present_combos, by = c("experiment", "method"))
if (nrow(missing_combos) > 0) {
  message("No training data yet for the following experiment/method combinations ",
          "(skipped, not an error):")
  print(missing_combos)
}
