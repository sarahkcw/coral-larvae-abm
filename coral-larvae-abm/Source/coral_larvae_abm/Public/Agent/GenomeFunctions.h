#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GenomeFunctions.generated.h"

constexpr uint8_t GSensor = 1; // Always a source
constexpr uint8_t GAction = 1; // Always a sink
constexpr uint8_t GNeuron = 0; // Can be either a source or sink

USTRUCT(BlueprintType)
struct FGene
{
	FORCEINLINE bool operator==(const FGene& Other) const
	{
		return SourceIdx == Other.SourceIdx &&
			TargetIdx == Other.TargetIdx &&
			Weight == Other.Weight &&
			bSourceType == Other.bSourceType &&
			bTargetType == Other.bTargetType;
	}
	GENERATED_USTRUCT_BODY()
	// Connection between neurons (source and target)
	// Note: Order is important here! Otherwise, the memory will be scrambled
	uint16_t bSourceType:1; // 0 = Neuron, 1 = Sensor
	uint16_t SourceIdx:7; // Index of the source
	uint16_t bTargetType:1; // 0 = Neuron, 1 = Action
	uint16_t TargetIdx:7; // Index of the target
	int16_t Weight; // Weight of the connection

	float WeightAsFloat() const { return Weight / 8192.0; }
	static int16_t MakeRandomWeight() { return FMath::RandRange(0, 0xffff) - 0x8000; }
};

USTRUCT(BlueprintType)
struct FNeuron
{
	GENERATED_USTRUCT_BODY()
	UPROPERTY(BlueprintReadOnly)
	float Output = 0.0f; // Weighted sum of inputs + tanh
	UPROPERTY(BlueprintReadOnly)
	bool bDriven = false; // If the neuron is active
};

USTRUCT(BlueprintType)
struct FNeuralNet
{
	GENERATED_USTRUCT_BODY()
	UPROPERTY(BlueprintReadOnly)
	TArray<FNeuron> Neurons;
	UPROPERTY(BlueprintReadOnly)
	TArray<FGene> Connections;
};

USTRUCT(BlueprintType)
struct FNeuralNode
{
	GENERATED_USTRUCT_BODY()
	FNeuralNode() : RemappedNumber(0), NumOutputs(0), NumSelfInputs(0), NumInputsFromSensorsOrOtherNeurons(0) {}
	uint16_t RemappedNumber; 
	uint16_t NumOutputs; 
	uint16_t NumSelfInputs; 
	uint16_t NumInputsFromSensorsOrOtherNeurons; 
};

typedef TMap<uint16_t, FNeuralNode> FNodeMap;
constexpr float InitialNeuronOutput() { return 0.5; }

USTRUCT(BlueprintType)
struct FGenome // Genetic Encoding of Neural Net Structure 
{
	GENERATED_USTRUCT_BODY()
	TArray<FGene> Genes;
	FORCEINLINE bool operator==(const FGenome& Other) const { return Genes == Other.Genes; }
};

UCLASS()
class CORAL_LARVAE_ABM_API UGenomeFunctions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
	static FGene MakeRandomGene();
	static FGenome MakeRandomGenome(int GenomeLength);
	static void RandomBitFlip(FGenome& Genome);
	static void RandomBitFlip(FGene& Gene);
	static float GetHammingDistanceBytes(const FGenome& Genome1, const FGenome& Genome2);
	
	// Debugging
	static float EvaluateGeneticDiversity(const int Population, TArray<FGenome>& Genomes);
	static float EvaluateGenomeSimilarity(const FGenome& Genome1, const FGenome& Genome2);
	static FGenome GetGenomeFromString(const FString& GenomeString);
	static void LogGenome(const FGenome& Genome, const FString& Label);
	static bool CompareGenomesBitwise(const FGenome& Genome1, const FGenome& Genome2);
	static void LogGenomeBinary(const FGenome& Genome, const FString& Label);
	static void LogGeneBinary(const FGene& Gene);
	static FString ToBinaryString(uint16_t Value);
	static FString ToBinaryString(int16_t Value);
	static FString ToBinaryString(uint8_t Value);
};

