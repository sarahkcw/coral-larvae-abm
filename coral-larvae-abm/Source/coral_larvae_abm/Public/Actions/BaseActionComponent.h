#pragma once
#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "DataGrid/DataGridStructs.h"
#include "BaseActionComponent.generated.h"

USTRUCT()
struct FActionInitParams
{
	GENERATED_USTRUCT_BODY()
	float LarvalSize;
	float MaxForwardStrength;
	float MaxRotationAnglePerStep; 
	float MinSettlementAge;
};

USTRUCT()
struct FActionUpdateParams
{
	GENERATED_BODY()
	float Activation;
	FTransform ActorTransform;
	float LarvalAge;
	float EnergyResources;
	float LightIntensity;
	int OscillatorPeriod;
	// Per-agent deterministic RNG stream for any stochastic action (currently the settle decision).
	// Not a UPROPERTY: a transient, per-step, per-agent pointer into the owning agent's own stream.
	// Using this instead of the global FMath::Rand makes fixed-seed runs reproducible and is safe
	// under data-parallel stepping (each agent owns its stream). Null falls back to the global RNG.
	FRandomStream* Rng = nullptr;
};

USTRUCT()
struct FActionResult
{
	GENERATED_BODY()
	FVector DeltaTranslation;
	FRotator DeltaRotation;
	int OscillatorPeriod;
	bool bSettled;

	static FActionResult WithOscillatorPeriod(const int Period)
	{
		FActionResult Result = Empty();
		Result.OscillatorPeriod = Period;
		return Result;
	}
	
	static FActionResult WithDeltaTranslation(const FVector& DeltaTranslation)
	{
		FActionResult Result = Empty();
		Result.DeltaTranslation = DeltaTranslation;
		return Result;
	}

	static FActionResult WithDeltaRotation(const FRotator& DeltaRotation)
	{
		FActionResult Result = Empty();
		Result.DeltaRotation = DeltaRotation;
		return Result;
	}
	
	static FActionResult WithSettlement(const bool bSettled)
	{
		FActionResult Result = Empty();
		Result.bSettled = bSettled;
		return Result;
	}

	static FActionResult Empty()
	{
		// OscillatorPeriod 0 = "unset" sentinel (valid periods clamp to [1,100]). Lets operator+ and
		// ActivateActions tell "no SET_OSC action ran this step" (keep current period) apart from an
		// explicit new period. Previously this was 2, which silently discarded the SET_OSC output.
		return FActionResult{FVector(0), FRotator(0), 0,  false};
	}

	static FActionResult Default()
	{
		FActionResult Result;
		Result.DeltaTranslation = FVector::ZeroVector;
		Result.DeltaRotation = FRotator::ZeroRotator;
		Result.OscillatorPeriod = 0; // unset sentinel, see Empty()
		Result.bSettled = false;
		return Result;
	}

	FActionResult operator+(const FActionResult& Other) const
	{
		FActionResult Result;
		Result.DeltaTranslation = DeltaTranslation + Other.DeltaTranslation;
		Result.DeltaRotation = DeltaRotation + Other.DeltaRotation;
		Result.bSettled = bSettled || Other.bSettled;
		// Carry an explicitly-set oscillator period (non-zero) through the accumulation; only the
		// OscillatorAction sets one (dedup by class => at most one per step), the others leave it 0.
		Result.OscillatorPeriod = (Other.OscillatorPeriod != 0) ? Other.OscillatorPeriod : OscillatorPeriod;
		return Result;
	}
};

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class CORAL_LARVAE_ABM_API UBaseActionComponent : public UActorComponent
{
	GENERATED_BODY()
	
public:
	virtual FActionResult ExecuteAction(const FActionUpdateParams& UpdateParams) { return FActionResult(); }
	void InitAction(const FActionInitParams& Params) { InitParams = Params; }

protected:
	FActionInitParams InitParams;
};
