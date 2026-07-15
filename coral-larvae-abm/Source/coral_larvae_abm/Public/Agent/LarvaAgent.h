#pragma once
#include "CoreMinimal.h"
#include "AgentBrainComponent.h"
#include "GenomeFunctions.h"
#include "Actions/BaseActionComponent.h"
#include "Sensors/LarvalSensorBaseComponent.h"
#include "Components/ActorComponent.h"
#include "DataGrid/DataGrid.h"
#include "LarvaAgent.generated.h"

/*
 * This is a nice overview about how this  system will roughly work once completed:
 * https://www.youtube.com/watch?v=N3tRFayqVtk
 * This is the Github repo to the video above for reference:
 * https://github.com/davidrmiller/biosim4
*/

USTRUCT(BlueprintType)
struct FLarvaAgentStatus
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FTransform Transform = FTransform();
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FTransform LastTransform = FTransform();
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bSettled = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int OscillatorPeriod = 2;
};

// AP1: one row of per-step sensor/action/position logging, recorded into a per-agent buffer on
// the agent's own worker thread (RunAgentSimStep) and flushed to CSV on the game thread after the
// run finishes -- see ALarvaAgent::GetTrajectoryBuffer / ASimulationManager::FlushPerStepTrajectoryLog.
// Fixed-size sensor/action vectors (NUM_SENSORS / NUM_ACTIONS, see SensorActionMapping.h) so the
// row is feedable to Plotting/09_nn_cue_ablation.py's offline forward pass.
USTRUCT()
struct FLarvaTrajectoryStep
{
	GENERATED_BODY()
	int32 SimStep = 0;
	FVector Position = FVector::ZeroVector;
	TArray<float> SensorValues;
	TArray<float> ActionValues;
};

USTRUCT(BlueprintType)
struct FAgentInitParams
{
	GENERATED_USTRUCT_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector StartLocation = FVector(0.f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FRotator StartRotation = FRotator(0.f); 	
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int GenomeLength = 150;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int MaxInnerNeurons = 50;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int SettlementCompetencyAge = 2;
	// Per-larva competency-onset heterogeneity (Tay mechanism). If CompetencyAgeMax > Min, each
	// larva draws its own competency age uniformly in [Min,Max] at init, so the population becomes
	// competent (and starts sinking via geotaxis) at different times -> reproduces Tay's gradual,
	// bimodal vertical distribution instead of all larvae sinking together. Set Max beyond the run
	// length so a fraction never becomes competent within a run (stays up -> Tay's top fraction).
	// If Max <= Min the fixed SettlementCompetencyAge is used (E1/E3, homogeneous). Log the range.
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int CompetencyAgeMin = 2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int CompetencyAgeMax = 2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float LarvalVolume = 0.0314f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MaxSensingDistance = 10.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float SensingAngle = 60.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float EnergyResources = 100.f; 
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MaxForwardStrength = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MaxRotationAnglePerStep = 15.f;
	// Age/competency-gated downward geotactic bias, applied each step independent of the NN
	// output. This approximates the empirically-observed positive geotaxis of competent coral
	// larvae (Vermeij, Fogarty & Miller 2006; Acropora swim-speed bound in Okubo et al. 2023),
	// NOT passive negative buoyancy (deciliated planulae are near-neutral/slightly positive).
	// Onset uses SettlementCompetencyAge; magnitude is kept well below active swim speed so the
	// evolved controller can still override it (e.g. E3 sound-driven upward tracking).
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bEnableGeotacticBias = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float GeotacticDownwardSpeedCmPerStep = 0.05f;
	UPROPERTY() bool bUseE2Fitness = false;
	UPROPERTY() bool bUseE3Fitness = false;
	// Reviewer R1-5 sensory-noise robustness: additive Gaussian noise on every sensor reading before
	// the controller sees it, as a fraction of the [0,1] sensor range (0 = off). Applied at validation
	// on already-trained controllers -> graceful-degradation curve. Uses the per-agent RNG stream.
	UPROPERTY() float SensorNoiseStdDev = 0.f;
	// Reviewer R1-3 simpler-controller baseline: bypass the evolved neural net with a fixed reactive
	// rule (gradient taxis on the directional cue sensors + settle when the scalar cue exceeds a
	// threshold). Answers "is the evolved NN necessary, or does a trivial reactive policy suffice?".
	UPROPERTY() bool bReactiveRule = false;
	UPROPERTY() float ReactiveSettleThreshold = 0.5f;
	// When false, skip building the per-agent editor-inspection strings (GenomeString/DebugBrain/
	// DebugGenome) in InitBrain -- pure overhead in unattended batch runs. Set false for batch.
	UPROPERTY() bool bComputeDebugStrings = true;
	// When true, the agent READS the shared environment grid directly instead of holding its own deep
	// copy (saves 1 full-grid copy per agent per generation + population-fold memory). ONLY safe when
	// the grid is never written during stepping. The manager sets this true exactly when the diurnal
	// light cycle is off (the sole mid-run grid mutation path); with diurnal on it stays false and each
	// agent keeps its private copy, so there is no read/write race by construction. Default false (safe).
	UPROPERTY() bool bShareEnvironmentGrid = false;
	// AP1: gated per-step sensor/action/position logging, off by default for perf. See
	// FLarvaTrajectoryStep. Set from ASimulationManager::bLogPerStepTrajectory.
	UPROPERTY() bool bLogPerStepTrajectory = false;
	// AP1: log only every Nth sim step (1 = every step). Keeps -Traj sweeps over many genome sets
	// from exploding on disk while retaining enough time resolution for the depth/time-course.
	UPROPERTY() int32 TrajectoryLogStride = 1;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSimulationFinishedEvent, float, Fitness, FGenome, Genome);

UCLASS(BlueprintType)
class CORAL_LARVAE_ABM_API ALarvaAgent final: public AActor
{
	GENERATED_BODY()

public:	
	ALarvaAgent();
	friend class FAgentSimTask;
	UPROPERTY() FSimulationFinishedEvent OnSimulationFinishedEvent;

