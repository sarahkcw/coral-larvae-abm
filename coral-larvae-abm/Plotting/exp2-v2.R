library(ggplot2)
library(dplyr)
library(forcats)
library(reshape2)
library(viridis)

# Correct data for each dataset
data_countsV2A <- data.frame(
  Category = c(">2.0m", "1.6-2.0m", "1.2-1.6m", "0.8-1.2m", "0.4-0.8m", "<0.4m"),
  Count = c(5.689655172, 0.3793103448, 11.65517241, 4.310344828, 2.655172414, 275.2758621)
)

data_countsV2B <- data.frame(
  Category = c(">2.0m", "1.6-2.0m", "1.2-1.6m", "0.8-1.2m", "0.4-0.8m", "<0.4m"),
  Count = c(4.102564103, 0.4358974359, 0.7692307692, 1.846153846, 29.8974359, 189.9230769)
)

data_countsV2C <- data.frame(
  Category = c(">2.0m", "1.6-2.0m", "1.2-1.6m", "0.8-1.2m", "0.4-0.8m", "<0.4m"),
  Count = c(5.733333333, 0.3, 0.1, 1.133333333, 0.3333333333, 229.9666667)
)

data_countsV2D <- data.frame(
  Category = c(">2.0m", "1.6-2.0m", "1.2-1.6m", "0.8-1.2m", "0.4-0.8m", "<0.4m"),
  Count = c(129.8666667, 46.46666667, 23.36666667, 50.53333333, 37.96666667, 10.76666667)
)

# Assign dataset labels
data_countsV2A$Dataset <- "V2A"
data_countsV2B$Dataset <- "V2B"
data_countsV2C$Dataset <- "V2C"
data_countsV2D$Dataset <- "V2D"

# Combine the datasets
data_combined <- bind_rows(data_countsV2A, data_countsV2B, data_countsV2C, data_countsV2D)

# Reverse the factor levels so <0.4m is at the top
data_combined <- data_combined %>%
  mutate(Category = factor(Category, levels = c("<0.4m", "0.4-0.8m", "0.8-1.2m", "1.2-1.6m", "1.6-2.0m", ">2.0m")))

# Reshape the data for heatmap
data_heatmap <- data_combined %>%
  rename(HeightZone = Category) %>%
  select(Dataset, HeightZone, Count)

# Create heatmap with values inside blocks
ggplot(data_heatmap, aes(x = Dataset, y = HeightZone, fill = Count)) +
  geom_tile(color = "white") +  # Add a border to tiles
  geom_text(aes(label = sprintf("%.2f", Count)), size = 4, color = "white") +  # Add numbers inside blocks
  scale_fill_viridis(option = "D", direction = 1, name = "Number of Larvae") +  # Corrected to make larger values more yellow
  labs(
    x = "Validation Rounds",
    y = "Height Zone",
    title = ""
  ) +
  theme_minimal() +
  theme(
    text = element_text(size = 16),
    axis.title.x = element_text(size = 16, margin = margin(t = 10)),
    axis.title.y = element_text(size = 16, margin = margin(r = 10)),  # Y-axis label on the left
    axis.text.x = element_text(size = 14, hjust = 1),  # Rotate x-axis labels
    axis.text.y = element_text(size = 14),
    legend.title = element_text(size = 16),
    legend.text = element_text(size = 14)
  )
