#pragma once
#include "CoreMinimal.h"
#include "Agent/Threading/AgentSimTask.h"
#include "Agent/LarvaAgent.h"
#include "Components/ActorComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EvolutionManager.h"
#include "DataGrid/DataGrid.h"
#include "DataGrid/DataGridOrganizer.h"
#include "SimulationManager.generated.h"

// Which lab experiment a scene reproduces.
UENUM(BlueprintType)
enum class EExperiment : uint8
{
	E1 UMETA(DisplayName = "E1 - CCA settlement"),
	E2 UMETA(DisplayName = "E2 - Vertical distribution"),
	E3 UMETA(DisplayName = "E3 - Phonotaxis")
};

// Add an event on simulation finish
DECLARE_DYNAMIC_MULTICAST_DELEGATE_SixParams(FOnGenerationFinished, float, AvgFitness, int, Generation, float, Diversity, float, MaxFitness, int, Settlers, FString, BestGenome);

// Manages the simulation environment and simulation as a whole
UCLASS()
class CORAL_LARVAE_ABM_API  ASimulationManager : public AActor
{
	GENERATED_BODY()
	
public:
	
	ASimulationManager();

	virtual void TickActor(float DeltaTime, ELevelTick TickType, FActorTickFunction& ThisTickFunction) override;
	
	// Only one Release of agents 
	// Further implementation can include several release points
	UPROPERTY() TArray<ALarvaAgent*> Agents;
	
