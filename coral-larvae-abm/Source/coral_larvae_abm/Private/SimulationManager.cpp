#include "SimulationManager.h"
#include "EvolutionManager.h"
#include "Agent/Threading/AgentSimTask.h"
#include "DataGrid/DataGridOrganizer.h"
#include "DataGrid/DataGridUtils.h"
#include "Experiments/ResultAnalysisFunctions.h"
#include "GenomeAnalysisFunctions.h"
#include "Agent/GenomeFunctions.h"
#include "SensorActionMapping.h"
#include "Kismet/GameplayStatics.h"
#include "Components/SceneComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformTime.h"
#include "Async/ParallelFor.h"

ASimulationManager::ASimulationManager() 
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.TickInterval = 0.03f;

	Agents = TArray<ALarvaAgent*>();
	TrainedBrains = TArray<FGenome>();
}

void ASimulationManager::BeginPlay()
{
	Super::BeginPlay();
	ApplyBenchmarkCommandLineOverrides();
	MaybeClearValidationLogs();
	InitializeRandomSeed();
	// Set up the simulation environment for eventual simulation
	DataGridOrganizer = FindDataGridOrganizer();
	ApplyEnvironmentOverrides();
	UpdateSimStepDelay(CurrentSimStepDelay);
	SpawnAgents(AgentInitParams.StartLocation);
	ApplySoundSourceOverride();

	if(Experiment == EExperiment::E1)
	{
		DataGridOrganizer->GenerateNewLimestoneTiles();
		FTimerHandle InitialTileUpdateTimerHandle;
		GetWorld()->GetTimerManager().SetTimer(
			InitialTileUpdateTimerHandle,
			DataGridOrganizer,
			&ADataGridOrganizer::GenerateNewDataGrid,
			0.05f,
			false);
	}

	// Batch training: auto-start the first (non-skipped) seed run without a UI click.
	if (bBatchSeeds && bIsTraining)
	{
		int32 FirstSeed = BatchSeedStart;
		while (FirstSeed <= BatchSeedEnd && bBatchSkipExisting && DoesSeedOutputExist(FirstSeed))
		{
			UE_LOG(LogTemp, Warning, TEXT("Batch: skipping existing seed %d (%s/%s)"), FirstSeed, *GetExperimentLabel(), *GetMethodLabel());
			FirstSeed++;
		}
		if (FirstSeed <= BatchSeedEnd)
		{
			const int32 SeedToRun = FirstSeed;
			FTimerHandle BatchStartTimerHandle;
			GetWorld()->GetTimerManager().SetTimer(
				BatchStartTimerHandle,
				[this, SeedToRun]() { StartBatchRun(SeedToRun); },
				0.2f,
				false);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Batch: nothing to do, all seeds %d-%d already present."), BatchSeedStart, BatchSeedEnd);
		}
	}

	// Batch validation: auto-start the first (non-skipped) genome-seed/RNG-seed pair without a UI
	// click. Only applies to validation runs; independent of bBatchSeeds (training).
	if (bBatchValidation && !bIsTraining)
	{
		int32 FirstGenomeSeed, FirstRngSeed;
		if (FindNextPendingValidationPair(BatchValGenomeSeedStart, BatchValRngSeedStart, FirstGenomeSeed, FirstRngSeed))
		{
			FTimerHandle BatchValStartTimerHandle;
			GetWorld()->GetTimerManager().SetTimer(
				BatchValStartTimerHandle,
				[this, FirstGenomeSeed, FirstRngSeed]() { StartBatchValidationRun(FirstGenomeSeed, FirstRngSeed); },
				0.2f,
				false);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Batch validation: nothing to do, all genome seeds %d-%d x RNG seeds %d-%d already present."),
				BatchValGenomeSeedStart, BatchValGenomeSeedEnd, BatchValRngSeedStart, BatchValRngSeedEnd);
		}
	}
}

