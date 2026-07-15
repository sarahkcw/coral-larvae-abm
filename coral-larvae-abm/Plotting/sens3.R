# Load necessary libraries
library(ggplot2)
library(reshape2)
library(viridis)

# Create the data for sound sensitivity analysis (full distribution)
data_exp3 <- data.frame(
  Near_Sound = c(2, 5, 5, 3, 1),
  Zone2 = c(6, 5, 10, 3, 9),
  Zone3 = c(11, 22, 11, 10, 19),
  Zone4 = c(40, 43, 31, 22, 49),
  Far_Sound = c(441, 425, 443, 444, 422)
)

# Add a column for the actual sound setups (modify these based on your actual test setup)
data_exp3$Sound_Setup <- c("SPL 140 dB", "SPL 160 dB", "Freq 100 Hz", "Freq 1000 Hz", "Baseline SPL 153 dB")

# Melt the data for heatmap
data_melt_exp3 <- melt(data_exp3, id.vars = "Sound_Setup")

# Create heatmap with viridis palette and the modified axes
ggplot(data_melt_exp3, aes(x = variable, y = Sound_Setup, fill = value)) +
  geom_tile(color = "white") +
  scale_fill_viridis(option = "D", direction = 1, name = "Number of larvae") +
  labs(title = "",
       x = "Distance from Sound Source", y = "") +
  theme_minimal() +
  theme(
    text = element_text(size = 20),
    axis.title = element_text(size = 20),
    axis.text = element_text(size = 20),
    legend.title = element_text(size = 20),
    legend.text = element_text(size = 20),
    axis.title.y = element_text(margin = margin(r = 15), size = 20),
    axis.title.x = element_text(margin = margin(t = 10), size = 20)
  )

