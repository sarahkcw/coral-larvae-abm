suppressPackageStartupMessages({
  library(dplyr)
  library(tidyr)
  library(ggplot2)
  library(purrr)
  library(boot)
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
plotting_dir <- file.path(repo_root, "Plotting")
data_dir <- file.path(plotting_dir, "output", "data")
tables_dir <- file.path(plotting_dir, "output", "tables")
plots_dir <- file.path(plotting_dir, "output", "plots")
dir.create(tables_dir, recursive = TRUE, showWarnings = FALSE)
dir.create(plots_dir, recursive = TRUE, showWarnings = FALSE)

summary_path <- file.path(data_dir, "hyperparameter_run_summary_generation999.csv")
long_path <- file.path(data_dir, "hyperparameter_generation_long.csv")
if (!file.exists(summary_path) || !file.exists(long_path)) source(file.path(plotting_dir, "03_import_hyperparameter_data.R"))

run_summary <- read.csv(summary_path, stringsAsFactors = FALSE) %>%
  mutate(
    experiment = factor(experiment, levels = c("E1", "E2", "E3")),
    setup_id = factor(setup_id, levels = c("H1", "H2", "H3", "H4")),
    setup_label = factor(setup_label, levels = c("Baseline", "Reduced mutation", "Reduced genome", "Reduced pop"))
  )
training_long <- read.csv(long_path, stringsAsFactors = FALSE) %>%
  mutate(
    experiment = factor(experiment, levels = c("E1", "E2", "E3")),
    setup_id = factor(setup_id, levels = c("H2", "H3", "H4")),
    setup_label = factor(setup_label, levels = c("Reduced mutation", "Reduced genome", "Reduced pop"))
  )

metrics <- c("final_avg_fitness", "final_max_fitness", "final_correct_settlers", "final_total_settlers", "final_boundary_contacts", "final_genetic_diversity", "correct_settlement_rate", "incorrect_settlers")
primary_metric <- tibble::tribble(
  ~experiment, ~primary_metric,          ~diagnostic_metric,
  "E1",        "final_correct_settlers", "final_avg_fitness",
  "E2",        "final_correct_settlers", "final_avg_fitness",
  "E3",        "final_avg_fitness",      "final_total_settlers"
)

median_ci <- function(x, conf = 0.95, reps = 5000) {
  x <- x[is.finite(x)]
  if (length(x) < 2) return(c(low = NA_real_, high = NA_real_))
  stat <- function(data, indices) median(data[indices], na.rm = TRUE)
  set.seed(20260702)
  b <- boot::boot(x, statistic = stat, R = reps)
  alpha <- (1 - conf) / 2
  stats::quantile(b$t[, 1], probs = c(alpha, 1 - alpha), na.rm = TRUE, names = FALSE) |>
    setNames(c("low", "high"))
}

summarise_metric <- function(data, metric) {
  data %>%
    group_by(experiment, setup_id, setup_label, parameter_change) %>%
    summarise(
      metric = metric,
      n = sum(is.finite(.data[[metric]])),
      mean = mean(.data[[metric]], na.rm = TRUE),
      sd = sd(.data[[metric]], na.rm = TRUE),
      median = median(.data[[metric]], na.rm = TRUE),
      q1 = quantile(.data[[metric]], 0.25, na.rm = TRUE),
      q3 = quantile(.data[[metric]], 0.75, na.rm = TRUE),
      min = min(.data[[metric]], na.rm = TRUE),
      max = max(.data[[metric]], na.rm = TRUE),
      median_ci_low = median_ci(.data[[metric]])["low"],
      median_ci_high = median_ci(.data[[metric]])["high"],
      .groups = "drop"
    )
}

method_summary <- map_dfr(metrics, ~ summarise_metric(run_summary, .x)) %>%
  arrange(experiment, metric, setup_id)
write.csv(method_summary, file.path(tables_dir, "hyperparameter_summary_generation999.csv"), row.names = FALSE)

kruskal_tests <- expand.grid(experiment = levels(run_summary$experiment), metric = metrics, stringsAsFactors = FALSE) %>%
  mutate(test = map2(experiment, metric, function(exp_id, metric) {
    dat <- run_summary %>% filter(.data$experiment == .env$exp_id)
    stats::kruskal.test(dat[[metric]] ~ dat$setup_id)
  })) %>%
  transmute(
    experiment,
    metric,
    statistic = map_dbl(test, ~ unname(.x$statistic)),
    df = map_dbl(test, ~ unname(.x$parameter)),
    p_value = map_dbl(test, ~ .x$p.value)
  ) %>%
  group_by(metric) %>%
  mutate(p_holm_across_experiments = p.adjust(p_value, method = "holm")) %>%
  ungroup()
write.csv(kruskal_tests, file.path(tables_dir, "hyperparameter_kruskal_generation999.csv"), row.names = FALSE)

pairwise_vs_baseline <- function(data, exp_id, metric) {
  dat <- data %>% filter(.data$experiment == .env$exp_id)
  baseline <- dat %>% filter(setup_id == "H1") %>% pull(all_of(metric))
  c("H2", "H3", "H4") %>%
    map_dfr(function(setup) {
      changed <- dat %>% filter(setup_id == setup) %>% pull(all_of(metric))
      test <- stats::wilcox.test(baseline, changed, exact = FALSE)
      tibble(
        experiment = exp_id,
        metric = metric,
        baseline_setup = "H1",
        comparison_setup = setup,
        baseline_median = median(baseline, na.rm = TRUE),
        comparison_median = median(changed, na.rm = TRUE),
        median_difference_baseline_minus_comparison = baseline_median - comparison_median,
        p_value = test$p.value
      )
    }) %>%
    mutate(p_holm = p.adjust(p_value, method = "holm"))
}
pairwise_tests <- expand.grid(experiment = levels(run_summary$experiment), metric = metrics, stringsAsFactors = FALSE) %>%
  pmap_dfr(~ pairwise_vs_baseline(run_summary, ..1, ..2))
write.csv(pairwise_tests, file.path(tables_dir, "hyperparameter_pairwise_vs_baseline_generation999.csv"), row.names = FALSE)

cliffs_delta <- function(x, y) {
  x <- x[is.finite(x)]
  y <- y[is.finite(y)]
  if (length(x) == 0 || length(y) == 0) return(NA_real_)
  comparisons <- outer(x, y, "-")
  (sum(comparisons > 0) - sum(comparisons < 0)) / (length(x) * length(y))
}
delta_magnitude <- function(delta) {
  abs_delta <- abs(delta)
  case_when(
    is.na(abs_delta) ~ NA_character_,
    abs_delta < 0.147 ~ "negligible",
    abs_delta < 0.33 ~ "small",
    abs_delta < 0.474 ~ "medium",
    TRUE ~ "large"
  )
}
cliffs_delta_table <- expand.grid(experiment = levels(run_summary$experiment), metric = metrics, stringsAsFactors = FALSE) %>%
  pmap_dfr(function(experiment, metric) {
    exp_id <- experiment
    dat <- run_summary %>% filter(.data$experiment == .env$exp_id)
    baseline <- dat %>% filter(setup_id == "H1") %>% pull(all_of(metric))
    c("H2", "H3", "H4") %>%
      map_dfr(function(setup) {
        changed <- dat %>% filter(setup_id == setup) %>% pull(all_of(metric))
        delta <- cliffs_delta(baseline, changed)
        tibble(experiment = exp_id, metric = metric, baseline_setup = "H1", comparison_setup = setup, cliffs_delta = delta, magnitude = delta_magnitude(delta))
      })
  })
write.csv(cliffs_delta_table, file.path(tables_dir, "hyperparameter_cliffs_delta_vs_baseline_generation999.csv"), row.names = FALSE)

primary_summary <- method_summary %>%
  inner_join(primary_metric, by = "experiment") %>%
  filter(metric == primary_metric) %>%
  select(experiment, setup_id, setup_label, parameter_change, metric, n, mean, sd, median, q1, q3, min, max, median_ci_low, median_ci_high)
write.csv(primary_summary, file.path(tables_dir, "hyperparameter_primary_summary_generation999.csv"), row.names = FALSE)

plot_primary <- run_summary %>%
  mutate(
    primary_value = case_when(
      experiment %in% c("E1", "E2") ~ final_correct_settlers,
      experiment == "E3" ~ final_avg_fitness,
      TRUE ~ NA_real_
    )
  )

p_primary <- ggplot(plot_primary, aes(x = setup_label, y = primary_value, fill = setup_label)) +
  geom_boxplot(outlier.shape = NA, alpha = 0.75) +
  geom_jitter(width = 0.12, size = 1.7, alpha = 0.75) +
  facet_wrap(~ experiment, scales = "free_y") +
  labs(x = NULL, y = "Primary endpoint at generation 999") +
  theme_minimal(base_size = 11) +
  theme(legend.position = "none", axis.text.x = element_text(angle = 25, hjust = 1), panel.grid.minor = element_blank(), strip.text = element_text(face = "bold"))
ggsave(file.path(plots_dir, "hyperparameter_primary_endpoint_boxplot.png"), p_primary, width = 8.5, height = 5, dpi = 300)

p_e3_settlement <- ggplot(run_summary %>% filter(experiment == "E3"), aes(x = setup_label, y = final_total_settlers, fill = setup_label)) +
  geom_boxplot(outlier.shape = NA, alpha = 0.75) +
  geom_jitter(width = 0.12, size = 1.7, alpha = 0.75) +
  labs(x = NULL, y = "E3 total settlers at generation 999") +
  theme_minimal(base_size = 11) +
  theme(legend.position = "none", axis.text.x = element_text(angle = 25, hjust = 1), panel.grid.minor = element_blank())
ggsave(file.path(plots_dir, "hyperparameter_e3_total_settlers_boxplot.png"), p_e3_settlement, width = 6.5, height = 4.5, dpi = 300)

message("Wrote hyperparameter tables to: ", tables_dir)
message("Wrote hyperparameter plots to: ", plots_dir)