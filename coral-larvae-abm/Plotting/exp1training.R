library(ggplot2)

# Manually specify column names if they are missing or incomplete
dataeli1 <- read.csv("copieddata.CSV", sep = ";", dec = ",")

# Create the line graph for AvgFitness and MaxFitness over Generation
ggplot(dataeli1, aes(x = Generation)) +
  geom_line(aes(y = AvgFitness, color = "Average Fitness"), size = 1) +  # Line for AvgFitness
  geom_line(aes(y = MaxFitness, color = "Maximum Fitness"), size = 1) +  # Line for MaxFitness
  scale_color_viridis_d() +  # Use Viridis colors for the lines
  theme_minimal() +  # Minimal theme for clean look
  labs(
    title = "",
    x = "Generation",
    y = "Fitness",
    color = ""

  ) +
  theme(
    text = element_text(family = "serif"),
    plot.title = element_text(size = 16),
    axis.text.x = element_text(size = 20),
    axis.text.y = element_text(size = 20),
    axis.title.x = element_text(size = 20, margin = margin(t = 10, b = 10)),
    axis.title.y = element_text(size = 20, margin = margin(t = 15, b = 15)),
    legend.text = element_text(size = 24)
  )