	// ===== Simulation =====
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation") bool bIsTraining = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation") FAgentInitParams AgentInitParams;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation")	FString DebugGenomeOverride = "";
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation")	int PopulationSize = 20;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation")	int MaxGenerations = 100;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation")	int MaxSimSteps = 50;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Simulation")	float CurrentSimStepDelay = 1.0;

	// ===== Experiment =====
	// Which lab experiment this scene reproduces. E1 = CCA settlement, E2 = vertical
	// distribution, E3 = phonotaxis. Drives the fitness function and environment build.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment") EExperiment Experiment = EExperiment::E1;
	// E1: how often (in generations) fresh CCA-covered limestone tiles are regenerated.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Experiment|E1", meta = (EditCondition = "Experiment == EExperiment::E1", EditConditionHides)) int TileDropFrequency = 1000;
	// E2: diurnal light cycle (see the E2 clock params below for why this defaults OFF).
	UPROPERTY(EditAnywhere, Category = "Experiment|E2", meta = (EditCondition = "Experiment == EExperiment::E2", EditConditionHides)) bool bEnableDiurnalLightCycle = false;
	// E3: cycle the training sound-source position across generations (robustness).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment|E3", meta = (EditCondition = "Experiment == EExperiment::E3", EditConditionHides)) bool bCycleE3SoundSourcesDuringTraining = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment|E3", meta = (EditCondition = "Experiment == EExperiment::E3 && bCycleE3SoundSourcesDuringTraining", EditConditionHides)) float E3TrainingSoundHorizontalOffset = 60.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment|E3", meta = (EditCondition = "Experiment == EExperiment::E3 && bCycleE3SoundSourcesDuringTraining", EditConditionHides)) float E3TrainingSoundVerticalOffset = 60.f;

	// ===== Evolution =====
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evolution") ESelectionStrategy SelectionStrategy = ESelectionStrategy::ELITISM;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evolution") ECrossoverStrategy CrossoverStrategy = ECrossoverStrategy::OVERLAY;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evolution") EMutationStrategy MutationStrategy = EMutationStrategy::MULTIPOINTMUTATION;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evolution") float SelectionRate = 0.01f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evolution") float RestSelectionRate = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evolution") float MutationRate = 0.002f;
	// RNG seed for the run (agent spawn, GA operators, stochastic settlement). Fixed seed = reproducible.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evolution|Randomness") bool bUseFixedRandomSeed = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evolution|Randomness", meta = (EditCondition = "bUseFixedRandomSeed")) int32 RandomSeed = 1;

	// ===== Batch =====
	// --- Batch training over a seed range (unattended) ---
	// When enabled, BeginPlay auto-starts a fresh training run for each seed in
	// [BatchSeedStart, BatchSeedEnd]; after each run finishes the manager saves
	// the seed-specific genomes and per-generation CSV into the training output
	// folder and advances to the next seed with no manual clicking or copying.
	// Set the Experiment and SelectionStrategy in the editor once; the whole seed
	// sweep then runs from a single Play. (Disables the manual Init/Run buttons.)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Batch|Training") bool bBatchSeeds = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Batch|Training", meta = (EditCondition = "bBatchSeeds")) int32 BatchSeedStart = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Batch|Training", meta = (EditCondition = "bBatchSeeds")) int32 BatchSeedEnd = 30;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Batch|Training", meta = (EditCondition = "bBatchSeeds")) bool bBatchSkipExisting = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Batch|Training", meta = (EditCondition = "bBatchSeeds")) bool bBatchQuitWhenDone = false;
	// Output root for batch genomes + per-generation CSVs. If empty, defaults to
	// <ProjectContent>/Evolution/Training. Set to the repo Training folder to write
	// directly where the R analysis reads (e.g. .../Repo_New/Training).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Batch|Training") FString TrainingOutputRoot = "";
	// Optional method-label override for output subfolder/filenames; if empty the
	// label is derived from SelectionStrategy (Elitism/Truncation/SUS/...).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Batch|Training") FString BatchMethodOverride = "";

	// --- Batch validation over a genome-set x RNG-seed sweep (unattended) ---
	// When enabled, BeginPlay auto-starts a nested sweep: for each ValidationGenomeSeed in
	// [BatchValGenomeSeedStart, BatchValGenomeSeedEnd], run one validation sim per RandomSeed in
	// [BatchValRngSeedStart, BatchValRngSeedEnd]. RandomSeed and ValidationGenomeSeed are always
	// set from separate ranges (see docs/findings.md seed-confound entry) -- use disjoint ranges
	// (e.g. genome seeds 1-30, RNG seeds 101-130) so a genome seed can never equal the RNG seed.
	// Only runs when bIsTraining is false; does not interact with bBatchSeeds.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Batch|Validation") bool bBatchValidation = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Batch|Validation", meta = (EditCondition = "bBatchValidation")) int32 BatchValGenomeSeedStart = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Batch|Validation", meta = (EditCondition = "bBatchValidation")) int32 BatchValGenomeSeedEnd = 30;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Batch|Validation", meta = (EditCondition = "bBatchValidation")) int32 BatchValRngSeedStart = 101;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Batch|Validation", meta = (EditCondition = "bBatchValidation")) int32 BatchValRngSeedEnd = 130;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Batch|Validation", meta = (EditCondition = "bBatchValidation")) bool bBatchValSkipExisting = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Batch|Validation", meta = (EditCondition = "bBatchValidation")) bool bBatchValQuitWhenDone = false;

	// ===== Validation =====
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Validation") FString ValidationScenarioLabel = "V?";
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Validation") int32 ValidationGenomeSeed = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Validation") FString ValidationGenomeSourceLabel = "best_genomes.txt";
	// AP1: per-step sensor/action + position trajectory logging for validation runs. Off by
	// default (perf). See FLarvaTrajectoryStep / ALarvaAgent::GetTrajectoryBuffer.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Validation") bool bLogPerStepTrajectory = false;

	// Start each validation battery from an empty log instead of appending forever. When true,
	// BeginPlay deletes the shared validation logs (positions/results/environment) before this run
	// writes. Tick it for the FIRST scene of a fresh battery; leave OFF for the following scenes
	// (e.g. E2/E3) so their rows accumulate into the same battery file instead of wiping the earlier
	// ones. Default OFF -> never clears on its own. Ignored during training.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Validation") bool bClearValidationLogsOnStart = false;

	// Background run results: not editor-exposed, but BlueprintReadOnly so the on-scene
	// readout widget + data export can read them.
	UPROPERTY(BlueprintReadOnly) int LastTotalSettlers = 0;
	UPROPERTY(BlueprintReadOnly) int LastCorrectSettlers = 0;
	UPROPERTY(BlueprintReadOnly) int LastBoundaryContacts = 0;

	UFUNCTION(BlueprintCallable, Category = "Controls")
	void InitFirstGeneration();
	
	void InitForNextGeneration(const TArray<FGenome>& CrossingGenomes);
	void InitForNextGenerationWithNewLimestoneTiles(const TArray<FGenome>& CrossingGenomes);

	/**
	 * Starts the simulation
	 * Triggered the first time through ui, after the first generation will be triggered through code
	 */
	UFUNCTION(BlueprintCallable, Category = "Controls")
	void StartNextGeneration();
	
	UFUNCTION(BlueprintCallable)
	void UpdateSimStepDelay(float NewSimStepDelay);
	
	UFUNCTION(BlueprintCallable) void ResumeGeneration();
	UFUNCTION(BlueprintCallable) void PauseGeneration();
	UFUNCTION(BlueprintCallable) void UiStopGeneration();
	
	UPROPERTY(BlueprintAssignable) FOnGenerationFinished OnGenerationFinished;

