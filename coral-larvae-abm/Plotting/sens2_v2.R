library(ggplot2)
library(reshape2)
library(viridis)
library(forcats)

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

# Map zones to the new height categories
zone_labels <- c(
  "Zone1" = ">2.0m",
  "Zone2" = "1.6-2.0m",
  "Zone3" = "1.2-1.6m",
  "Zone4" = "0.8-1.2m",
  "Zone5" = "0.4-0.8m",
  "Zone6" = "<0.4m"
)

# Apply the new labels
data_melt_exp2$variable <- factor(data_melt_exp2$variable, 
                                  levels = names(zone_labels),  # Keep original order
                                  labels = zone_labels)         # Replace with new labels

# Reverse the order for the heatmap (so "<0.4m" is at the bottom)
data_melt_exp2$variable <- fct_rev(data_melt_exp2$variable)

# Create heatmap with numbers inside blocks
ggplot(data_melt_exp2, aes(x = AttenuationCoefficient, y = variable, fill = value)) +
  geom_tile(color = "white") +
  geom_text(aes(label = value), size = 5, color = "white") +  # Add numbers inside blocks
  scale_fill_viridis(option = "D", direction = 1, name = "Number of Larvae") +
  labs(title = "",
       x = "Light Attenuation Coefficient", 
       y = "Height Zone") +  # Updated axis labels
  theme_minimal() +
  theme(
    text = element_text(size = 20),  # Set text size to 20
    axis.title = element_text(size = 20, margin = margin(r = 40)),  # Axis titles
    axis.text = element_text(size = 18),   # Axis labels
    legend.title = element_text(size = 20),  # Legend title
    legend.text = element_text(size = 18),   # Legend text
    axis.title.x = element_text(margin = margin(t = 15), size = 20),
    axis.title.y = element_text(margin = margin(r = 15), size = 20)
  )
