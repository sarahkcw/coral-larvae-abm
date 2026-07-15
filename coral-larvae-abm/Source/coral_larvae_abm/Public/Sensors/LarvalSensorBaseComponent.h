#pragma once
#include "CoreMinimal.h"
#include "..\DataGrid\DataGridStructs.h"
#include "LarvalSensorBaseComponent.generated.h"

USTRUCT()
// Define all needed init params for all sensors here.
// Each sensor will get this thing each iteration in the init call and is responsible to extract whatever it needs from it.
// As this data will be copied to each agents sensor every simulation step, dont put too much data into it! Otherwise memory might quickly be gone :D
// (This is mostly important for pushing a large set of the DataGrid into this as there is a lot of data stored in it)
struct FSensorInitParams
{
	GENERATED_BODY()
	float MaxSensingDistance = 0.f;
	float MaxSensingAngle = 0.f;
	FVector InitialAgentPosition;
	FRotator InitialAgentRotation;
	 
	float InitialEnergyResources = 100.f;
};

USTRUCT(BlueprintType)
struct FSensorUpdateParams
{
	GENERATED_BODY()
	FDataChunk DataChunk; // This will be precomputed by the Agent for cells in reach
	FDataConfig GlobalExperimentConfig;
	FTransform ActorTransform;
	FTransform LastActorTransform;
	int OscillationPeriod;
	float Energy;
	int Age;
	int SimulationStep;
	// Reviewer R1-5 sensory-noise robustness (validation-time). When SensorNoiseStdDev > 0 and
	// NoiseRng is set, FeedForward adds Gaussian noise (this SD, as a fraction of the [0,1] range)
	// to each sensor value before it enters the net. NoiseRng points at the agent's per-agent stream.
	float SensorNoiseStdDev = 0.f;
	FRandomStream* NoiseRng = nullptr;
};

USTRUCT()
struct FActorDirection
{
	GENERATED_BODY()
	FVector Forward;
	FVector Right;
	FVector Up;

	static FActorDirection FromTransform(const FTransform& Transform)
	{
		FActorDirection Result;
		Result.Forward = Transform.GetRotation().GetForwardVector();
		Result.Right = Transform.GetRotation().GetRightVector();
		Result.Up = Transform.GetRotation().GetUpVector();
		return Result;		
	}
};

// BaseClass for all sensors. Quite basic, all logic for getting sensor output is implemented in the sensors themselves.
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class CORAL_LARVAE_ABM_API ULarvalSensorBaseComponent : public UActorComponent
{
	GENERATED_BODY()
	
public:
	virtual float GetSensor(const FSensorUpdateParams& UpdateParams) { return 0.f; }
	
	// Init does not need to be overwritten as it always behaves the same way, storing the InitData for later use in the GetSensor function.
	virtual void InitSensor(const FSensorInitParams InitParams)
	{
		SensorParams = InitParams;
	}
protected:
	// This is a local copy of all the data passed to the sensors. Main reason is that it will be nice and easy to multi-thread later :party:
	FSensorInitParams SensorParams;
};
