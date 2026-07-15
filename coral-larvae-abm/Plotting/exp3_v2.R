library(ggplot2)
library(dplyr)
library(viridis)

# Horizontal Distribution (formerly Vertical)
horizontal_combined <- data.frame(
  Category = rep(c("<200cm", "200-400cm", "400-600cm", "600-800cm", ">800cm"), 5),
  Dataset = rep(c("V3A", "V3B", "V3C", "V3D", "V3E"), each = 5),
  Value = c(
    460.83, 19.73, 9.97, 5.67, 9.96,  # V3A
    396, 73.6, 19, 6.9, 4.5,           # V3B
    20.77, 186.83, 209.33, 71.23, 11.83, # V3C
    158.17, 97.07, 93.77, 83.87, 67.13, # V3D
    101.93, 96.7, 97.47, 99.47, 104.43  # V3E
  )
)

# Vertical Distribution (formerly Horizontal)
vertical_combined <- data.frame(
  Category = rep(c(">80cm", "80-60cm", "60-40cm", "40-20cm", "<20cm"), 5),
  Dataset = rep(c("V3A", "V3B", "V3C", "V3D", "V3E"), each = 5),
  Value = c(
    58.6, 112.9, 106.93, 110.03, 111.53, # V3A
    98.73, 154.83, 110.97, 70.47, 65,    # V3B
    88.67, 146.8, 109.37, 85.27, 69.9,   # V3C
    266.8, 32.63, 26.27, 34.7, 139.6,    # V3D
    86.73, 126.2, 103.43, 95.37, 88.27   # V3E
  )
)

# Calculate Totals for Each Dataset
horizontal_totals <- horizontal_combined %>%
  group_by(Dataset) %>%
  summarise(Total = sum(Value))

vertical_totals <- vertical_combined %>%
  group_by(Dataset) %>%
  summarise(Total = sum(Value))

# Heatmap for Horizontal Distribution (formerly Vertical)
horizontal_heatmap <- ggplot(horizontal_combined, aes(x = Category, y = Dataset, fill = Value)) +
  geom_tile(color = "white") +  # Add a border to tiles
  geom_text(aes(label = sprintf("%.2f", Value)), size = 4, color = "black") +  # Add data labels
  scale_fill_viridis(option = "D", direction = 1, name = "Larvae Count") +
  scale_x_discrete(limits = rev(unique(horizontal_combined$Category))) +  # Reverse x-axis order
  scale_y_discrete(limits = rev(unique(horizontal_combined$Dataset))) +  # Reverse y-axis order
  labs(
    title = "Horizontal Distribution",
    x = "Distance to Sound Source",
    y = "Validation Round"
  ) +
  theme_minimal() +
  theme(
    text = element_text(size = 16),
    plot.title = element_text(size = 18, face = "bold"),
    axis.title.x = element_text(size = 16),
    axis.title.y = element_text(size = 16),
    axis.text.x = element_text(size = 14, angle = 45, hjust = 1),
    axis.text.y = element_text(size = 14),
    legend.title = element_text(size = 16),
    legend.text = element_text(size = 14)
  )

# Heatmap for Vertical Distribution (formerly Horizontal)
vertical_heatmap <- ggplot(vertical_combined, aes(x = Dataset, y = Category, fill = Value)) +
  geom_tile(color = "white") +  # Add a border to tiles
  geom_text(aes(label = sprintf("%.2f", Value)),  size = 4, family = "sans", color = "white", fontface = "bold") +  # Add data labels
  scale_fill_viridis(option = "D", direction = 1, name = "Larvae Count") +
  scale_y_discrete(limits = rev(unique(vertical_combined$Category))) +  # Reverse y-axis order
  labs(
    title = "Vertical Distribution",
    x = "Validation Rounds",
    y = NULL
  ) +
  theme_minimal() +
  theme(
    text = element_text(size = 12),
    plot.title = element_text(size = 12, face = "bold"),
    axis.title.x = element_text(size = 16, margin = margin(t = 10)),
    axis.title.y = element_text(size = 12),
    axis.text.x = element_text(size = 12, hjust = 1),
    axis.text.y = element_text(size = 12),
    legend.title = element_text(size = 12),
    legend.text = element_text(size = 12)
  )

# Print Totals for Clarity
print("Horizontal Totals (Sum of Rows):")
print(horizontal_totals)

print("Vertical Totals (Sum of Rows):")
print(vertical_totals)

# Display the heatmaps
print(horizontal_heatmap)
 print(vertical_heatmap)
 
 
#ggsave("D:/_Sarah/paperbackup/2025/Alife/Figures/exp3-vert.pdf", plot = vertical_heatmap, width = 8, height = 6, dpi = 300, device = cairo_pdf)
