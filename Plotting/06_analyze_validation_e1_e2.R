suppressPackageStartupMessages({
  library(dplyr)
  library(tidyr)
  library(purrr)
  library(ggplot2)
  library(boot)
})

find_repo_root <- function(start = getwd()) {
  current <- normalizePath(start, winslash = "/", mustWork = TRUE)
  repeat {
    if (dir.exists(file.path(current, "Validation")) && dir.exists(file.path(current, "PLOS-Submission"))) return(current)
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

results_path <- file.path(data_dir, "validation_results_e1_e2.csv")
positions_path <- file.path(data_dir, "validation_positions_e1_e2.csv")
env_path <- file.path(data_dir, "validation_environment_e1_e2.csv")
if (!file.exists(results_path) || !file.exists(positions_path) || !file.exists(env_path)) {
  source(file.path(plotting_dir, "05_import_validation_data.R"))
}

validation_results <- read.csv(results_path, stringsAsFactors = FALSE)
validation_positions <- read.csv(positions_path, stringsAsFactors = FALSE)
validation_enviro <- read.csv(env_path, stringsAsFactors = FALSE)

scenario_levels <- c("V1A", "V1B", "V1C", "V1D", "V2A", "V2B", "V2C", "V2D",
                      "V2BASE", "V2CUR",                        # E2 final battery at n=300 (base + current)
                      "V2M1", "V2M2", "V2M3", "V2M4", "V2M5")  # E2 light-attenuation sweep (R1-3)
validation_results <- validation_results %>%
  mutate(
    scenario = factor(scenario, levels = scenario_levels),
    experiment_id = factor(experiment_id, levels = c("E1", "E2"))
  )
validation_positions <- validation_positions %>%
  mutate(
    scenario = factor(scenario, levels = scenario_levels),
    experiment_id = factor(experiment_id, levels = c("E1", "E2"))
  )
validation_enviro <- validation_enviro %>%
  mutate(
    scenario = factor(scenario, levels = scenario_levels),
    experiment_id = factor(experiment_id, levels = c("E1", "E2"))
  )

median_ci <- function(x, conf = 0.95, reps = 5000) {
  x <- x[is.finite(x)]
  if (length(x) < 2) return(c(low = NA_real_, high = NA_real_))
  stat <- function(data, indices) median(data[indices], na.rm = TRUE)
  set.seed(20260706)
  b <- boot::boot(x, statistic = stat, R = reps)
  alpha <- (1 - conf) / 2
  stats::quantile(b$t[, 1], probs = c(alpha, 1 - alpha), na.rm = TRUE, names = FALSE) |>
    setNames(c("low", "high"))
}

summarise_metric <- function(data, metric) {
  data %>%
    group_by(experiment_id, scenario) %>%
    summarise(
      metric = metric,
      n = sum(is.finite(.data[[metric]])),
      mean = mean(.data[[metric]], na.rm = TRUE),
      sd = sd(.data[[metric]], na.rm = TRUE),
      se = sd / sqrt(n),
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

run_metrics <- validation_results %>%
  mutate(
    total_settlement_rate = total_settlers / agents,
    correct_settlement_rate = correct_settlers / agents,
    incorrect_settlers = total_settlers - correct_settlers,
    boundary_contacts_per_agent = boundary_contacts / agents
  )

result_metrics <- c(
  "total_settlers", "correct_settlers", "total_settlement_rate", "correct_settlement_rate",
  "settlement_percent", "correct_settlement_percent", "premature_settlers",
  "mean_settlement_step", "boundary_contacts", "boundary_contacts_per_agent",
  "avg_fitness", "max_fitness"
)

validation_summary <- map_dfr(result_metrics, ~ summarise_metric(run_metrics, .x)) %>%
  arrange(experiment_id, metric, scenario)
write.csv(validation_summary, file.path(tables_dir, "validation_summary_e1_e2.csv"), row.names = FALSE)

kruskal_metrics <- c("correct_settlement_rate", "total_settlement_rate", "boundary_contacts_per_agent", "avg_fitness")
kruskal_tests <- expand.grid(experiment_id = c("E1", "E2"), metric = kruskal_metrics, stringsAsFactors = FALSE) %>%
  mutate(test = map2(experiment_id, metric, function(exp_id, metric) {
    dat <- run_metrics %>% filter(.data$experiment_id == .env$exp_id)
    stats::kruskal.test(dat[[metric]] ~ dat$scenario)
  })) %>%
  transmute(
    experiment_id,
    metric,
    statistic = map_dbl(test, ~ unname(.x$statistic)),
    df = map_dbl(test, ~ unname(.x$parameter)),
    p_value = map_dbl(test, ~ .x$p.value)
  ) %>%
  group_by(experiment_id) %>%
  mutate(p_holm_within_experiment = p.adjust(p_value, method = "holm")) %>%
  ungroup()
write.csv(kruskal_tests, file.path(tables_dir, "validation_kruskal_e1_e2.csv"), row.names = FALSE)

pairwise_wilcox <- function(data, exp_id, metric) {
  dat <- data %>% filter(.data$experiment_id == .env$exp_id)
  scenarios <- sort(unique(as.character(dat$scenario)))
  combn(scenarios, 2, simplify = FALSE) %>%
    map_dfr(function(pair) {
      x <- dat %>% filter(as.character(scenario) == pair[1]) %>% pull(all_of(metric))
      y <- dat %>% filter(as.character(scenario) == pair[2]) %>% pull(all_of(metric))
      test <- stats::wilcox.test(x, y, exact = FALSE)
      tibble(
        experiment_id = exp_id,
        metric = metric,
        scenario_1 = pair[1],
        scenario_2 = pair[2],
        median_1 = median(x, na.rm = TRUE),
        median_2 = median(y, na.rm = TRUE),
        median_difference = median_1 - median_2,
        p_value = test$p.value
      )
    }) %>%
    mutate(p_holm = p.adjust(p_value, method = "holm"))
}

pairwise_tests <- expand.grid(experiment_id = c("E1", "E2"), metric = kruskal_metrics, stringsAsFactors = FALSE) %>%
  pmap_dfr(~ pairwise_wilcox(run_metrics, ..1, ..2))
write.csv(pairwise_tests, file.path(tables_dir, "validation_pairwise_wilcox_e1_e2.csv"), row.names = FALSE)

nearest_neighbor_mean <- function(x, y) {
  coords <- cbind(x, y)
  coords <- coords[stats::complete.cases(coords), , drop = FALSE]
  n <- nrow(coords)
  if (n < 2) return(NA_real_)
  d <- as.matrix(stats::dist(coords))
  diag(d) <- Inf
  mean(apply(d, 1, min), na.rm = TRUE)
}

e1_area <- run_metrics %>%
  filter(experiment_id == "E1") %>%
  transmute(scenario, run_seed, tank_area_cm2 = world_x * world_y)

e1_spatial_by_run <- validation_positions %>%
  filter(experiment_id == "E1", correct_settler == 1) %>%
  group_by(experiment_id, scenario, run_seed) %>%
  summarise(
    correct_settlers_from_positions = n(),
    mean_nearest_neighbor_cm = nearest_neighbor_mean(final_x, final_y),
    .groups = "drop"
  ) %>%
  left_join(e1_area, by = c("scenario", "run_seed")) %>%
  mutate(
    expected_nn_random_cm = if_else(correct_settlers_from_positions > 1, 0.5 / sqrt(correct_settlers_from_positions / tank_area_cm2), NA_real_),
    clarke_evans_r = mean_nearest_neighbor_cm / expected_nn_random_cm
  ) %>%
  right_join(
    run_metrics %>% filter(experiment_id == "E1") %>% select(experiment_id, scenario, run_seed, correct_settlers, total_settlers, correct_settlement_rate, actual_agents = agents),
    by = c("experiment_id", "scenario", "run_seed")
  ) %>%
  arrange(scenario, run_seed)

write.csv(e1_spatial_by_run, file.path(tables_dir, "e1_spatial_summary_by_run.csv"), row.names = FALSE)

e1_spatial_summary <- bind_rows(
  summarise_metric(e1_spatial_by_run, "mean_nearest_neighbor_cm"),
  summarise_metric(e1_spatial_by_run, "clarke_evans_r")
) %>% arrange(metric, scenario)
write.csv(e1_spatial_summary, file.path(tables_dir, "e1_spatial_summary_by_scenario.csv"), row.names = FALSE)

# E1 binomial GLMs corresponding to the manuscript Table 2 logic.
# The new validation logs recover combined tile area per run, not a per-agent tile assignment.
# Therefore patch-size GLMs are rebuilt as run-level binomial models against total tile area.
e1_env_area <- validation_enviro %>%
  filter(experiment_id == "E1") %>%
  transmute(
    experiment_id,
    scenario,
    run_seed,
    actual_tile_area_sum = as.numeric(actual_tile_area_sum),
    actual_tile_count = as.numeric(actual_tile_count)
  )

e1_glm_data <- run_metrics %>%
  filter(experiment_id == "E1") %>%
  select(experiment_id, scenario, run_seed, agents, correct_settlers, total_settlers) %>%
  left_join(e1_env_area, by = c("experiment_id", "scenario", "run_seed")) %>%
  left_join(
    e1_spatial_by_run %>%
      select(experiment_id, scenario, run_seed, correct_settlers_from_positions, mean_nearest_neighbor_cm, clarke_evans_r),
    by = c("experiment_id", "scenario", "run_seed")
  ) %>%
  mutate(
    non_correct = agents - correct_settlers,
    # Strict direct-contact definition from the current larval volume under the 1 UU = 1 cm scale.
    larval_volume_cm3 = 0.0314,
    larval_contact_diameter_cm = 2 * ((3 * larval_volume_cm3) / (4 * pi))^(1 / 3)
  )

has_direct_contact <- function(x, y, threshold_cm) {
  coords <- cbind(x, y)
  coords <- coords[stats::complete.cases(coords), , drop = FALSE]
  if (nrow(coords) < 2) return(0L)
  d <- as.matrix(stats::dist(coords))
  diag(d) <- Inf
  as.integer(any(d < threshold_cm, na.rm = TRUE))
}

e1_aggregation_by_run <- validation_positions %>%
  filter(experiment_id == "E1", correct_settler == 1) %>%
  group_by(experiment_id, scenario, run_seed) %>%
  summarise(
    aggregation_strict = has_direct_contact(final_x, final_y, 2 * ((3 * 0.0314) / (4 * pi))^(1 / 3)),
    # Retained only as a provenance check for the old C++ analysis helper, which used an unrealistically large hard-coded threshold.
    aggregation_legacy_cpp_threshold = has_direct_contact(final_x, final_y, 7.0),
    .groups = "drop"
  )

e1_glm_data <- e1_glm_data %>%
  left_join(e1_aggregation_by_run, by = c("experiment_id", "scenario", "run_seed")) %>%
  mutate(
    aggregation_strict = replace_na(aggregation_strict, 0L),
    aggregation_legacy_cpp_threshold = replace_na(aggregation_legacy_cpp_threshold, 0L)
  )

fit_e1_settlement_glm <- function(data) {
  # Use the position-derived correct-settler count: the results-log correct_settlers field is
  # unreliable for E1 (logs 0), whereas the per-agent correct_settler flags in the position log are
  # correct. Recompute the binomial denominator from it.
  dat <- data %>%
    filter(is.finite(actual_tile_area_sum), agents > 0,
           is.finite(correct_settlers_from_positions)) %>%
    mutate(cs = pmin(pmax(as.integer(round(correct_settlers_from_positions)), 0L), as.integer(agents)),
           non_cs = as.integer(agents) - cs)
  if (nrow(dat) < 3 || dplyr::n_distinct(dat$actual_tile_area_sum) < 2) return(NULL)
  stats::glm(cbind(cs, non_cs) ~ actual_tile_area_sum, data = dat, family = stats::binomial())
}

fit_e1_aggregation_glm <- function(data, response) {
  dat <- data %>% filter(is.finite(actual_tile_area_sum))
  if (nrow(dat) < 3 || dplyr::n_distinct(dat[[response]]) < 2 || dplyr::n_distinct(dat$actual_tile_area_sum) < 2) return(NULL)
  stats::glm(stats::as.formula(paste(response, "~ actual_tile_area_sum")), data = dat, family = stats::binomial())
}

tidy_glm_slope <- function(glm_model, model_name, response_name) {
  if (is.null(glm_model)) {
    return(tibble(model = model_name, response = response_name, intercept = NA_real_, slope = NA_real_, std_error = NA_real_, z_value = NA_real_, p_value = NA_real_, aic = NA_real_))
  }
  coefs <- summary(glm_model)$coefficients
  tibble(
    model = model_name,
    response = response_name,
    intercept = unname(coefs["(Intercept)", "Estimate"]),
    slope = unname(coefs["actual_tile_area_sum", "Estimate"]),
    std_error = unname(coefs["actual_tile_area_sum", "Std. Error"]),
    z_value = unname(coefs["actual_tile_area_sum", "z value"]),
    p_value = unname(coefs["actual_tile_area_sum", "Pr(>|z|)"]),
    aic = stats::AIC(glm_model)
  )
}

e1_glm_results <- e1_glm_data %>%
  group_by(experiment_id, scenario) %>%
  group_modify(~ bind_rows(
    tidy_glm_slope(fit_e1_settlement_glm(.x), "binomial_settlement_total_area", "correct_settlers_vs_not"),
    tidy_glm_slope(fit_e1_aggregation_glm(.x, "aggregation_strict"), "binomial_aggregation_total_area_strict", "any_direct_contact_strict"),
    tidy_glm_slope(fit_e1_aggregation_glm(.x, "aggregation_legacy_cpp_threshold"), "binomial_aggregation_total_area_legacy_cpp_threshold", "any_direct_contact_legacy_cpp_threshold")
  )) %>%
  ungroup()

write.csv(e1_glm_data, file.path(tables_dir, "e1_glm_input_by_run.csv"), row.names = FALSE)
write.csv(e1_glm_results, file.path(tables_dir, "e1_glm_total_area_results.csv"), row.names = FALSE)

# E2 vertical zones use the manuscript/original-experiment scale in cm.
depth_zone_levels <- c("<40 cm", "40-80 cm", "80-120 cm", "120-160 cm", "160-200 cm", ">200 cm")
e2_positions <- validation_positions %>%
  filter(experiment_id == "E2") %>%
  mutate(
    experiment_id = factor(as.character(experiment_id)),
    scenario = factor(as.character(scenario)),
    depth_zone = cut(
      final_z,
      breaks = c(-Inf, 40, 80, 120, 160, 200, Inf),
      labels = depth_zone_levels,
      right = FALSE
    ),
    depth_zone = factor(as.character(depth_zone), levels = depth_zone_levels)
  )

e2_depth_by_run <- e2_positions %>%
  count(experiment_id, scenario, run_seed, depth_zone, name = "larvae") %>%
  tidyr::complete(experiment_id, scenario, run_seed, depth_zone, fill = list(larvae = 0L)) %>%
  group_by(experiment_id, scenario, run_seed) %>%
  mutate(total_larvae = sum(larvae), proportion = if_else(total_larvae > 0, larvae / total_larvae, NA_real_)) %>%
  ungroup() %>%
  arrange(scenario, run_seed, depth_zone)
write.csv(e2_depth_by_run, file.path(tables_dir, "e2_depth_zone_counts_by_run.csv"), row.names = FALSE)

e2_depth_by_scenario <- e2_depth_by_run %>%
  group_by(experiment_id, scenario, depth_zone) %>%
  summarise(
    runs = n(),
    total_larvae = sum(larvae),
    mean_larvae_per_run = mean(larvae),
    sd_larvae_per_run = sd(larvae),
    mean_proportion = mean(proportion),
    sd_proportion = sd(proportion),
    .groups = "drop"
  ) %>%
  arrange(scenario, depth_zone)
write.csv(e2_depth_by_scenario, file.path(tables_dir, "e2_depth_zone_counts_by_scenario.csv"), row.names = FALSE)

e2_depth_distribution_test <- e2_depth_by_run %>%
  group_by(scenario, depth_zone) %>%
  summarise(larvae = sum(larvae), .groups = "drop") %>%
  tidyr::pivot_wider(names_from = depth_zone, values_from = larvae, values_fill = 0)
chi_matrix <- as.matrix(e2_depth_distribution_test[, -1])
rownames(chi_matrix) <- as.character(e2_depth_distribution_test$scenario)
chi_test <- suppressWarnings(stats::chisq.test(chi_matrix))
set.seed(20260706)
chi_test_simulated <- stats::chisq.test(chi_matrix, simulate.p.value = TRUE, B = 10000)
e2_depth_chi <- tibble(
  test = c(
    "E2 depth-zone distribution differs among validation scenarios; asymptotic chi-square",
    "E2 depth-zone distribution differs among validation scenarios; Monte Carlo p-value"
  ),
  statistic = c(unname(chi_test$statistic), unname(chi_test_simulated$statistic)),
  df = c(unname(chi_test$parameter), NA_real_),
  p_value = c(chi_test$p.value, chi_test_simulated$p.value)
)
write.csv(e2_depth_chi, file.path(tables_dir, "e2_depth_zone_chisquare_across_scenarios.csv"), row.names = FALSE)

# Compact manuscript-facing tables.
e1_manuscript_summary <- validation_summary %>%
  filter(experiment_id == "E1", metric %in% c("correct_settlement_rate", "total_settlement_rate", "boundary_contacts_per_agent")) %>%
  select(experiment_id, scenario, metric, n, mean, sd, se, median, q1, q3, min, max)
write.csv(e1_manuscript_summary, file.path(tables_dir, "e1_validation_manuscript_summary.csv"), row.names = FALSE)

e2_manuscript_summary <- validation_summary %>%
  filter(experiment_id == "E2", metric %in% c("correct_settlement_rate", "total_settlement_rate", "boundary_contacts_per_agent", "mean_settlement_step")) %>%
  select(experiment_id, scenario, metric, n, mean, sd, se, median, q1, q3, min, max)
write.csv(e2_manuscript_summary, file.path(tables_dir, "e2_validation_manuscript_summary.csv"), row.names = FALSE)

p_e1_settlement <- ggplot(run_metrics %>% filter(experiment_id == "E1"), aes(x = scenario, y = correct_settlement_rate, fill = scenario)) +
  geom_boxplot(outlier.shape = NA, alpha = 0.75) +
  geom_jitter(width = 0.12, size = 1.7, alpha = 0.7) +
  scale_y_continuous(labels = scales::percent_format(accuracy = 1)) +
  labs(x = NULL, y = "Correct settlement rate") +
  theme_minimal(base_size = 11) +
  theme(legend.position = "none", panel.grid.minor = element_blank())
ggsave(file.path(plots_dir, "e1_validation_correct_settlement_rate.png"), p_e1_settlement, width = 6.5, height = 4.2, dpi = 300)

p_e2_settlement <- ggplot(run_metrics %>% filter(experiment_id == "E2"), aes(x = scenario, y = correct_settlement_rate, fill = scenario)) +
  geom_boxplot(outlier.shape = NA, alpha = 0.75) +
  geom_jitter(width = 0.12, size = 1.7, alpha = 0.7) +
  scale_y_continuous(labels = scales::percent_format(accuracy = 1)) +
  labs(x = NULL, y = "Correct settlement rate") +
  theme_minimal(base_size = 11) +
  theme(legend.position = "none", panel.grid.minor = element_blank())
ggsave(file.path(plots_dir, "e2_validation_correct_settlement_rate.png"), p_e2_settlement, width = 6.5, height = 4.2, dpi = 300)

p_e2_depth <- ggplot(e2_depth_by_scenario, aes(x = scenario, y = mean_proportion, fill = depth_zone)) +
  geom_col(color = "white", linewidth = 0.2) +
  scale_y_continuous(labels = scales::percent_format(accuracy = 1)) +
  labs(x = NULL, y = "Mean final-position proportion", fill = "Height zone") +
  theme_minimal(base_size = 11) +
  theme(panel.grid.minor = element_blank(), legend.position = "right")
ggsave(file.path(plots_dir, "e2_validation_depth_zone_distribution.png"), p_e2_depth, width = 7.2, height = 4.5, dpi = 300)

message("Wrote E1/E2 validation summary tables to: ", tables_dir)
message("Wrote E1/E2 validation plots to: ", plots_dir)




