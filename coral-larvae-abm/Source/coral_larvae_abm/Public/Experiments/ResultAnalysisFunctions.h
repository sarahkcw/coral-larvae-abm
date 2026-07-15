#pragma once
#include "CoreMinimal.h"
#include "EvolutionManager.h"
#include "DataGrid/DataGridStructs.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Runtime/Engine/Classes/Kismet/BlueprintFunctionLibrary.h"
//#include <Eigen/Dense>
#include "ResultAnalysisFunctions.generated.h"

struct FGLMResult
{
	float Intercept;      // Beta(0) // baseline log-odds of settlement success when the substrate area is zero
	float Slope;          // Beta(1) // coefficient for substrate area // how the log-odds of settlement success change with substrate area
	float StdError;       // Standard error of Beta(1)
	float PValue;         // p-value for testing the significance of Beta(1) // indicates whether the relationship between substrate area and settlement success is statistically significant
};

UCLASS()
class CORAL_LARVAE_ABM_API UResultAnalysisFunctions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:	
	static void SaveGenomesToFile(TArray<FGenomeFitnessPair> GenomeFitnessPairs, const FString& FilePath);
	static TArray<FGenomeFitnessPair> LoadGenomesFromFile(const FString& FilePath);

	static float NearestNeighborDistances(TArray<FVector>& Positions);
	// Whether Spatial Distribution is random (R = 1), uniform (R > 1), or aggregated (R < 1)
	static float ClarkeAndEvansR(TArray<FVector>& Positions, float Area);
	// If the p-value is low (e.g., < 0.05), the observed pattern is unlikely to be due to random chance
	static float MonteCarloForSignificanceTesting(const FDataChunk& DataChunk, TArray<TArray<FVector>>& AllRunPositions, TArray<float>& Areas);
	// Analyze the relationship between substrate area (the independent variable) and the probability of settlement success (the dependent variable, which is binary: success or no success)
	static FGLMResult BinomialGeneralizedLinearModels(const TArray<float>& SubstrateAreas, const TArray<int>& SettlementResults);
	static int Aggregation(TArray<FVector>& Positions, float LarvalVolume);
	static TArray<int> CalcAggregationResults();
	static FGLMResult BinomialGLMForAggregation(const TArray<float>& PatchSizes, const TArray<int>& AggregationResults);
	static TArray<int> E2Counting(TArray<FVector>& Positions);
	static TArray<int> E3CountingSide(TArray<FVector>& Positions);
	static TArray<int> E3CountingAbove(TArray<FVector>& Positions);
	static TArray<TArray<FVector>> ReadSavedArrayPositions();
	static void ShuffleSettlementResults(TArray<int>& SettlementResults);
	static float ChiSquareTest();
	static float Anova();
 
private:
	void Shuffle(TArray<FVector>& Positions);
	static TArray<FVector> ShufflePositionsOnTiles(const FDataChunk& DataChunk, int NumLarvae);
	//static Eigen::VectorXf ConvertTArrayToEigen(const TArray<float>& Array);
	//static Eigen::VectorXi ConvertTArrayToEigen(const TArray<int>& Array);
	static float CalculatePValue(float ZValue);
};

