library(ggplot2)
library(viridis)  # Load the viridis package for the color palette

# Load your data
dataeli1 <- read.csv("copieddata.CSV", sep = ";", dec = ",")

# Create the line graph for AvgFitness over Generation
ggplot(dataeli1, aes(x = Generation, y = AvgFitness)) +
  geom_line(size = 1, color = viridis(1, option = "A", begin = 0.1, end = 0.7)[1]) +  # Use Viridis color
  labs(
    title = "",  # Add title if desired
    x = "Generation",
    y = "Average Fitness"  # Label for y-axis
  ) +
  theme(
    # Set a consistent font family for all text
    text = element_text(family = "serif", size = 12),
    plot.title = element_text(size = 16, hjust = 0.5),  # Center title
    axis.title.x = element_text(size = 14, margin = margin(t = 15)),
    axis.title.y = element_text(size = 14, margin = margin(r = 15)),
    axis.text.x = element_text(size = 12),
    axis.text.y = element_text(size = 12),
    panel.background = element_blank(),  # Remove background
    panel.grid.major = element_line(color = "grey80"),  # Adjust gridlines
    panel.grid.minor = element_line(color = "grey90")
  )
