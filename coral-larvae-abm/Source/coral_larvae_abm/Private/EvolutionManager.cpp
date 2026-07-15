#include "EvolutionManager.h"

#include <cassert>

#include "Agent/LarvaAgent.h"

namespace
{
	TArray<double> BuildNonNegativeFitnessWeights(const TArray<FGenomeFitnessPair>& GenomeFitnessPairs)
	{
		TArray<double> Weights;
		if (GenomeFitnessPairs.IsEmpty())
		{
			return Weights;
		}

		double MinFitness = GenomeFitnessPairs[0].FitnessScore;
		for (const auto& GenomeFitnessPair : GenomeFitnessPairs)
		{
			MinFitness = FMath::Min(MinFitness, static_cast<double>(GenomeFitnessPair.FitnessScore));
		}

		const double Offset = MinFitness < 0.0 ? -MinFitness : 0.0;
		for (const auto& GenomeFitnessPair : GenomeFitnessPairs)
		{
			Weights.Add(static_cast<double>(GenomeFitnessPair.FitnessScore) + Offset + 1e-6);
		}

		return Weights;
	}
}

TArray<FGenome> UEvolutionManager::Selection(TArray<FGenomeFitnessPair>& GenomeFitnessPairs, ESelectionStrategy SelectionStrategy, float SelectionRate, float RestSelectionRate)
{
	switch (SelectionStrategy)
	{
		case ESelectionStrategy::TOURNAMENT:
			return TournamentSelection(GenomeFitnessPairs, 5);
		case ESelectionStrategy::ROULETTE:
			return RouletteSelection(GenomeFitnessPairs);
		case ESelectionStrategy::ELITISM:
			return ElitismSelection(GenomeFitnessPairs,SelectionRate, RestSelectionRate);
		case ESelectionStrategy::RANKBASED:
			return RankBasedSelection(GenomeFitnessPairs);
		case ESelectionStrategy::SUS:
			return SUSSelection(GenomeFitnessPairs);
		case ESelectionStrategy::TRUNCATION:
			return TruncationSelection(GenomeFitnessPairs, SelectionRate);
		default:
			return TruncationSelection(GenomeFitnessPairs, SelectionRate);
	}
}

FGenome UEvolutionManager::GenerateChildGenome(const TArray<FGenome>& ParentGenomes, double MutationRate, ECrossoverStrategy CrossoverStrategy, EMutationStrategy MutationStrategy)
{
	int Parent1Idx;
	int Parent2Idx;
	
	// Choose two parents randomly from the candidates. 
	if (ParentGenomes.Num() > 1)
	{
		Parent1Idx = FMath::RandRange(1, ParentGenomes.Num() - 1);
		Parent2Idx = FMath::RandRange(0, Parent1Idx - 1);
	}
	else
	{
		Parent1Idx = FMath::RandRange(0, ParentGenomes.Num() - 1);
		Parent2Idx = FMath::RandRange(0, ParentGenomes.Num() - 1);
	}
	
	const FGenome& ParentGenome1 = ParentGenomes[Parent1Idx];
	const FGenome& ParentGenome2 = ParentGenomes[Parent2Idx];
	
	if (ParentGenome1.Genes.IsEmpty() || ParentGenome2.Genes.IsEmpty())
	{
		UE_LOG(LogTemp, Log, TEXT("invalid genome"));
		assert(false);
	}
	
	FGenome ChildGenome;
	switch (CrossoverStrategy)
	{
		case ECrossoverStrategy::OVERLAY:
			ChildGenome = OverlayCrossover(ParentGenome1, ParentGenome2);
			break;
		case ECrossoverStrategy::SINGLEPOINT:
			ChildGenome = SinglePointCrossover(ParentGenome1, ParentGenome2);
			break;
		case ECrossoverStrategy::MULTIPOINT:
			ChildGenome = MultiplePointCrossover(ParentGenome1, ParentGenome2);
			break;
		default: 
			assert(false);
	}
	
	ApplyMutation(ChildGenome, MutationRate, MutationStrategy);
	
	assert(!Genome.empty());
	
	return ChildGenome;
}

FGenome UEvolutionManager::GenerateNextGenGenome(const TArray<FGenome>& ParentGenomes, double MutationRate, ECrossoverStrategy CrossoverStrategy, EMutationStrategy MutationStrategy, int GenomeLength, const FString GenomeOverride)
{
	if (!GenomeOverride.IsEmpty()) return UGenomeFunctions::GetGenomeFromString(GenomeOverride);
	if (ParentGenomes.IsEmpty()) return UGenomeFunctions::MakeRandomGenome(GenomeLength);
	return GenerateChildGenome(ParentGenomes, MutationRate, CrossoverStrategy, MutationStrategy);
}

