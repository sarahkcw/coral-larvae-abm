#include "DataGrid/DataGridPreprocessor.h"
#include "SimulationManager.h"
#include "DataGrid/DataGridUtils.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"

// R1-3 E3 acoustic sensitivity-sweep knobs. Defaults reproduce the original hardcoded values, so
// default (V3A-F) runs are byte-identical; overridden at validation via -SoundSPL/-SoundFreqLow/-SoundFreqHigh.
float UDataGridPreprocessor::SoundSourceLevelDb = 153.0f;
float UDataGridPreprocessor::SoundMinFrequency  = 100.0f;
float UDataGridPreprocessor::SoundMaxFrequency  = 10000.0f;

void UDataGridPreprocessor::PrepareExperimentEnvironment(UWorld* World, UDataGrid& DataGrid, const FEnvironmentalParams& Params, float CCACover, int GradientResolution, bool bSoundEnabled)
{
	TArray<AActor*> SoundSources;
	UGameplayStatics::GetAllActorsWithTag(World, FName(SoundSourceTag), SoundSources);
	FVector SoundLocation = FVector(0.f);
	if(SoundSources.Num() > 0)
		SoundLocation = SoundSources[0]->GetActorLocation();

	if(SoundSources.Num() == 0)
	{
		//UE_LOG(LogTemp, Warning, TEXT("No Sound Sources found!"));
	}
	
	// Assign default environmental parameters to the data grid
	PrepareDefaultEnvironment(DataGrid, Params, SoundLocation);

	CalculateE3Values(World, DataGrid, bSoundEnabled);
	
	auto Config = DataGrid.GetDataConfig();
	// Get all limestone tiles and assign reef cells
	TArray<AActor*> LimestoneTiles;
	UGameplayStatics::GetAllActorsWithTag(World, FName(TileTag), LimestoneTiles);
	
	if(LimestoneTiles.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("No limestone tiles found!"));
		return;
	}
		
	int ReefCellNumber = 0;
	for (FIndexedCellData Data : DataGrid.GetBulkData().Data)
	{
		auto CellData = Data.Data;
		for(auto Tile : LimestoneTiles)
		{
			FBox TileBounds = Tile->GetComponentsBoundingBox();
			FBox CellBounds = FBox(Data.WorldPosition - Config.CellEdgeLength / 2, Data.WorldPosition + Config.CellEdgeLength / 2);
			if(TileBounds.Intersect(CellBounds))
			{
				CellData.bIsReefCell = true;
				
				if (FMath::RandRange(0.f, 1.f) < CCACover)
					CellData.ReefData.CCA = 1.f;
				else CellData.ReefData.CCA = 0.f;
				
				ReefCellNumber++;
				break; 
			}
		}
		DataGrid.SetDataAtWorldLocation(Data.WorldPosition, CellData);
	}
	// Assign CCA gradient distribution to the reef cells
	CalculateGradientDistribution(DataGrid);
	DistributeAlteromonas(DataGrid);
}

void UDataGridPreprocessor::AdaptE2Values(UDataGrid& DataGrid, int Hour, float AttenuationCoefficient)
{
	auto BulkDataChunk = DataGrid.GetBulkData();
	const auto DataConfig = DataGrid.GetDataConfig();
	
	float DepthThreshold = 0.001f;
	
	float MaxLightIntensity = 1.f;  // Maximum light intensity at the surface // Normalized to 1.0
	// Diurnal cycle, peaks at noon (Hour 12) and is dark at midnight (Hour 0/24), always >= 0.
	// (The old 0.5*(1+sin(PI/12*Hour)) peaked at 06:00 and gave only 0.5 at noon.)
	float LightIntensity = MaxLightIntensity * (0.5f * (1.f - FMath::Cos(PI / 12.f * Hour)));

	float MinTemperature = 24.0f;
	float MaxTemperature = 33.2f;
	float PeakTime = 15.0f;
	float BaseTemperature = (MinTemperature + MaxTemperature) / 2.0f;
	float TemperatureAmplitude = (MaxTemperature - MinTemperature) / 2.0f;
	
	for (FIndexedCellData Data : DataGrid.GetBulkData().Data)
	{
		auto CellData = Data.Data;
		const float LocalZ = Data.WorldPosition.Z - DataConfig.ChunkWorldOrigin.Z;
		const float DepthCm = DataConfig.LocalBounds.Z - LocalZ - DataConfig.CellEdgeLength / 2 + DepthThreshold;
		const float DepthM = DepthCm / 100.0f;
		CellData.LightData.LightIntensity = LightIntensity * FMath::Exp(-AttenuationCoefficient * DepthM);
		float Temperature = BaseTemperature + TemperatureAmplitude * FMath::Sin((Hour - PeakTime) * (PI / 12.0f));
		CellData.WaterData.Temperature = Temperature;
		DataGrid.SetDataAtWorldLocation(Data.WorldPosition, CellData);
	}
}

