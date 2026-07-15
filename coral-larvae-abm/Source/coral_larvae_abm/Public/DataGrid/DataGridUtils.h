#pragma once
#include "CoreMinimal.h"
#include "DataGrid.h"
#include "DataGridStructs.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DataGridUtils.generated.h"

UCLASS()
class CORAL_LARVAE_ABM_API UDataGridUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
	static int GetMemoryIndexAt(FDataIndex Index, const FDataConfig& Config);
	static FVector GetLocalCellOrigin(FDataIndex Index, const FDataConfig& Config);
	static FDataIndex GetCellDataIndexFromWorldPosition(const FVector& Position, const FDataConfig& Config);
	static bool IsDataIndexInChunk(const FDataIndex& Index, const FDataConfig& Config);
	static int GetNumCells(const FDataConfig& Config) { return Config.CellCountX*Config.CellCountY*Config.CellCountZ; }
	static bool IsInBounds(const FDataConfig& Config, const FVector& Position);
	static bool IsInBoundsWithThreshold(const FDataConfig& Config, const FVector& Position, float Threshold);
	static bool IsInReefCell(const FDataChunk& DataChunk, const FVector& Position);
	static TArray<FVector> GetReefCellPositions(const FDataChunk& DataChunk);
	
	UFUNCTION(BlueprintCallable, Category = "Data reading")
	static FIndexedCellData GetCellAtPoint(FVector WorldPosition, const FDataChunk& DataChunk);
private: 
	static constexpr float Small_Epsilon = 1e-4;
};
