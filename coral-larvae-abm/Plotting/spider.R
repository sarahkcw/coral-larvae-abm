# Load required libraries
library(fmsb)
library(viridis)

# Your data
data_spider <- data.frame(
  Biological_Realism = c(3.5, 4.5, 2.5),
  Model_Complexity = c(3.5, 4.5, 3),
  Computational_Demand = c(3.5, 4.5, 4.5),
  Adaptiveness = c(3, 5, 2),
  Predictive_Power = c(3, 4, 3.5),
  Data_Requirements = c(3.5, 4, 5),
  Spatial_Temporal_Res = c(3.5, 3.5, 4.5)
)

# Add min/max to the data frame
data_spider <- rbind(
  max = c(5, 5, 5, 5, 5, 5, 5),  # Maximum values for each category
  min = c(0, 0, 0, 0, 0, 0, 0),  # Minimum values for each category
  data_spider
)

# Create the radar chart (spider plot) with viridis colors
radarchart(
  data_spider,
  axistype = 1,                      # Type of axis
  pcol = viridis(3),                 # Line colors from viridis palette
  pfcol = scales::alpha(viridis(3), 0.05),  # Fill colors with transparency
  plwd = 2,                          # Line width
  cglcol = "grey",                   # Color for grid lines
  cglty = 1,                         # Line type for grid
  axislabcol = "black",              # Axis label color
  caxislabels = seq(0, 5, 1),        # Axis tick labels from 0 to 5
  vlcex = 0.8                        # Text size for variable names
)

# Add a legend with viridis colors
legend(
  x = "topright", 
  legend = c("Thesis Approach with NE", "IBM", "LPT"), 
  col = viridis(3), 
  lty = 1, 
  lwd = 2
)