void ASimulationManager::ApplyBenchmarkCommandLineOverrides()
{
	// Headless benchmark / automation entry point. A run launched with `-BenchTrain` forces a
	// deterministic, self-terminating single-seed training run whose size is fully controlled from
	// the command line, independent of the map's serialized SimulationManager settings. Used for
	// reproducible perf benchmarking and for unattended Phase-4 runs. No effect without -BenchTrain.
	const TCHAR* Cmd = FCommandLine::Get();

	// Scenario environment knobs (applied to the DataGridOrganizer in BeginPlay). Shared by the
	// -AutoTrain and -AutoValidate paths. Floats parsed via FString+Atof (FParse's float overload is
	// unreliable). Ints: -Sound/-Current take 0/1.
	{
		int32 iv = 0; FString fs;
		if (FParse::Value(Cmd, TEXT("Sound="),        iv)) { bOvSound = true;        OvSoundEnabled = (iv != 0); }
		if (FParse::Value(Cmd, TEXT("Current="),      iv)) { bOvCurrent = true;      OvCurrentEnabled = (iv != 0); }
		if (FParse::Value(Cmd, TEXT("CurrentSpeed="), fs)) { bOvCurrentSpeed = true; OvCurrentSpeed = FCString::Atof(*fs); }
		if (FParse::Value(Cmd, TEXT("CCACover="),     fs)) { bOvCCA = true;          OvCCACover = FCString::Atof(*fs); }
		if (FParse::Value(Cmd, TEXT("MinTile="),      fs)) { bOvMinTile = true;      OvMinTileArea = FCString::Atof(*fs); }
		if (FParse::Value(Cmd, TEXT("MaxTile="),      fs)) { bOvMaxTile = true;      OvMaxTileArea = FCString::Atof(*fs); }
		if (FParse::Value(Cmd, TEXT("LightAtten="),   fs)) { bOvLightAtten = true;   OvLightAtten = FCString::Atof(*fs); }
		// R1-3 environmental sensitivity sweeps (validation-time; defaults preserve behavior, applied only when passed).
		// -Exp50Atten: E2 (Tay) light attenuation Kd (the field E2 actually uses; -LightAtten does NOT affect E2).
		// -SoundSPL / -SoundFreqLow / -SoundFreqHigh: E3 acoustic source level + frequency band.
		if (FParse::Value(Cmd, TEXT("Exp50Atten="),   fs)) { bOvE2Atten = true;   OvE2Atten = FCString::Atof(*fs); }
		if (FParse::Value(Cmd, TEXT("SoundSPL="),     fs)) UDataGridPreprocessor::SoundSourceLevelDb = FCString::Atof(*fs);
		if (FParse::Value(Cmd, TEXT("SoundFreqLow="), fs)) UDataGridPreprocessor::SoundMinFrequency  = FCString::Atof(*fs);
		if (FParse::Value(Cmd, TEXT("SoundFreqHigh="),fs)) UDataGridPreprocessor::SoundMaxFrequency  = FCString::Atof(*fs);
		// E2 (Tay) competency-onset heterogeneity + descent speed. Per-larva competency age drawn in
		// [CompMin,CompMax] (CompMax>CompMin -> heterogeneous onset -> gradual bimodal depth). GeoSpeed
		// = passive downward geotaxis cm/step once competent (dominates the descent-lag time course;
		// default 0.05 is far too slow for the 220cm column in 5000 steps). Set directly on
		// AgentInitParams here in the SHARED block so calibration sweeps apply at validation too, not
		// only training (they previously lived in the -AutoTrain-only branch and were ignored on validate).
		if (FParse::Value(Cmd, TEXT("CompMin="),  iv)) AgentInitParams.CompetencyAgeMin = FMath::Max(0, iv);
		if (FParse::Value(Cmd, TEXT("CompMax="),  iv)) AgentInitParams.CompetencyAgeMax = FMath::Max(0, iv);
		if (FParse::Value(Cmd, TEXT("GeoSpeed="), fs)) AgentInitParams.GeotacticDownwardSpeedCmPerStep = FMath::Max(0.f, FCString::Atof(*fs));
		// -TrajStride=N: with -Traj, log only every Nth step. Lets a depth/time-course sweep over many
		// genome sets stay small on disk (e.g. stride 100 over 5000 steps = 50 samples/agent vs 5000).
		if (FParse::Value(Cmd, TEXT("TrajStride="), iv)) AgentInitParams.TrajectoryLogStride = FMath::Max(1, iv);
		// Reviewer-gap baselines/robustness (validation-time; applied to already-trained controllers):
		// -SensorNoise=frac  R1-5 additive Gaussian sensor noise (fraction of [0,1] range).
		// -ReactiveRule      R1-3 replace the evolved net with a fixed reactive taxis rule.
		// -RandomBrain       R1-3 null baseline: deploy untrained random genomes instead of loading trained.
		if (FParse::Value(Cmd, TEXT("SensorNoise="), fs)) AgentInitParams.SensorNoiseStdDev = FMath::Max(0.f, FCString::Atof(*fs));
		if (FParse::Param(Cmd, TEXT("ReactiveRule")))     AgentInitParams.bReactiveRule = true;
		if (FParse::Param(Cmd, TEXT("RandomBrain")))      bUseRandomBrainBaseline = true;
	}

	// Real unattended training run (Phase-4 orchestration): a full multi-seed batch for one
	// experiment x method, fully specified from the command line so the orchestrator can drive every
	// config headless without hand-editing the map. The experiment kind (E3/E2) still comes from
	// the loaded map. Resume-safe (skips seeds whose output already exists). Distinct from -BenchTrain,
	// which is a tiny single-seed perf probe.
	if (FParse::Param(Cmd, TEXT("AutoTrain")))
	{
		UE_LOG(LogTemp, Warning, TEXT("[AUTO] raw cmdline: %s"), Cmd);
		int32 IntVal = 0;
		if (FParse::Value(Cmd, TEXT("Pop="),     IntVal))   PopulationSize = FMath::Max(1, IntVal);
		if (FParse::Value(Cmd, TEXT("Gens="),    IntVal))   MaxGenerations = FMath::Max(1, IntVal);
		if (FParse::Value(Cmd, TEXT("Steps="),   IntVal))   MaxSimSteps    = FMath::Max(1, IntVal);
		if (FParse::Value(Cmd, TEXT("Genome="),  IntVal))   AgentInitParams.GenomeLength   = FMath::Max(1, IntVal);
		if (FParse::Value(Cmd, TEXT("Neurons="), IntVal))   AgentInitParams.MaxInnerNeurons = FMath::Max(1, IntVal);
		// Competency range + descent speed (-CompMin/-CompMax/-GeoSpeed) are parsed in the shared
		// override block above so they apply to both training and validation.
		// Parse floats via FString -> Atof: FParse::Value's float overload silently yielded 0 for these
		// (would have trained with zero selection/mutation rate). Atof is robust.
		FString FloatStr;
		if (FParse::Value(Cmd, TEXT("Delay="),   FloatStr)) CurrentSimStepDelay = FMath::Max(0.f, FCString::Atof(*FloatStr));
		if (FParse::Value(Cmd, TEXT("MutRate="), FloatStr)) MutationRate = FMath::Max(0.f, FCString::Atof(*FloatStr));
		if (FParse::Value(Cmd, TEXT("SelRate="), FloatStr)) SelectionRate = FCString::Atof(*FloatStr);
		if (FParse::Value(Cmd, TEXT("RestSel="), FloatStr)) RestSelectionRate = FCString::Atof(*FloatStr);

		FString CrossStr;
		if (FParse::Value(Cmd, TEXT("Crossover="), CrossStr))
		{
			if      (CrossStr.Equals(TEXT("Overlay"),    ESearchCase::IgnoreCase)) CrossoverStrategy = ECrossoverStrategy::OVERLAY;
			else if (CrossStr.Equals(TEXT("MultiPoint"), ESearchCase::IgnoreCase)) CrossoverStrategy = ECrossoverStrategy::MULTIPOINT;
			else UE_LOG(LogTemp, Warning, TEXT("[AUTO] unknown -Crossover='%s', keeping map default"), *CrossStr);
		}

		FString MethodStr;
		if (FParse::Value(Cmd, TEXT("Method="), MethodStr))
		{
			if      (MethodStr.Equals(TEXT("Elitism"),    ESearchCase::IgnoreCase)) SelectionStrategy = ESelectionStrategy::ELITISM;
			else if (MethodStr.Equals(TEXT("SUS"),        ESearchCase::IgnoreCase)) SelectionStrategy = ESelectionStrategy::SUS;
			else if (MethodStr.Equals(TEXT("Truncation"), ESearchCase::IgnoreCase)) SelectionStrategy = ESelectionStrategy::TRUNCATION;
			else if (MethodStr.Equals(TEXT("Roulette"),   ESearchCase::IgnoreCase)) SelectionStrategy = ESelectionStrategy::ROULETTE;
			else if (MethodStr.Equals(TEXT("RankBased"),  ESearchCase::IgnoreCase)) SelectionStrategy = ESelectionStrategy::RANKBASED;
			else UE_LOG(LogTemp, Warning, TEXT("[AUTO] unknown -Method='%s', keeping map default"), *MethodStr);
		}

		int32 SeedStart = 1, SeedEnd = 30;
		FString SeedRange;
		if (FParse::Value(Cmd, TEXT("Seeds="), SeedRange))
		{
			FString A, B;
			if (SeedRange.Split(TEXT("-"), &A, &B)) { SeedStart = FCString::Atoi(*A); SeedEnd = FCString::Atoi(*B); }
			else { SeedStart = SeedEnd = FCString::Atoi(*SeedRange); }
		}

		FString OutRoot;
		if (FParse::Value(Cmd, TEXT("Out="), OutRoot)) TrainingOutputRoot = OutRoot;
		// Optional output-subfolder/filename label (BatchMethodOverride) so hyperparameter variants of
		// one selection method land in distinct folders (e.g. SUS_H2) instead of colliding in e3/SUS.
		FString MethodLabel;
		if (FParse::Value(Cmd, TEXT("MethodLabel="), MethodLabel)) BatchMethodOverride = MethodLabel;
		if (FParse::Param(Cmd, TEXT("NoShare"))) bDisableGridShare = true;

		bIsTraining        = true;
		bBatchSeeds        = true;
		bBatchSkipExisting = true;   // resume-safe across restarts of a long unattended sweep
		bBatchQuitWhenDone = true;
		BatchSeedStart     = SeedStart;
		BatchSeedEnd       = SeedEnd;
		bBenchmarkMode     = true;   // emit [BENCH] per-generation + per-seed timing

		UE_LOG(LogTemp, Warning,
			TEXT("[AUTO] train: Method='%s' Pop=%d Genome=%d Neurons=%d Gens=%d Steps=%d Seeds=%d-%d SelRate=%.3f RestSel=%.3f MutRate=%.4f E3=%d E2=%d Out='%s'"),
			*MethodStr, PopulationSize, AgentInitParams.GenomeLength, AgentInitParams.MaxInnerNeurons, MaxGenerations, MaxSimSteps,
			SeedStart, SeedEnd, SelectionRate, RestSelectionRate, MutationRate, (Experiment == EExperiment::E3) ? 1 : 0, (Experiment == EExperiment::E2) ? 1 : 0, *TrainingOutputRoot);
		return;
	}

	// Real unattended VALIDATION run (Phase-4 orchestration): nested genome-seed x RNG-seed sweep for
	// one experiment x scenario x trained controller set, fully CLI-specified. Drives the existing
	// batch-validation path (BeginPlay auto-starts it). Experiment kind from the loaded *-Validation
	// map. Scenario-specific environment knobs that live on the DataGridOrganizer (bSoundEnabled,
	// bCurrentEnabled, tile params) are applied later in BeginPlay after the organizer is resolved;
	// the primary scenarios (V1A/V2A/V3A) use the defaults (sound on, current off), so no override is
	// needed for the method-comparison pass.
	if (FParse::Param(Cmd, TEXT("AutoValidate")))
	{
		UE_LOG(LogTemp, Warning, TEXT("[AUTO] raw cmdline: %s"), Cmd);
		int32 IntVal = 0;
		if (FParse::Value(Cmd, TEXT("Pop="),   IntVal)) PopulationSize = FMath::Max(1, IntVal);
		if (FParse::Value(Cmd, TEXT("Steps="), IntVal)) MaxSimSteps    = FMath::Max(1, IntVal);

		FString S;
		if (FParse::Value(Cmd, TEXT("Scenario="),  S)) ValidationScenarioLabel     = S;
		if (FParse::Value(Cmd, TEXT("GenomeSrc="), S)) ValidationGenomeSourceLabel = S;

		auto ParseRange = [Cmd](const TCHAR* Key, int32& OutStart, int32& OutEnd)
		{
			FString R;
			if (FParse::Value(Cmd, Key, R))
			{
				FString A, B;
				if (R.Split(TEXT("-"), &A, &B)) { OutStart = FCString::Atoi(*A); OutEnd = FCString::Atoi(*B); }
				else { OutStart = OutEnd = FCString::Atoi(*R); }
			}
		};
		ParseRange(TEXT("GenomeSeeds="), BatchValGenomeSeedStart, BatchValGenomeSeedEnd);
		ParseRange(TEXT("RngSeeds="),    BatchValRngSeedStart,    BatchValRngSeedEnd);

		if (FParse::Param(Cmd, TEXT("Traj"))) bLogPerStepTrajectory = true;

		// Three separate keys (not a comma-list): FParse::Value for FString stops at the comma, so
		// "-SoundPos=203,5,5" only yielded "203". -SoundX/-SoundY/-SoundZ each parse cleanly.
		FString SX, SY, SZ;
		if (FParse::Value(Cmd, TEXT("SoundX="), SX) && FParse::Value(Cmd, TEXT("SoundY="), SY) && FParse::Value(Cmd, TEXT("SoundZ="), SZ))
		{
			SoundSourceOverridePos = FVector(FCString::Atof(*SX), FCString::Atof(*SY), FCString::Atof(*SZ));
			bSoundSourceOverride = true;
		}

		bIsTraining           = false;
		bBatchValidation      = true;
		bBatchValSkipExisting = true;
		bBatchValQuitWhenDone = true;
		bBenchmarkMode        = true;

		UE_LOG(LogTemp, Warning,
			TEXT("[AUTO] validate: Scenario='%s' Src='%s' GenomeSeeds=%d-%d RngSeeds=%d-%d Pop=%d Steps=%d Traj=%d"),
			*ValidationScenarioLabel, *ValidationGenomeSourceLabel, BatchValGenomeSeedStart, BatchValGenomeSeedEnd,
			BatchValRngSeedStart, BatchValRngSeedEnd, PopulationSize, MaxSimSteps, bLogPerStepTrajectory ? 1 : 0);
		return;
	}

	if (!FParse::Param(Cmd, TEXT("BenchTrain")))
		return;

	int32 IntVal = 0;
	float FloatVal = 0.f;
	if (FParse::Value(Cmd, TEXT("BenchPop="), IntVal))   PopulationSize = FMath::Max(1, IntVal);
	if (FParse::Value(Cmd, TEXT("BenchGens="), IntVal))  MaxGenerations = FMath::Max(1, IntVal);
	if (FParse::Value(Cmd, TEXT("BenchSteps="), IntVal)) MaxSimSteps    = FMath::Max(1, IntVal);
	if (FParse::Value(Cmd, TEXT("BenchDelay="), FloatVal)) CurrentSimStepDelay = FMath::Max(0.f, FloatVal);

	int32 BenchSeed = 42;
	FParse::Value(Cmd, TEXT("BenchSeed="), BenchSeed);
	bUseFixedRandomSeed = true;
	RandomSeed = BenchSeed;

	// Drive it through the existing single-seed batch path so setup/advance/quit are all reused.
	bIsTraining        = true;
	bBatchSeeds        = true;
	bBatchSkipExisting = false;
	bBatchQuitWhenDone = true;
	BatchSeedStart     = BenchSeed;
	BatchSeedEnd       = BenchSeed;

	FString OutRoot;
	if (FParse::Value(Cmd, TEXT("BenchOut="), OutRoot))
		TrainingOutputRoot = OutRoot;

	if (FParse::Param(Cmd, TEXT("BenchNoShare")))
		bDisableGridShare = true;

	bBenchmarkMode = true;
	UE_LOG(LogTemp, Warning,
		TEXT("[BENCH] overrides: Pop=%d Gens=%d Steps=%d Delay=%.3f Seed=%d E3=%d E2=%d Out='%s'"),
		PopulationSize, MaxGenerations, MaxSimSteps, CurrentSimStepDelay, RandomSeed, (Experiment == EExperiment::E3) ? 1 : 0, (Experiment == EExperiment::E2) ? 1 : 0, *TrainingOutputRoot);
}

void ASimulationManager::ApplyEnvironmentOverrides()
{
	if (!DataGridOrganizer)
		return;
	if (bOvSound)        DataGridOrganizer->bSoundEnabled        = OvSoundEnabled;
	if (bOvCurrent)      DataGridOrganizer->bCurrentEnabled      = OvCurrentEnabled;
	if (bOvCurrentSpeed) DataGridOrganizer->CurrentSpeedCmPerStep = OvCurrentSpeed;
	if (bOvCCA)          DataGridOrganizer->CCACover             = OvCCACover;
	if (bOvMinTile)      DataGridOrganizer->MinTileArea          = OvMinTileArea;
	if (bOvMaxTile)      DataGridOrganizer->MaxTileArea          = OvMaxTileArea;
	if (bOvLightAtten)   DataGridOrganizer->EnvironmentalParams.LightAttenuationCoefficient = OvLightAtten;
	if (bOvE2Atten)   DataGridOrganizer->E2AttenuationCoefficient = OvE2Atten; // E2 light Kd sweep (R1-3)
	if (bOvSound || bOvCurrent || bOvCurrentSpeed || bOvCCA || bOvMinTile || bOvMaxTile || bOvLightAtten || bOvE2Atten)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[AUTO] env overrides: Sound=%d Current=%d CurSpeed=%.3f CCACover=%.3f MinTile=%.2f MaxTile=%.2f LightAtten=%.3f"),
			DataGridOrganizer->bSoundEnabled ? 1 : 0, DataGridOrganizer->bCurrentEnabled ? 1 : 0,
			DataGridOrganizer->CurrentSpeedCmPerStep, DataGridOrganizer->CCACover,
			DataGridOrganizer->MinTileArea, DataGridOrganizer->MaxTileArea,
			DataGridOrganizer->EnvironmentalParams.LightAttenuationCoefficient);
	}
}

void ASimulationManager::ApplySoundSourceOverride()
{
	TArray<AActor*> Sources;
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("SoundSource"), Sources);

	// Always log the current source geometry + grid frame so scenario positions can be chosen.
	const FVector GridOrigin = DataGridOrganizer ? DataGridOrganizer->GetActorLocation() : FVector::ZeroVector;
	FString Cur;
	for (const AActor* S : Sources) { if (S) Cur += S->GetActorLocation().ToString() + TEXT("; "); }
	UE_LOG(LogTemp, Warning, TEXT("[AUTO] SoundSources=%d gridOrigin=%s currentPos=[%s]"),
		Sources.Num(), *GridOrigin.ToString(), *Cur);

	if (!bSoundSourceOverride)
		return;
	// SoundSource actors are placed Static in the map, so SetActorLocation is a silent no-op unless we
	// first make the root component Movable (the training-time mover does the same). Log the ACTUAL
	// resulting location (not the requested one) so a failed move is visible.
	FString Actual;
	for (AActor* S : Sources)
	{
		if (!S) continue;
		if (USceneComponent* Root = S->GetRootComponent())
			Root->SetMobility(EComponentMobility::Movable);
		S->SetActorLocation(SoundSourceOverridePos);
		Actual += S->GetActorLocation().ToString() + TEXT("; ");
	}
	UE_LOG(LogTemp, Warning, TEXT("[AUTO] requested move of %d SoundSource actor(s) to %s -> actual=[%s]"),
		Sources.Num(), *SoundSourceOverridePos.ToString(), *Actual);
}

