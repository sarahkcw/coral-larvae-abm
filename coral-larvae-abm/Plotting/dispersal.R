library(ggplot2)
library(dplyr)
library(viridis) 

# Your new data
data2 <- data.frame(
  DispersalType = c("Passive Dispersal", "Combination of Passive and Active Dispersal"),
  Percentage = c(64.9, 35.1)
)

# Create the ring (donut) chart with a larger hole in the middle
ggplot(data2, aes(x = 2, y = Percentage, fill = DispersalType)) +
  geom_bar(stat = "identity", width = 1) + 
  theme_minimal() +
  coord_polar(theta = "y") +
  xlim(0.1, 2.5) +  # Adjust the x limits to increase the size of the hole
  theme_void() +  # Remove background and axes
  
  scale_fill_viridis_d() +
  labs(
    fill = ""  # Remove the legend title by setting it as an empty string
  ) +
  
  # Customize the fonts and text (with default font)
  theme(
    text = element_text(family = "serif"),
    plot.title = element_text(size = 20),  # Title with default font
    legend.text = element_text(size = 24),  # Legend text with default font
    legend.title = element_text(size = 24),  # Legend title with default font
    legend.position = "right"
  )


