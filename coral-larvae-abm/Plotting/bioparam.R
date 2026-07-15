library(ggplot2)
library(dplyr)
library(viridis)
library(treemapify)

# Your data in a data frame
data3 <- data.frame(
  MainCategory = c(rep("Coral Species and Biology", 16), 
                   rep("Reproductive Timing and Larval Release", 2), 
                   rep("Larval Development and Competency", 4),
                   rep("Larval Behavior", 4),
                   rep("Settlement and Habitat Parameters", 5),
                   rep("Environmental and Regional Parameters", 4)),
  SubCategory = c("Bleaching susceptibility", "Growth rates", "Sexual system", 
                  "Competition with algae and other corals", "Recruitment rates",
                  "Age at maturity", "Colony maximum diameter", "Corallite area (polyp size)", 
                  "Egg diameter", "Polyp fecundity", "Growth form", "Mode of larval development", 
                  "Mortality rates", "Fertilization success", "Post-settlement survivorship", 
                  "Planula volume and weight growth over time", 
                  "Spawning events", "Larval release density", 
                  "Larval buoyancy", "Competency period (gain and loss)", 
                  "Pelagic larval duration (PLD)", "Survival rates",
                  "Larval swimming behavior", "Step-down photophobic response", 
                  "Wavelength sensitivity", "Larval perception distance", 
                  "Substrate type", "Current velocity at settlement", 
                  "Detection of reef presence through cues", "Genetic data used to infer successful larval settlement", 
                  "Settlement assumed based on particle proximity to reef sites",
                  "Local and regional larval supply", 
                  "Environmental disturbances", "Connectivity", "Temperature stress"),
  Count = c(2, 2, 3, 2, 1, 1, 1, 1, 1, 1, 1, 1, 5, 1, 2, 1, 
            11, 1, 2, 11, 11, 4, 4, 1, 1, 5, 1, 1, 1, 9, 
            2, 2, 2, 4, 1)
)

data3 <- data3 %>%
  arrange(desc(Count))

# Create a grouped bar chart, ordered by Count
ggplot(data3, aes(x = Count, y = reorder(SubCategory, Count), fill = MainCategory)) +
  geom_bar(stat = "identity", position = "dodge") +
  theme_minimal() +
  scale_fill_viridis_d() +  # Use viridis color palette
  labs(
    x = "Number of mentions of each subcategory",
    y = "",  # Leave the y-axis label blank to remove "Subcategory" title
    fill = ""  # Remove the legend title by setting it as an empty string
  ) +
  theme(
    text = element_text(family = "serif"),
    axis.text.y = element_text(size = 16, margin = margin(r = 20)),  # Adjust font size for subcategory labels
    axis.text.x = element_text(size = 20, margin = margin(r = 20)),  # Adjust font size for count axis
    plot.title = element_text(size = 20, margin = margin(r = 20)),
    axis.title.x = element_text(size = 20, margin = margin(r = 20)),  
    legend.text = element_text(size = 20)  # Adjust the size of legend text
  )