void ASimulationManager::TickActor(float DeltaTime, ELevelTick TickType, FActorTickFunction& ThisTickFunction)
{
	// Only for visualization 
	if (AgentThreads.Num() > 0)
	{
		for (const auto AgentTask : AgentTasks)
		{
			if (AgentTask->GetShouldTerminate()) continue;
			FTransform NewTransform = AgentTask->GetAgentStatus().Transform;
			AgentTask->Agent->SetActorTransform(NewTransform);
		}
	}

	if (!bIsTraining && bClockRunning)
	{
		// Calculate the elapsed time
		float CurrentTime = GetWorld()->GetTimeSeconds();
		float ElapsedTime = CurrentTime - StartTime;

		// Check if the countdown has reached the end
		if (ElapsedTime >= CountdownTime)
		{
			StopClock();
		}
	}

	if (Experiment == EExperiment::E2)
		HandleE2Time();
	
	Super::TickActor(DeltaTime, TickType, ThisTickFunction);
}

void ASimulationManager::StartNextGeneration()
{
	// Batch/unattended runs: data-parallel generation on the engine's PERSISTENT worker pool instead
	// of one freshly-created OS thread per agent. The old thread-per-agent model created ~pop threads
	// every generation; over a long sweep that leaked task-graph/lock-free resources and hard-crashed
	// (LockFreeList exhaustion ~gen 261). Each larva is independent (own brain, own deterministic RNG
	// stream, shared READ-ONLY grid), so a ParallelFor with fitness collected in agent-index order is
	// behaviour-identical to the threaded path but allocates no per-agent thread and no per-agent
	// game-thread task. FinishGeneration is driven synchronously via ReportAgentGenerationResult; the
	// next generation is deferred through InitForNextGeneration's timer, so this does not recurse.
	if (bBatchSeeds || bBatchValidation)
	{
		for (const auto Agent : Agents)
			AgentTasks.Add(new FAgentSimTask(Agent, MaxSimSteps, Agent->GetTransform(), 0.f));

		const int32 NumAgents = AgentTasks.Num();
		TArray<float> Fitnesses;
		Fitnesses.SetNumZeroed(NumAgents);
		ParallelFor(NumAgents, [this, &Fitnesses](int32 Index)
		{
			Fitnesses[Index] = AgentTasks[Index]->RunGenerationSync();
		});

		for (int32 Index = 0; Index < NumAgents; ++Index)
			ReportAgentGenerationResult(Fitnesses[Index], Agents[Index]->GetGenome());
		return;
	}

	// Interactive path: one OS thread per agent so the editor can animate the larvae live.
	for (const auto Agent : Agents)
	{
		FAgentSimTask* AgentTask = new FAgentSimTask(Agent, MaxSimSteps, Agent->GetTransform(), CurrentSimStepDelay / 1000.0f);
		auto ThreadName = FString::Printf(TEXT("AgentThread_%s"), *Agent->GetName());
		FRunnableThread* AgentThread = FRunnableThread::Create(AgentTask, *ThreadName, 0, TPri_Normal);

		AgentTasks.Add(AgentTask);
		AgentThreads.Add(AgentThread);
	}
}

void ASimulationManager::SpawnAgents(const FVector& SpawnLocation)
{
	for (int i = 0; i < PopulationSize; i++)
	{
		ALarvaAgent* Agent = GetWorld()->SpawnActor<ALarvaAgent>(SpawnLocation, FRotator::ZeroRotator);
		Agent->OnSimulationFinishedEvent.AddDynamic(this, &ASimulationManager::ReportAgentGenerationResult);
		// Batch sweeps run unattended over thousands of generations; rendering 250 agents every
		// frame exhausts the D3D12 view-descriptor heap and forces UE's slow context-local-heap
		// fallback, so the sweep crawls/stalls (observed around seed 2). Hide the agent visuals
		// during batch training/validation — the sim runs on worker threads and is unaffected.
		if (bBatchSeeds || bBatchValidation)
			Agent->SetActorHiddenInGame(true);
		Agents.Add(Agent);
	}
}

void ASimulationManager::InitAgents(const TArray<FGenome>& CrossingGenomes)
{
	auto Idx = 0;
	for (const auto Agent : Agents)
	{
		FAgentInitParams InitParams = AgentInitParams;
		InitParams.bUseE2Fitness = (Experiment == EExperiment::E2);
		InitParams.bUseE3Fitness = (Experiment == EExperiment::E3);
		// Unattended batch runs never inspect the per-agent debug strings; skip building them.
		InitParams.bComputeDebugStrings = !(bBatchSeeds || bBatchValidation);
		// Share the read-only environment grid across agents ONLY when nothing mutates it during
		// stepping. The sole mid-run mutation is the diurnal light cycle (HandleE2Time), so sharing
		// is safe exactly when it is off; with it on, each agent keeps a private copy (no read/write race).
		InitParams.bShareEnvironmentGrid = !bEnableDiurnalLightCycle && !bDisableGridShare;
		auto AgentTransform = GetRandomAgentSpawnTransform();
		InitParams.StartLocation = AgentTransform.GetLocation();
		InitParams.StartRotation = AgentTransform.GetRotation().Rotator();
		
		Agent->InitAgent(InitParams, DataGridOrganizer->DataGrid);
		Agent->InitBrain(GetNewGenomeForAgent(CrossingGenomes, Idx), InitParams.MaxInnerNeurons);
		// Seed this agent's own deterministic RNG stream from (run seed, generation, agent index) so
		// stochastic actions (settle) are reproducible and independent of step/thread order. Includes
		// GenerationNumber so agents don't replay the identical stream every generation.
		Agent->SeedAgentRandom(HashCombine(HashCombine(GetTypeHash(RandomSeed), GetTypeHash(GenerationNumber)), GetTypeHash(Idx)));
		Agent->SetActorLocation(InitParams.StartLocation);
		Agent->SetActorRotation(InitParams.StartRotation);
		Idx++;
	}
}

void ASimulationManager::InitFirstGeneration()
{
	InitializeRandomSeed();
	if (Experiment == EExperiment::E2)
	{
		ResetE2Clock();
	}
	UpdateE3SoundSourcesForTraining();
	// Prepare the simulation environment
	DataGridOrganizer->GenerateNewDataGrid();
	if (Experiment == EExperiment::E2)
	{
		DataGridOrganizer->AdaptE2DataGrid(Hour);
	}

	if(bIsTraining)
	{
		InitFirstGenForTraining();
	}
	else
	{
		InitFirstGenForValidationSim();
	}
}

/**
 * Initializes the next environment and generation of agents
 * @param CrossingGenomes - Genomes that will be used for the next generation, can be null in which case random genomes will be generated
*/
void ASimulationManager::InitForNextGeneration(const TArray<FGenome>& CrossingGenomes)
{
	if (Experiment == EExperiment::E2)
	{
		ResetE2Clock();
	}
	UpdateE3SoundSourcesForTraining();
	// Prepare the simulation environment
	DataGridOrganizer->GenerateNewDataGrid();
	if (Experiment == EExperiment::E2)
	{
		DataGridOrganizer->AdaptE2DataGrid(Hour);
	}
	// Grid can also be reset to the last state like this: DataGridOrganizer->ResetDataGrid();
	InitAgents(CrossingGenomes);

	CurrentGenerationStep = 0;
	
	// Batch runs ALWAYS defer via the timer so each generation returns to the game-thread tick before
	// the next starts: this prevents the synchronous ParallelFor path from re-entering
	// StartNextGeneration recursively across generations, and lets the engine pump between generations.
	if (GenerationNumber % 5 == 0 || bBatchSeeds || bBatchValidation)
	{
		// Wait for one frame to let the game thread update
		FTimerHandle TimerHandle;
		GetWorld()->GetTimerManager().SetTimer(TimerHandle, this, &ASimulationManager::StartNextGeneration, 0.0001f, false);
	} else {
		StartNextGeneration();
	}
}

void ASimulationManager::InitForNextGenerationWithNewLimestoneTiles(const TArray<FGenome>& CrossingGenomes)
{
	FTimerHandle TileUpdateTimerHandle;
	DataGridOrganizer->GenerateNewLimestoneTiles();
	GetWorld()->GetTimerManager().SetTimer(
		TileUpdateTimerHandle,
		[this, CrossingGenomes]() { OnLimestoneTileDelayCompleted(CrossingGenomes); },
		0.05f,
		false);
}


void ASimulationManager::ResumeGeneration()
{
	for (const auto AgentThread : AgentThreads)
		AgentThread->Suspend(false);
}

void ASimulationManager::PauseGeneration()
{
	for (const auto AgentThread : AgentThreads)
		AgentThread->Suspend(true);
}

void ASimulationManager::UiStopGeneration()
{
	// Wat. Yea, this is not nice but :shrugg:
	CleanupSimulation();
}

void ASimulationManager::StopGeneration()
{
	for (const auto AgentTask : AgentTasks)
		AgentTask->Stop(); 
}

ADataGridOrganizer* ASimulationManager::FindDataGridOrganizer() const
{
	// Search for the DataGridOrganizer in the scene to get the template data grid
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ADataGridOrganizer::StaticClass(), FoundActors);
	check(FoundActors.Num() > 0);
	auto Organizer = Cast<ADataGridOrganizer>(FoundActors[0]);
	check(Organizer != nullptr);
	return Organizer;
}

void ASimulationManager::MaybeClearValidationLogs()
{
	// Validation runs only; opt-in via the editor flag. Lets a fresh battery start from an
	// empty log instead of appending to (and mixing with) previous runs.
	if (bIsTraining || !bClearValidationLogsOnStart)
		return;

	const FString EvolutionDir = FPaths::ProjectContentDir() / TEXT("Evolution");
	IFileManager& FileManager = IFileManager::Get();
	int32 ClearedCount = 0;
	for (const TCHAR* LogName : { TEXT("validation_positions.txt"),
								  TEXT("validation_results.txt"),
								  TEXT("validation_environment.txt") })
	{
		const FString LogFilePath = EvolutionDir / LogName;
		if (FileManager.FileExists(*LogFilePath) && FileManager.Delete(*LogFilePath, /*RequireExists*/ false))
			++ClearedCount;
	}
	UE_LOG(LogTemp, Warning,
		TEXT("bClearValidationLogsOnStart: cleared %d existing validation log(s); this battery starts fresh."),
		ClearedCount);
}

FGenome ASimulationManager::GetNewGenomeForAgent(const TArray<FGenome>& CrossingGenomes, const int CallCount) const
{
	// Note: this is a bit of a mess, because elitism requires a special case of handling genome crossing.
	if(SelectionStrategy == ESelectionStrategy::ELITISM && CrossingGenomes.Num() > 0)
	{
		const int NumElite = FMath::CeilToInt(PopulationSize * SelectionRate);
		if(CallCount < NumElite) return CrossingGenomes[CallCount];
	}
	return UEvolutionManager::GenerateNextGenGenome(CrossingGenomes, MutationRate, CrossoverStrategy, MutationStrategy, AgentInitParams.GenomeLength);
}