void UDataGridPreprocessor::CalculateE3Values(UWorld* World, UDataGrid& DataGrid, bool bSoundEnabled)
{
	if (!bSoundEnabled)
	{
		// V3E "sound off" control: leave every cell at DefaultParticleMotion (set in
		// PrepareDefaultEnvironment) instead of populating the sound field, without touching
		// level SoundSource actors.
		return;
	}

	auto BulkDataChunk = DataGrid.GetBulkData();

	TArray<AActor*> SoundSources;
	UGameplayStatics::GetAllActorsWithTag(World, FName(SoundSourceTag), SoundSources);
	if(SoundSources.Num() == 0)
	{
		//UE_LOG(LogTemp, Warning, TEXT("No Sound Sources found!"));
		return;
	}
	float SourceLevel = SoundSourceLevelDb; // Source Level in dB re 1 µPa at 1 meter (default 153; -SoundSPL)
	float ReferenceDistance = 1.0f; // Reference distance in meters
	float WaterDensity = 1000.0f; // Density of water in kg/m^3
	float SpeedOfSoundInWater = 1500.0f; // Speed of sound in water in m/s
	float MinFrequency = SoundMinFrequency; // Minimum frequency in range (Hz) (default 100; -SoundFreqLow)
	float MaxFrequency = SoundMaxFrequency; // Maximum frequency in range (Hz) (default 10000; -SoundFreqHigh)
	float AirToWaterTransmissionCoeff = 0.5f; // Transmission coefficient (approximation)
	float ScalingFactor = 1e9;
	
	for (auto& DataCellPackage : BulkDataChunk.Data)
	{
		auto CellData = DataCellPackage.Data;

		float TotalParticleVelocity = 0.0f;
		for (const AActor* SoundSource : SoundSources)
		{
			const float Distance = FVector::Dist(SoundSource->GetActorLocation(), DataCellPackage.WorldPosition);
			const float DistanceInMeters = FMath::Max(Distance / 100.0f, 0.01f); // UE units are centimeters.
			// Approximate the three synchronized speakers as additive scalar cue sources.
			const float SPL = SourceLevel - 11.1f * FMath::LogX(10.0f, DistanceInMeters / ReferenceDistance);
			float Pressure = FMath::Pow(10.0f, SPL / 20.0f) * 1e-6f; // Convert dB SPL to Pascals
			Pressure *= AirToWaterTransmissionCoeff;
			const float MedianFrequency = (MinFrequency + MaxFrequency) / 2.0f;
			float ParticleVelocity = (Pressure / (WaterDensity * SpeedOfSoundInWater)) / (2.0f * PI * MedianFrequency);
			ParticleVelocity *= ScalingFactor;
			TotalParticleVelocity += ParticleVelocity;
		}

		CellData.WaterData.ParticleMotion = TotalParticleVelocity;
		DataGrid.SetDataAtWorldLocation(DataCellPackage.WorldPosition, CellData);
	}
}