TArray<FGenome> UEvolutionManager::TournamentSelection(TArray<FGenomeFitnessPair>& GenomeFitnessPairs, int TournamentSize)
{
	TArray<FGenome> BestGenomes;
	// Select the best agent from a random subset of the population
	TArray<FGenomeFitnessPair> Tournament;
	for (int i = 0; i < TournamentSize; i++)
	{
		const auto RandomIndex = FMath::RandRange(0, GenomeFitnessPairs.Num() - 1);
		Tournament.Add(GenomeFitnessPairs[RandomIndex]);
	}
			
	// Find the best genome in the tournament
	auto BestGenome = Tournament[0];
	for (int i = 0; i < Tournament.Num(); i++)
	{
		if (Tournament[i].FitnessScore > BestGenome.FitnessScore)
		{
			BestGenome = Tournament[i];
		}
	}
	BestGenomes.Add(BestGenome.Genome);
	return BestGenomes;
}

TArray<FGenome> UEvolutionManager::RouletteSelection(TArray<FGenomeFitnessPair>& GenomeFitnessPairs)
{
	TArray<FGenome> SelectedGenomes;
	if (GenomeFitnessPairs.IsEmpty())
	{
		return SelectedGenomes;
	}

	const TArray<double> Weights = BuildNonNegativeFitnessWeights(GenomeFitnessPairs);
	double TotalFitness = 0.0;

	for (const double Weight : Weights)
	{
		TotalFitness += Weight;
	}
	
	for (int i = 0; i < GenomeFitnessPairs.Num(); i++)
	{
		const double RandomValue = FMath::RandRange(0.0, TotalFitness);
		double RunningSum = 0.0;

		for (int32 GenomeIndex = 0; GenomeIndex < GenomeFitnessPairs.Num(); GenomeIndex++)
		{
			RunningSum += Weights[GenomeIndex];
			if (RunningSum >= RandomValue)
			{
				SelectedGenomes.Add(GenomeFitnessPairs[GenomeIndex].Genome);
				break;
			}
		}
	}

	return SelectedGenomes;
}

TArray<FGenome> UEvolutionManager::ElitismSelection(TArray<FGenomeFitnessPair>& GenomeFitnessPairs, float ElitismRate, float RestRate)
{
	TArray<FGenome> SelectedGenomes;

	GenomeFitnessPairs.Sort([](const FGenomeFitnessPair& A, const FGenomeFitnessPair& B) {
		return A.FitnessScore > B.FitnessScore;
	});

	const int EliteCount = FMath::CeilToInt(GenomeFitnessPairs.Num() * ElitismRate); // Select top X to be saved%
	for (int i = 0; i < EliteCount; i++)
	{
		SelectedGenomes.Add(GenomeFitnessPairs[i].Genome);
	}
	
	// Select the rest using truncation selection
	SelectedGenomes.Append(TruncationSelection(GenomeFitnessPairs, RestRate)); 
	
	return SelectedGenomes;
}

TArray<FGenome> UEvolutionManager::RankBasedSelection(TArray<FGenomeFitnessPair>& GenomeFitnessPairs)
{
	TArray<FGenome> SelectedGenomes;
	GenomeFitnessPairs.Sort([](const FGenomeFitnessPair& A, const FGenomeFitnessPair& B) {
		return A.FitnessScore > B.FitnessScore;
	});

	TArray<int> Ranks;
	int TotalRank = 0;
	for (int i = 0; i < GenomeFitnessPairs.Num(); i++)
	{
		Ranks.Add(i + 1);
		TotalRank += (i + 1);
	}

	for (int i = 0; i < GenomeFitnessPairs.Num(); i++)
	{
		const double RandomValue = FMath::RandRange(0, TotalRank);
		double RunningSum = 0;

		for (int j = 0; j < GenomeFitnessPairs.Num(); j++)
		{
			RunningSum += Ranks[j];
			if (RunningSum >= RandomValue)
			{
				SelectedGenomes.Add(GenomeFitnessPairs[j].Genome);
				break;
			}
		}
	}

	return SelectedGenomes;
}

TArray<FGenome> UEvolutionManager::SUSSelection(TArray<FGenomeFitnessPair>& GenomeFitnessPairs)
{
	TArray<FGenome> SelectedGenomes;
	if (GenomeFitnessPairs.IsEmpty())
	{
		return SelectedGenomes;
	}

	const TArray<double> Weights = BuildNonNegativeFitnessWeights(GenomeFitnessPairs);
	double TotalFitness = 0.0;

	for (const double Weight : Weights)
	{
		TotalFitness += Weight;
	}

	const double PointDistance = TotalFitness / GenomeFitnessPairs.Num();
	const double StartPoint = FMath::RandRange(0.0, PointDistance);

	TArray<double> Points;
	for (int i = 0; i < GenomeFitnessPairs.Num(); i++)
	{
		Points.Add(StartPoint + i * PointDistance);
	}

	int PointIndex = 0;
	double RunningSum = 0.0;

	for (int32 GenomeIndex = 0; GenomeIndex < GenomeFitnessPairs.Num(); GenomeIndex++)
	{
		RunningSum += Weights[GenomeIndex];
		while (PointIndex < Points.Num() && RunningSum >= Points[PointIndex])
		{
			SelectedGenomes.Add(GenomeFitnessPairs[GenomeIndex].Genome);
			PointIndex++;
		}
	}

	return SelectedGenomes;
}

