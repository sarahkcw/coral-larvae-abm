#include "DataGrid/DataGridOrganizer.h"
#include "DataGrid/DataGrid.h"
#include "DataGrid/DataGridPreprocessor.h"
#include "DataGrid/DataGridVisualizer.h"

ADataGridOrganizer::ADataGridOrganizer()
{
	PrimaryActorTick.bCanEverTick = true;

	// Root = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	// this->SetRootComponent(Root);
	DataGrid = CreateDefaultSubobject<UDataGrid>(TEXT("DataGrid"));
	DataGridPreprocessor = CreateDefaultSubobject<UDataGridPreprocessor>(TEXT("DataGridPreprocessor"));
	DataGridVisualizer = CreateDefaultSubobject<UDataGridVisualizer>(TEXT("DataGridVisualizer"));
	BackupDataGrid = CreateDefaultSubobject<UDataGrid>(TEXT("BackupDataGrid"));
	
}

void ADataGridOrganizer::GenerateNewDataGrid() const
{
	DataGridPreprocessor->PrepareExperimentEnvironment(GetWorld(), *DataGrid, GetActiveEnvironmentalParams(), CCACover, GradientResolution, bSoundEnabled);
	BackupDataGrid->CopyDataFrom(DataGrid);
}

FEnvironmentalParams ADataGridOrganizer::GetActiveEnvironmentalParams() const
{
	FEnvironmentalParams ActiveEnvironmentalParams = EnvironmentalParams;
	ActiveEnvironmentalParams.bCurrentEnabled = bCurrentEnabled || EnvironmentalParams.bCurrentEnabled;
	ActiveEnvironmentalParams.CurrentSpeedCmPerStep = CurrentSpeedCmPerStep > 0.f
		? CurrentSpeedCmPerStep
		: EnvironmentalParams.CurrentSpeedCmPerStep;
	return ActiveEnvironmentalParams;
}
 
void ADataGridOrganizer::GenerateNewLimestoneTiles() 
{
	DataGridPreprocessor->RemoveLimestoneTiles(GetWorld(), *DataGrid);
	DataGridPreprocessor->SpawnLimestoneTiles(GetWorld(), *DataGrid, TileCount, MinTileArea, MaxTileArea);
}

void ADataGridOrganizer::AdaptE2DataGrid(const int Hour)
{
	DataGridPreprocessor->AdaptE2Values(*DataGrid, Hour, E2AttenuationCoefficient);
}

void ADataGridOrganizer::BeginPlay()
{
	Super::BeginPlay();
	FDataConfig DataConfig = FDataConfig{WorldBounds, 0, 0, 0, CellEdgeLength, GetActorLocation()};
	DataGrid->Init(DataConfig);
	DataGridVisualizer->Init();
	
	GenerateNewDataGrid();
}

void ADataGridOrganizer::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	FlushPersistentDebugLines(GetWorld());
	const FVector GridCenter = GetActorLocation() + WorldBounds / 2.f;
	const FVector GridExtent = WorldBounds / 2.f;
	DrawDebugBox(GetWorld(), GridCenter, GridExtent, GetActorRotation().Quaternion(), FColor::White, false, 5000.f, 0, 0.05f);

}

void ADataGridOrganizer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void ADataGridOrganizer::OnLimestoneTileDelayCompleted()
{
	DataGridPreprocessor->PrepareExperimentEnvironment(GetWorld(), *DataGrid, GetActiveEnvironmentalParams(), CCACover, GradientResolution, bSoundEnabled);
}

