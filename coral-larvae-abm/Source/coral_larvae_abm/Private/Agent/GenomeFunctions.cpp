#include "Agent/GenomeFunctions.h"
#include <cassert>
#include <sstream>
#include <string>
#include <algorithm>
#include "SensorActionMapping.h"

FGenome UGenomeFunctions::GetGenomeFromString(const FString& GenomeString)
{
	FGenome Genome;
	std::stringstream ss(TCHAR_TO_UTF8(*GenomeString));
	std::string GeneString;

	while (ss >> GeneString) {
		try {
			uint32_t n = std::stoul(GeneString, nullptr, 16);
			FGene Gene;
			std::memcpy(&Gene, &n, sizeof(n));
			Genome.Genes.Add(Gene);
		} catch (const std::out_of_range&) {
			UE_LOG(LogTemp, Error, TEXT("Invalid gene value: %s"), *FString(GeneString.c_str()));
		}
	}
	return Genome;
}

FGene UGenomeFunctions::MakeRandomGene()
{
	FGene Gene;
	Gene.bSourceType = FMath::Rand() & 1;  // Random boolean
	Gene.bTargetType = FMath::Rand() & 1;
	Gene.SourceIdx = static_cast<uint16_t>(FMath::RandRange(0, 0x7fff));  // Random number
	Gene.TargetIdx = static_cast<uint16_t>(FMath::RandRange(0, 0x7fff));
	Gene.Weight = FGene::MakeRandomWeight();
	return Gene;
}

FGenome UGenomeFunctions::MakeRandomGenome(int GenomeLength)
{
	FGenome Genome;
	for(auto i = 0; i < GenomeLength; ++i)
	{
		Genome.Genes.Add(MakeRandomGene());
	}
	return Genome;
}

void UGenomeFunctions::RandomBitFlip(FGenome& Genome)
{
	const unsigned ElementIndex = FMath::RandRange(0, Genome.Genes.Num() - 1);
	const uint8_t BitIndex8 = 1 << FMath::RandRange(0, 7);

	const float Chance = FMath::FRand();

	if (Chance < 0.2) Genome.Genes[ElementIndex].bSourceType ^= 1;
	else if (Chance < 0.4) Genome.Genes[ElementIndex].bTargetType ^= 1;
	else if (Chance < 0.6) Genome.Genes[ElementIndex].SourceIdx ^= BitIndex8;
	else if (Chance < 0.8) Genome.Genes[ElementIndex].TargetIdx ^= BitIndex8;
	else Genome.Genes[ElementIndex].Weight ^= (1 << FMath::RandRange(1, 15));
}

void UGenomeFunctions::RandomBitFlip(FGene& Gene)
{
	const uint8_t BitIndex8 = 1 << FMath::RandRange(0, 7);
	const float Chance = FMath::FRand();
	if (Chance < 0.2) Gene.bSourceType ^= 1;
	else if (Chance < 0.4) Gene.bTargetType ^= 1;
	else if (Chance < 0.6) Gene.SourceIdx ^= BitIndex8;
	else if (Chance < 0.8) Gene.TargetIdx ^= BitIndex8;
	else Gene.Weight ^= (1 << FMath::RandRange(1, 15));
}

float UGenomeFunctions::GetHammingDistanceBytes(const FGenome& Genome1, const FGenome& Genome2)
{
	assert(Genome1.size() == Genome2.size());
	const unsigned NumElements = Genome1.Genes.Num();
	if (NumElements == 0)
		return 1.0f;

	unsigned MatchingGenes = 0;
	for (unsigned Index = 0; Index < NumElements; ++Index) {
		MatchingGenes += static_cast<unsigned>(Genome1.Genes[Index] == Genome2.Genes[Index]);
	}

	return MatchingGenes / static_cast<float>(NumElements);
}

