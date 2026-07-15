library(ggplot2)
library(viridis)

# Your data
data_time <- data.frame(
  Year = c(1950, 1955, 1960, 1965, 1970, 1975, 1980, 1985, 1990, 1995, 2000, 2005, 2010, 2015, 2020, 2024),
  Value = c(226, 374, 451, 563, 786, 1490, 2150, 3170, 4170, 5410, 8090, 13700, 16000, 16200, 16500, 17300)
)

# Create a line plot
ggplot(data_time, aes(x = Year, y = Value)) +
  geom_line(color = viridis(1, option = "viridis"), size = 1) + 
  geom_point(size = 2, color = viridis(1, option = "plasma")) +  
  theme_minimal() +
  labs(
    x = "Year",
    y = "Number of Search Results in Google Scholar"
  ) +
  theme(
    text = element_text(family = "serif"),
    plot.title = element_text(size = 24),
    axis.text.x = element_text(size = 20),
    axis.text.y = element_text(size = 20),
    axis.title.y = element_text(size = 20, margin = margin(r = 15)),
    axis.title.x = element_text(size = 24)  
  )
