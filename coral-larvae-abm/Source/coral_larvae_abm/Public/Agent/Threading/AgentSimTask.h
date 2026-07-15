#pragma once
#include "CoreMinimal.h"
#include "Agent/LarvaAgent.h"
#include "HAL/Runnable.h"
#include "HAL/CriticalSection.h"

class FAgentSimTask final : public FRunnable
{
	float UpdateDelay = 0.5f;
	FCriticalSection UpdateDelayCriticalSection;
	bool ShouldTerminate;
	FCriticalSection ShouldTerminateCriticalSection;
	
	FLarvaAgentStatus AgentStatus;
	FCriticalSection AgentStatusCriticalSection;
	
	int MaxSimSteps;
	
public:
	
	FAgentSimTask(ALarvaAgent* InAgent, int InMaxSimSteps, const FTransform& InitTransform, float InUpdateDelay)
		: UpdateDelay(InUpdateDelay), ShouldTerminate(false), MaxSimSteps(InMaxSimSteps), Agent(InAgent)
	{
		auto InitStatus = FLarvaAgentStatus();
		InitStatus.Transform = InitTransform;
		SetAgentStatus(InitStatus);
	}
	
	ALarvaAgent* Agent;
	
	// Thread-Save
	void SetAgentStatus(const FLarvaAgentStatus& NewStatus);
	FLarvaAgentStatus GetAgentStatus();
	
	void SetUpdateDelay(float NewDelay);
	float GetUpdateDelay();
	bool GetShouldTerminate();
	void SetShouldTerminate(bool bTerminate);
	
	virtual bool Init() override { return true; }
	virtual uint32 Run() override;

	// Synchronous, lock-free, single-threaded run of one full generation for this agent, returning its
	// fitness. Used by the batch ParallelFor path instead of Run() on a dedicated OS thread: it does
	// the identical per-step logic as Run() but without the FScopeLock getters, the FPlatformProcess
	// sleep, or the ShouldTerminate flag, and computes fitness directly (no game-thread AsyncTask).
	// This removes the per-agent-per-generation thread creation that leaked task-graph/lock-free
	// resources and crashed long runs. Behaviour is identical to Run() (batch delay is 0).
	float RunGenerationSync();
	virtual void Stop() override;
	virtual void Exit() override { }
};