void UDataGridPreprocessor::PrepareDefaultEnvironment(UDataGrid& DataGrid, const FEnvironmentalParams& Params, FVector SoundSourceLocation)
{
	auto BulkDataChunk = DataGrid.GetBulkData();
	const auto DataConfig = DataGrid.GetDataConfig();
	
	int MaxDepth = DataConfig.CellCountZ;
	float AttenuationCoefficient = Params.LightAttenuationCoefficient; // default 0.085; -LightAtten overrides (V2C)
	float FromPaToAtmospheres = 101325.0f;
	float VioletEnd = 400.0f; 
	float BlueEnd = 475.0f;
	float Threshold = 0.001f;
	float AtmosphericPressure = 1.f; // Pressure at the surface in ATM (Atmospheres)
	float Density = 1025.0f; // kg/m³, density of seawater
	float Gravity = 9.81f; // m/s², acceleration due to gravity
	float DefaultParticleMotion = 1e-8f;
	
	float MaxSoundDistance = FVector::Dist(SoundSourceLocation, DataConfig.ChunkWorldOrigin + DataConfig.LocalBounds - DataConfig.CellEdgeLength / 2.f);
	MaxSoundDistance /= DataConfig.CellEdgeLength;
	
	FVector CurrentDirection = FVector(0,0,0);

	if(Params.bCurrentEnabled)
	{
		// Simplified uniform horizontal flow along +Y. The replicated lab experiments
		// (E1/E2/E3) run with bCurrentEnabled = false, so this only affects optional
		// "current-on" robustness scenarios; magnitude is set per-cell below via
		// CurrentSpeedCmPerStep and a linear depth factor. A calibrated flow field is
		// out of scope (see reviewer R3-6 note).
		CurrentDirection = FVector(0, 1, 0);
	}
	
	for (auto& DataCellPackage : BulkDataChunk.Data)
	{
		auto CellData = DataCellPackage.Data;
		auto CellPosition = DataCellPackage.WorldPosition;
		const float LocalZ = CellPosition.Z - DataConfig.ChunkWorldOrigin.Z;
		const float Depth = (DataConfig.LocalBounds.Z - LocalZ - DataConfig.CellEdgeLength / 2 + Threshold) / 100.0f;
		const float MaxDepthMeters = FMath::Max(DataConfig.LocalBounds.Z / 100.0f, 0.001f);
		const float DepthFactor = Depth / MaxDepthMeters;
		CellData.WaterData.Temperature = Params.MinTemperature + (Params.MaxTemperature - Params.MinTemperature) * DepthFactor;
		CellData.WaterData.Salinity = Params.MeanSalinity;
		CellData.WaterData.Pressure = AtmosphericPressure + (Density * Gravity * Depth) / FromPaToAtmospheres; 
		CellData.WaterData.Current = CurrentDirection.GetSafeNormal();
		CellData.WaterData.ParticleMotion = DefaultParticleMotion;
		if(Params.bCurrentEnabled)
		{
			const float DepthCurrentFactor = FMath::Clamp(1.f - (LocalZ / FMath::Max(DataConfig.LocalBounds.Z, 0.001f)), 0.f, 1.f);
			CellData.WaterData.CurrentForce = FMath::Max(Params.CurrentSpeedCmPerStep, 0.f) * DepthCurrentFactor;
		}
		else
		{
			CellData.WaterData.CurrentForce = 0.0f;
		}
		CellData.LightData.LightIntensity = Params.SurfaceLight * FMath::Exp(-AttenuationCoefficient * Depth);
		if (Depth > Params.DepthForBlueDominance) {
			CellData.LightData.LightWavelength = BlueEnd;
		} else {
			float FractionTowardsBlue = Depth / Params.DepthForBlueDominance;
			CellData.LightData.LightWavelength = VioletEnd + (BlueEnd - VioletEnd) * FractionTowardsBlue;
		}
		float Distance = FVector::Dist(SoundSourceLocation, DataCellPackage.WorldPosition);
		Distance /= DataConfig.CellEdgeLength;
		// Store the updated data back into the grid
		DataGrid.SetDataAtWorldLocation(CellPosition, CellData);
	}
}