	// Debugging and inspection
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Coral Larva") FString GenomeString;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Coral Larva") FString DebugBrain = "";
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Coral Larva") FString DebugGenome = "";
	UPROPERTY(VisibleAnywhere, Category="Coral Larva") int Age = 0;
	UPROPERTY(VisibleAnywhere, Category="Coral Larva") int SettlementTime = 0; 
	
	UPROPERTY(BlueprintReadOnly) FString NeuralNetString;

	UFUNCTION() void InitAgent(const FAgentInitParams& AgentInitParams, const UDataGrid* NewDataGrid);
	UFUNCTION() void UpdateGrid(const UDataGrid* NewDataGrid);

	// The environment grid this agent reads: the shared organizer grid when bShareEnvironmentGrid is
	// set (read-only during stepping), otherwise the agent's own copy. All simulation reads go through
	// this so the two modes are interchangeable. Returns non-const for the (non-mutating) read API.
	UDataGrid* GetGrid() const { return SharedDataGrid ? SharedDataGrid : DataGrid; }
	
	void InitBrain(FGenome NewGenome, int MaxNumInnerNeurons);
	// Seed this agent's per-agent deterministic RNG stream (used by stochastic actions, e.g. settle).
	// Seeded per (run seed, agent index) so fixed-seed runs are reproducible regardless of the order
	// agents are stepped in (thread order / data-parallel), unlike the old shared global FMath::Rand.
	void SeedAgentRandom(int32 Seed) { AgentRandomStream.Initialize(Seed); }
	virtual void BeginPlay() override;
	virtual void BeginDestroy() override;

	// Fitness relevant functions
	int GetBoundaryContactCount() const { return BoundaryContactCount; }
	int GetSettlementTime() const { return IsSettled() ? SettlementTime : -1; }
	int GetCurrentAge() const { return Age; }
	FVector GetStartLocation() const { return AgentParams.StartLocation; }
	// Passive downward geotactic bias for this step (0 before competency age), see FAgentInitParams.
	FVector GetGeotacticBias(int SimStep) const;
	// R1-3 simpler-controller baseline: fixed reactive rule that replaces the evolved net. Constant
	// forward swim, steer (yaw/pitch) up the directional CCA/particle-motion cue gradient, settle when
	// the scalar cue exceeds ReactiveSettleThreshold. Returns pre-tanh action activations.
	TArray<float> ComputeReactiveActivations(const FSensorUpdateParams& SensorUpdateParams) const;
	// AP1: per-step trajectory buffer (empty unless AgentInitParams.bLogPerStepTrajectory is set).
	// Only ever written from this agent's own FAgentSimTask worker thread inside RunAgentSimStep;
	// safe to read from the game thread once that thread has terminated (i.e. after the agent's
	// OnSimulationFinishedEvent has fired / before the next InitAgent call resets it).
	const TArray<FLarvaTrajectoryStep>& GetTrajectoryBuffer() const { return TrajectoryBuffer; }
	bool IsSettled() const { return AgentStatus.bSettled; }
	void AdaptEnergy(const FTransform& LastTransform, const FTransform& CurrentTransform);
	
