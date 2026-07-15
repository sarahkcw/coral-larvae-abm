#pragma once
#include "CoreMinimal.h"
#include "BaseActionComponent.h"
#include "DataGrid/DataGridUtils.h"
#include "Actions.generated.h"

UCLASS()
class UForwardForceAction : public UBaseActionComponent
{
	GENERATED_BODY()
public:
	virtual FActionResult ExecuteAction(const FActionUpdateParams& UpdateParams) override
	{
		auto Activation = (UpdateParams.Activation + 1) / 2.f;
		const FVector Forward = UpdateParams.ActorTransform.GetRotation().GetForwardVector();

		float StrengthFactor = UpdateParams.EnergyResources / 100.f;
		

		// Apply the computed strength factor to the normalized directional vector
		const FVector ResultantForce = Forward * (Activation * InitParams.MaxForwardStrength * StrengthFactor);
		// Return the action result with the computed translation vector
		return FActionResult::WithDeltaTranslation(ResultantForce);
	}
};

UCLASS()
class URotateYawAction : public UBaseActionComponent
{
	GENERATED_BODY()
public:
	virtual FActionResult ExecuteAction(const FActionUpdateParams& UpdateParams) override
	{
		const float YawChange = UpdateParams.Activation * InitParams.MaxRotationAnglePerStep;
		return FActionResult::WithDeltaRotation(FRotator (0, YawChange, 0)); 
	}
};

UCLASS()
class URotatePitchAction : public UBaseActionComponent
{
	GENERATED_BODY()
public:
	virtual FActionResult ExecuteAction(const FActionUpdateParams& UpdateParams) override
	{
		const float PitchChange = UpdateParams.Activation * InitParams.MaxRotationAnglePerStep;
		return FActionResult::WithDeltaRotation(FRotator (PitchChange, 0, 0));  
	}
};

UCLASS()
class UOscillatorAction : public UBaseActionComponent
{
	GENERATED_BODY()
public:
	virtual FActionResult ExecuteAction(const FActionUpdateParams& UpdateParams) override
	{
		const float NewPeriod = UpdateParams.Activation * UpdateParams.OscillatorPeriod;
		const int NewPeriodInt = FMath::RoundToInt(FMath::Clamp(NewPeriod, 1.f, 100.f));
		return FActionResult::WithOscillatorPeriod(static_cast<float>(NewPeriodInt));
	}
};

UCLASS()
class USettleReadinessAction : public UBaseActionComponent
{
	GENERATED_BODY()
public:
	virtual FActionResult ExecuteAction(const FActionUpdateParams& UpdateParams) override
	{
		if (UpdateParams.LarvalAge < InitParams.MinSettlementAge) 
			return FActionResult::WithSettlement(false);
		
		auto bSettling = false;
		const auto SettlementProbability = FMath::Clamp(UpdateParams.Activation, 0.f, 1.f);
		// Draw from the agent's own deterministic stream (thread-safe, reproducible); fall back to the
		// global RNG only if no stream was supplied. FRand() gives [0,1) like the old RandRange(0,1).
		const float Sample = UpdateParams.Rng ? UpdateParams.Rng->FRand() : FMath::FRand();
		if (Sample < SettlementProbability)
			bSettling = true;
		
		return FActionResult::WithSettlement(bSettling);
	}
};
