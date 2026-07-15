library(ggplot2)
library(tidyr)
library(cowplot)

#V3A
horizontal_data_V3A <- data.frame(
  Category = factor(c(">80cm", "80-60cm", "60-40cm", "40-20cm", "<20cm"), 
                    levels = c(">80cm", "80-60cm", "60-40cm", "40-20cm", "<20cm")),
  Value = c(58.6, 112.9, 106.93, 110.93, 111.53)
)

vertical_data_V3A <- data.frame(
  Category = factor(c("<200cm", "200-400cm", "400-600cm", "600-800cm", ">800cm"), 
                    levels = c("<200cm", "200-400cm", "400-600cm", "600-800cm", ">800cm")), 
  Value = c(460.83, 19.73, 9.97, 5.6, 3.8)
)

#V3B
horizontal_data_V3B <- data.frame(
  Category = factor(c(">80cm", "80-60cm", "60-40cm", "40-20cm", "<20cm"), 
                    levels = c(">80cm", "80-60cm", "60-40cm", "40-20cm", "<20cm")),
  Value = c(98.73, 154.83, 110.97, 70.47, 65)
)

vertical_data_V3B <- data.frame(
  Category = factor(c("<200cm", "200-400cm", "400-600cm", "600-800cm", ">800cm"), 
                    levels = c("<200cm", "200-400cm", "400-600cm", "600-800cm", ">800cm")), 
  Value = c(396, 73.6, 19, 6.9, 4.5)
)

#V3C
horizontal_data_V3C <- data.frame(
  Category = factor(c(">80cm", "80-60cm", "60-40cm", "40-20cm", "<20cm"), 
                    levels = c(">80cm", "80-60cm", "60-40cm", "40-20cm", "<20cm")),
  Value = c(88.67, 146.8, 109.37, 85.37, 69.9)
)

vertical_data_V3C <- data.frame(
  Category = factor(c("<200cm", "200-400cm", "400-600cm", "600-800cm", ">800cm"), 
                    levels = c("<200cm", "200-400cm", "400-600cm", "600-800cm", ">800cm")), 
  Value = c(20.76, 186.63, 209.33, 71.23, 11.83)
)

#V3D current
horizontal_data_V3D <- data.frame(
  Category = factor(c(">80cm", "80-60cm", "60-40cm", "40-20cm", "<20cm"), 
                    levels = c(">80cm", "80-60cm", "60-40cm", "40-20cm", "<20cm")),
  Value = c(266.8, 32.63, 26.27, 34.7, 139.6)
)

vertical_data_V3D <- data.frame(
  Category = factor(c("<200cm", "200-400cm", "400-600cm", "600-800cm", ">800cm"), 
                    levels = c("<200cm", "200-400cm", "400-600cm", "600-800cm", ">800cm")), 
  Value = c(158.17, 97.07, 93.77, 83.87, 67.13)
)

#V3E no sound
horizontal_data_V3E <- data.frame(
  Category = factor(c(">80cm", "80-60cm", "60-40cm", "40-20cm", "<20cm"), 
                    levels = c(">80cm", "80-60cm", "60-40cm", "40-20cm", "<20cm")),
  Value = c(101.94, 96.7, 97.47, 99.47, 104.43)
)

vertical_data_V3E <- data.frame(
  Category = factor(c("<200cm", "200-400cm", "400-600cm", "600-800cm", ">800cm"), 
                    levels = c("<200cm", "200-400cm", "400-600cm", "600-800cm", ">800cm")), 
  Value = c(86.73, 126.2, 103.43, 95.37, 88.27)
)


# Vertical Bar Chart
vertical_bar <- ggplot(vertical_data_V3A, aes(x = "", y = Value, fill = Category)) +
  geom_bar(stat = "identity") +
  coord_flip() +  # Flip the coordinates to create a horizontal bar chart
  theme_minimal() +
  scale_fill_viridis_d() +
  labs(
    title = "Vertical Distribution",
    x = "",
    y = "",
    fill = ""
  ) +
  theme(
    plot.title = element_text(size = 16),
    axis.text.x = element_text(size = 16),
    axis.text.y = element_text(size = 16),
    legend.position = "right",
    legend.text = element_text(size = 16),
    aspect.ratio = 1/3  
  )

# Horizontal Bar Chart
horizontal_bar <- ggplot(horizontal_data_V3A, aes(x = "", y = Value, fill = Category)) +
  geom_bar(stat = "identity") +
  theme_minimal() +
  scale_fill_viridis_d() +
  labs(
    title = "Horizontal Distribution",
    x = "",
    y = "",
    fill = ""
  ) +
  theme(
    plot.title = element_text(size = 16),
    axis.text.x = element_text(size = 16),
    axis.text.y = element_text(size = 16),
    legend.position = "right",
    legend.text = element_text(size = 16),
    aspect.ratio = 6/1  
  )

# Arrange the vertical and horizontal bar charts side by side
combined_plot <- plot_grid(vertical_bar, horizontal_bar, ncol = 2)

# Display the combined plot
print(combined_plot)
