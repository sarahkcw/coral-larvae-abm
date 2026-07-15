#include "Agent/NeuralNetFunctions.h"
#include <cmath>
#include "GenomeAnalysisFunctions.h"

float UNeuralNetFunctions::GetSensorFromAgent(const ALarvaAgent* Agent, const ESensorType Sensor,
                                              const FSensorUpdateParams& SensorUpdateParams)
{
	// O(1) cached lookup (built in ALarvaAgent::InitSensors) instead of the former per-call
	// AActor::GetComponentByClass linear component scan.
	if (ULarvalSensorBaseComponent* SensorComponent = Agent->GetCachedSensor(static_cast<int32>(Sensor)))
	{
		return SensorComponent->GetSensor(SensorUpdateParams);
	}
	return 0.0f;
}

TArray<float> UNeuralNetFunctions::FeedForward(FNeuralNet& NeuralNet, const FSensorUpdateParams& SensorUpdateParams, const ALarvaAgent* Agent)
{
	TArray<float> ActionLevels;
	ActionLevels.Init(0.0f, EActionType::NUM_ACTIONS);
    
	TArray<float> NeuronAccumulators;
	NeuronAccumulators.Init(0.0f, NeuralNet.Neurons.Num() + 1);
        
	for (FGene& Connection : NeuralNet.Connections) {
		float SensorResult;
		int SourceIdx = Connection.SourceIdx;
		if (Connection.bSourceType == GSensor)
		{
			SensorResult = GetSensorFromAgent(Agent, static_cast<ESensorType>(SourceIdx), SensorUpdateParams);
			// R1-5 sensory-noise robustness: additive Gaussian noise (fraction of the [0,1] range) on the
			// sensed value before it enters the net. Box-Muller from the agent's per-agent stream, clamped.
			if (SensorUpdateParams.SensorNoiseStdDev > 0.f && SensorUpdateParams.NoiseRng)
			{
				const float U1 = FMath::Max(SensorUpdateParams.NoiseRng->GetFraction(), 1e-6f);
				const float U2 = SensorUpdateParams.NoiseRng->GetFraction();
				const float Gauss = FMath::Sqrt(-2.f * FMath::Loge(U1)) * FMath::Cos(2.f * PI * U2);
				SensorResult = FMath::Clamp(SensorResult + SensorUpdateParams.SensorNoiseStdDev * Gauss, 0.f, 1.f);
			}
		}
		else
			SensorResult = NeuralNet.Neurons[SourceIdx].Output;

		int TargetIdx = Connection.TargetIdx;
		if (Connection.bTargetType == GAction)
			ActionLevels[TargetIdx] += SensorResult * Connection.WeightAsFloat();
		else
			NeuronAccumulators[TargetIdx] += SensorResult * Connection.WeightAsFloat();
	}

	for (auto NeuronIdx = 0; NeuronIdx < NeuralNet.Neurons.Num(); ++NeuronIdx) {
		if (NeuralNet.Neurons[NeuronIdx].bDriven) {
			NeuralNet.Neurons[NeuronIdx].Output = std::tanh(NeuronAccumulators[NeuronIdx]);
		}
	}
	return ActionLevels;
}

FLarvaAgentStatus UNeuralNetFunctions::ActivateActions(const TArray<float>& Activations, const TArray<UBaseActionComponent*>& Actions,
	FActionUpdateParams ActionUpdateParams)
{
	assert(Activations.Num() == Actions.Num());
	FActionResult Result = FActionResult::Default();

	for (auto i = 0; i < Actions.Num(); i++)
	{
		// Each action has its own activation factor, that need to be converted in a range of -1 to 1
		// Some actions might convert the activation into a positive range, we don't care about that here
		ActionUpdateParams.Activation = std::tanh(Activations[i]); 

		auto ActionResult = Actions[i]->ExecuteAction(ActionUpdateParams);
		Result = Result + ActionResult;
	}

	FRotator CurrentRotation = ActionUpdateParams.ActorTransform.GetRotation().Rotator();
	FRotator NewRotation = CurrentRotation + Result.DeltaRotation;
	const FVector ForwardDirection = FQuat(NewRotation).GetForwardVector().GetSafeNormal();
	const float ForwardDistance = FVector::DotProduct(
		Result.DeltaTranslation,
		ActionUpdateParams.ActorTransform.GetRotation().GetForwardVector().GetSafeNormal());
	const FVector NextAgentLocation = ActionUpdateParams.ActorTransform.GetLocation() + ForwardDirection * ForwardDistance;

	FTransform SimStepResultingTransform = FTransform();
	SimStepResultingTransform.SetLocation(NextAgentLocation);
	SimStepResultingTransform.SetRotation(FQuat(NewRotation));

	FLarvaAgentStatus AgentStatus;
	AgentStatus.Transform = SimStepResultingTransform;
	AgentStatus.bSettled = Result.bSettled;
	// Propagate the oscillator period: use the value a SET_OSC action produced this step (non-zero),
	// otherwise keep the agent's current period. Previously this was never set, so the SET_OSC action
	// had no effect and the oscillator sensor was stuck at the default period.
	AgentStatus.OscillatorPeriod = (Result.OscillatorPeriod != 0) ? Result.OscillatorPeriod : ActionUpdateParams.OscillatorPeriod;
	return AgentStatus;
}

TArray<TSubclassOf<ULarvalSensorBaseComponent>> UNeuralNetFunctions::GetSensorClasses(FNeuralNet NeuralNet)
{
	TArray<TSubclassOf<ULarvalSensorBaseComponent>> SensorClasses;
	for (auto& Connection : NeuralNet.Connections)
	{
		if (Connection.bSourceType == GSensor)
		{
			TSubclassOf<ULarvalSensorBaseComponent> SensorClass = GetSensorClassFromEnum(
				static_cast<ESensorType>(Connection.SourceIdx));
			SensorClasses.AddUnique(SensorClass);
		}
	}
	return SensorClasses;
}

TArray<TSubclassOf<UActorComponent>> UNeuralNetFunctions::GetActionClasses(FNeuralNet NeuralNet)
{
	TArray<TSubclassOf<UActorComponent>> ActionClasses;
	for (auto& Connection : NeuralNet.Connections)
	{
		if (Connection.bTargetType == GAction)
		{
			TSubclassOf<UActorComponent> ActionClass = GetActionClassFromEnum(
				static_cast<EActionType>(Connection.TargetIdx));
			ActionClasses.AddUnique(ActionClass);
		}
	}
	return ActionClasses;
}
