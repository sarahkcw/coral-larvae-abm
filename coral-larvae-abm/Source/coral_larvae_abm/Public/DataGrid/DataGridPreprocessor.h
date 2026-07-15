#pragma once
#include "CoreMinimal.h"
#include "DataGrid.h"
#include "Components/ActorComponent.h"
#include "DataGridPreprocessor.generated.h"

USTRUCT(BlueprintType)
struct FEnvironmentalParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MinTemperature = 25.f; // Typical lower bound for coral larvae, in Celsius
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MaxTemperature = 30.f; // Typical upper bound for coral larvae, in Celsius
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MeanSalinity = 35.f; // Standard ocean salinity in PSU (Practical Salinity Units)
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Pressure = 1.f; // Pressure at the surface in ATM (Atmospheres)
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float SurfaceLight = 1.f; // Surface light intensity (normalized to 1.0 as maximum daylight)
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float LightAttenuationCoefficient = 0.085f; // PAR diffuse attenuation Kd (per metre); light = SurfaceLight*exp(-Kd*depth). Overridable via -LightAtten for the E2 light-perturbation scenario (V2C).
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float DepthForBlueDominance = 10.f; // Depth in meters where blue light becomes dominant
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float DepthThresholdFrequency = 10.f; // Depth in meters at which low frequency sounds dominate
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float LowFrequency = 0.1f; // Low frequency sound value (could represent decibels or normalized value)
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float HighFrequency = 0.5f; // High frequency sound value (could represent decibels or normalized value)
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float SoundSurfaceIntensity = 0.1f; // Sound intensity at the surface
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float SoundDeepWaterIntensity = 0.05f; // Reduced sound intensity in deeper water
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bCurrentEnabled = false; // Assuming no current by default
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float CurrentSpeedCmPerStep = 0.1f; // Simplified current displacement at maximum force
};

static const FString TileTag = TEXT("LimestoneTile");
static const FString SoundSourceTag = TEXT("SoundSource");

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class CORAL_LARVAE_ABM_API UDataGridPreprocessor : public UActorComponent
{
	GENERATED_BODY()

public:	
	UDataGridPreprocessor() {}

	static void PrepareExperimentEnvironment(UWorld* World, UDataGrid& DataGrid, const FEnvironmentalParams& Params, float CCACover, int GradientResolution, bool bSoundEnabled = true);
	static void AdaptE2Values(UDataGrid& DataGrid, int Hour, float AttenuationCoefficient = 0.1f);
	// V3E "sound off" control: if bSoundEnabled is false, the sound/particle-motion field is left
	// at DefaultParticleMotion (no sources populated) without deleting level SoundSource actors.
	static void CalculateE3Values(UWorld* World, UDataGrid& DataGrid, bool bSoundEnabled = true);

	// R1-3 E3 acoustic sensitivity-sweep knobs. Defaults equal the original hardcoded values in
	// CalculateE3Values, so default runs are unchanged; overridable at validation via
	// -SoundSPL / -SoundFreqLow / -SoundFreqHigh to sweep source level and frequency band.
	static float SoundSourceLevelDb;   // dB re 1 uPa at 1 m   (default 153)
	static float SoundMinFrequency;    // Hz                   (default 100)
	static float SoundMaxFrequency;    // Hz                   (default 10000)

	static void PrepareDefaultEnvironment(UDataGrid& DataGrid, const FEnvironmentalParams& Params, FVector SoundSourceLocation = FVector(0.f));
	
	static void CalculateGradientDistribution(UDataGrid& DataGrid);
	static void DistributeAlteromonas(UDataGrid& DataGrid);

	static float SpawnLimestoneTiles(UWorld* World, UDataGrid& DataGrid, int TileCount, float MinTileArea, float MaxTileArea);
	static void RemoveLimestoneTiles(UWorld* World, UDataGrid& DataGrid);

	static void CreateLimestoneTile(UWorld* World, const FVector& Position, const FVector& TileSize);

private:
	static void Shuffle(TArray<FIndexedCellData>& Array);
};