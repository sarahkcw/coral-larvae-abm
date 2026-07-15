library(ggplot2)
library(viridis)

# Create a dataframe for the CCA sensitivity analysis results
data <- data.frame(
  CCA_Concentration = c("100%", "75%", "50%", "25%", "0%"),
  Settlement_Success = c(96, 96, 72, 52, 1)
)

data$CCA_Concentration <- factor(data$CCA_Concentration, 
                                 levels = c("0%", "25%", "50%", "75%", "100%"), 
                                 ordered = TRUE)

# Plot the line chart
ggplot(data, aes(x = CCA_Concentration, y = Settlement_Success, group = 1)) +
  geom_line(color = viridis(1, option = "D")) +
  geom_point(size = 3, color = viridis(1, option = "D")) +
  labs(title = "",
       x = "CCA Concentration",
       y = "Settlement Success") +
  theme_minimal() +
  theme(
    text = element_text(family = "serif"),
    axis.title.y = element_text(size = 20, margin = margin(r = 20)), 
    axis.title.x = element_text(size = 20, margin = margin(r = 20)),  
    axis.text.y = element_text(size = 20, margin = margin(r = 20)), 
    axis.text.x = element_text(size = 20, margin = margin(r = 20)),  
  )