void UDataGridPreprocessor::CalculateGradientDistribution(UDataGrid& DataGrid)
{
	const float MinimumCCA = 0.00001f;  
	
	TArray<FIndexedCellData> ReefCells;
	for (FIndexedCellData DataPackage : DataGrid.GetBulkData().Data)
	{
		if (!DataPackage.Data.bIsReefCell)
			continue;
		ReefCells.Add(DataPackage);
	}

	auto NumReefCells = ReefCells.Num();
	if (NumReefCells == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Skipping CCA gradient distribution because no reef cells were found."));
		return;
	}
	
	// Iterate over each cell, calculate the cca concentration based on exponential decay
	// and cap based on number of reef cells to avoid over saturation
	/*for (FIndexedCellData Data : DataGrid.GetBulkData().Data)
	{
		if (Data.Data.bIsReefCell) continue;
		
		auto AbsCCA = 0.0f;
		for (FIndexedCellData ReefCell : ReefCells)
		{
			auto DistToCell = (ReefCell.WorldPosition - Data.WorldPosition).Length();
			auto DistAsCells = DistToCell / DataGrid.GetDataConfig().CellEdgeLength;
			// Exponential decay of CCA concentration based on distance measured in cells
			AbsCCA += ReefCell.Data.ReefData.CCA * FMath::Pow(0.5, DistAsCells); 
		}

        Data.Data.ReefData.CCA = FMath::Max(AbsCCA / NumReefCells, MinimumCCA);
		DataGrid.SetDataAtWorldLocation(Data.WorldPosition, Data.Data);
	}*/

	const float Sigma = 5.f;//100.0f; // 65: 5.0f // 
	for (FIndexedCellData Data : DataGrid.GetBulkData().Data)
	{
		if (Data.Data.bIsReefCell) continue;

		float AbsCCA = 0.0f;
		for (FIndexedCellData ReefCell : ReefCells)
		{
			auto DistToCell = (ReefCell.WorldPosition - Data.WorldPosition).Length();
			auto DistAsCells = DistToCell / DataGrid.GetDataConfig().CellEdgeLength;

			// Gaussian decay of CCA concentration based on distance
			AbsCCA += ReefCell.Data.ReefData.CCA * FMath::Exp(-FMath::Pow(DistAsCells, 2) / (2 * FMath::Pow(Sigma, 2)));
		}
		// Calculate average CCA concentration and apply the minimum threshold
		Data.Data.ReefData.CCA = AbsCCA / NumReefCells;

		DataGrid.SetDataAtWorldLocation(Data.WorldPosition, Data.Data);
	} 
}

void UDataGridPreprocessor::DistributeAlteromonas(UDataGrid& DataGrid)
{
	const float AlteromonasScaleFactor = 0.9f;
	for (auto& Cell : DataGrid.GetBulkData().Data)
	{
		if (Cell.Data.bIsReefCell)
		{
			// Alteromonas concentration depends on the CCA value
            FCellData IndexedCellData = Cell.Data;
			float AlteromonasConcentration = Cell.Data.ReefData.CCA * AlteromonasScaleFactor;
			IndexedCellData.ReefData.Alteromonas = FMath::Clamp(AlteromonasConcentration, 0.0f, 1.0f);
			DataGrid.SetDataAtWorldLocation(Cell.WorldPosition, IndexedCellData);
		}
	}
}

float UDataGridPreprocessor::SpawnLimestoneTiles(UWorld* World, UDataGrid& DataGrid, int TileCount, float MinTileArea, float MaxTileArea)
{
    auto DataConfig = DataGrid.GetDataConfig();
    const float DesiredTotalArea = FMath::RandRange(MinTileArea, MaxTileArea);
    const float MinAreaPerTile = TileCount > 0 ? FMath::Min(1.0f, DesiredTotalArea / TileCount) : 0.0f;
    float RemainingArea = DesiredTotalArea;
    float CurrentTotalArea = 0.0f;

    UE_LOG(LogTemp, Warning, TEXT("DesiredTotalArea: %f"), DesiredTotalArea);

    for(int i = 0; i < TileCount; i++)
    {
        const int RemainingTilesAfterThis = TileCount - i - 1;
        const float MaxAreaForThisTile = FMath::Max(MinAreaPerTile, RemainingArea - RemainingTilesAfterThis * MinAreaPerTile);
        float TileArea = RemainingTilesAfterThis == 0
            ? RemainingArea
            : FMath::RandRange(MinAreaPerTile, MaxAreaForThisTile);
        TileArea = FMath::Clamp(TileArea, MinAreaPerTile, MaxAreaForThisTile);
        RemainingArea -= TileArea;
        CurrentTotalArea += TileArea;

        // Calculate the side size of this tile
        float TileSize = FMath::Sqrt(TileArea);
        if (TileSize < 1.0f)
            TileSize = 1.0f;

        const float HalfTileDiagonal = TileSize * UE_SQRT_2 / 2.0f;
        const float SpawnMargin = HalfTileDiagonal + DataConfig.CellEdgeLength / 2.0f;
        const float MinX = DataConfig.ChunkWorldOrigin.X + SpawnMargin;
        const float MinY = DataConfig.ChunkWorldOrigin.Y + SpawnMargin;
        const float MaxX = DataConfig.ChunkWorldOrigin.X + DataConfig.LocalBounds.X - SpawnMargin;
        const float MaxY = DataConfig.ChunkWorldOrigin.Y + DataConfig.LocalBounds.Y - SpawnMargin;
        const float TileThickness = 0.2f;
        const float SpawnPositionX = MaxX > MinX ? FMath::RandRange(MinX, MaxX) : DataConfig.ChunkWorldOrigin.X + DataConfig.LocalBounds.X / 2.0f;
        const float SpawnPositionY = MaxY > MinY ? FMath::RandRange(MinY, MaxY) : DataConfig.ChunkWorldOrigin.Y + DataConfig.LocalBounds.Y / 2.0f;
        const float SpawnPositionZ = DataConfig.ChunkWorldOrigin.Z + TileThickness / 2.0f;
        CreateLimestoneTile(World, FVector(SpawnPositionX, SpawnPositionY, SpawnPositionZ), FVector(TileSize, TileSize, TileThickness));
    }
    UE_LOG(LogTemp, Warning, TEXT("CurrentTotalArea: %f"), CurrentTotalArea);
    return CurrentTotalArea;
}

