suppressPackageStartupMessages({
  library(dplyr)
  library(tidyr)
  library(ggplot2)
  library(purrr)
  library(stringr)
  library(boot)
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
plotting_dir <- file.path(repo_root, "Plotting")
data_dir <- file.path(plotting_dir, "output", "data")
tables_dir <- file.path(plotting_dir, "output", "tables")
plots_dir <- file.path(plotting_dir, "output", "plots")
dir.create(tables_dir, recursive = TRUE, showWarnings = FALSE)
dir.create(plots_dir, recursive = TRUE, showWarnings = FALSE)

run_summary_path <- file.path(data_dir, "training_run_summary_generation999.csv")
long_path <- file.path(data_dir, "training_generation_long.csv")

if (!file.exists(run_summary_path) || !file.exists(long_path)) {
  source(file.path(plotting_dir, "01_import_training_data.R"))
}

run_summary <- read.csv(run_summary_path, stringsAsFactors = FALSE)
training_long <- read.csv(long_path, stringsAsFactors = FALSE)

method_levels <- c("Elitism", "Truncation", "SUS")
run_summary <- run_summary %>%
  mutate(
    experiment = factor(experiment, levels = c("E1", "E2", "E3")),
    method = factor(method, levels = method_levels)
  )
training_long <- training_long %>%
  mutate(
    experiment = factor(experiment, levels = c("E1", "E2", "E3")),
    method = factor(method, levels = method_levels)
  )

metrics <- c(
  "final_avg_fitness",
  "final_max_fitness",
  "final_correct_settlers",
  "final_total_settlers",
  "final_boundary_contacts",
  "final_genetic_diversity",
  "correct_settlement_rate",
  "incorrect_settlers"
)

median_ci <- function(x, conf = 0.95, reps = 5000) {
  x <- x[is.finite(x)]
  if (length(x) < 2) {
    return(c(low = NA_real_, high = NA_real_))
  }
  stat <- function(data, indices) median(data[indices], na.rm = TRUE)
  set.seed(20260623)
  b <- boot::boot(x, statistic = stat, R = reps)
  alpha <- (1 - conf) / 2
  stats::quantile(b$t[, 1], probs = c(alpha, 1 - alpha), na.rm = TRUE, names = FALSE) |>
    setNames(c("low", "high"))
}

summarise_metric <- function(data, metric) {
  data %>%
    group_by(experiment, method) %>%
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
  arrange(experiment, metric, method)

write.csv(
  method_summary,
  file.path(tables_dir, "method_summary_generation999.csv"),
  row.names = FALSE
)

kruskal_tests <- expand.grid(
  experiment = levels(run_summary$experiment),
  metric = metrics,
  stringsAsFactors = FALSE
) %>%
  mutate(test = map2(experiment, metric, function(exp_id, metric) {
    dat <- run_summary %>% filter(experiment == exp_id)
    stats::kruskal.test(dat[[metric]] ~ dat$method)
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

write.csv(
  kruskal_tests,
  file.path(tables_dir, "kruskal_tests_generation999.csv"),
  row.names = FALSE
)

pairwise_wilcox <- function(data, exp_id, metric) {
  dat <- data %>% filter(experiment == exp_id)
  pairs <- combn(method_levels, 2, simplify = FALSE)
  map_dfr(pairs, function(pair) {
    x <- dat %>% filter(method == pair[1]) %>% pull(all_of(metric))
    y <- dat %>% filter(method == pair[2]) %>% pull(all_of(metric))
    test <- stats::wilcox.test(x, y, exact = FALSE)
    tibble(
      experiment = exp_id,
      metric = metric,
      method_1 = pair[1],
      method_2 = pair[2],
      median_1 = median(x, na.rm = TRUE),
      median_2 = median(y, na.rm = TRUE),
      median_difference = median_1 - median_2,
      p_value = test$p.value
    )
  }) %>%
    mutate(p_holm = p.adjust(p_value, method = "holm"))
}

pairwise_tests <- expand.grid(
  experiment = levels(run_summary$experiment),
  metric = metrics,
  stringsAsFactors = FALSE
) %>%
  pmap_dfr(~ pairwise_wilcox(run_summary, ..1, ..2))

write.csv(
  pairwise_tests,
  file.path(tables_dir, "pairwise_wilcox_generation999.csv"),
  row.names = FALSE
)

cliffs_delta <- function(x, y) {
  x <- x[is.finite(x)]
  y <- y[is.finite(y)]
  if (length(x) == 0 || length(y) == 0) {
    return(NA_real_)
  }
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

cliffs_delta_table <- expand.grid(
  experiment = levels(run_summary$experiment),
  metric = metrics,
  stringsAsFactors = FALSE
) %>%
  pmap_dfr(function(experiment, metric) {
    exp_id <- experiment
    dat <- run_summary %>% filter(.data$experiment == .env$exp_id)
    combn(method_levels, 2, simplify = FALSE) %>%
      map_dfr(function(pair) {
        x <- dat %>% filter(method == pair[1]) %>% pull(all_of(metric))
        y <- dat %>% filter(method == pair[2]) %>% pull(all_of(metric))
        delta <- cliffs_delta(x, y)
        tibble(
          experiment = exp_id,
          metric = metric,
          method_1 = pair[1],
          method_2 = pair[2],
          cliffs_delta = delta,
          magnitude = delta_magnitude(delta)
        )
      })
  })

write.csv(
  cliffs_delta_table,
  file.path(tables_dir, "cliffs_delta_generation999.csv"),
  row.names = FALSE
)

primary_metric <- tibble::tribble(
  ~experiment, ~primary_metric,              ~diagnostic_metric,
  "E1",        "final_correct_settlers",     "final_avg_fitness",
  "E2",        "final_correct_settlers",     "final_avg_fitness",
  "E3",        "final_avg_fitness",          "final_total_settlers"
)

recommendation <- method_summary %>%
  inner_join(primary_metric, by = "experiment") %>%
  filter(metric == primary_metric) %>%
  group_by(experiment) %>%
  arrange(desc(median), desc(mean), .by_group = TRUE) %>%
  mutate(rank_primary = row_number()) %>%
  ungroup() %>%
  select(
    experiment,
    recommended_method = method,
    primary_metric = metric,
    primary_median = median,
    primary_q1 = q1,
    primary_q3 = q3,
    primary_mean = mean,
    primary_sd = sd,
    rank_primary
  ) %>%
  filter(rank_primary == 1) %>%
  left_join(
    method_summary %>%
      inner_join(primary_metric, by = "experiment") %>%
      filter(metric == diagnostic_metric) %>%
      select(
        experiment,
        recommended_method = method,
        diagnostic_metric = metric,
        diagnostic_median = median,
        diagnostic_q1 = q1,
        diagnostic_q3 = q3
      ),
    by = c("experiment", "recommended_method")
  )

write.csv(
  recommendation,
  file.path(tables_dir, "experiment_method_recommendation.csv"),
  row.names = FALSE
)

plot_metric_box <- function(metric, y_label, file_name) {
  p <- ggplot(run_summary, aes(x = method, y = .data[[metric]], fill = method)) +
    geom_boxplot(width = 0.65, outlier.shape = NA, alpha = 0.75) +
    geom_jitter(width = 0.12, size = 1.8, alpha = 0.75) +
    facet_wrap(~ experiment, scales = "free_y") +
    labs(x = NULL, y = y_label) +
    theme_minimal(base_size = 11) +
    theme(
      legend.position = "none",
      panel.grid.minor = element_blank(),
      strip.text = element_text(face = "bold")
    )
  ggsave(file.path(plots_dir, file_name), p, width = 8, height = 4.8, dpi = 300)
}

plot_metric_box("final_avg_fitness", "Final average fitness, generation 999", "final_avg_fitness_boxplot.png")
plot_metric_box("final_correct_settlers", "Correct settlers, generation 999", "correct_settlers_boxplot.png")
plot_metric_box("final_total_settlers", "Total settlers, generation 999", "total_settlers_boxplot.png")
plot_metric_box("final_boundary_contacts", "Boundary contacts, generation 999", "boundary_contacts_boxplot.png")

learning_curves <- training_long %>%
  group_by(experiment, method, Generation) %>%
  summarise(
    median_avg_fitness = median(AvgFitness, na.rm = TRUE),
    q1_avg_fitness = quantile(AvgFitness, 0.25, na.rm = TRUE),
    q3_avg_fitness = quantile(AvgFitness, 0.75, na.rm = TRUE),
    .groups = "drop"
  )

p_learning <- ggplot(
  learning_curves,
  aes(x = Generation, y = median_avg_fitness, color = method, fill = method)
) +
  geom_ribbon(aes(ymin = q1_avg_fitness, ymax = q3_avg_fitness), alpha = 0.15, color = NA) +
  geom_line(linewidth = 0.7) +
  facet_wrap(~ experiment, scales = "free_y") +
  labs(x = "Generation", y = "Median average fitness") +
  theme_minimal(base_size = 11) +
  theme(
    panel.grid.minor = element_blank(),
    strip.text = element_text(face = "bold"),
    legend.position = "bottom"
  )

ggsave(
  file.path(plots_dir, "learning_curves_avg_fitness.png"),
  p_learning,
  width = 8,
  height = 5,
  dpi = 300
)

message("Wrote tables to: ", tables_dir)
message("Wrote plots to: ", plots_dir)

