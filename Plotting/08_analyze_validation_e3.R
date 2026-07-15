suppressPackageStartupMessages({
  library(dplyr)
  library(tidyr)
  library(purrr)
  library(ggplot2)
  library(stringr)
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
      stop("Could not locate repository root.")
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

results_path <- file.path(data_dir, "validation_results_e3.csv")
positions_path <- file.path(data_dir, "validation_positions_e3.csv")
env_path <- file.path(data_dir, "validation_environment_e3.csv")
if (!file.exists(results_path) || !file.exists(positions_path) || !file.exists(env_path)) {
  source(file.path(plotting_dir, "07_import_validation_e3.R"))
}

validation_results <- read.csv(results_path, stringsAsFactors = FALSE)
validation_positions <- read.csv(positions_path, stringsAsFactors = FALSE)
validation_enviro <- read.csv(env_path, stringsAsFactors = FALSE)

scenario_levels <- c("V3A", "V3B", "V3C", "V3D", "V3E", "V3F",
                      "V3SPL140", "V3SPL153", "V3SPL160", "V3FLO", "V3FHI")  # E3 acoustic sweep (R1-3); source at V3A position
validation_results <- validation_results %>%
  mutate(
    scenario = factor(scenario, levels = scenario_levels),
    experiment_id = factor(experiment_id, levels = "E3")
  )
validation_positions <- validation_positions %>%
  mutate(
    scenario = factor(scenario, levels = scenario_levels),
    experiment_id = factor(experiment_id, levels = "E3")
  )
validation_enviro <- validation_enviro %>%
  mutate(
    scenario = factor(scenario, levels = scenario_levels),
    experiment_id = factor(experiment_id, levels = "E3")
  )

parse_number <- function(x) {
  as.numeric(gsub(",", ".", x, fixed = TRUE))
}

parse_sound_center <- function(sound_source_positions) {
  if (is.na(sound_source_positions) || sound_source_positions == "" || sound_source_positions == "none") {
    return(tibble(sound_x = NA_real_, sound_y = NA_real_, sound_z = NA_real_))
  }

  parts <- unlist(strsplit(sound_source_positions, ";", fixed = TRUE))
  parsed <- map_dfr(parts, function(part) {
    tibble(
      sound_x = parse_number(str_match(part, "x=(.*?),y=")[, 2]),
      sound_y = parse_number(str_match(part, "y=(.*?),z=")[, 2]),
      sound_z = parse_number(str_match(part, "z=(.*)$")[, 2])
    )
  })

  summarise(parsed, sound_x = mean(sound_x, na.rm = TRUE), sound_y = mean(sound_y, na.rm = TRUE), sound_z = mean(sound_z, na.rm = TRUE))
}

sound_centers <- validation_enviro %>%
  mutate(sound_source_positions = if ("sound_source_positions" %in% names(.)) sound_source_positions else NA_character_) %>%
  select(experiment_id, scenario, run_seed, sound_source_positions) %>%
  mutate(parsed = map(sound_source_positions, parse_sound_center)) %>%
  tidyr::unnest(parsed)

# The environment log does not always record the sound-source coordinates, so fall back to the known
# per-scenario source position (world cm). This lets the horizontal analysis use Vermeij's
# distance-to-source zones (5 zones), including for the no-sound control (distance to the speaker
# location the sound would have come from), rather than a degenerate distance-to-origin measure.
e3_source_lookup <- tibble::tribble(
  ~scenario, ~src_x, ~src_y, ~src_z,
  "V3A", -100, 5,  5,
  "V3B",   85, 5,  5,
  "V3C",   15, 5,  5,
  "V3D", -100, 5,  5,
  "V3E", -100, 5,  5,
  "V3F",   50, 5, -40,
  "V3SPL140", -100, 5, 5,
  "V3SPL153", -100, 5, 5,
  "V3SPL160", -100, 5, 5,
  "V3FLO",    -100, 5, 5,
  "V3FHI",    -100, 5, 5
)
sound_centers <- sound_centers %>%
  left_join(e3_source_lookup, by = "scenario") %>%
  mutate(
    sound_x = dplyr::coalesce(sound_x, src_x),
    sound_y = dplyr::coalesce(sound_y, src_y),
    sound_z = dplyr::coalesce(sound_z, src_z)
  ) %>%
  select(-src_x, -src_y, -src_z)

run_metrics <- validation_results %>%
  mutate(
    total_settlement_rate = total_settlers / agents,
    correct_settlement_rate = correct_settlers / agents,
    boundary_contacts_per_agent = boundary_contacts / agents
  )

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
      .groups = "drop"
    )
}

