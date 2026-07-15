library(ggplot2)
library(dplyr)
library(forcats)
library(reshape2)
library(viridis)

# Correct data for each dataset
data_countsV2A <- data.frame(
  Category = c(">2.0m", "1.6-2.0m", "1.2-1.6m", "0.8-1.2m", "0.4-0.8m", "<0.4m"),
  Count = c(5.69, 0.38, 11.66, 4.31, 2.66, 275.28)
)

data_countsV2B <- data.frame(
  Category = c(">2.0m", "1.6-2.0m", "1.2-1.6m", "0.8-1.2m", "0.4-0.8m", "<0.4m"),
  Count = c(4.10, 0.44, 0.77, 1.85, 29.90, 189.92)
)

data_countsV2C <- data.frame(
  Category = c(">2.0m", "1.6-2.0m", "1.2-1.6m", "0.8-1.2m", "0.4-0.8m", "<0.4m"),
  Count = c(5.73, 0.30, 0.10, 1.13, 0.33, 229.97)
)

data_countsV2D <- data.frame(
  Category = c(">2.0m", "1.6-2.0m", "1.2-1.6m", "0.8-1.2m", "0.4-0.8m", "<0.4m"),
  Count = c(129.87, 46.47, 23.37, 50.53, 37.97, 10.77)
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
my_heatmap <- ggplot(data_heatmap, aes(x = Dataset, y = HeightZone, fill = Count)) +
  geom_tile(color = "white") +  # Add a border to tiles
  geom_text(aes(label = sprintf("%.2f", Count)), size = 4, family = "sans", color = "white", fontface = "bold") +  
  scale_fill_viridis(option = "D", direction = 1, name = "Number of Larvae") +
  labs(
    x = "Validation Rounds",
    y = NULL,
    title = ""
  ) +
  theme_minimal() +
  theme(
    text = element_text(size = 12),
    axis.title.x = element_text(size = 12, margin = margin(t = 10)),
    axis.title.y = element_text(size = 12, margin = margin(r = 10)),
    axis.text.x = element_text(size = 12),
    axis.text.y = element_text(size = 12),
    legend.title = element_text(size = 12),
    legend.text = element_text(size = 12)
  )

ggsave("D:/_Sarah/paperbackup/2025/Alife/Figures/exp2-dis.pdf", plot = my_heatmap, width = 8, height = 6, dpi = 300, device = cairo_pdf)

