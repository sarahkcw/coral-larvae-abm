library(ggplot2)
library(dplyr)
library(tidyr)

# Your data in a data frame
data_comparison <- data.frame(
  Dimension = c("3D", "2D", "1D"),
  Percentage = c(71.4, 28.6, 0),
  Count = c(77.9, 20.8, 2.6)
)

# Reshape data into long format to make it easier to plot side-by-side
data_long <- data_comparison %>%
  pivot_longer(cols = c(Percentage, Count), names_to = "Dataset", values_to = "Value")

custom_labels <- c(Percentage = "Coral Species", Count = "All Marine Species")

# Create donut charts
ggplot(data_long, aes(x = 2, y = Value, fill = Dimension)) +
  geom_bar(stat = "identity", width = 1) +
  coord_polar(theta = "y") +  # Polar coordinates for donut chart
  xlim(0.1, 2.5) +  # Create a hole in the middle
  facet_wrap(~ Dataset, scales = "free", labeller = labeller(Dataset = custom_labels)) +  # Separate plots for each dataset
  theme_void() +  # Remove background and axes
  scale_fill_viridis_d() +
  labs(
    fill = ""
  ) +
  theme(
    text = element_text(family = "serif"),
    plot.title = element_text(size = 24),
    strip.text = element_text(size = 24, margin = margin(t = 10, b = 10)),  # Customize facet label text size
    legend.text =element_text(size = 24),
    legend.position = "right"  # Show legend on the right side
  )

