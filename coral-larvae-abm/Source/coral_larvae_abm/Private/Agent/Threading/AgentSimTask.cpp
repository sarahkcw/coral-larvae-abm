#include "Agent/Threading/AgentSimTask.h"

#include "DataGrid/DataGridUtils.h"

void FAgentSimTask::SetAgentStatus(const FLarvaAgentStatus& NewStatus)
{
	FScopeLock Lock(&AgentStatusCriticalSection);
	AgentStatus = NewStatus;
}

FLarvaAgentStatus FAgentSimTask::GetAgentStatus()
{
	FScopeLock Lock(&AgentStatusCriticalSection);
	return AgentStatus;
}

void FAgentSimTask::SetUpdateDelay(float NewDelay)
{
	FScopeLock Lock(&UpdateDelayCriticalSection);
	UpdateDelay = NewDelay;
}

float FAgentSimTask::GetUpdateDelay()
{
	FScopeLock Lock(&UpdateDelayCriticalSection);
	return UpdateDelay;
}

bool FAgentSimTask::GetShouldTerminate()
{
	FScopeLock Lock(&ShouldTerminateCriticalSection);
	return ShouldTerminate;
}

void FAgentSimTask::SetShouldTerminate(const bool bTerminate)
{
	FScopeLock Lock(&ShouldTerminateCriticalSection);
	ShouldTerminate = bTerminate;
}

uint32 FAgentSimTask::Run()
{
	while (!GetShouldTerminate())
	{
		int CurrentGenerationStep = 0;
		while (CurrentGenerationStep < MaxSimSteps && !GetShouldTerminate() && !Agent->IsSettled())
		{
			const auto CurrentTransform = GetAgentStatus().Transform;
			const auto LastTransform = GetAgentStatus().LastTransform;
			auto NewAgentState = Agent->RunAgentSimStep(CurrentGenerationStep++, CurrentTransform, LastTransform);
			FVector Current = FVector::Zero();
			
            if(UDataGridUtils::IsInBounds(Agent->GetGrid()->GetDataConfig(), NewAgentState.Transform.GetLocation()))
            {
                auto Cell = Agent->GetGrid()->GetCellAtPoint(NewAgentState.Transform.GetLocation()); 
                const float CurrentForce = FMath::Max(Cell.Data.WaterData.CurrentForce, 0.f);
                Current = Cell.Data.WaterData.Current.GetSafeNormal() * CurrentForce;
            }
			
			NewAgentState.LastTransform = CurrentTransform;
            // Passive advection (current) + age-gated downward geotactic bias, both applied
            // independent of the NN action, then clamped back into the tank.
            const FVector GeotacticBias = Agent->GetGeotacticBias(CurrentGenerationStep);
            NewAgentState.Transform.SetLocation(Agent->ConstrainToAquariumBounds(NewAgentState.Transform.GetLocation() + Current + GeotacticBias));
			SetAgentStatus(NewAgentState);
			if (NewAgentState.bSettled) break;
			Agent->AdaptEnergy(LastTransform, CurrentTransform);
			
			FPlatformProcess::Sleep(GetUpdateDelay());
		}

		Agent->EvaluateLarvaPerformance(GetAgentStatus());
		SetShouldTerminate(true);
	}

	return 0;
}

void FAgentSimTask::Stop()
{
	SetShouldTerminate(true);
}

float FAgentSimTask::RunGenerationSync()
{
	// Mirror of Run()'s inner step loop, minus the locks / sleep / ShouldTerminate (this runs on a
	// pooled worker inside a ParallelFor, single-threaded per agent). AgentStatus is this task's own
	// member, so direct access is safe and needs no critical section.
	int CurrentGenerationStep = 0;
	while (CurrentGenerationStep < MaxSimSteps && !Agent->IsSettled())
	{
		const FTransform CurrentTransform = AgentStatus.Transform;
		const FTransform LastTransform = AgentStatus.LastTransform;
		FLarvaAgentStatus NewAgentState = Agent->RunAgentSimStep(CurrentGenerationStep++, CurrentTransform, LastTransform);

		FVector Current = FVector::Zero();
		if (UDataGridUtils::IsInBounds(Agent->GetGrid()->GetDataConfig(), NewAgentState.Transform.GetLocation()))
		{
			auto Cell = Agent->GetGrid()->GetCellAtPoint(NewAgentState.Transform.GetLocation());
			const float CurrentForce = FMath::Max(Cell.Data.WaterData.CurrentForce, 0.f);
			Current = Cell.Data.WaterData.Current.GetSafeNormal() * CurrentForce;
		}

		NewAgentState.LastTransform = CurrentTransform;
		const FVector GeotacticBias = Agent->GetGeotacticBias(CurrentGenerationStep);
		NewAgentState.Transform.SetLocation(Agent->ConstrainToAquariumBounds(NewAgentState.Transform.GetLocation() + Current + GeotacticBias));
		AgentStatus = NewAgentState;
		if (NewAgentState.bSettled)
			break;
		Agent->AdaptEnergy(LastTransform, CurrentTransform);
	}

	return Agent->ComputeFitness(AgentStatus);
}