FTransform ASimulationManager::GetRandomAgentSpawnTransform() const
{
	auto TemplateConfig = DataGridOrganizer->GetConfigOfGrid();
	// Spawn on top of grid at middle height of cell
	float SpawnLocationZ = TemplateConfig.LocalBounds.Z + TemplateConfig.ChunkWorldOrigin.Z - TemplateConfig.CellEdgeLength / 2;
	if (Experiment == EExperiment::E3)
		SpawnLocationZ = FMath::RandRange(TemplateConfig.ChunkWorldOrigin.Z, TemplateConfig.ChunkWorldOrigin.Z + TemplateConfig.LocalBounds.Z);
	float RandomX = FMath::RandRange(TemplateConfig.ChunkWorldOrigin.X, TemplateConfig.ChunkWorldOrigin.X + TemplateConfig.LocalBounds.X);
	float RandomY = FMath::RandRange(TemplateConfig.ChunkWorldOrigin.Y, TemplateConfig.ChunkWorldOrigin.Y + TemplateConfig.LocalBounds.Y);
	FVector RandomLocation = FVector(RandomX, RandomY, SpawnLocationZ);
	FRotator RandomRotator = FRotator(FMath::RandRange(0, 360), FMath::RandRange(0, 360), 0);
	return FTransform(RandomRotator, RandomLocation);
}

void ASimulationManager::StartClock()
{
	StartTime = GetWorld()->GetTimeSeconds();
	bClockRunning = true;

	UE_LOG(LogTemp, Warning, TEXT("Clock started!"));
}

void ASimulationManager::StopClock()
{
	bClockRunning = false;
	UE_LOG(LogTemp, Warning, TEXT("Countdown finished!"));
	FinishGeneration();
}

float ASimulationManager::GetRemainingTime() const
{
	if (bClockRunning)
	{
		float ElapsedTime = GetWorld()->GetTimeSeconds() - StartTime;
		return CountdownTime - ElapsedTime;
	}

	return CountdownTime;
}

FString ASimulationManager::FormatFloatWithComma(float Value)
{
	FString FloatString = FString::SanitizeFloat(Value);
	FloatString.ReplaceInline(TEXT("."), TEXT(","), ESearchCase::CaseSensitive);
	return FloatString;
}

void ASimulationManager::InitializeRandomSeed()
{
	if (!bUseFixedRandomSeed)
		return;

	uint32 MixedSeed = static_cast<uint32>(RandomSeed) + 0x9E3779B9u;
	MixedSeed ^= MixedSeed >> 16;
	MixedSeed *= 0x85EBCA6Bu;
	MixedSeed ^= MixedSeed >> 13;
	MixedSeed *= 0xC2B2AE35u;
	MixedSeed ^= MixedSeed >> 16;

	FMath::RandInit(static_cast<int32>(MixedSeed));
	UE_LOG(LogTemp, Warning, TEXT("Using fixed random seed: %d (mixed: %u)"), RandomSeed, MixedSeed);
}

void ASimulationManager::ResetE2Clock()
{
	Hour = 12.0f;
	TickCount = 0;
}

FString ASimulationManager::GetExperimentLabel() const
{
	if (Experiment == EExperiment::E3)
	{
		return TEXT("e3");
	}

	if (Experiment == EExperiment::E2)
	{
		return TEXT("e2");
	}

	return TEXT("e1");
}

FString ASimulationManager::GetGenomeArchiveFilePath() const
{
	const FString SeedLabel = bUseFixedRandomSeed
		? FString::Printf(TEXT("%02d"), RandomSeed)
		: TEXT("rnd");

	const FString FileName = FString::Printf(
		TEXT("best_genomes_%s_seed_%s.txt"),
		*GetExperimentLabel(),
		*SeedLabel);

	// In batch mode, write genomes straight into the per-experiment/per-method
	// training folder so no manual copy is needed.
	if (bBatchSeeds)
		return GetTrainingOutputDir() / FileName;

	return FPaths::ProjectContentDir() / TEXT("Evolution") / FileName;
}

FString ASimulationManager::GetMethodLabel() const
{
	if (!BatchMethodOverride.IsEmpty())
		return BatchMethodOverride;

	switch (SelectionStrategy)
	{
	case ESelectionStrategy::ELITISM:    return TEXT("Elitism");
	case ESelectionStrategy::TRUNCATION: return TEXT("Truncation");
	case ESelectionStrategy::SUS:        return TEXT("SUS");
	case ESelectionStrategy::TOURNAMENT: return TEXT("Tournament");
	case ESelectionStrategy::ROULETTE:   return TEXT("Roulette");
	case ESelectionStrategy::RANKBASED:  return TEXT("RankBased");
	default:                             return TEXT("Unknown");
	}
}

FString ASimulationManager::GetTrainingOutputDir() const
{
	const FString Root = TrainingOutputRoot.IsEmpty()
		? (FPaths::ProjectContentDir() / TEXT("Evolution/Training"))
		: TrainingOutputRoot;
	// No trailing separator; callers append filenames with operator/.
	return Root / GetExperimentLabel() / GetMethodLabel();
}

bool ASimulationManager::DoesSeedOutputExist(int32 Seed) const
{
	const FString Path = GetTrainingOutputDir() / FString::Printf(
		TEXT("best_genomes_%s_seed_%02d.txt"), *GetExperimentLabel(), Seed);
	return FPaths::FileExists(Path);
}

void ASimulationManager::StartBatchRun(int32 Seed)
{
	CurrentBatchSeed = Seed;
	bUseFixedRandomSeed = true;
	RandomSeed = Seed;
	GenerationNumber = 0;
	BenchRunStartSeconds = FPlatformTime::Seconds();
	BenchGenStartSeconds = BenchRunStartSeconds;
	// Reset per-run transient state so each seed is an independent run.
	Hour = 12.0f;
	TickCount = 0;
	bCachedE3SoundSourceOffsets = false;

	UE_LOG(LogTemp, Warning, TEXT("Batch: starting run seed %d (%s/%s), seeds %d-%d"),
		Seed, *GetExperimentLabel(), *GetMethodLabel(), BatchSeedStart, BatchSeedEnd);

	// Launch generation 0 automatically (manual mode relies on the "Run Sim" click; batch has none,
	// so without this the run would idle at generation 0 forever).
	InitializeRandomSeed(); // seed the RNG BEFORE anything stochastic (tiles, genomes) for this seed
	if (Experiment == EExperiment::E1)
	{
		// Tile experiment (E1): regenerate limestone tiles with THIS seed's RNG, then (after the
		// tile-settle delay) build the grid, init agents and start gen 0 — via the existing delayed
		// path (spawned tile actors need a frame to register before the grid can read them). Gives
		// each seed a fresh, seed-specific tile layout from generation 0 = properly independent runs.
		InitForNextGenerationWithNewLimestoneTiles(TArray<FGenome>());
	}
	else
	{
		// E2/E3: no limestone tiles; the environment builds synchronously inside InitFirstGeneration
		// (grid is ready before agents copy it), so prepare and kick directly.
		InitFirstGeneration();
		StartNextGeneration();
	}
}

bool ASimulationManager::AdvanceBatchSeedOrFinish()
{
	int32 NextSeed = CurrentBatchSeed + 1;
	while (NextSeed <= BatchSeedEnd && bBatchSkipExisting && DoesSeedOutputExist(NextSeed))
	{
		UE_LOG(LogTemp, Warning, TEXT("Batch: skipping existing seed %d (%s/%s)"), NextSeed, *GetExperimentLabel(), *GetMethodLabel());
		NextSeed++;
	}
	if (NextSeed > BatchSeedEnd)
		return false;

	StartBatchRun(NextSeed);
	return true;
}

bool ASimulationManager::DoesValidationRowExist(int32 GenomeSeed, int32 RngSeed) const
{
	const FString ResultsPath = FPaths::ProjectContentDir() / TEXT("Evolution/validation_results.txt");
	if (!FPaths::FileExists(ResultsPath))
		return false;

	FString ExistingContent;
	if (!FFileHelper::LoadFileToString(ExistingContent, *ResultsPath))
		return false;

	// Row layout: ...\tscenario\tvalidation_genome_seed\tgenome_source\t...\tfixed_random_seed_enabled\trandom_seed
	// Matching on "\t<scenario>\t<GenomeSeed>\t" plus a trailing "\t<RngSeed>\n" is sufficient to
	// detect a prior completed row for this exact (genome seed, RNG seed, scenario) triple.
	const FString ScenarioTag = ValidationScenarioLabel.Replace(TEXT("\t"), TEXT(" "));
	const FString RowMarker = FString::Printf(TEXT("\t%s\t%d\t"), *ScenarioTag, GenomeSeed);
	const FString RngTail = FString::Printf(TEXT("\t%d\n"), RngSeed);

	TArray<FString> Lines;
	ExistingContent.ParseIntoArrayLines(Lines);
	for (const FString& Line : Lines)
	{
		if (Line.Contains(RowMarker) && Line.EndsWith(RngTail))
			return true;
	}
	return false;
}

bool ASimulationManager::FindNextPendingValidationPair(int32 StartGenomeSeed, int32 StartRngSeed, int32& OutGenomeSeed, int32& OutRngSeed) const
{
	for (int32 GenomeSeed = StartGenomeSeed; GenomeSeed <= BatchValGenomeSeedEnd; ++GenomeSeed)
	{
		const int32 RngStart = (GenomeSeed == StartGenomeSeed) ? StartRngSeed : BatchValRngSeedStart;
		for (int32 RngSeed = RngStart; RngSeed <= BatchValRngSeedEnd; ++RngSeed)
		{
			if (bBatchValSkipExisting && DoesValidationRowExist(GenomeSeed, RngSeed))
			{
				UE_LOG(LogTemp, Warning, TEXT("Batch validation: skipping existing genome seed %d / RNG seed %d (%s)"), GenomeSeed, RngSeed, *ValidationScenarioLabel);
				continue;
			}
			OutGenomeSeed = GenomeSeed;
			OutRngSeed = RngSeed;
			return true;
		}
	}
	return false;
}

void ASimulationManager::StartBatchValidationRun(int32 GenomeSeed, int32 RngSeed)
{
	// Seed decoupling: ValidationGenomeSeed selects which trained controller set to load,
	// RandomSeed drives the env RNG. These are always taken from disjoint ranges (enforced by the
	// BatchValGenomeSeedStart/End vs BatchValRngSeedStart/End editor ranges) so a run never has
	// genome seed == RNG seed -- see docs/findings.md seed-confound entry.
	// Guard against re-introducing the seed confound: the genome seed (controller identity) and the
	// env RNG seed must differ. If the editor ranges overlap and a pair collides, warn loudly rather
	// than silently producing a confounded run (see docs/findings.md seed-confound entry).
	if (GenomeSeed == RngSeed)
	{
		UE_LOG(LogTemp, Error, TEXT("Batch validation: genome seed == RNG seed (%d) — SEED CONFOUND. Set BatchValGenomeSeed and BatchValRngSeed ranges to be disjoint (e.g. genomes 1-30, RNG 101-130)."), GenomeSeed);
	}

	ValidationGenomeSeed = GenomeSeed;
	bUseFixedRandomSeed = true;
	RandomSeed = RngSeed;
	GenerationNumber = 0;
	Hour = 12.0f;
	TickCount = 0;
	bCachedE3SoundSourceOffsets = false;

	UE_LOG(LogTemp, Warning, TEXT("Batch validation: starting genome seed %d / RNG seed %d (%s, %s), genome seeds %d-%d x RNG seeds %d-%d"),
		GenomeSeed, RngSeed, *GetExperimentLabel(), *ValidationScenarioLabel,
		BatchValGenomeSeedStart, BatchValGenomeSeedEnd, BatchValRngSeedStart, BatchValRngSeedEnd);

	InitFirstGeneration();
	// Kick the first generation — InitFirstGeneration only prepares it (manual mode relies on the
	// "Run Sim" click). Without this the batch validation run would idle at generation 0.
	StartNextGeneration();
}