float UGenomeFunctions::EvaluateGeneticDiversity(const int Population, TArray<FGenome>& Genomes)
{
	if (Population < 2) {
		return 0.0f;
	}

	// count limits the number of genomes sampled for performance reasons.
	unsigned Count = std::min(200, Population);
	int NumSamples = 0;
	float SimilaritySum = 0.0f;

	while (Count > 0) {
		// Ensure that two distinct genomes are selected randomly
		unsigned Index0 = FMath::RandRange(0, Population - 1);
		unsigned Index1;
        
		do {
			Index1 = FMath::RandRange(0, Population - 1);
		} while (Index0 == Index1);  // Ensure Index1 is different from Index0
        
		SimilaritySum += EvaluateGenomeSimilarity(Genomes[Index0], Genomes[Index1]);
		--Count;
		++NumSamples;
	}

	float Diversity = 1.0f - (SimilaritySum / NumSamples);
	return Diversity;
}

float UGenomeFunctions::EvaluateGenomeSimilarity(const FGenome& Genome1, const FGenome& Genome2)
{
	return GetHammingDistanceBytes(Genome1, Genome2);
}

void UGenomeFunctions::LogGenome(const FGenome& Genome, const FString& Label)
{
	UE_LOG(LogTemp, Warning, TEXT("%s Genome:"), *Label);
	for (const FGene& Gene : Genome.Genes)
	{
		FString GeneStr = FString::Printf(TEXT("%02x%02x %02x %02x %04x"),
										  Gene.bSourceType,
										  Gene.bTargetType,
										  Gene.SourceIdx,
										  Gene.TargetIdx,
										  Gene.Weight);
		UE_LOG(LogTemp, Warning, TEXT("%s"), *GeneStr);
	}
}

bool UGenomeFunctions::CompareGenomesBitwise(const FGenome& Genome1, const FGenome& Genome2)
{
	if (Genome1.Genes.Num() != Genome2.Genes.Num())
		return false;

	for (int i = 0; i < Genome1.Genes.Num(); ++i)
	{
		const FGene& Gene1 = Genome1.Genes[i];
		const FGene& Gene2 = Genome2.Genes[i];

		if (Gene1.bSourceType != Gene2.bSourceType ||
			Gene1.bTargetType != Gene2.bTargetType ||
			Gene1.SourceIdx != Gene2.SourceIdx ||
			Gene1.TargetIdx != Gene2.TargetIdx ||
			Gene1.Weight != Gene2.Weight)
		{
			return false;
		}
	}

	return true;
}

void UGenomeFunctions::LogGenomeBinary(const FGenome& Genome, const FString& Label)
{
	UE_LOG(LogTemp, Warning, TEXT("%s Genome (Binary):"), *Label);
	for (const FGene& Gene : Genome.Genes)
	{
		LogGeneBinary(Gene);
	}
}

void UGenomeFunctions::LogGeneBinary(const FGene& Gene)
{
	FString BinaryStr = FString::Printf(TEXT("bSourceType: %s, bTargetType: %s, SourceIdx: %s, TargetIdx: %s, Weight: %s"),
									   *ToBinaryString(Gene.bSourceType),
									   *ToBinaryString(Gene.bTargetType),
									   *ToBinaryString(Gene.SourceIdx),
									   *ToBinaryString(Gene.TargetIdx),
									   *ToBinaryString(Gene.Weight));
	UE_LOG(LogTemp, Warning, TEXT("%s"), *BinaryStr);
}

FString UGenomeFunctions::ToBinaryString(uint16_t Value)
{
	FString BinaryStr;
	for (int i = 15; i >= 0; --i)
	{
		BinaryStr.AppendChar((Value & (1 << i)) ? '1' : '0');
	}
	return BinaryStr;
}

FString UGenomeFunctions::ToBinaryString(int16_t Value)
{
	FString BinaryStr;
	for (int i = 15; i >= 0; --i)
	{
		BinaryStr.AppendChar((Value & (1 << i)) ? '1' : '0');
	}
	return BinaryStr;
}

FString UGenomeFunctions::ToBinaryString(uint8_t Value)
{
	FString BinaryStr;
	for (int i = 7; i >= 0; --i)
	{
		BinaryStr.AppendChar((Value & (1 << i)) ? '1' : '0');
	}
	return BinaryStr;
}