result_metrics <- c(
  "avg_fitness", "max_fitness", "total_settlers", "correct_settlers",
  "total_settlement_rate", "correct_settlement_rate", "boundary_contacts",
  "boundary_contacts_per_agent", "mean_final_step"
)
validation_summary <- map_dfr(result_metrics, ~ summarise_metric(run_metrics, .x)) %>%
  arrange(metric, scenario)
write.csv(validation_summary, file.path(tables_dir, "validation_summary_e3.csv"), row.names = FALSE)

e3_positions <- validation_positions %>%
  left_join(sound_centers, by = c("experiment_id", "scenario", "run_seed")) %>%
  left_join(
    run_metrics %>% select(experiment_id, scenario, run_seed, world_x, world_y, world_z),
    by = c("experiment_id", "scenario", "run_seed")
  ) %>%
  mutate(
    horizontal_distance_to_sound_cm = sqrt((final_x - sound_x)^2 + (final_y - sound_y)^2),
    three_d_distance_to_sound_cm = sqrt((final_x - sound_x)^2 + (final_y - sound_y)^2 + (final_z - sound_z)^2),
    height_zone = cut(
      final_z,
      breaks = c(-Inf, 2, 4, 6, 8, Inf),
      labels = c("<2 cm", "2-4 cm", "4-6 cm", "6-8 cm", ">8 cm"),
      right = FALSE
    )
  ) %>%
  # Five distance-to-source zones per scenario (equal-width over the observed range), as in Vermeij's
  # horizontal distance-zone analysis. Per-scenario binning handles the different source placements
  # (and the no-sound control, where the source position is the geometric speaker location) rather
  # than assuming a fixed near-end origin.
  group_by(experiment_id, scenario) %>%
  mutate(
    horizontal_zone = cut(
      horizontal_distance_to_sound_cm,
      breaks = seq(min(horizontal_distance_to_sound_cm, na.rm = TRUE),
                   max(horizontal_distance_to_sound_cm, na.rm = TRUE), length.out = 6),
      labels = c("zone1 (nearest)", "zone2", "zone3", "zone4", "zone5 (farthest)"),
      include.lowest = TRUE
    )
  ) %>%
  ungroup()

e3_horizontal_by_run <- e3_positions %>%
  count(experiment_id, scenario, run_seed, horizontal_zone, name = "larvae") %>%
  complete(experiment_id, scenario, run_seed, horizontal_zone, fill = list(larvae = 0L)) %>%
  group_by(experiment_id, scenario, run_seed) %>%
  mutate(total_larvae = sum(larvae), proportion = if_else(total_larvae > 0, larvae / total_larvae, NA_real_)) %>%
  ungroup()
write.csv(e3_horizontal_by_run, file.path(tables_dir, "e3_horizontal_zone_counts_by_run.csv"), row.names = FALSE)

e3_vertical_by_run <- e3_positions %>%
  count(experiment_id, scenario, run_seed, height_zone, name = "larvae") %>%
  complete(experiment_id, scenario, run_seed, height_zone, fill = list(larvae = 0L)) %>%
  group_by(experiment_id, scenario, run_seed) %>%
  mutate(total_larvae = sum(larvae), proportion = if_else(total_larvae > 0, larvae / total_larvae, NA_real_)) %>%
  ungroup()
write.csv(e3_vertical_by_run, file.path(tables_dir, "e3_height_zone_counts_by_run.csv"), row.names = FALSE)

summarise_zones <- function(data, zone_col) {
  data %>%
    group_by(experiment_id, scenario, .data[[zone_col]]) %>%
    summarise(
      runs = n(),
      total_larvae = sum(larvae),
      mean_larvae_per_run = mean(larvae),
      sd_larvae_per_run = sd(larvae),
      mean_proportion = mean(proportion),
      sd_proportion = sd(proportion),
      .groups = "drop"
    ) %>%
    rename(zone = all_of(zone_col))
}