bool ASimulationManager::AdvanceBatchValidationOrFinish()
{
	int32 NextRngSeed = RandomSeed + 1;
	int32 NextGenomeSeed = ValidationGenomeSeed;
	if (NextRngSeed > BatchValRngSeedEnd)
	{
		NextGenomeSeed++;
		NextRngSeed = BatchValRngSeedStart;
	}
	if (NextGenomeSeed > BatchValGenomeSeedEnd)
		return false;

	int32 FoundGenomeSeed, FoundRngSeed;
	if (!FindNextPendingValidationPair(NextGenomeSeed, NextRngSeed, FoundGenomeSeed, FoundRngSeed))
		return false;

	StartBatchValidationRun(FoundGenomeSeed, FoundRngSeed);
	return true;
}

void ASimulationManager::WriteTrainingGenerationRow(float AvgFitness, float MaxFitness, float Diversity, int32 TotalSettlers, int32 CorrectSettlers, int32 BoundaryContacts)
{
	const FString Dir = GetTrainingOutputDir();
	IFileManager::Get().MakeDirectory(*Dir, true);
	const FString Path = Dir / FString::Printf(
		TEXT("train_%s_%s_seed_%02d.csv"), *GetExperimentLabel(), *GetMethodLabel(), RandomSeed);

	// Truncate + rewrite the header at generation 0 of each seed (not just when the file is absent),
	// so a seed that is re-run after an interrupted/crashed sweep starts a FRESH CSV instead of
	// appending onto a stale partial. GenerationNumber resets to 0 at the start of every seed.
	if (GenerationNumber == 0 || !FPaths::FileExists(Path))
	{
		const FString Header = TEXT("Generation,AvgFitness,MaxFitness,GeneticDiversity,TotalSettlers,CorrectSettlers,BoundaryContacts\n");
		FFileHelper::SaveStringToFile(Header, *Path);
	}

	const FString Row = FString::Printf(TEXT("%d,%s,%s,%s,%d,%d,%d\n"),
		GenerationNumber,
		*FString::SanitizeFloat(AvgFitness),
		*FString::SanitizeFloat(MaxFitness),
		*FString::SanitizeFloat(Diversity),
		TotalSettlers, CorrectSettlers, BoundaryContacts);
	FFileHelper::SaveStringToFile(Row, *Path, FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), FILEWRITE_Append);
}

void ASimulationManager::CacheE3SoundSourceOffsets()
{
	if (bCachedE3SoundSourceOffsets)
		return;

	TArray<AActor*> SoundSources;
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName(TEXT("SoundSource")), SoundSources);
	if (SoundSources.Num() == 0)
		return;
	SoundSources.Sort([](const AActor& Left, const AActor& Right) { return Left.GetName() < Right.GetName(); });

	FVector ClusterCenter = FVector::ZeroVector;
	int32 ValidSoundSources = 0;
	for (const AActor* SoundSource : SoundSources)
	{
		if (!SoundSource) continue;
		ClusterCenter += SoundSource->GetActorLocation();
		ValidSoundSources++;
	}
	if (ValidSoundSources == 0)
		return;
	ClusterCenter /= static_cast<float>(ValidSoundSources);

	CachedE3SoundSourceOffsets.Empty();
	for (const AActor* SoundSource : SoundSources)
	{
		CachedE3SoundSourceOffsets.Add(SoundSource ? SoundSource->GetActorLocation() - ClusterCenter : FVector::ZeroVector);
	}
	bCachedE3SoundSourceOffsets = true;
}

FVector ASimulationManager::GetE3TrainingSoundCenterForGeneration() const
{
	const FVector TankOrigin = DataGridOrganizer->GetActorLocation();
	const FVector TankBounds = DataGridOrganizer->WorldBounds;
	const float HorizontalOffset = FMath::Max(1.f, E3TrainingSoundHorizontalOffset);
	const float VerticalOffset = FMath::Max(1.f, E3TrainingSoundVerticalOffset);

	TArray<FVector> TrainingCenters;
	TrainingCenters.Reserve(12);
	// Lateral (all four horizontal sides, mid-height)
	TrainingCenters.Add(TankOrigin + FVector(-HorizontalOffset, TankBounds.Y * 0.5f, TankBounds.Z * 0.5f));
	TrainingCenters.Add(TankOrigin + FVector(TankBounds.X + HorizontalOffset, TankBounds.Y * 0.5f, TankBounds.Z * 0.5f));
	TrainingCenters.Add(TankOrigin + FVector(TankBounds.X * 0.5f, -HorizontalOffset, TankBounds.Z * 0.5f));
	TrainingCenters.Add(TankOrigin + FVector(TankBounds.X * 0.5f, TankBounds.Y + HorizontalOffset, TankBounds.Z * 0.5f));
	// Above (centre + two upper corners)
	TrainingCenters.Add(TankOrigin + FVector(TankBounds.X * 0.5f, TankBounds.Y * 0.5f, TankBounds.Z + VerticalOffset));
	TrainingCenters.Add(TankOrigin + FVector(-HorizontalOffset, -HorizontalOffset, TankBounds.Z + VerticalOffset * 0.5f));
	TrainingCenters.Add(TankOrigin + FVector(TankBounds.X + HorizontalOffset, TankBounds.Y + HorizontalOffset, TankBounds.Z + VerticalOffset * 0.5f));
	// Lateral corners (mid-height)
	TrainingCenters.Add(TankOrigin + FVector(-HorizontalOffset, TankBounds.Y + HorizontalOffset, TankBounds.Z * 0.5f));
	TrainingCenters.Add(TankOrigin + FVector(TankBounds.X + HorizontalOffset, -HorizontalOffset, TankBounds.Z * 0.5f));
	// Below (centre + two lower corners) -- omnidirectional-phonotaxis robustness coverage
	TrainingCenters.Add(TankOrigin + FVector(TankBounds.X * 0.5f, TankBounds.Y * 0.5f, -VerticalOffset));
	TrainingCenters.Add(TankOrigin + FVector(-HorizontalOffset, -HorizontalOffset, -VerticalOffset * 0.5f));
	TrainingCenters.Add(TankOrigin + FVector(TankBounds.X + HorizontalOffset, TankBounds.Y + HorizontalOffset, -VerticalOffset * 0.5f));

	const int32 PositionIndex = FMath::Abs(GenerationNumber) % TrainingCenters.Num();
	return TrainingCenters[PositionIndex];
}

void ASimulationManager::UpdateE3SoundSourcesForTraining()
{
	if (Experiment != EExperiment::E3 || !bIsTraining || !bCycleE3SoundSourcesDuringTraining || !DataGridOrganizer)
		return;

	TArray<AActor*> SoundSources;
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName(TEXT("SoundSource")), SoundSources);
	if (SoundSources.Num() == 0)
		return;
	SoundSources.Sort([](const AActor& Left, const AActor& Right) { return Left.GetName() < Right.GetName(); });

	CacheE3SoundSourceOffsets();
	if (CachedE3SoundSourceOffsets.Num() != SoundSources.Num())
	{
		bCachedE3SoundSourceOffsets = false;
		CacheE3SoundSourceOffsets();
	}

	const FVector NewClusterCenter = GetE3TrainingSoundCenterForGeneration();
	for (int32 Index = 0; Index < SoundSources.Num(); ++Index)
	{
		AActor* SoundSource = SoundSources[Index];
		if (!SoundSource)
			continue;

		if (USceneComponent* SoundRootComponent = SoundSource->GetRootComponent())
		{
			SoundRootComponent->SetMobility(EComponentMobility::Movable);
		}

		const FVector Offset = CachedE3SoundSourceOffsets.IsValidIndex(Index)
			? CachedE3SoundSourceOffsets[Index]
			: FVector::ZeroVector;
		SoundSource->SetActorLocation(NewClusterCenter + Offset);
	}
}
void ASimulationManager::OnLimestoneTileDelayCompleted(const TArray<FGenome>& CrossingGenomes)
{
	if (Experiment == EExperiment::E2)
	{
		ResetE2Clock();
	}
	DataGridOrganizer->GenerateNewDataGrid();
	if (Experiment == EExperiment::E2)
	{
		DataGridOrganizer->AdaptE2DataGrid(Hour);
	}
	InitAgents(CrossingGenomes);
	
	CurrentGenerationStep = 0;
	
	// Batch runs ALWAYS defer via the timer so each generation returns to the game-thread tick before
	// the next starts: this prevents the synchronous ParallelFor path from re-entering
	// StartNextGeneration recursively across generations, and lets the engine pump between generations.
	if (GenerationNumber % 5 == 0 || bBatchSeeds || bBatchValidation)
	{
		// Wait for one frame to let the game thread update
		FTimerHandle TimerHandle;
		GetWorld()->GetTimerManager().SetTimer(TimerHandle, this, &ASimulationManager::StartNextGeneration, 0.0001f, false);
	} else {
		StartNextGeneration();
	}
}

void ASimulationManager::InitFirstGenForTraining()
{
	// Initialize the agents
	auto Idx = 0;
	for (const auto Agent : Agents)
	{
		FAgentInitParams InitParams = AgentInitParams;
		InitParams.bUseE2Fitness = (Experiment == EExperiment::E2);
		InitParams.bUseE3Fitness = (Experiment == EExperiment::E3);
		// Unattended batch runs never inspect the per-agent debug strings; skip building them.
		InitParams.bComputeDebugStrings = !(bBatchSeeds || bBatchValidation);
		// Share the read-only environment grid across agents ONLY when nothing mutates it during
		// stepping. The sole mid-run mutation is the diurnal light cycle (HandleE2Time), so sharing
		// is safe exactly when it is off; with it on, each agent keeps a private copy (no read/write race).
		InitParams.bShareEnvironmentGrid = !bEnableDiurnalLightCycle && !bDisableGridShare;
		auto AgentTransform = GetRandomAgentSpawnTransform();
		InitParams.StartLocation = AgentTransform.GetLocation();
		InitParams.StartRotation = AgentTransform.GetRotation().Rotator();
		
		Agent->InitAgent(InitParams, DataGridOrganizer->DataGrid);
		// Empty genomes for the first generation because they are randomly generated
		TArray<FGenome> EmptyGenomes;
		Agent->InitBrain(GetNewGenomeForAgent(EmptyGenomes, Idx), InitParams.MaxInnerNeurons);
		Agent->SetActorLocation(InitParams.StartLocation);
		Agent->SetActorRotation(InitParams.StartRotation);
		Idx++;
	}
}