void UDataGridPreprocessor::RemoveLimestoneTiles(UWorld* World, UDataGrid& DataGrid)
{
	TArray<AActor*> LimestoneTiles;
	UGameplayStatics::GetAllActorsWithTag(World, FName(TileTag), LimestoneTiles);
	for(auto Tile : LimestoneTiles)
	{
		Tile->Destroy();
	}

	FReefData CleanReefData = FReefData();
	//Clean environment from limestone tiles
	for (auto& Cell : DataGrid.GetBulkData().Data)
	{
		if (Cell.Data.bIsReefCell)
		{
			// Alteromonas concentration depends on the CCA value
			FCellData IndexedCellData = Cell.Data;
			IndexedCellData.bIsReefCell = false;
			IndexedCellData.ReefData = CleanReefData;
			DataGrid.SetDataAtWorldLocation(Cell.WorldPosition, IndexedCellData);
		}
	}
}

void UDataGridPreprocessor::CreateLimestoneTile(UWorld* World, const FVector& Position, const FVector& TileSize)
{
	UStaticMesh* TileMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UMaterialInterface* TileMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/Reef_Mat")); 
	if (!TileMaterial)
		UE_LOG(LogTemp, Error, TEXT("TileMaterial not found!"));
	
	if (!TileMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("Tile mesh not found!"));
	}
	else
	{
		FActorSpawnParameters SpawnParams;
		const FRotator RandomRotator = FRotator(0.0f, FMath::RandRange(0.0f, 360.0f), 0.0f);
	
		AStaticMeshActor* SpawnedTile = World->SpawnActor<AStaticMeshActor>(Position, RandomRotator, FActorSpawnParameters());
		if (SpawnedTile)
		{
			// The engine cube mesh is 100 cm per side; TileSize is specified in centimeters.
			SpawnedTile->SetActorScale3D(TileSize / 100.0f);
			UStaticMeshComponent* TileMeshComponent = SpawnedTile->GetStaticMeshComponent();
			TileMeshComponent->SetMobility(EComponentMobility::Movable);
			TileMeshComponent->SetStaticMesh(TileMesh);
			TileMeshComponent->SetMaterial(0, TileMaterial);
			TileMeshComponent->SetSimulatePhysics(false);
			TileMeshComponent->SetEnableGravity(false);
			SpawnedTile->Tags.Add(FName(TileTag));
		}
	}
}


void UDataGridPreprocessor::Shuffle(TArray<FIndexedCellData>& Array)
{
	const int32 LastIndex = Array.Num() - 1;
	for (int32 i = 0; i <= LastIndex; ++i)
	{
		int32 j = FMath::RandRange(i, LastIndex);
		if (i != j)
		{
			Array.Swap(i, j);
		}
	}
}











