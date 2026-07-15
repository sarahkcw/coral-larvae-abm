# Load necessary libraries
library(ggplot2)
library(reshape2)
library(viridis)

# Create the data for light attenuation sensitivity analysis
data_exp2 <- data.frame(
  Zone1 = c(6, 7, 6, 5, 7),
  Zone2 = c(0, 0, 0, 0, 0),
  Zone3 = c(5, 4, 2, 14, 19),
  Zone4 = c(0, 5, 26, 15, 13),
  Zone5 = c(1, 79, 266, 266, 261),
  Zone6 = c(288, 205, 0, 0, 0)
)

# Add a column for the light attenuation coefficients
data_exp2$AttenuationCoefficient <- c(0.0085, 0.017, 0.034, 0.05, 0.085)

# Melt the data for heatmap
data_melt_exp2 <- melt(data_exp2, id.vars = "AttenuationCoefficient")

# Create heatmap with viridis palette and customize text and legend
ggplot(data_melt_exp2, aes(x = variable, y = AttenuationCoefficient, fill = value)) +
  geom_tile(color = "white") +
  scale_fill_viridis(option = "D", direction = 1, name = "Number of Larvae") +
  labs(title = "",
       x = "", y = "Light Attenuation Coefficient") +
  theme_minimal() +
  theme(
    text = element_text(size = 20),  # Set text size to 20
    axis.title = element_text(size = 20, margin = margin(r = 40)),  # Axis titles
    axis.text = element_text(size = 18),   # Axis labels
    legend.title = element_text(size = 20),  # Legend title
    legend.text = element_text(size = 18),   # Legend text
    axis.title.y = element_text(margin = margin(r = 15), size = 20)
  )