FString ASimulationManager::ResolveValidationGenomeFilePath() const
{
	FString Source = ValidationGenomeSourceLabel.TrimStartAndEnd();
	if (Source.IsEmpty() || Source.Equals(TEXT("best_genomes.txt"), ESearchCase::IgnoreCase))
	{
		return FPaths::ProjectContentDir() / TEXT("Evolution/best_genomes.txt");
	}

	FPaths::NormalizeFilename(Source);
	FString AbsoluteSource = Source;
	if (FPaths::IsRelative(Source))
	{
		AbsoluteSource = FPaths::ProjectContentDir() / Source;
		FPaths::NormalizeFilename(AbsoluteSource);
	}

	if (FPaths::GetExtension(AbsoluteSource).Equals(TEXT("txt"), ESearchCase::IgnoreCase))
	{
		return AbsoluteSource;
	}

	const FString SeedLabel = FString::Printf(TEXT("%02d"), ValidationGenomeSeed);
	return AbsoluteSource / FString::Printf(
		TEXT("best_genomes_%s_seed_%s.txt"),
		*GetExperimentLabel(),
		*SeedLabel);
}
void ASimulationManager::InitFirstGenForValidationSim()
{
	const FString FilePath = ResolveValidationGenomeFilePath();
	LastLoadedValidationGenomePath = FilePath;
	if(!FPaths::FileExists(FilePath))
	{
		UE_LOG(LogTemp, Error, TEXT("There is no validation genome file: %s"), *FilePath);
		return;
	}
	auto OldGenomeFitnessPairs = UResultAnalysisFunctions::LoadGenomesFromFile(FilePath);
	if (OldGenomeFitnessPairs.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("Validation genome file is empty or unreadable: %s"), *FilePath);
		return;
	}

	OldGenomeFitnessPairs.Sort([](const FGenomeFitnessPair& A, const FGenomeFitnessPair& B)
	{
		return A.FitnessScore > B.FitnessScore; // Best fitness score first
	});
	
	TrainedBrains = TArray<FGenome>();
	for (const auto GenomeFitnessPair : OldGenomeFitnessPairs)
	{
		TrainedBrains.Add(GenomeFitnessPair.Genome);
	}

	// R1-3 null baseline: replace the loaded trained controllers with UNTRAINED random genomes of the
	// same count and length. Isolates "what evolution added" — if random controllers match the trained
	// ones on the primary metric, the evolved network is not doing the work. Random draw is on the game
	// thread at setup (serial), so it does not interact with the per-agent stepping RNG.
	if (bUseRandomBrainBaseline)
	{
		const int32 N = FMath::Max(1, TrainedBrains.Num());
		const int32 Len = FMath::Max(1, AgentInitParams.GenomeLength);
		TrainedBrains.Reset();
		for (int32 g = 0; g < N; ++g)
			TrainedBrains.Add(UGenomeFunctions::MakeRandomGenome(Len));
		UE_LOG(LogTemp, Warning, TEXT("[AUTO] RandomBrain baseline: deploying %d untrained random genomes (len %d)"), N, Len);
	}

	// Initialize the agents
	auto Idx = 0;
	auto TrainedBrainsIdx = 0;
	for(int i = 0; i < Agents.Num(); i++)
	{
		FAgentInitParams InitParams = AgentInitParams;
		InitParams.bUseE2Fitness = (Experiment == EExperiment::E2);
		InitParams.bUseE3Fitness = (Experiment == EExperiment::E3);
		// Unattended batch runs never inspect the per-agent debug strings; skip building them.
		InitParams.bComputeDebugStrings = !(bBatchSeeds || bBatchValidation);
		// Share the read-only environment grid across agents ONLY when nothing mutates it during
		// stepping. The sole mid-run mutation is the diurnal light cycle (HandleE2Time), so sharing
		// is safe exactly when it is off; with it on, each agent keeps a private copy (no read/write race).
		InitParams.bShareEnvironmentGrid = !bEnableDiurnalLightCycle && !bDisableGridShare;
		InitParams.bLogPerStepTrajectory = bLogPerStepTrajectory;
		auto AgentTransform = GetRandomAgentSpawnTransform();
		InitParams.StartLocation = AgentTransform.GetLocation();
		InitParams.StartRotation = AgentTransform.GetRotation().Rotator();

		Agents[i]->InitAgent(InitParams, DataGridOrganizer->DataGrid);
		Agents[i]->InitBrain(TrainedBrains[TrainedBrainsIdx++], InitParams.MaxInnerNeurons);
		Agents[i]->SetActorLocation(InitParams.StartLocation);
		Agents[i]->SetActorRotation(InitParams.StartRotation);
		Idx++;
		if(TrainedBrainsIdx >= TrainedBrains.Num())
		{
			TrainedBrainsIdx = 0;
		}
	}
	if(!bIsTraining)
		StartClock();
}
void ASimulationManager::SaveGenomesForValidation()
{
	const FString LatestGenomePath = FPaths::ProjectContentDir() / TEXT("Evolution/best_genomes.txt");
	const FString ArchiveGenomePath = GetGenomeArchiveFilePath();

	// Ensure the archive directory exists (batch mode writes into per-method subfolders).
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ArchiveGenomePath), true);

	UResultAnalysisFunctions::SaveGenomesToFile(GenomeFitnessPairs, LatestGenomePath);
	UResultAnalysisFunctions::SaveGenomesToFile(GenomeFitnessPairs, ArchiveGenomePath);

	UE_LOG(LogTemp, Warning, TEXT("Saved genomes to %s and %s"), *LatestGenomePath, *ArchiveGenomePath);
}

