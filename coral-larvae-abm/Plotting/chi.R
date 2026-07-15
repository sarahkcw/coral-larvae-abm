library(ggplot2)

# Example data (replace with your actual data)
observed_counts <- c(13825, 592, 299, 170, 114)  # Summed observed counts from all runs
total_observed <- sum(observed_counts)
expected_counts <- rep(total_observed / 5, 5)  # Assuming equal distribution

# Create a data frame for plotting
data <- data.frame(
  Chamber = factor(c('Chamber 1', 'Chamber 2', 'Chamber 3', 'Chamber 4', 'Chamber 5'), 
                   levels = c('Chamber 1', 'Chamber 2', 'Chamber 3', 'Chamber 4', 'Chamber 5')),
  Count = c(observed_counts, expected_counts),
  Type = rep(c('Observed', 'Expected'), each = 5)
)

# Create the plot
ggplot(data, aes(x = Chamber, y = Count, fill = Type)) +
  geom_bar(stat = "identity", position = "dodge") +
  labs(title = "Observed vs Expected Counts (Chi-Square Test)", 
       x = "Chambers", y = "Counts") +
  theme_minimal() +
  scale_fill_viridis_d()
