#pragma once
#include "CoreMinimal.h"
#include "GenomeFunctions.h"
#include "LarvaAgent.h"
#include "SensorActionMapping.h"
#include "Actions/BaseActionComponent.h"
#include "Sensors/LarvalSensorBaseComponent.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "NeuralNetFunctions.generated.h"

struct FNeuralNet;
UCLASS()
class CORAL_LARVAE_ABM_API UNeuralNetFunctions final : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Exposed (not just used internally by FeedForward) so callers can sample the full sensor
	// vector for logging (see AP1 / RunAgentSimStep) without duplicating the per-sensor-component
	// lookup logic.
	static float GetSensorFromAgent(const ALarvaAgent* Agent, ESensorType Sensor, const FSensorUpdateParams& SensorUpdateParams);
	static TArray<float> FeedForward(FNeuralNet& NeuralNet, const FSensorUpdateParams& SensorUpdateParams, const ALarvaAgent* Agent);
	static FLarvaAgentStatus ActivateActions(const TArray<float>& Activations, const TArray<UBaseActionComponent*>& Actions, FActionUpdateParams ActionUpdateParams);

	UFUNCTION(BlueprintCallable, Category = "Neural Net")
	static TArray<TSubclassOf<ULarvalSensorBaseComponent>> GetSensorClasses(FNeuralNet NeuralNet);
	UFUNCTION(BlueprintCallable, Category = "Neural Net")
	static TArray<TSubclassOf<UActorComponent>> GetActionClasses(FNeuralNet NeuralNet);
};