void ASimulationManager::AnalyzeValidationSim()
{
	const FString Timestamp = FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M:%S"));
	const FString ExperimentLabel = GetExperimentLabel();
	const FString ScenarioLabel = ValidationScenarioLabel.Replace(TEXT("\t"), TEXT(" "));
	const FString GenomeSourceLabel = LastLoadedValidationGenomePath.Replace(TEXT("\t"), TEXT(" "));
	const int32 AgentCount = Agents.Num();

	float AverageFitness = 0.f;
	float MaxFitness = -FLT_MAX;
	for (const auto& GenomeFitnessPair : GenomeFitnessPairs)
	{
		AverageFitness += GenomeFitnessPair.FitnessScore;
		MaxFitness = FMath::Max(MaxFitness, GenomeFitnessPair.FitnessScore);
	}
	if (GenomeFitnessPairs.Num() > 0)
	{
		AverageFitness /= GenomeFitnessPairs.Num();
	}
	else
	{
		MaxFitness = 0.f;
	}

	int TotalSettlers = 0;
	int CorrectSettlers = 0;
	int EndedOnReefCellCount = 0;
	int BoundaryContacts = 0;
	int PrematureSettlers = 0;
	int SettlementStepSum = 0;
	int MinSettlementStep = MaxSimSteps + 1;
	int MaxSettlementStep = -1;
	int FinalStepSum = 0;
	int MinFinalStep = MaxSimSteps + 1;
	int MaxFinalStep = -1;

	FString PositionRows;
	for (int32 i = 0; i < AgentCount; ++i)
	{
		const auto Agent = Agents[i];
		if (!Agent) continue;

		const FLarvaAgentStatus FinalStatus = i < AgentTasks.Num()
			? AgentTasks[i]->GetAgentStatus()
			: FLarvaAgentStatus();
		const FVector FinalLocation = i < AgentTasks.Num()
			? FinalStatus.Transform.GetLocation()
			: Agent->GetActorLocation();

		const auto Cell = DataGridOrganizer->DataGrid->GetCellAtPoint(FinalLocation);
		const bool bSettled = Agent->IsSettled();
		const bool bEndedOnReefCell = Cell.Data.bIsReefCell;
		const bool bCorrectSettler = bSettled && bEndedOnReefCell;

		BoundaryContacts += Agent->GetBoundaryContactCount();
		if (bEndedOnReefCell) EndedOnReefCellCount++;
		if (bSettled) TotalSettlers++;
		if (bCorrectSettler) CorrectSettlers++;

		const int32 FinalStep = Agent->GetCurrentAge();
		const int32 SettlementStep = Agent->GetSettlementTime();
		const bool bPrematureSettlement = bSettled && SettlementStep >= 0 && SettlementStep < AgentInitParams.SettlementCompetencyAge;
		const FVector StartLocation = Agent->GetStartLocation();

		FinalStepSum += FinalStep;
		MinFinalStep = FMath::Min(MinFinalStep, FinalStep);
		MaxFinalStep = FMath::Max(MaxFinalStep, FinalStep);
		if (bSettled)
		{
			SettlementStepSum += SettlementStep;
			MinSettlementStep = FMath::Min(MinSettlementStep, SettlementStep);
			MaxSettlementStep = FMath::Max(MaxSettlementStep, SettlementStep);
			if (bPrematureSettlement) PrematureSettlers++;
		}

		const FString StartX = FormatFloatWithComma(StartLocation.X);
		const FString StartY = FormatFloatWithComma(StartLocation.Y);
		const FString StartZ = FormatFloatWithComma(StartLocation.Z);
		const FString FinalX = FormatFloatWithComma(FinalLocation.X);
		const FString FinalY = FormatFloatWithComma(FinalLocation.Y);
		const FString FinalZ = FormatFloatWithComma(FinalLocation.Z);
        PositionRows += FString::Printf(
            TEXT("%s\t%s\t%s\t%d\t%d\t%s\t%d\t%s\t%s\t%s\t%s\t%s\t%s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n"),
            *Timestamp,
            *ExperimentLabel,
            *ScenarioLabel,
            ValidationGenomeSeed,
            RandomSeed,
            *GenomeSourceLabel,
            i + 1,
            *StartX,
            *StartY,
            *StartZ,
            *FinalX,
            *FinalY,
            *FinalZ,
            FinalStep,
            SettlementStep,
            bSettled ? 1 : 0,
            bCorrectSettler ? 1 : 0,
            bEndedOnReefCell ? 1 : 0,
            bPrematureSettlement ? 1 : 0,
            Agent->GetBoundaryContactCount());
	}

	LastTotalSettlers = TotalSettlers;
	LastCorrectSettlers = CorrectSettlers;
	LastBoundaryContacts = BoundaryContacts;

	const float SettlementPercent = AgentCount > 0 ? 100.f * static_cast<float>(TotalSettlers) / static_cast<float>(AgentCount) : 0.f;
	const float CorrectSettlementPercent = AgentCount > 0 ? 100.f * static_cast<float>(CorrectSettlers) / static_cast<float>(AgentCount) : 0.f;
    const float MeanFinalStep = AgentCount > 0 ? static_cast<float>(FinalStepSum) / static_cast<float>(AgentCount) : -1.f;
    const int32 MinFinalStepOutput = AgentCount > 0 ? MinFinalStep : -1;
    const int32 MaxFinalStepOutput = AgentCount > 0 ? MaxFinalStep : -1;
    const float MeanSettlementStep = TotalSettlers > 0 ? static_cast<float>(SettlementStepSum) / static_cast<float>(TotalSettlers) : -1.f;
    const int32 MinSettlementStepOutput = TotalSettlers > 0 ? MinSettlementStep : -1;
    const int32 MaxSettlementStepOutput = TotalSettlers > 0 ? MaxSettlementStep : -1;

	const FString ResultsPath = FPaths::ProjectContentDir() / TEXT("Evolution/validation_results.txt");
	const FString PositionsPath = FPaths::ProjectContentDir() / TEXT("Evolution/validation_positions.txt");
	const FString EnvironmentPath = FPaths::ProjectContentDir() / TEXT("Evolution/validation_environment.txt");

    const FString ResultsHeader = TEXT("timestamp\texperiment\tscenario\tvalidation_genome_seed\tgenome_source\tagents\tmax_sim_steps\tmean_final_step\tmin_final_step\tmax_final_step\tavg_fitness\tmax_fitness\ttotal_settlers\tcorrect_settlers\tsettlement_percent\tcorrect_settlement_percent\tmean_settlement_step\tmin_settlement_step\tmax_settlement_step\tpremature_settlers\tended_on_reef_cell_count\tboundary_contacts\tworld_x\tworld_y\tworld_z\tcell_edge_length\tcca_cover\ttile_count\tmin_tile_area\tmax_tile_area\tcurrent_enabled\tcurrent_speed_cm_per_step\tfixed_random_seed_enabled\trandom_seed\n");
    const FString EnvironmentHeader = TEXT("timestamp\texperiment\tscenario\tvalidation_genome_seed\trandom_seed\tactual_tile_count\tactual_tile_area_sum\ttile_geometry\tsound_source_count\tsound_source_positions\tmin_temperature\tmax_temperature\tmean_salinity\tpressure\tsurface_light\tdepth_for_blue_dominance\tdepth_threshold_frequency\tlow_frequency\thigh_frequency\tsound_surface_intensity\tsound_deep_water_intensity\tcurrent_enabled\tcurrent_speed_cm_per_step\texperiment50_attenuation_coefficient\tgeotactic_downward_speed_cm_per_step\tgeotactic_bias_enabled\tsettlement_competency_age\tcompetency_age_min\tcompetency_age_max\tsound_enabled\n");
    auto EnsureOutputHeader = [](const FString& Path, const FString& Header)
    {
        if (!FPaths::FileExists(Path))
        {
            FFileHelper::SaveStringToFile(Header, *Path);
            return;
        }

        FString ExistingContent;
        if (FFileHelper::LoadFileToString(ExistingContent, *Path) && !ExistingContent.StartsWith(Header))
        {
            const FString BackupPath = Path + TEXT(".old_schema_") + FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")) + TEXT(".txt");
            IFileManager::Get().Move(*BackupPath, *Path);
            FFileHelper::SaveStringToFile(Header, *Path);
        }
    };

    EnsureOutputHeader(ResultsPath, ResultsHeader);
    EnsureOutputHeader(EnvironmentPath, EnvironmentHeader);

	const FVector WorldBounds = DataGridOrganizer->WorldBounds;
	const FEnvironmentalParams EnvParams = DataGridOrganizer->GetActiveEnvironmentalParams();
    float ActualTileAreaSum = 0.f;
    FString TileGeometry = TEXT("none");
    TArray<AActor*> TileActors;
    UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName(TEXT("LimestoneTile")), TileActors);
    if (TileActors.Num() > 0)
    {
        TileGeometry.Empty();
        for (int32 TileIndex = 0; TileIndex < TileActors.Num(); ++TileIndex)
        {
            const AActor* TileActor = TileActors[TileIndex];
            if (!TileActor) continue;

            const FVector TileLocation = TileActor->GetActorLocation();
            const FVector TileScale = TileActor->GetActorScale3D();
            const FRotator TileRotation = TileActor->GetActorRotation();
            const float TileSizeX = TileScale.X * 100.f;
            const float TileSizeY = TileScale.Y * 100.f;
            const float TileArea = TileSizeX * TileSizeY;
            ActualTileAreaSum += TileArea;

            if (!TileGeometry.IsEmpty()) TileGeometry += TEXT(";");
            TileGeometry += FString::Printf(
                TEXT("tile%d:x=%s,y=%s,z=%s,size_x=%s,size_y=%s,yaw=%s,area=%s"),
                TileIndex + 1,
                *FormatFloatWithComma(TileLocation.X),
                *FormatFloatWithComma(TileLocation.Y),
                *FormatFloatWithComma(TileLocation.Z),
                *FormatFloatWithComma(TileSizeX),
                *FormatFloatWithComma(TileSizeY),
                *FormatFloatWithComma(TileRotation.Yaw),
                *FormatFloatWithComma(TileArea));
        }
    }
    FString SoundSourcePositions = TEXT("none");
    TArray<AActor*> SoundSourceActors;
    UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName(TEXT("SoundSource")), SoundSourceActors);
    if (SoundSourceActors.Num() > 0)
    {
        SoundSourcePositions.Empty();
        for (int32 SoundSourceIndex = 0; SoundSourceIndex < SoundSourceActors.Num(); ++SoundSourceIndex)
        {
            const AActor* SoundSourceActor = SoundSourceActors[SoundSourceIndex];
            if (!SoundSourceActor) continue;

            const FVector SoundSourceLocation = SoundSourceActor->GetActorLocation();
            if (!SoundSourcePositions.IsEmpty()) SoundSourcePositions += TEXT(";");
            SoundSourcePositions += FString::Printf(
                TEXT("source%d:x=%s,y=%s,z=%s"),
                SoundSourceIndex + 1,
                *FormatFloatWithComma(SoundSourceLocation.X),
                *FormatFloatWithComma(SoundSourceLocation.Y),
                *FormatFloatWithComma(SoundSourceLocation.Z));
        }
    }
	const FString AverageFitnessString = FormatFloatWithComma(AverageFitness);
	const FString MaxFitnessString = FormatFloatWithComma(MaxFitness);
	const FString SettlementPercentString = FormatFloatWithComma(SettlementPercent);
	const FString CorrectSettlementPercentString = FormatFloatWithComma(CorrectSettlementPercent);
    const FString MeanFinalStepString = FormatFloatWithComma(MeanFinalStep);
    const FString MeanSettlementStepString = FormatFloatWithComma(MeanSettlementStep);
	const FString WorldXString = FormatFloatWithComma(WorldBounds.X);
	const FString WorldYString = FormatFloatWithComma(WorldBounds.Y);
	const FString WorldZString = FormatFloatWithComma(WorldBounds.Z);
	const FString CellEdgeLengthString = FormatFloatWithComma(DataGridOrganizer->CellEdgeLength);
	const FString CCACoverString = FormatFloatWithComma(DataGridOrganizer->CCACover);
	const FString MinTileAreaString = FormatFloatWithComma(DataGridOrganizer->MinTileArea);
	const FString MaxTileAreaString = FormatFloatWithComma(DataGridOrganizer->MaxTileArea);
	const FString CurrentSpeedString = FormatFloatWithComma(EnvParams.CurrentSpeedCmPerStep);
	const FString E2AttenuationCoefficientString = FormatFloatWithComma(DataGridOrganizer->E2AttenuationCoefficient);
    const FString ActualTileAreaSumString = FormatFloatWithComma(ActualTileAreaSum);
    const FString MinTemperatureString = FormatFloatWithComma(EnvParams.MinTemperature);
    const FString MaxTemperatureString = FormatFloatWithComma(EnvParams.MaxTemperature);
    const FString MeanSalinityString = FormatFloatWithComma(EnvParams.MeanSalinity);
    const FString PressureString = FormatFloatWithComma(EnvParams.Pressure);
    const FString SurfaceLightString = FormatFloatWithComma(EnvParams.SurfaceLight);
    const FString DepthForBlueDominanceString = FormatFloatWithComma(EnvParams.DepthForBlueDominance);
    const FString DepthThresholdFrequencyString = FormatFloatWithComma(EnvParams.DepthThresholdFrequency);
    const FString LowFrequencyString = FormatFloatWithComma(EnvParams.LowFrequency);
    const FString HighFrequencyString = FormatFloatWithComma(EnvParams.HighFrequency);
    const FString SoundSurfaceIntensityString = FormatFloatWithComma(EnvParams.SoundSurfaceIntensity);
    const FString SoundDeepWaterIntensityString = FormatFloatWithComma(EnvParams.SoundDeepWaterIntensity);
    const FString GeotacticDownwardSpeedString = FormatFloatWithComma(AgentInitParams.GeotacticDownwardSpeedCmPerStep);

    const FString ResultRow = FString::Printf(
        TEXT("%s\t%s\t%s\t%d\t%s\t%d\t%d\t%s\t%d\t%d\t%s\t%s\t%d\t%d\t%s\t%s\t%s\t%d\t%d\t%d\t%d\t%d\t%s\t%s\t%s\t%s\t%s\t%d\t%s\t%s\t%d\t%s\t%d\t%d\n"),
        *Timestamp,
        *ExperimentLabel,
        *ScenarioLabel,
        ValidationGenomeSeed,
        *GenomeSourceLabel,
        AgentCount,
        MaxSimSteps,
        *MeanFinalStepString,
        MinFinalStepOutput,
        MaxFinalStepOutput,
        *AverageFitnessString,
        *MaxFitnessString,
        TotalSettlers,
        CorrectSettlers,
        *SettlementPercentString,
        *CorrectSettlementPercentString,
        *MeanSettlementStepString,
        MinSettlementStepOutput,
        MaxSettlementStepOutput,
        PrematureSettlers,
        EndedOnReefCellCount,
        BoundaryContacts,
        *WorldXString,
        *WorldYString,
        *WorldZString,
        *CellEdgeLengthString,
        *CCACoverString,
        DataGridOrganizer->TileCount,
        *MinTileAreaString,
        *MaxTileAreaString,
        EnvParams.bCurrentEnabled ? 1 : 0,
        *CurrentSpeedString,
        bUseFixedRandomSeed ? 1 : 0,
        RandomSeed);

	FFileHelper::SaveStringToFile(ResultRow, *ResultsPath, FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), FILEWRITE_Append);
    TileGeometry.ReplaceInline(TEXT("\t"), TEXT(" "), ESearchCase::CaseSensitive);
    SoundSourcePositions.ReplaceInline(TEXT("\t"), TEXT(" "), ESearchCase::CaseSensitive);
    const FString EnvironmentRow = FString::Printf(
        TEXT("%s\t%s\t%s\t%d\t%d\t%d\t%s\t%s\t%d\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%d\t%s\t%s\t%s\t%d\t%d\t%d\t%d\t%d\n"),
        *Timestamp,
        *ExperimentLabel,
        *ScenarioLabel,
        ValidationGenomeSeed,
        RandomSeed,
        TileActors.Num(),
        *ActualTileAreaSumString,
        *TileGeometry,
        SoundSourceActors.Num(),
        *SoundSourcePositions,
        *MinTemperatureString,
        *MaxTemperatureString,
        *MeanSalinityString,
        *PressureString,
        *SurfaceLightString,
        *DepthForBlueDominanceString,
        *DepthThresholdFrequencyString,
        *LowFrequencyString,
        *HighFrequencyString,
        *SoundSurfaceIntensityString,
        *SoundDeepWaterIntensityString,
        EnvParams.bCurrentEnabled ? 1 : 0,
        *CurrentSpeedString,
        *E2AttenuationCoefficientString,
        *GeotacticDownwardSpeedString,
        AgentInitParams.bEnableGeotacticBias ? 1 : 0,
        AgentInitParams.SettlementCompetencyAge,
        AgentInitParams.CompetencyAgeMin,
        AgentInitParams.CompetencyAgeMax,
        DataGridOrganizer->bSoundEnabled ? 1 : 0);
    FFileHelper::SaveStringToFile(EnvironmentRow, *EnvironmentPath, FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), FILEWRITE_Append);
	const FString PositionHeader = TEXT("timestamp\texperiment\tscenario\tvalidation_genome_seed\trandom_seed\tgenome_source\tagent_index\tstart_x\tstart_y\tstart_z\tfinal_x\tfinal_y\tfinal_z\tfinal_step\tsettlement_step\tsettled\tcorrect_settler\tended_on_reef_cell\tpremature_settlement\tboundary_contacts\n");
	// Append (header once) so a multi-experiment / batch-validation session keeps every run's
	// per-agent positions instead of overwriting -- rows carry experiment/scenario/seed to
	// disambiguate. (Was previously overwritten, which lost E1/E2 positions when E3 ran next.)
	if (!FPaths::FileExists(PositionsPath))
		FFileHelper::SaveStringToFile(PositionHeader, *PositionsPath);
	FFileHelper::SaveStringToFile(PositionRows, *PositionsPath, FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), FILEWRITE_Append);

	const FString LegacyResultPath = FPaths::ProjectContentDir() / TEXT("Evolution/result.txt");
	const FString LegacyLogContent = FString::Printf(
		TEXT("Experiment: %s\nScenario: %s\nGenome seed: %d\nGenome source: %s\nAgents: %d\nAvgFitness: %s\nMaxFitness: %s\nTotalSettlers: %d\nCorrectSettlers: %d\nSettlementPercent: %s\nCorrectSettlementPercent: %s\nBoundaryContacts: %d\n"),
		*ExperimentLabel,
		*ScenarioLabel,
		ValidationGenomeSeed,
		*GenomeSourceLabel,
		AgentCount,
		*AverageFitnessString,
		*MaxFitnessString,
		TotalSettlers,
		CorrectSettlers,
		*SettlementPercentString,
		*CorrectSettlementPercentString,
		BoundaryContacts);
	FFileHelper::SaveStringToFile(LegacyLogContent, *LegacyResultPath);

	UE_LOG(LogTemp, Warning, TEXT("Validation result appended to %s"), *ResultsPath);
	UE_LOG(LogTemp, Warning, TEXT("Validation positions written to %s"), *PositionsPath);

	if (bLogPerStepTrajectory)
		FlushPerStepTrajectoryLog();
}

