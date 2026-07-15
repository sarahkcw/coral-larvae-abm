suppressPackageStartupMessages({
  library(dplyr)
  library(purrr)
  library(stringr)
})

find_repo_root <- function(start = getwd()) {
  current <- normalizePath(start, winslash = "/", mustWork = TRUE)
  repeat {
    if (dir.exists(file.path(current, "Training")) && dir.exists(file.path(current, "PLOS-Submission"))) return(current)
    parent <- dirname(current)
    if (identical(parent, current)) stop("Could not locate repository root.")
    current <- parent
  }
}

repo_root <- find_repo_root()
training_dir <- file.path(repo_root, "Training")
plotting_dir <- file.path(repo_root, "Plotting")
data_dir <- file.path(plotting_dir, "output", "data")
dir.create(data_dir, recursive = TRUE, showWarnings = FALSE)

phase1_summary_path <- file.path(data_dir, "training_run_summary_generation999.csv")
if (!file.exists(phase1_summary_path)) source(file.path(plotting_dir, "01_import_training_data.R"))
phase1_summary <- read.csv(phase1_summary_path, stringsAsFactors = FALSE)

baseline_map <- tibble::tribble(
  ~experiment, ~method,   ~setup_id, ~setup_label,
  "E1",        "Elitism", "H1",      "Baseline",
  "E2",        "SUS",     "H1",      "Baseline",
  "E3",        "Elitism", "H1",      "Baseline"
)

baseline_summary <- phase1_summary %>%
  inner_join(baseline_map, by = c("experiment", "method")) %>%
  mutate(
    parameter_change = "Baseline",
    population_size = 250,
    genome_length = 50,
    mutation_rate = 0.005,
    source = "Phase 1 winner baseline"
  )

# ---------------------------------------------------------------------------
# Discover per-seed hyperparameter-sensitivity training CSVs written by the
# batch driver (same WriteTrainingGenerationRow format as Phase 1), under the
# Reduced-Mutation / Reduced-Genome / Reduced-Pop subfolders of each
# experiment's baseline-method folder.
#
#   Training/E1/E1-Elitism/Reduced-Mutation/train_e1_Elitism_seed_01.csv
#   Training/E3/Elitism/Reduced-Pop/train_e3_Elitism_seed_07.csv
# ---------------------------------------------------------------------------

hyper_setup_map <- tibble::tribble(
  ~experiment, ~method,   ~setup_id, ~setup_label,        ~parameter_change, ~population_size, ~genome_length, ~mutation_rate, ~subfolder,
  "E1",        "Elitism", "H2",      "Reduced mutation", "mutation_rate",   250,              50,             0.001,          "Reduced-Mutation",
  "E1",        "Elitism", "H3",      "Reduced genome",   "genome_length",   250,              25,             0.005,          "Reduced-Genome",
  "E1",        "Elitism", "H4",      "Reduced pop",      "population_size", 125,              50,             0.005,          "Reduced-Pop",
  "E2",        "SUS",     "H2",      "Reduced mutation", "mutation_rate",   250,              50,             0.001,          "Reduced-Mutation",
  "E2",        "SUS",     "H3",      "Reduced genome",   "genome_length",   250,              25,             0.005,          "Reduced-Genome",
  "E2",        "SUS",     "H4",      "Reduced pop",      "population_size", 125,              50,             0.005,          "Reduced-Pop",
  "E3",        "Elitism", "H2",      "Reduced mutation", "mutation_rate",   250,              50,             0.001,          "Reduced-Mutation",
  "E3",        "Elitism", "H3",      "Reduced genome",   "genome_length",   250,              25,             0.005,          "Reduced-Genome",
  "E3",        "Elitism", "H4",      "Reduced pop",      "population_size", 125,              50,             0.005,          "Reduced-Pop"
)

# Baseline (Phase 1) method folder per experiment, used to locate the
# hyperparameter subfolders (mirrors the directory layout produced by the
# batch driver's GetTrainingOutputDir()).
method_folder_map <- tibble::tribble(
  ~experiment, ~method,   ~method_folder,
  "E1",        "Elitism", "E1-Elitism",
  "E2",        "SUS",     "SUS",
  "E3",        "Elitism", "Elitism"
)

