library(ggplot2)
library(forcats)

# V2
data_countsV2A <- data.frame(
  Category = c(">2.0m", "1.6-2.0m", "1.2-1.6m", "0.8-1.2m", "0.4-0.8m", "<0.4m"),
  # Count = c(5.69, 0.38, 11.66, 4.31, 2.66, 275.28)
  Count = c(1.9, 0.13, 3.89, 1.44, 0.89, 91.77)
)

data_countsV2B <- data.frame(
  Category = c(">2.0m", "1.6-2.0m", "1.2-1.6m", "0.8-1.2m", "0.4-0.8m", "<0.4m"),
 # Count = c(4.10,	0.44,	0.77,	1.85,	29.9,	189.92)
  Count = c(1.81, 0.19, 0.34, 0.81, 13.17, 83.68)
)

data_countsV2C <- data.frame(
  Category = c(">2.0m", "1.6-2.0m", "1.2-1.6m", "0.8-1.2m", "0.4-0.8m", "<0.4m"),
  #Count = c(5.73, 0.3, 0.1, 1.13, 0.33, 229.97)
  Count = c(2.41, 0.13, 0.04, 0.45, 0.13, 96.8)
)

data_countsV2D <- data.frame(
  Category = c(">2.0m", "1.6-2.0m", "1.2-1.6m", "0.8-1.2m", "0.4-0.8m", "<0.4m"),
  #Count = c(129.87, 46.47, 23.37, 50.53, 37.97, 10.77)
  Count = c(43.44, 15.54, 7.81, 16.9, 12.7, 3.6)
)


data_countsV2A$Dataset <- "V2A"
data_countsV2B$Dataset <- "V2B"
data_countsV2C$Dataset <- "V2C"
data_countsV2D$Dataset <- "V2D"

# Combine the datasets
data_combined <- bind_rows(data_countsV2A, data_countsV2B, data_countsV2C, data_countsV2D)

data_combined <- data_combined %>%
  mutate(Category = factor(Category, levels = c("<0.4m", "0.4-0.8m", "0.8-1.2m", "1.2-1.6m", "1.6-2.0m", ">2.0m")))


ggplot(data_combined, aes(x = Dataset, y = Count, fill = Category)) +
  geom_bar(stat = "identity", position = "stack") +  # Stacked bar chart
  theme_minimal() +  # Minimal theme for a clean look
  scale_fill_viridis_d() +  
  labs(
    x = "",
    y = "Percentage of larave in each height zone",
    fill = ""
  ) +
  theme(
    plot.title = element_text(size = 16),
    axis.text.x = element_text(size = 16),
    axis.text.y = element_text(size = 16),
    axis.title.y = element_text(size = 16),
    legend.position = "right",  # Keep the legend on the right
    legend.text = element_text(size = 16),
    aspect.ratio = 1/1  # Adjust the aspect ratio to your liking
  )
