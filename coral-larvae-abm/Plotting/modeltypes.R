library(ggplot2)
library(dplyr)
library(viridis)

# Your data
data <- data.frame(
  Model = c("Lagrangian Particle Tracking Models", "Individual-Based Models", 
            "Genetic Models", "(Math.) Behavioral Models", 
            "Metapopulation Models", "Process-Based Models"),
  Count = c(29, 13, 2, 1, 1, 1)
)

# Add percentage data to the data frame
data <- data %>% 
  mutate(Percentage = Count / sum(Count) * 100)

# Create the ring (donut) chart with a larger hole in the middle
ggplot(data, aes(x = 2, y = Percentage, fill = Model)) +
  geom_bar(stat = "identity", width = 1) +
  coord_polar(theta = "y") +
  xlim(0.1, 2.5) +  # Adjust the x limits to increase the size of the hole
  theme_void() +  # Remove background and axes
  
  scale_fill_viridis_d() +
  
  # Customize the fonts and text (with default font)
  theme(
    text = element_text(family = "serif"),
    plot.title = element_text(size = 24),  # Title with default font
    legend.text = element_text(size = 20),  # Legend text with default font
    legend.title = element_blank(),  # Remove legend title
    legend.position = "right"
  )
