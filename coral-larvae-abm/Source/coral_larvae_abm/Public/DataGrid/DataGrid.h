#pragma once
#include "CoreMinimal.h"
#include "DataGridStructs.h"
#include "Components/ActorComponent.h"
#include "DataGrid.generated.h"

/*
 * Central component to manage the data of the environment
 * Should be used only to store, expose and manage the data itself, should not be used to compute stuff itself.
 */
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class CORAL_LARVAE_ABM_API UDataGrid : public UActorComponent
{
	GENERATED_BODY()

public:	
	UDataGrid();
	UFUNCTION(BlueprintCallable) void Init(FDataConfig Config); 
	UFUNCTION(BlueprintCallable, Category = "Data manipulation") void SetDataAtWorldLocation(FVector Location, FCellData CellData);
	UFUNCTION(BlueprintCallable, Category = "Data manipulation") void SetCellInvalidAtWorldLocation(FVector Location);
	
	UFUNCTION(BlueprintCallable, Category = "Data reading")	FDataChunk GetCellDataSphereAround(FVector WorldPosition, float Radius);
	UFUNCTION(BlueprintCallable, Category = "Data reading")	FDataChunk GetCellDataAsChunkAt(FVector WorldPosition, int HalfHeightCells);
	UFUNCTION(BlueprintCallable, Category = "Data reading")	FIndexedCellData GetCellAtPoint(FVector WorldPosition);
	UFUNCTION(BlueprintCallable, Category = "Data reading")	const FDataChunk& GetBulkData();
	UFUNCTION(BlueprintCallable, Category = "Utilities") FDataConfig GetDataConfig();
	
	// This creates a deep copy of the data from another grid, ensuring that this data is thread safe for each agent
	void CopyDataFrom(const UDataGrid* OtherDataGrid);

private:	
	UPROPERTY()	FDataChunk Grid;
	UPROPERTY()	FDataConfig DataConfig;
	void SetDataAtCellIndex(FDataIndex Index, FCellData CellData);
	FDataChunk GetCellDataInRange(const FVector& Position, int HalfHeightCells, float Radius = -1.0f) const;
};