protected:
	virtual void BeginPlay() override;
	virtual void BeginDestroy() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	
	UPROPERTY() ADataGridOrganizer* DataGridOrganizer = nullptr;
	
	TArray<FGenomeFitnessPair> GenomeFitnessPairs;	
	//FCriticalSection FinishGenerationCriticalSection;
	
	TArray<FAgentSimTask*> AgentTasks;
	TArray<FRunnableThread*> AgentThreads;
	
	void SpawnAgents(const FVector& SpawnLocation);
	void InitAgents(const TArray<FGenome>& CrossingGenomes);
	void OnLimestoneTileDelayCompleted(const TArray<FGenome>& CrossingGenomes);
	void InitFirstGenForTraining();
	void InitFirstGenForValidationSim();
	FString ResolveValidationGenomeFilePath() const;
	void SaveGenomesForValidation();
	void AnalyzeValidationSim();
	void HandleE2Time();
	void CacheE3SoundSourceOffsets();
	void UpdateE3SoundSourcesForTraining();
	FVector GetE3TrainingSoundCenterForGeneration() const;
	
	UFUNCTION() void ReportAgentGenerationResult(const float Fitness, const FGenome Genome);
	void FinishGeneration();

	void CleanupSimulation();
	void StopGeneration();

	ADataGridOrganizer* FindDataGridOrganizer() const;
	FGenome GetNewGenomeForAgent(const TArray<FGenome>& CrossingGenomes, int CallCount = 0) const;
	FTransform GetRandomAgentSpawnTransform() const;
	
	int CurrentGenerationStep = 0;
	int GenerationNumber = 0;
	int NumFinishedAgents = 0;

	// Headless benchmark / automation support (see ApplyBenchmarkCommandLineOverrides). bBenchmarkMode
	// is set when the run was launched with -BenchTrain; the timing fields measure wall-clock per
	// generation and per run for perf comparisons and are logged for any batch-training run.
	void ApplyBenchmarkCommandLineOverrides();
	bool bBenchmarkMode = false;
	// Debug/verification toggle (-BenchNoShare): force per-agent grid copies even when sharing would
	// be safe, so a share-on vs share-off A/B run can confirm grid-sharing is bit-identical to copying.
	bool bDisableGridShare = false;

	// Sound-source position override (-SoundPos=X,Y,Z): moves the map's SoundSource-tagged actor(s)
	// to a given world location in BeginPlay (before the sound field is built), so the moved-source
	// phonotaxis-tracking scenarios (E3 V3B/V3C) can be driven headless without per-scenario map edits.
	void ApplySoundSourceOverride();
	bool bSoundSourceOverride = false;
	FVector SoundSourceOverridePos = FVector::ZeroVector;

	// Scenario environment overrides applied to the DataGridOrganizer in BeginPlay (after it is
	// resolved, before the field is built) so validation scenarios that live on the organizer --
	// no-sound control (V3E), current-on (…D), and E1's Sampayo CCA cover / tile-area range -- can be
	// driven headless from the command line. Each flag guards whether the paired value was supplied.
	void ApplyEnvironmentOverrides();
	bool bOvSound = false;        bool OvSoundEnabled = true;
	bool bOvCurrent = false;      bool OvCurrentEnabled = false;
	bool bOvCurrentSpeed = false; float OvCurrentSpeed = 0.1f;
	bool bOvCCA = false;          float OvCCACover = 1.0f;
	bool bOvMinTile = false;      float OvMinTileArea = 2.0f;
	bool bOvMaxTile = false;      float OvMaxTileArea = 57.0f;
	bool bOvLightAtten = false;   float OvLightAtten = 0.085f;
	bool bOvE2Atten = false;   float OvE2Atten = 0.1f;   // E2 (Tay) light Kd sweep (-Exp50Atten); default = current value
	bool bUseRandomBrainBaseline = false; // R1-3 null baseline: deploy untrained random genomes at validation
	double BenchRunStartSeconds = 0.0;
	double BenchGenStartSeconds = 0.0;

	// Batch training state + helpers
	int32 CurrentBatchSeed = 0;
	void StartBatchRun(int32 Seed);
	bool AdvanceBatchSeedOrFinish();
	bool DoesSeedOutputExist(int32 Seed) const;
	FString GetMethodLabel() const;
	FString GetTrainingOutputDir() const;
	void WriteTrainingGenerationRow(float AvgFitness, float MaxFitness, float Diversity, int32 TotalSettlers, int32 CorrectSettlers, int32 BoundaryContacts);

	// Batch validation state + helpers (genome-set x RNG-seed sweep; see "Batch Validation" UPROPERTYs)
	void StartBatchValidationRun(int32 GenomeSeed, int32 RngSeed);
	bool AdvanceBatchValidationOrFinish();
	bool FindNextPendingValidationPair(int32 StartGenomeSeed, int32 StartRngSeed, int32& OutGenomeSeed, int32& OutRngSeed) const;
	bool DoesValidationRowExist(int32 GenomeSeed, int32 RngSeed) const;

	// AP1 trajectory logging
	void FlushPerStepTrajectoryLog();

	// Deletes the shared validation logs at BeginPlay when bClearValidationLogsOnStart is set
	// (validation runs only); self-guarded, safe to call unconditionally.
	void MaybeClearValidationLogs();
	
	// E2 (vertical distribution) params
	float Hour = 12.0f; // Start at noon //
	float TimeStep = 1.0f;
	const int TicksPerHour = 1200;
	// The diurnal-light toggle (bEnableDiurnalLightCycle) is declared under Experiment|E2 above.
	// It defaults OFF: the cycle was driven by wall-clock game ticks (TicksPerHour) while agents
	// step on worker threads, so the light phase a larva experienced depended on sim SPEED
	// (CurrentSimStepDelay) — non-reproducible and a train/validation mismatch. Static noon light
	// is deterministic and a clean depth cue; light is a secondary cue for E2 anyway (geotaxis is
	// the primary vertical driver). Enable only if diurnal dynamics are driven off SimStep.
	int TickCount = 0;
	TArray<FGenome> TrainedBrains;
	FString LastLoadedValidationGenomePath = TEXT("best_genomes.txt");
	TArray<FVector> CachedE3SoundSourceOffsets;
	bool bCachedE3SoundSourceOffsets = false;
	
	bool bClockRunning = false;
	float CountdownTime = 15.f; 
	float StartTime = 0.f;
	void StartClock();
	void StopClock();
	float GetRemainingTime() const;
	FString FormatFloatWithComma(float Value);
	void InitializeRandomSeed();
	void ResetE2Clock();
	FString GetExperimentLabel() const;
	FString GetGenomeArchiveFilePath() const;
};