e3_horizontal_by_scenario <- summarise_zones(e3_horizontal_by_run, "horizontal_zone")
e3_vertical_by_scenario <- summarise_zones(e3_vertical_by_run, "height_zone")
write.csv(e3_horizontal_by_scenario, file.path(tables_dir, "e3_horizontal_zone_counts_by_scenario.csv"), row.names = FALSE)
write.csv(e3_vertical_by_scenario, file.path(tables_dir, "e3_height_zone_counts_by_scenario.csv"), row.names = FALSE)

chi_square_per_scenario <- function(count_data, zone_col, test_label) {
  count_data %>%
    group_by(scenario, .data[[zone_col]]) %>%
    summarise(larvae = sum(larvae), .groups = "drop") %>%
    group_by(scenario) %>%
    group_modify(function(dat, key) {
      # Whole-body guard: any failure (degenerate zone vector, zero total, etc.) yields NA rather
      # than halting the pipeline.
      tryCatch({
        counts <- as.numeric(dat$larvae)
        counts <- counts[is.finite(counts)]
        if (length(counts) < 2 || sum(counts) <= 0) stop("degenerate")
        t <- suppressWarnings(stats::chisq.test(counts, p = rep(1 / length(counts), length(counts))))
        tibble(test = test_label, statistic = unname(as.numeric(t$statistic)),
               df = unname(as.numeric(t$parameter)), p_value = as.numeric(t$p.value))
      }, error = function(e)
        tibble(test = test_label, statistic = NA_real_, df = NA_real_, p_value = NA_real_))
    }) %>%
    ungroup()
}

e3_horizontal_chi <- chi_square_per_scenario(
  e3_horizontal_by_run,
  "horizontal_zone",
  "Horizontal distribution differs from uniform across sound-distance zones"
)
write.csv(e3_horizontal_chi, file.path(tables_dir, "e3_horizontal_chisquare_by_scenario.csv"), row.names = FALSE)

anova_per_scenario <- e3_vertical_by_run %>%
  group_by(scenario) %>%
  group_modify(function(dat, key) {
    fit_summary <- tryCatch({
      fit <- stats::aov(larvae ~ height_zone, data = dat)
      summary(fit)[[1]]
    }, error = function(e) NULL)
    if (is.null(fit_summary) || !("height_zone" %in% rownames(fit_summary))) {
      tibble(test = "One-way ANOVA of larval counts across height zones",
             df_between = NA_real_, df_within = NA_real_, statistic = NA_real_, p_value = NA_real_)
    } else {
      tibble(
        test = "One-way ANOVA of larval counts across height zones",
        df_between = fit_summary["height_zone", "Df"],
        df_within = fit_summary["Residuals", "Df"],
        statistic = fit_summary["height_zone", "F value"],
        p_value = fit_summary["height_zone", "Pr(>F)"]
      )
    }
  }) %>%
  ungroup()
write.csv(anova_per_scenario, file.path(tables_dir, "e3_height_zone_anova_by_scenario.csv"), row.names = FALSE)

p_horizontal <- ggplot(e3_horizontal_by_scenario, aes(x = zone, y = scenario, fill = mean_larvae_per_run)) +
  geom_tile(color = "white", linewidth = 0.2) +
  geom_text(aes(label = sprintf("%.1f", mean_larvae_per_run)), size = 3.3) +
  scale_fill_viridis_c(name = "Mean larvae/run") +
  labs(x = "Horizontal distance to sound cluster", y = NULL) +
  theme_minimal(base_size = 11) +
  theme(panel.grid = element_blank())
ggsave(file.path(plots_dir, "e3_validation_horizontal_distance_heatmap.png"), p_horizontal, width = 7.2, height = 4.4, dpi = 300)

p_vertical <- ggplot(e3_vertical_by_scenario, aes(x = scenario, y = zone, fill = mean_larvae_per_run)) +
  geom_tile(color = "white", linewidth = 0.2) +
  geom_text(aes(label = sprintf("%.1f", mean_larvae_per_run)), size = 3.3) +
  scale_fill_viridis_c(name = "Mean larvae/run") +
  labs(x = NULL, y = "Height zone") +
  theme_minimal(base_size = 11) +
  theme(panel.grid = element_blank())
ggsave(file.path(plots_dir, "e3_validation_height_zone_heatmap.png"), p_vertical, width = 6.2, height = 4.4, dpi = 300)

message("Wrote E3 validation summary tables to: ", tables_dir)
message("Wrote E3 validation plots to: ", plots_dir)

