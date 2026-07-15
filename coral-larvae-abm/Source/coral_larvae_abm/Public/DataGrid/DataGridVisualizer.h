#pragma once
#include "CoreMinimal.h"
#include "DataGrid.h"
#include "DebugControlSphere.h"
#include "Components/ActorComponent.h"
#include "DataGridVisualizer.generated.h"

/*
 * This component is attached next to a EnvironmentDataGrid Component to debug and visualize information from the DataGrid.
 * It is extremely restrictive, so any missing components or configurations will throw exceptions!
 * It should only be used with the DataGridOrganizer Actor.
 */

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class CORAL_LARVAE_ABM_API UDataGridVisualizer : public UActorComponent
{
	GENERATED_BODY()

public:	
	UDataGridVisualizer();
	
	UFUNCTION(BlueprintCallable)
	void Init();
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Debugging")
	ADebugControlSphere* DebugControlActor;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Debugging")
	bool DrawDebug = true; 
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Debugging")
	bool DrawCellBorder = true;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Debugging")
	bool DrawCurrent = false;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Debugging")
	bool DrawReefCells = true;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Debugging")
	bool DrawInvalidCells = false;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Debugging")
	bool DrawCCAConcentration = false;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Debugging")
	bool DrawAlteromonas = false;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Debugging")
	bool DrawSoundIntensity = false;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Debugging")
	bool DrawLightIntensity = false;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Debugging")
	bool DrawPressure = false;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Debugging")
	float GridThickness = 0.05f;

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	
	UPROPERTY()
	UDataGrid* DataGrid;

	UPROPERTY()
	AActor* Owner;
	
private:
	
	void DebugGrid() const; 
	void DebugReefCells() const; 
	void DebugInvalidCells() const; 
	void DebugCCA() const; 
	void DebugAlteromonas() const; 
	void DebugParticleMotion() const; 
	void DebugLightIntensity() const;
	void DebugPressure() const;
};