TArray<FGenome> UEvolutionManager::TruncationSelection(TArray<FGenomeFitnessPair>& GenomeFitnessPairs, float TruncationRate)
{
	TArray<FGenome> SelectedGenomes;
	GenomeFitnessPairs.Sort([](const FGenomeFitnessPair& A, const FGenomeFitnessPair& B) {
		return A.FitnessScore > B.FitnessScore;
	});

	const int TruncationSize = FMath::CeilToInt(GenomeFitnessPairs.Num() * TruncationRate); // Keep top TruncationRate%
	for (int i = 0; i < TruncationSize; i++)
	{
		SelectedGenomes.Add(GenomeFitnessPairs[i].Genome);
	}
	return SelectedGenomes;
}

FGenome UEvolutionManager::SinglePointCrossover(const FGenome& Parent1, const FGenome& Parent2)
{
	FGenome Child;
	const int CrossoverPoint = FMath::RandRange(0, Parent1.Genes.Num() - 1);

	for (int i = 0; i < Parent1.Genes.Num(); i++)
	{
		if (i < CrossoverPoint)
			Child.Genes.Add(Parent1.Genes[i]);
		else
			Child.Genes.Add(Parent2.Genes[i]);
	}

	return Child;
}

FGenome UEvolutionManager::MultiplePointCrossover(const FGenome& Parent1, const FGenome& Parent2)
{
	FGenome Child;
	const int NumberOfPoints = FMath::RandRange(1, Parent1.Genes.Num() / 2);

	TArray<int> CrossoverPoints;
	for (int i = 0; i < NumberOfPoints; i++)
	{
		CrossoverPoints.Add(FMath::RandRange(0, Parent1.Genes.Num() - 1));
	}
	CrossoverPoints.Sort();

	bool UseParent1 = true;
	int CrossoverIndex = 0;
	for (int i = 0; i < Parent1.Genes.Num(); i++)
	{
		if (CrossoverIndex < CrossoverPoints.Num() && i == CrossoverPoints[CrossoverIndex])
		{
			UseParent1 = !UseParent1;
			CrossoverIndex++;
		}
		if (UseParent1)
		{
			Child.Genes.Add(Parent1.Genes[i]);
		}
		else
		{
			Child.Genes.Add(Parent2.Genes[i]);
		}
	}
	return Child;
}

FGenome UEvolutionManager::OverlayCrossover(const FGenome& Parent1, const FGenome& Parent2)
{
	FGenome Child;
	const int MidPoint = Parent1.Genes.Num() / 2;

	for (int i = 0; i < Parent1.Genes.Num(); i++)
	{
		if (i < MidPoint)
			Child.Genes.Add(Parent1.Genes[i]);
		else
			Child.Genes.Add(Parent2.Genes[i]);
	}

	return Child;
}

void UEvolutionManager::ApplyMutation(FGenome& Genome, float MutationRate, EMutationStrategy MutationStrategy)
{
	switch (MutationStrategy)
	{
	case EMutationStrategy::POINTMUTATION:
		PointMutation(Genome, MutationRate);
		break;
	case EMutationStrategy::MULTIPOINTMUTATION:
		MultiPointMutation(Genome, MutationRate);
		break;
	default:
		assert(false);
	}
}

FGene UEvolutionManager::MutateGene(const FGene& Gene)
{
	FGene NewGene = Gene;
	UGenomeFunctions::RandomBitFlip(NewGene);
	return NewGene;
}

void UEvolutionManager::PointMutation(FGenome& Genome, const float MutationRate)
{
	const int MutationPoint = FMath::RandRange(0, Genome.Genes.Num() - 1);
	if (FMath::FRand() < MutationRate)
	{
		MutateGene(Genome.Genes[MutationPoint]);
	}
}

void UEvolutionManager::MultiPointMutation(FGenome& Genome, const float MutationRate)
{
	for (int i = 0; i < Genome.Genes.Num(); i++)
	{
		if (FMath::FRand() < MutationRate)
		{
			Genome.Genes[i] = MutateGene(Genome.Genes[i]);
		}
	}
}

void UEvolutionManager::CheckPopulationDiversity(const TArray<FGenome>& Population)
{
	TSet<FString> UniqueGenomes;

	for (const FGenome& Genome : Population)
	{
		FString GenomeStr;
		for (const FGene& Gene : Genome.Genes)
		{
			GenomeStr += FString::Printf(TEXT("%02x%02x%02x%02x%04x"),
										 Gene.bSourceType,
										 Gene.bTargetType,
										 Gene.SourceIdx,
										 Gene.TargetIdx,
										 Gene.Weight);
		}
		UniqueGenomes.Add(GenomeStr);
	}

	UE_LOG(LogTemp, Warning, TEXT("Unique Genomes: %d"), UniqueGenomes.Num());
}
