library(ggplot2)
library(dplyr)
library(viridis)
library(cowplot)

# Combine data for global color scale
combined_data <- bind_rows(
  horizontal_combined %>% mutate(Type = "Horizontal"),
  vertical_combined %>% mutate(Type = "Vertical")
)

# Define a global color scale
shared_scale <- scale_fill_viridis(
  option = "D", direction = 1, name = "Number of Larvae",
  limits = c(min(combined_data$Value), max(combined_data$Value))
)

# Horizontal Heatmap
horizontal_heatmap <- ggplot(horizontal_combined, aes(x = Category, y = Dataset, fill = Value)) +
  geom_tile(color = "white") +
  geom_text(aes(label = sprintf("%.2f", Value)), size = 4, color = "white") +
  shared_scale +
  scale_x_discrete(limits = rev(unique(horizontal_combined$Category))) +
  scale_y_discrete(limits = rev(unique(horizontal_combined$Dataset))) +
  labs(title = "Horizontal Distribution", x = "Distance to Sound Source", y = "Validation Rounds") +
  theme_minimal() +
  theme(
    text = element_text(size = 12),
    axis.text.x = element_text(size = 12, hjust = 1),
    axis.text.y = element_text(size = 12),
    axis.title.x = element_text(size = 12, margin = margin(t = 10)),
    axis.title.y = element_text(size = 12),
    legend.position = "none"
  )

# Vertical Heatmap
vertical_heatmap <- ggplot(vertical_combined, aes(x = Dataset, y = Category, fill = Value)) +
  geom_tile(color = "white") +
  geom_text(aes(label = sprintf("%.2f", Value)), size = 4, color = "white") +
  shared_scale +
  scale_y_discrete(limits = rev(unique(vertical_combined$Category))) +
  labs(title = "Vertical Distribution", x = "Validation Rounds", y = "Height Zone") +
  theme_minimal() +
  theme(
    text = element_text(size = 12),
    axis.text.x = element_text(size = 12, hjust = 1),
    axis.text.y = element_text(size = 12),
    axis.title.x = element_text(size = 12, margin = margin(t = 10)),
    axis.title.y = element_text(size = 12),
    legend.position = "none"
  )

# Extract shared legend
shared_legend <- get_legend(
  ggplot(horizontal_combined, aes(x = Category, y = Dataset, fill = Value)) +
    geom_tile() +
    shared_scale +
    theme_minimal() +
    theme(legend.title = element_text(size = 12), legend.text = element_text(size = 10))
)

# Combine plots with shared legend
combined_plot <- plot_grid(
  vertical_heatmap, horizontal_heatmap, shared_legend,
  ncol = 3, rel_widths = c(1, 1, 0.3)
)

# Save the plot
ggsave("D:/_Sarah/paperbackup/2025/Alife/Figures/exp3-dis.pdf", plot = combined_plot, width = 19, height = 6, dpi = 300, device = cairo_pdf)