void ASimulationManager::FlushPerStepTrajectoryLog()
{
	// AP1: flush each agent's per-step trajectory buffer (sim step, position, full sensor vector,
	// full action vector) to a single per-run CSV, appended so a batch validation sweep
	// accumulates all runs in one file. Feedable to Plotting/09_nn_cue_ablation.py's offline
	// forward pass for the trajectory-based cue-dominance analysis, and to R for checking whether
	// the E3 horizontal peak tracks a moved sound source.
	const FString TrajectoryPath = FPaths::ProjectContentDir() / FString::Printf(
		TEXT("Evolution/validation_trajectory_%s_%s.csv"), *GetExperimentLabel(), *ValidationScenarioLabel.Replace(TEXT("\t"), TEXT(" ")));

	FString Header = TEXT("timestamp,experiment,scenario,validation_genome_seed,random_seed,agent_index,sim_step,pos_x,pos_y,pos_z");
	for (int32 SensorIdx = 0; SensorIdx < ESensorType::NUM_SENSORS; ++SensorIdx)
		Header += FString::Printf(TEXT(",sensor_%s"), UTF8_TO_TCHAR(SensorShortName(static_cast<ESensorType>(SensorIdx)).c_str()));
	for (int32 ActionIdx = 0; ActionIdx < EActionType::NUM_ACTIONS; ++ActionIdx)
		Header += FString::Printf(TEXT(",action_%s"), UTF8_TO_TCHAR(ActionShortName(static_cast<EActionType>(ActionIdx)).c_str()));
	Header += TEXT("\n");

	if (!FPaths::FileExists(TrajectoryPath))
		FFileHelper::SaveStringToFile(Header, *TrajectoryPath);

	const FString Timestamp = FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M:%S"));
	const FString ExperimentLabel = GetExperimentLabel();
	const FString ScenarioLabel = ValidationScenarioLabel.Replace(TEXT("\t"), TEXT(" "));

	FString Rows;
	for (int32 AgentIdx = 0; AgentIdx < Agents.Num(); ++AgentIdx)
	{
		const ALarvaAgent* Agent = Agents[AgentIdx];
		if (!Agent) continue;

		for (const FLarvaTrajectoryStep& Step : Agent->GetTrajectoryBuffer())
		{
			Rows += FString::Printf(TEXT("%s,%s,%s,%d,%d,%d,%d,%s,%s,%s"),
				*Timestamp, *ExperimentLabel, *ScenarioLabel, ValidationGenomeSeed, RandomSeed, AgentIdx + 1, Step.SimStep,
				*FString::SanitizeFloat(Step.Position.X), *FString::SanitizeFloat(Step.Position.Y), *FString::SanitizeFloat(Step.Position.Z));
			for (int32 SensorIdx = 0; SensorIdx < ESensorType::NUM_SENSORS; ++SensorIdx)
				Rows += FString::Printf(TEXT(",%s"), Step.SensorValues.IsValidIndex(SensorIdx) ? *FString::SanitizeFloat(Step.SensorValues[SensorIdx]) : TEXT(""));
			for (int32 ActionIdx = 0; ActionIdx < EActionType::NUM_ACTIONS; ++ActionIdx)
				Rows += FString::Printf(TEXT(",%s"), Step.ActionValues.IsValidIndex(ActionIdx) ? *FString::SanitizeFloat(Step.ActionValues[ActionIdx]) : TEXT(""));
			Rows += TEXT("\n");
		}
	}

	if (!Rows.IsEmpty())
	{
		FFileHelper::SaveStringToFile(Rows, *TrajectoryPath, FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), FILEWRITE_Append);
		UE_LOG(LogTemp, Warning, TEXT("Per-step trajectory log appended to %s"), *TrajectoryPath);
	}
}

void ASimulationManager::HandleE2Time()
{
	if (AgentThreads.Num() == 0)
	{
		return;
	}

	// Static light by default: do not advance the diurnal clock. The old wall-clock-driven cycle
	// made the light phase depend on sim speed (train/validation mismatch, non-reproducible).
	if (!bEnableDiurnalLightCycle)
		return;

	TickCount++;
	if (TickCount % TicksPerHour == 0) // One hour has passed
	{
		Hour++;
		if (Hour == 24)
			Hour = 0;
		if(AgentThreads.Num() > 0)
		{
			DataGridOrganizer->AdaptE2DataGrid(Hour);
			for (const auto AgentTask : AgentTasks)
			{
				if (AgentTask->GetShouldTerminate()) continue;
				AgentTask->Agent->UpdateGrid(DataGridOrganizer->DataGrid);
			}
		}
	}
}

void ASimulationManager::ReportAgentGenerationResult(const float Fitness, const FGenome Genome)
{
	{
		GenomeFitnessPairs.Add(FGenomeFitnessPair(Fitness, Genome));
		
		NumFinishedAgents++;
	}
	
	// When all agents have reported their results, finish this generation
	if (NumFinishedAgents >= Agents.Num()) FinishGeneration();	
}

void ASimulationManager::FinishGeneration()
{
	if(!bIsTraining)
	{
		AnalyzeValidationSim();
		CleanupSimulation();

		// Batch validation mode: advance to the next (genome seed, RNG seed) pair, or stop.
		if (bBatchValidation)
		{
			if (AdvanceBatchValidationOrFinish())
				return;
			UE_LOG(LogTemp, Warning, TEXT("Batch validation finished (genome seeds %d-%d x RNG seeds %d-%d, %s/%s)."),
				BatchValGenomeSeedStart, BatchValGenomeSeedEnd, BatchValRngSeedStart, BatchValRngSeedEnd,
				*GetExperimentLabel(), *ValidationScenarioLabel);
			if (bBatchValQuitWhenDone)
				FPlatformMisc::RequestExit(false);
		}
		return;
	}
	
	// Average + Max Fitness of Generation X
	float AverageFitness = 0.f;
	float MaxFitness = -100000.f;
	FString BestGenome;
	for(const auto GenomeFitnessPair : GenomeFitnessPairs)
	{
		AverageFitness += GenomeFitnessPair.FitnessScore;
		if (GenomeFitnessPair.FitnessScore > MaxFitness)
		{
			MaxFitness = GenomeFitnessPair.FitnessScore;
			BestGenome = UAgentBrainComponent::GetStringForGenome(GenomeFitnessPair.Genome);
		}
	}
	AverageFitness /= GenomeFitnessPairs.Num();
	// Get Genomes from the last generation
	auto Genomes = TArray<FGenome>();
	for (const auto& GenomeFitnessPair : GenomeFitnessPairs)
		Genomes.Add(GenomeFitnessPair.Genome);
	
	int TotalSettlers = 0;
	int CorrectSettlers = 0;
	int BoundaryContacts = 0;
	for (int32 i = 0; i < Agents.Num(); ++i)
	{
		const auto Agent = Agents[i];
		BoundaryContacts += Agent->GetBoundaryContactCount();

		const FVector FinalLocation = i < AgentTasks.Num()
			? AgentTasks[i]->GetAgentStatus().Transform.GetLocation()
			: Agent->GetActorLocation();
		auto Cell = DataGridOrganizer->DataGrid->GetCellAtPoint(FinalLocation);
		if (Agent->IsSettled())
		{
			TotalSettlers++;
			if (Cell.Data.bIsReefCell)
			{
				CorrectSettlers++;
			}
		}
	}
	LastTotalSettlers = TotalSettlers;
	LastCorrectSettlers = CorrectSettlers;
	LastBoundaryContacts = BoundaryContacts;

	const auto Diversity = UGenomeFunctions::EvaluateGeneticDiversity(Agents.Num(), Genomes);
	if (bBatchSeeds)
		WriteTrainingGenerationRow(AverageFitness, MaxFitness, Diversity, TotalSettlers, CorrectSettlers, BoundaryContacts);

	// Per-generation wall-clock timing for perf benchmarking (logged for any batch-training run).
	if (bBatchSeeds)
	{
		const double Now = FPlatformTime::Seconds();
		UE_LOG(LogTemp, Warning, TEXT("[BENCH] gen %d: %.3fs (pop=%d steps=%d settlers=%d/%d avgFit=%.3f)"),
			GenerationNumber, Now - BenchGenStartSeconds, PopulationSize, MaxSimSteps, CorrectSettlers, TotalSettlers, AverageFitness);
		BenchGenStartSeconds = Now;
	}

	OnGenerationFinished.Broadcast(AverageFitness, GenerationNumber, Diversity, MaxFitness, CorrectSettlers, BestGenome);
	GenerationNumber++;

	// End simulation if max generations reached
	if(GenerationNumber >= MaxGenerations)
	{
		SaveGenomesForValidation();
		CleanupSimulation();

		// Batch mode: advance to the next seed, or stop when the sweep is done.
		if (bBatchSeeds)
		{
			UE_LOG(LogTemp, Warning, TEXT("[BENCH] seed %d total: %.3fs (%d gens, pop=%d, steps=%d)"),
				CurrentBatchSeed, FPlatformTime::Seconds() - BenchRunStartSeconds, MaxGenerations, PopulationSize, MaxSimSteps);
			if (AdvanceBatchSeedOrFinish())
				return;
			UE_LOG(LogTemp, Warning, TEXT("Batch training finished (seeds %d-%d, %s/%s)."),
				BatchSeedStart, BatchSeedEnd, *GetExperimentLabel(), *GetMethodLabel());
			if (bBatchQuitWhenDone)
				FPlatformMisc::RequestExit(false);
		}
		return;
	}
	
	const TArray<FGenome> ParentGenomes = UEvolutionManager::Selection(GenomeFitnessPairs, SelectionStrategy, SelectionRate, RestSelectionRate);
	
	// Prepare for and start the next generation
	CleanupSimulation();

	if(Experiment == EExperiment::E1 && GenerationNumber % TileDropFrequency == 0)
		InitForNextGenerationWithNewLimestoneTiles(ParentGenomes);
	else
		InitForNextGeneration(ParentGenomes);
}

void ASimulationManager::UpdateSimStepDelay(float NewSimStepDelay)
{
	CurrentSimStepDelay = NewSimStepDelay;
	for (const auto AgentTask : AgentTasks)
	{
		AgentTask->SetUpdateDelay(NewSimStepDelay / 1000);
	}
}

void ASimulationManager::CleanupSimulation()
{
	StopGeneration();

	for (const auto AgentThread : AgentThreads)
	{
		if (AgentThread)
		{
			AgentThread->WaitForCompletion();
			delete AgentThread;
		}
	}

	for (const auto AgentTask : AgentTasks)
	{
		delete AgentTask;
	}

	AgentThreads.Empty();
	AgentTasks.Empty();
	GenomeFitnessPairs.Empty();
	NumFinishedAgents = 0;
}

void ASimulationManager::BeginDestroy()
{
	CleanupSimulation();
	Super::BeginDestroy();
}

void ASimulationManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CleanupSimulation();
	Super::EndPlay(EndPlayReason);
}