hyper_setup_map <- hyper_setup_map %>%
  left_join(method_folder_map, by = c("experiment", "method")) %>%
  mutate(dir = file.path(training_dir, experiment, method_folder, subfolder))

filename_pattern <- "^train_([A-Za-z0-9]+)_(.+)_seed_([0-9]+)\\.csv$"

parse_number <- function(x) {
  if (is.numeric(x)) return(x)
  as.numeric(gsub(",", ".", as.character(x), fixed = TRUE))
}

expected_cols <- c(
  "Generation", "AvgFitness", "MaxFitness", "GeneticDiversity",
  "TotalSettlers", "CorrectSettlers", "BoundaryContacts"
)

read_hyper_csv <- function(path, experiment, method, setup_id, setup_label, parameter_change,
                            population_size, genome_length, mutation_rate) {
  fname <- basename(path)
  m <- str_match(fname, filename_pattern)
  if (is.na(m[1, 1])) {
    warning("Skipping file with unexpected name pattern: ", path)
    return(NULL)
  }
  seed <- as.integer(m[1, 4])

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
      setup_id = setup_id,
      setup_label = setup_label,
      parameter_change = parameter_change,
      population_size = population_size,
      genome_length = genome_length,
      mutation_rate = mutation_rate,
      seed = seed,
      source = normalizePath(path, winslash = "/", mustWork = TRUE),
      .before = 1
    ) %>%
    filter(Generation >= 0, Generation <= 999)
}

read_hyper_setup <- function(experiment, method, setup_id, setup_label, parameter_change,
                              population_size, genome_length, mutation_rate, dir, ...) {
  if (!dir.exists(dir)) {
    message("No hyperparameter folder yet for ", experiment, "/", method, "/", setup_label,
            " at ", dir, " (skipping).")
    return(NULL)
  }
  files <- list.files(dir, pattern = "^train_.*_seed_[0-9]+\\.csv$", full.names = TRUE)
  if (length(files) == 0) {
    message("No training CSVs yet for ", experiment, "/", method, "/", setup_label,
            " under ", dir, " (skipping).")
    return(NULL)
  }
  map_dfr(files, read_hyper_csv, experiment = experiment, method = method,
           setup_id = setup_id, setup_label = setup_label, parameter_change = parameter_change,
           population_size = population_size, genome_length = genome_length,
           mutation_rate = mutation_rate)
}

hyper_long <- pmap_dfr(hyper_setup_map, read_hyper_setup)

if (nrow(hyper_long) == 0) {
  warning("No hyperparameter-sensitivity training CSVs found yet under any Reduced-Mutation/",
          "Reduced-Genome/Reduced-Pop folder. Writing baseline-only outputs.")
}

hyper_long <- hyper_long %>%
  arrange(experiment, setup_id, seed, desc(Generation))

hyper_run_summary <- if (nrow(hyper_long) > 0) {
  hyper_long %>%
    group_by(experiment, method, setup_id, setup_label, parameter_change, population_size, genome_length, mutation_rate, seed) %>%
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
      incorrect_settlers = final_total_settlers - final_correct_settlers,
      source = "Phase 2 hyperparameter run"
    )
} else {
  tibble::tibble()
}

baseline_summary <- baseline_summary %>%
  select(any_of(names(hyper_run_summary))) %>%
  mutate(source = "Phase 1 winner baseline")

combined_hyper_summary <- bind_rows(baseline_summary, hyper_run_summary) %>%
  mutate(
    setup_id = factor(setup_id, levels = c("H1", "H2", "H3", "H4")),
    setup_label = factor(setup_label, levels = c("Baseline", "Reduced mutation", "Reduced genome", "Reduced pop"))
  ) %>%
  arrange(experiment, setup_id, seed)

write.csv(hyper_long, file.path(data_dir, "hyperparameter_generation_long.csv"), row.names = FALSE)
write.csv(combined_hyper_summary, file.path(data_dir, "hyperparameter_run_summary_generation999.csv"), row.names = FALSE)

message("Wrote hyperparameter generation data: ", file.path(data_dir, "hyperparameter_generation_long.csv"))
message("Wrote hyperparameter run summary: ", file.path(data_dir, "hyperparameter_run_summary_generation999.csv"))