	void SetSettleTime(float InSettleTime) { SettlementTime = InSettleTime; }
	float FitnessTmp(FTransform Transform);
	
	
public:
	// Genome this agent is currently running (for synchronous result collection in the batch path).
	const FGenome& GetGenome() const { return Genome; }

protected:
	void EvaluateLarvaPerformance(const FLarvaAgentStatus& AgentStatus) const;
	// Pure fitness computation (no broadcast / no thread hop) — used by the batch ParallelFor path
	// and by EvaluateLarvaPerformance (interactive path). Reads only this agent's state + its grid.
	float ComputeFitness(const FLarvaAgentStatus& ResultAgentStatus) const;
	UDataGrid* GetDataGrid() const { return DataGrid; }
	FLarvaAgentStatus RunAgentSimStep(int SimStep, FTransform CurrentAgentTransform, FTransform LastActorTransform);
	
private:
	// Agent Brain
	FNeuralNet NeuralNet;
	FGenome Genome; 
	FSensorUpdateParams GetBaseUpdateSensorParams() const;
	UPROPERTY() TArray<ULarvalSensorBaseComponent*> Sensors;
	UPROPERTY() TArray<UBaseActionComponent*> Actions;
	// Per-class pools of previously-created sensor/action components, reused across generations
	// instead of destroy+recreate (the dominant per-generation setup cost). Populated in ResetAgent,
	// drained in InitSensors/InitActions. The active Sensors/Actions arrays always hold exactly the
	// current genome's set, so simulation behavior is unchanged.
	UPROPERTY() TMap<UClass*, ULarvalSensorBaseComponent*> SensorPool;
	UPROPERTY() TMap<UClass*, UBaseActionComponent*> ActionPool;
	// Sensor component looked up by sensor-type index (ESensorType), built once in InitSensors.
	// Replaces a per-connection-per-step AActor::GetComponentByClass linear scan in the NN forward
	// pass with an O(1) array index. Indexed by the raw ESensorType value; nullptr where unused.
	UPROPERTY() TArray<ULarvalSensorBaseComponent*> SensorByType;

public:
	// O(1) cached sensor lookup by ESensorType index (see SensorByType); nullptr if not present.
	ULarvalSensorBaseComponent* GetCachedSensor(int32 SensorTypeIndex) const
	{
		return SensorByType.IsValidIndex(SensorTypeIndex) ? SensorByType[SensorTypeIndex] : nullptr;
	}
private:
	
	// Note: each agent has its own instance of the data grid and brain component,
	// so we can safely access it even in a multithreaded environment
	UPROPERTY(VisibleAnywhere) UDataGrid* DataGrid;
	// Non-null when sharing the organizer's read-only grid (see bShareEnvironmentGrid); points at the
	// organizer-owned grid (kept alive by the organizer). GetGrid() returns this when set.
	UPROPERTY() UDataGrid* SharedDataGrid = nullptr;
	UPROPERTY(VisibleAnywhere) UAgentBrainComponent* AgentBrain;
	
	// Visualization stuff
	UPROPERTY() USceneComponent* Root;
	UPROPERTY(VisibleAnywhere) UStaticMeshComponent* VisualSphere;
	UPROPERTY() UMaterialInterface* BaseMaterial;
	UPROPERTY() UMaterialInstanceDynamic* DynamicMaterialInstance;

	// Init Values, passed on from external initialization, so all agents are created equal
	FAgentInitParams AgentParams;
	
	// Internal state, might change over the sim time
	FLarvaAgentStatus AgentStatus;

	// Per-agent deterministic RNG stream (see SeedAgentRandom). Owned by this agent and only ever
	// drawn from on this agent's own step path, so it needs no locking and gives reproducible,
	// thread-order-independent results.
	FRandomStream AgentRandomStream;

	// AP1: per-step trajectory buffer, only populated when AgentParams.bLogPerStepTrajectory is
	// set. Written exclusively from this agent's own worker thread in RunAgentSimStep.
	TArray<FLarvaTrajectoryStep> TrajectoryBuffer;

	// Fitness tracking variables
	float MaxCCAFound = 0.f;
	float MaxAlteromonasFound = 0.f;
	int BoundaryContactCount = 0;
	UPROPERTY() 
	TArray<AActor*> SoundSources = TArray<AActor*>();
	
	void ResetAgent();
	void InitAgentDatabaseComponent(const UDataGrid* NewDataGrid);
	float BasicDistanceFitness(const FVector& AgentLocation) const;
	float BasicGoalDistFitness(const FVector& AgentLocation) const;
	float NearestReefDistFitness(const FVector& AgentLocation) const;
	
	void CalculateReefExplorationBonus(const FVector& AgentLocation, const FVector& LastLocation);
	FVector ConstrainToAquariumBounds(const FVector& Location);
	// Clamps location to the tank and, on wall contact, removes the into-wall component of the
	// heading so the larva slides along the surface (tangential thigmotaxis) instead of pinning.
	FTransform ConstrainTransformToAquariumBounds(const FTransform& InTransform);
	float ReefGoalFitness(const FVector& AgentLocation) const;
	float ReefGoalFitnessE2(const FVector& AgentLocation) const;
	float ReefSoundFitness(const FTransform& AgentTransform) const;
	
	void InitSensors();
	void InitActions();
	void Settle();
	void CalculateEnergy(const FTransform& LastTransform, const FTransform& CurrentTransform) const;
};


