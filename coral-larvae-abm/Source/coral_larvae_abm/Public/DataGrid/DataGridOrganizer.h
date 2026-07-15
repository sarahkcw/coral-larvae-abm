#pragma once
#include "DataGrid.h"
#include "DataGridPreprocessor.h"
#include "DataGridVisualizer.h"
#include "DataGridOrganizer.generated.h"

UCLASS()
class CORAL_LARVAE_ABM_API ADataGridOrganizer : public AActor
{
	GENERATED_BODY()
public:

	ADataGridOrganizer();
	FDataConfig GetConfigOfGrid() const { return DataGrid->GetDataConfig(); }
	FEnvironmentalParams GetActiveEnvironmentalParams() const;
	void GenerateNewDataGrid() const;
	void GenerateNewLimestoneTiles();
	void AdaptE2DataGrid(int Hour);
	void ResetDataGrid() const { DataGrid->CopyDataFrom(BackupDataGrid); }
	
protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

public:    
	virtual void Tick(float DeltaTime) override;
	
	// UPROPERTY() USceneComponent* Root;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DataGridComponents")
	UDataGrid* DataGrid;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DataGridComponents")
	UDataGridPreprocessor* DataGridPreprocessor;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DataGridComponents")
	UDataGridVisualizer* DataGridVisualizer;

	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnvironmentSetup") FEnvironmentalParams EnvironmentalParams = FEnvironmentalParams();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnvironmentSetup|Current") bool bCurrentEnabled = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnvironmentSetup|Current") float CurrentSpeedCmPerStep = 0.1f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GridSetUp") FVector WorldBounds = FVector(100.f, 100.f, 100.f);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GridSetUp") float CellEdgeLength = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GridSetUp") float RelativeSmoothingIteration = 15.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GridSetUp") int GradientResolution = 4;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Experiment") bool E1 = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Experiment") bool E2 = false;
	// PAR diffuse attenuation coefficient Kd (per metre) for the E2 water column. The old 0.017
	// was clearer than the clearest open ocean, so over the 2.2 m column light dropped <4% end to
	// end -> light carried essentially no depth information (the intensity sensor is a scalar, no
	// directional gradient). 0.1/m is a defensible clear coastal/reef value giving a real but
	// non-dominant vertical gradient (~20% over 2.2 m). Vertical distribution is driven primarily
	// by the age-gated geotactic bias (see FAgentInitParams / findings), with light as a secondary
	// cue. Tune per scene; cite the chosen Kd in the manuscript.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment|E2") float E2AttenuationCoefficient = 0.1f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Experiment") bool E3 = false;
	// V3E "sound off" control: when false, CalculateE3Values skips populating the
	// sound/particle-motion field (cells stay at DefaultParticleMotion) without removing level
	// SoundSource actors, giving a clean no-sound E3 control condition.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment|E3", meta = (EditCondition = "E3")) bool bSoundEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Experiment|E1") int TileCount = 30;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Experiment|E1") float CCACover = 0.341f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Experiment|E1") float MinTileArea = 2.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Experiment|E1") float MaxTileArea = 57.f;

private:
	UPROPERTY()
	UDataGrid* BackupDataGrid;

	void OnLimestoneTileDelayCompleted();
};
