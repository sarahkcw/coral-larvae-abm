#pragma once
#include "CoreMinimal.h"
#include "Agent/LarvaAgent.h"
#include "GameFramework/Actor.h"
#include "EvolutionManager.generated.h"

UENUM(BlueprintType)
enum class ESelectionStrategy : uint8 {
	TOURNAMENT  UMETA(DisplayName = "Tournament"),
	ROULETTE    UMETA(DisplayName = "Roulette"),
	ELITISM     UMETA(DisplayName = "Elitism"),
	RANKBASED   UMETA(DisplayName = "Rank-Based"),
	SUS         UMETA(DisplayName = "Stochastic Universal Sampling"),
	TRUNCATION  UMETA(DisplayName = "Truncation")
};

UENUM(BlueprintType)
enum class ECrossoverStrategy : uint8 {
	SINGLEPOINT  UMETA(DisplayName = "Single Point"),
	MULTIPOINT    UMETA(DisplayName = "Multi Point"),
	OVERLAY     UMETA(DisplayName = "Overlay")
};

UENUM()
enum class EMutationStrategy : uint8 {
	POINTMUTATION UMETA(DisplayName = "Point"),
	MULTIPOINTMUTATION UMETA(DisplayName = "Multi Point")
};

USTRUCT(BlueprintType)
struct FGenomeFitnessPair
{
	GENERATED_BODY()

public:
	UPROPERTY()
	float FitnessScore;

	UPROPERTY()
	FGenome Genome;

	FGenomeFitnessPair() : FitnessScore(0.0f) {}
	FGenomeFitnessPair(const float InFitnessScore, FGenome InGenome) : FitnessScore(InFitnessScore), Genome(InGenome) {}
};

// Manages the evolution of the population of agents
UCLASS()
class CORAL_LARVAE_ABM_API  UEvolutionManager : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
	
	// Select the agents that will reproduce
	static TArray<FGenome> Selection(TArray<FGenomeFitnessPair>& GenomeFitnessPairs, ESelectionStrategy SelectionStrategy, float SelectionRate, float RestSelectionRate);
	// Crossover the selected agents to create new offspring
	static FGenome GenerateChildGenome(const TArray<FGenome>& ParentGenomes, double MutationRate, ECrossoverStrategy CrossoverStrategy, EMutationStrategy MutationStrategy);
	static FGenome GenerateNextGenGenome(const TArray<FGenome>& ParentGenomes, double MutationRate, ECrossoverStrategy CrossoverStrategy, EMutationStrategy MutationStrategy, int GenomeLength, const FString GenomeOverride = "");
	
	// Debugging
	static void CheckPopulationDiversity(const TArray<FGenome>& Population);
	
private:
	// Selects the best genome from a randomly chosen subset of the population //  20-30% tournament size
	static TArray<FGenome> TournamentSelection(TArray<FGenomeFitnessPair>& GenomeFitnessPairs, int TournamentSize = 5);
	//  Selects genomes with a probability proportional to their fitness
	static TArray<FGenome> RouletteSelection(TArray<FGenomeFitnessPair>& GenomeFitnessPairs);
	// Always selects the top-performing genomes from the population // 5%-10% of the population
	static TArray<FGenome> ElitismSelection(TArray<FGenomeFitnessPair>& GenomeFitnessPairs, float ElitismRate = 0.05f, float RestRate = 0.5f); 
	// Selects genomes based on their rank, with higher-ranked genomes having a higher chance of selection
	static TArray<FGenome> RankBasedSelection(TArray<FGenomeFitnessPair>& GenomeFitnessPairs);
	//  Distributes selection points evenly across the fitness distribution to ensure a more balanced selection
	static TArray<FGenome> SUSSelection(TArray<FGenomeFitnessPair>& GenomeFitnessPairs);
	// Selects the top portion of the population, discarding the rest // 50%-60%
	static TArray<FGenome> TruncationSelection(TArray<FGenomeFitnessPair>& GenomeFitnessPairs, float TruncationRate = 0.5f);

	// Crossover strategies
	// Combines parent genes up to a random crossover point // 80%-90%
	static FGenome SinglePointCrossover(const FGenome& Parent1, const FGenome& Parent2);
	// Alternates between parents at multiple crossover points // 80%-90%
	static FGenome MultiplePointCrossover(const FGenome& Parent1, const FGenome& Parent2);
	// Combines genes from the first and second parents at a midpoint // 70%-80%
	static FGenome OverlayCrossover(const FGenome& Parent1, const FGenome& Parent2);
	
	// Mutation strategies
	static void ApplyMutation(FGenome& Genome, float MutationRate, EMutationStrategy MutationStrategy = EMutationStrategy::POINTMUTATION);
	static FGene MutateGene(const FGene& Gene);
	// Mutates a single gene in the genome // 1%-5%
	static void PointMutation(FGenome& Genome, const float MutationRate);
	// Mutates multiple genes in the genome based on a probability // 5%-10%
	static void MultiPointMutation(FGenome& Genome, const float MutationRate);

};
