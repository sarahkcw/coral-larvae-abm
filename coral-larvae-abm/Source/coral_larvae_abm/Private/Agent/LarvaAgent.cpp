#include "Agent/LarvaAgent.h"
#include "Actions/BaseActionComponent.h"
#include "Agent/NeuralNetFunctions.h"
#include "DataGrid/DataGridVisualizer.h"
#include <cassert>
#include <cmath>

#include "Kismet/GameplayStatics.h"

namespace
{
	constexpr float UnrealSphereDiameterCm = 100.0f;

	float GetLarvalVisualScale(const float LarvalVolume)
	{
		const float LarvalRadiusCm = FMath::Sqrt(LarvalVolume / (4.0f * PI));
		const float LarvalDiameterCm = LarvalRadiusCm * 2.0f;
		return LarvalDiameterCm / UnrealSphereDiameterCm;
	}
}

ALarvaAgent::ALarvaAgent()
{
	PrimaryActorTick.bCanEverTick = true;	
	
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	this->SetRootComponent(Root);
	VisualSphere = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualSphere"));
	VisualSphere->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
	const ConstructorHelpers::FObjectFinder<UStaticMesh> MeshObj(TEXT("/Script/Engine.StaticMesh'/Engine/EngineMeshes/Sphere.Sphere'"));
	VisualSphere->SetStaticMesh(MeshObj.Object);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Mat(TEXT("MaterialConstant'/Game/Materials/AgentColor'"));
	
	if (Mat.Succeeded())
		BaseMaterial = Mat.Object;

	// Add a sphere as larva visual representation
	VisualSphere->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));
	VisualSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VisualSphere->SetCastShadow(false);
	VisualSphere->SetWorldScale3D(FVector(GetLarvalVisualScale(AgentParams.LarvalVolume)));

	AgentBrain = CreateDefaultSubobject<UAgentBrainComponent>(TEXT("AgentBrain"));
	DataGrid = CreateDefaultSubobject<UDataGrid>(TEXT("DataGrid"));
}

void ALarvaAgent::InitSensors()
{
	FSensorInitParams InitParams;
	InitParams.MaxSensingDistance = AgentParams.MaxSensingDistance;
	InitParams.MaxSensingAngle = AgentParams.SensingAngle;
	InitParams.InitialAgentPosition = GetActorLocation();
	InitParams.InitialAgentRotation = GetActorRotation();
	InitParams.InitialEnergyResources = 100.0f;

	auto SensorClasses = UNeuralNetFunctions::GetSensorClasses(NeuralNet);
	for (const auto SensorClass : SensorClasses)
	{
		// Reuse a pooled component of this class if one exists (see SensorPool); only create a new
		// UObject component on a cache miss. Avoids destroy+recreate every generation, which was the
		// dominant per-generation cost. The active Sensors set is still exactly the genome's sensors.
		ULarvalSensorBaseComponent* SensorComp = nullptr;
		if (ULarvalSensorBaseComponent** Pooled = SensorPool.Find(SensorClass))
		{
			SensorComp = *Pooled;
			SensorPool.Remove(SensorClass);
		}
		else
		{
			SensorComp = Cast<ULarvalSensorBaseComponent>(this->AddComponentByClass(SensorClass, true, FTransform(), true));
		}
		assert(SensorComp != nullptr);
		Sensors.Add(SensorComp);
		SensorComp->InitSensor(InitParams);
	}

	// Build the ESensorType-indexed lookup once, so the per-step NN forward pass can resolve a
	// sensor component by O(1) array index instead of AActor::GetComponentByClass (a linear scan
	// over all components, previously done for every sensor connection every step). Exact-class
	// match (not IsA) so a derived-sensor component is never mis-mapped to a base-class enum.
	SensorByType.Init(nullptr, ESensorType::NUM_SENSORS);
	for (const FGene& Connection : NeuralNet.Connections)
	{
		if (Connection.bSourceType != GSensor)
			continue;
		const int32 TypeIndex = Connection.SourceIdx;
		if (!SensorByType.IsValidIndex(TypeIndex) || SensorByType[TypeIndex] != nullptr)
			continue;
		const TSubclassOf<ULarvalSensorBaseComponent> WantedClass = GetSensorClassFromEnum(static_cast<ESensorType>(TypeIndex));
		if (!WantedClass)
			continue;
		for (ULarvalSensorBaseComponent* Sensor : Sensors)
		{
			if (Sensor && Sensor->GetClass() == WantedClass)
			{
				SensorByType[TypeIndex] = Sensor;
				break;
			}
		}
	}
}

void ALarvaAgent::InitActions()
{
	FActionInitParams InitParams;
	InitParams.LarvalSize = AgentParams.LarvalVolume;
	InitParams.MaxForwardStrength = AgentParams.MaxForwardStrength;
	InitParams.MaxRotationAnglePerStep = AgentParams.MaxRotationAnglePerStep;
	InitParams.MinSettlementAge = AgentParams.SettlementCompetencyAge;
	
	auto ActionClasses = UNeuralNetFunctions::GetActionClasses(NeuralNet);
	for (const auto ActionClass : ActionClasses)
	{
		// Reuse a pooled action component of this class if present (see ActionPool); create only on a
		// cache miss. Same rationale as sensors: avoid per-generation UObject component churn while
		// keeping the active Actions set exactly the genome's actions (so behavior is unchanged).
		UClass* ActionClassPtr = ActionClass;
		UBaseActionComponent* ActionComp = nullptr;
		if (UBaseActionComponent** Pooled = ActionPool.Find(ActionClassPtr))
		{
			ActionComp = *Pooled;
			ActionPool.Remove(ActionClassPtr);
		}
		else
		{
			ActionComp = Cast<UBaseActionComponent>(this->AddComponentByClass(ActionClass, true, FTransform(), true));
		}
		assert(ActionComp != nullptr);
		Actions.Add(ActionComp);
		ActionComp->InitAction(InitParams);
	}
}

void ALarvaAgent::BeginPlay()
{
	if (BaseMaterial)
	{
		DynamicMaterialInstance = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		if (DynamicMaterialInstance)
			VisualSphere->SetMaterial(0, DynamicMaterialInstance);
	}
	
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("SoundSource"), SoundSources);
	Super::BeginPlay();
}

void ALarvaAgent::BeginDestroy()
{
	if (DynamicMaterialInstance)
	{
		DynamicMaterialInstance = nullptr;
		if(BaseMaterial)
			BaseMaterial = nullptr;
	}
	Super::BeginDestroy();
}

void ALarvaAgent::InitAgent(const FAgentInitParams& AgentInitParams, const UDataGrid* NewDataGrid)
{
	ResetAgent();
	AgentParams = AgentInitParams;

	// Per-larva competency onset (Tay mechanism): draw this larva's own competency age so the
	// population becomes competent at different times. Deterministic under the fixed sim RNG.
	// Drives BOTH the geotactic bias onset (GetGeotacticBias) and settle eligibility
	// (InitActions -> MinSettlementAge), which read AgentParams.SettlementCompetencyAge.
	if (AgentParams.CompetencyAgeMax > AgentParams.CompetencyAgeMin)
	{
		AgentParams.SettlementCompetencyAge = FMath::RandRange(AgentParams.CompetencyAgeMin, AgentParams.CompetencyAgeMax);
	}

	VisualSphere->SetWorldScale3D(FVector(GetLarvalVisualScale(AgentParams.LarvalVolume)));
	
	InitAgentDatabaseComponent(NewDataGrid);
	SetActorLocation(AgentParams.StartLocation);
	SetActorRotation(AgentParams.StartRotation);
	
	if (DynamicMaterialInstance)
		DynamicMaterialInstance->SetVectorParameterValue("Color",  FLinearColor::Blue);
}

void ALarvaAgent::UpdateGrid(const UDataGrid* NewDataGrid)
{
	InitAgentDatabaseComponent(NewDataGrid);
}

void ALarvaAgent::Settle()
{
	AgentStatus.bSettled = true;
	SettlementTime = Age;
	
	FLinearColor Color = FLinearColor::Yellow;
	if(GetGrid()->GetCellAtPoint(GetActorLocation()).Data.bIsReefCell)
		Color = FLinearColor::Green;
	
	if (DynamicMaterialInstance)
	{
		AsyncTask(ENamedThreads::GameThread, [this, Color]()
		{
			if (DynamicMaterialInstance)
				DynamicMaterialInstance->SetVectorParameterValue("Color", Color);
		});
	}
}

void ALarvaAgent::CalculateEnergy(const FTransform& LastTransform, const FTransform& CurrentTransform) const
{
	float BaseEnergyCost = 0.00001f * Age; // Energy consumed per timestep just for being alive
	float MinTemperature = 20.0f; // Minimum temperature for optimal metabolism
	float MaxTemperature = 40.0f; // Maximum temperature for optimal metabolism

	auto CurrentCell = GetGrid()->GetCellAtPoint(CurrentTransform.GetLocation());
	
	// Calculate movement energy cost
	float DistanceMoved = FVector::Dist(LastTransform.GetLocation(), CurrentTransform.GetLocation());
	float MaxDistance = 10000000.0f; // Can travel 
	float MovementEnergyCost = DistanceMoved / MaxDistance; 

	// Consider temperature effects on metabolism
	float TemperatureEffect = 1.0f; // Neutral effect
	float Temperature = CurrentCell.Data.WaterData.Temperature;
	if (Temperature < MinTemperature) {
		TemperatureEffect = FMath::Lerp(1.5f, 1.0f, Temperature / MinTemperature); // Increased energy cost below minimum temperature
	} else if (Temperature > MaxTemperature) {
		TemperatureEffect = FMath::Lerp(1.0f, 1.5f, (Temperature - MaxTemperature) / (MaxTemperature - MinTemperature)); // Increased energy cost above maximum temperature
	}
	
	float TotalEnergyCostPercent = (BaseEnergyCost + MovementEnergyCost) * TemperatureEffect;
	
	float EnergyReduction = AgentParams.EnergyResources * TotalEnergyCostPercent / 100.0f;
	
	// Ensure the energy reduction is not too severe
	float MaxEnergyReduction = AgentParams.EnergyResources * 0.05f; // Cap energy reduction to a maximum of 5% per timestep
	EnergyReduction = FMath::Min(EnergyReduction, MaxEnergyReduction);

	//AgentParams.EnergyResources -= EnergyReduction;
}

void ALarvaAgent::InitAgentDatabaseComponent(const UDataGrid* NewDataGrid)
{
	if (AgentParams.bShareEnvironmentGrid)
	{
		// Share the organizer's read-only grid directly (no per-agent copy). Safe only because the
		// manager sets bShareEnvironmentGrid exactly when nothing mutates the grid during stepping
		// (diurnal cycle off); all agents concurrently READ it, which is race-free. const_cast is
		// benign: the grid's read API is non-mutating (verified) but not marked const.
		SharedDataGrid = const_cast<UDataGrid*>(NewDataGrid);
	}
	else
	{
		// Private per-agent deep copy (original behaviour). Used whenever the grid can change mid-run
		// (diurnal light cycle on -> HandleE2Time re-copies into each agent), so no sharing race.
		SharedDataGrid = nullptr;
		DataGrid->CopyDataFrom(NewDataGrid);
	}
}

void ALarvaAgent::ResetAgent()
{
	// Return the current sensor/action components to their per-class pools for reuse next generation
	// instead of destroying them (GetSensorClasses/GetActionClasses dedupe by class, so at most one
	// component per class is active). This removes the per-generation DestroyComponent + recreate
	// churn that dominated setup cost. Components stay attached but inert while pooled.
	for (const auto Sensor : Sensors) if (Sensor) SensorPool.Add(Sensor->GetClass(), Sensor);
	for (const auto Action : Actions) if (Action) ActionPool.Add(Action->GetClass(), Action);
	Sensors.Empty();
	Actions.Empty();
	SensorByType.Reset();
	AgentStatus.bSettled = false;
	BoundaryContactCount = 0;
	MaxCCAFound = 0.f;
	MaxAlteromonasFound = 0.f;
	Age = 0.f;
	TrajectoryBuffer.Reset();
}

FSensorUpdateParams ALarvaAgent::GetBaseUpdateSensorParams() const
{
	FSensorUpdateParams SensorUpdateParams;
	SensorUpdateParams.Age = Age;
	SensorUpdateParams.OscillationPeriod = AgentStatus.OscillatorPeriod;
	// Other parameters have to be set somewhere else
	return SensorUpdateParams;
}

void ALarvaAgent::InitBrain(FGenome NewGenome, int MaxNumInnerNeurons)
{
	Genome = NewGenome;
	NeuralNet = AgentBrain->CreateWiringForGenome(Genome, MaxNumInnerNeurons);
	// GenomeString / DebugBrain / DebugGenome are editor-inspection-only strings: they are never read
	// by the simulation or by the output/analysis paths (which recompute GetStringForGenome from the
	// genome directly). Building them for every agent every generation -- PrintDotGraph in particular
	// -- is pure overhead in unattended batch runs, so skip unless requested. No behavior/output change.
	if (AgentParams.bComputeDebugStrings)
	{
		GenomeString = AgentBrain->GetStringForGenome(this->Genome);
		DebugBrain = AgentBrain->PrintDotGraph(NeuralNet);
		DebugGenome = GenomeString;
	}
	InitSensors();
	InitActions();
}

FLarvaAgentStatus ALarvaAgent::RunAgentSimStep(int SimStep, FTransform CurrentAgentTransform, FTransform LastActorTransform)
{
	// 4 simulation steps per day // Biologically not correct, but for now we use the simulation step as age
	Age = SimStep;
	
	CurrentAgentTransform.SetLocation(ConstrainToAquariumBounds(CurrentAgentTransform.GetLocation()));

	auto SensorUpdateParams = GetBaseUpdateSensorParams();
	SensorUpdateParams.LastActorTransform = LastActorTransform;
	SensorUpdateParams.ActorTransform = CurrentAgentTransform;
	SensorUpdateParams.SimulationStep = SimStep;
	SensorUpdateParams.Age = SimStep; // Biologically not correct, but for now we use the simulation step as age
	SensorUpdateParams.Energy = AgentParams.EnergyResources;
	
	// Box Chunk - Cones will be computed in the sensor components
	SensorUpdateParams.DataChunk = GetGrid()->GetCellDataAsChunkAt(CurrentAgentTransform.GetLocation(), 1); // Or: GetGrid()->GetBulkData();
	SensorUpdateParams.GlobalExperimentConfig = GetGrid()->GetDataConfig();
	// R1-5 sensory-noise robustness (validation-time; 0 = off): perturb sensor inputs via the
	// per-agent RNG stream inside FeedForward.
	SensorUpdateParams.SensorNoiseStdDev = AgentParams.SensorNoiseStdDev;
	SensorUpdateParams.NoiseRng = &AgentRandomStream;

	// Run the controller: either the evolved neural net, or (R1-3 baseline) a fixed reactive rule.
	const auto Activations = AgentParams.bReactiveRule
		? ComputeReactiveActivations(SensorUpdateParams)
		: UNeuralNetFunctions::FeedForward(NeuralNet, SensorUpdateParams, this);

	// AP1: record the full sensor input vector + action output vector for this step (gated, off
	// by default). Sampling all NUM_SENSORS values directly (rather than only the ones this
	// genome happens to wire) keeps the logged vector a fixed shape across agents/genomes, which
	// is what Plotting/09_nn_cue_ablation.py's offline forward pass expects.
	if (AgentParams.bLogPerStepTrajectory &&
		(AgentParams.TrajectoryLogStride <= 1 || (SimStep % AgentParams.TrajectoryLogStride) == 0))
	{
		FLarvaTrajectoryStep TrajectoryStep;
		TrajectoryStep.SimStep = SimStep;
		TrajectoryStep.Position = CurrentAgentTransform.GetLocation();

		TrajectoryStep.SensorValues.Reserve(ESensorType::NUM_SENSORS);
		for (int32 SensorIdx = 0; SensorIdx < ESensorType::NUM_SENSORS; ++SensorIdx)
		{
			TrajectoryStep.SensorValues.Add(UNeuralNetFunctions::GetSensorFromAgent(this, static_cast<ESensorType>(SensorIdx), SensorUpdateParams));
		}

		TrajectoryStep.ActionValues.Reserve(EActionType::NUM_ACTIONS);
		for (int32 ActionIdx = 0; ActionIdx < EActionType::NUM_ACTIONS; ++ActionIdx)
		{
			TrajectoryStep.ActionValues.Add(Activations.IsValidIndex(ActionIdx) ? std::tanh(Activations[ActionIdx]) : 0.f);
		}

		TrajectoryBuffer.Add(MoveTemp(TrajectoryStep));
	}

	FActionUpdateParams ActionUpdateParams;
	ActionUpdateParams.ActorTransform = CurrentAgentTransform;
	ActionUpdateParams.LarvalAge = Age;
	ActionUpdateParams.OscillatorPeriod = AgentStatus.OscillatorPeriod;
	ActionUpdateParams.Rng = &AgentRandomStream;
	auto CurrentCell = GetGrid()->GetCellAtPoint(CurrentAgentTransform.GetLocation());
	ActionUpdateParams.LightIntensity = CurrentCell.Data.LightData.LightIntensity;
	ActionUpdateParams.EnergyResources = AgentParams.EnergyResources;
	
	// Run the actions based on activations from neural net
	auto Status = UNeuralNetFunctions::ActivateActions(Activations, Actions, ActionUpdateParams);
	Status.Transform = ConstrainTransformToAquariumBounds(Status.Transform);
	if (Status.OscillatorPeriod != AgentStatus.OscillatorPeriod) AgentStatus.OscillatorPeriod = Status.OscillatorPeriod;
	if (Status.bSettled) Settle();

	CalculateReefExplorationBonus(CurrentAgentTransform.GetLocation(), LastActorTransform.GetLocation());
	
	return Status;
}

FVector ALarvaAgent::GetGeotacticBias(int SimStep) const
{
	// Age/competency-gated downward behavioral bias (positive geotaxis), NOT passive buoyancy.
	// Zero before competency, constant downward (-Z toward the substrate) afterwards. Kept below
	// active swim speed so the evolved controller can still override it. See FAgentInitParams.
	if (!AgentParams.bEnableGeotacticBias || SimStep < AgentParams.SettlementCompetencyAge)
		return FVector::ZeroVector;
	return FVector(0.f, 0.f, -FMath::Max(AgentParams.GeotacticDownwardSpeedCmPerStep, 0.f));
}

TArray<float> ALarvaAgent::ComputeReactiveActivations(const FSensorUpdateParams& P) const
{
	// R1-3 baseline: a fixed reactive taxis rule, no evolved network. Directional cue sensors are in
	// [0,1] with 0.5 = no gradient; (sensor-0.5) points up the local cue gradient. CCA (E1) and
	// particle-motion (E3) are summed so the one informative in a given experiment dominates while the
	// other sits near 0.5 (~0 contribution). Constant forward swim + steer toward the gradient + settle
	// when the scalar cue is high. (E2 has no directional settlement cue; there descent still comes from
	// the competency-gated geotactic bias applied outside the controller, so reactive ~ evolved there.)
	TArray<float> A; A.Init(0.f, EActionType::NUM_ACTIONS);
	auto S = [&](ESensorType t){ return UNeuralNetFunctions::GetSensorFromAgent(this, t, P); };
	const float fb = (S(CCA_FORWARD_BACK) - 0.5f) + (S(PARTICLE_MOTION_FORWARD_BACK) - 0.5f);
	const float lr = (S(CCA_LEFT_RIGHT)  - 0.5f) + (S(PARTICLE_MOTION_LEFT_RIGHT)  - 0.5f);
	const float ud = (S(CCA_UP_DOWN)     - 0.5f) + (S(PARTICLE_MOTION_UP_DOWN)     - 0.5f);
	const float scalarCue = FMath::Max(S(CCA), S(PARTICLE_MOTION));
	A[FORWARD]      = 2.0f + 2.0f * fb;   // constant swim, faster when cue is ahead
	A[ROTATE_YAW]   = 3.0f * lr;          // turn toward higher cue (left/right)
	A[ROTATE_PITCH] = 3.0f * ud;          // pitch toward higher cue (up/down)
	A[SET_OSC]      = 0.0f;
	A[SETTLE]       = (scalarCue > AgentParams.ReactiveSettleThreshold) ? 3.0f : -3.0f;
	return A;
}

FVector ALarvaAgent::ConstrainToAquariumBounds(const FVector& Location)
{
	const FDataConfig DataConfig = GetGrid()->GetDataConfig();
	if (DataConfig.CellEdgeLength <= 0.0f)
		return Location;

	const FVector MinCorner = DataConfig.ChunkWorldOrigin;
	const FVector MaxCorner = DataConfig.ChunkWorldOrigin + DataConfig.LocalBounds;
	const float Epsilon = FMath::Max(DataConfig.CellEdgeLength * 0.001f, KINDA_SMALL_NUMBER);

	const FVector ClampedLocation(
		FMath::Clamp(Location.X, MinCorner.X + Epsilon, MaxCorner.X - Epsilon),
		FMath::Clamp(Location.Y, MinCorner.Y + Epsilon, MaxCorner.Y - Epsilon),
		FMath::Clamp(Location.Z, MinCorner.Z + Epsilon, MaxCorner.Z - Epsilon));

	if (!Location.Equals(ClampedLocation, Epsilon))
	{
		BoundaryContactCount++;
	}

	return ClampedLocation;
}

FTransform ALarvaAgent::ConstrainTransformToAquariumBounds(const FTransform& InTransform)
{
	const FVector Location = InTransform.GetLocation();
	const FVector ClampedLocation = ConstrainToAquariumBounds(Location); // clamps + counts contact

	FTransform Out = InTransform;
	Out.SetLocation(ClampedLocation);

	// Tangential slide: on wall contact, remove the into-wall component of the heading so the
	// larva swims along the surface next step instead of pinning nose-first into the wall/corner
	// (elastic reflection is unphysical at this scale/Reynolds number). Purely counted, no penalty.
	if (!Location.Equals(ClampedLocation, KINDA_SMALL_NUMBER))
	{
		FVector Forward = InTransform.GetRotation().GetForwardVector();
		if (!FMath::IsNearlyEqual(Location.X, ClampedLocation.X)) Forward.X = 0.f;
		if (!FMath::IsNearlyEqual(Location.Y, ClampedLocation.Y)) Forward.Y = 0.f;
		if (!FMath::IsNearlyEqual(Location.Z, ClampedLocation.Z)) Forward.Z = 0.f;
		if (!Forward.IsNearlyZero())
			Out.SetRotation(Forward.GetSafeNormal().ToOrientationQuat());
	}

	return Out;
}

void ALarvaAgent::AdaptEnergy(const FTransform& LastTransform, const FTransform& CurrentTransform)
{
	CalculateEnergy(LastTransform, CurrentTransform);
}

float ALarvaAgent::FitnessTmp(FTransform Transform)
{
	return ReefSoundFitness(Transform);
}

float ALarvaAgent::ComputeFitness(const FLarvaAgentStatus& ResultAgentStatus) const
{
	if (AgentParams.bUseE3Fitness)
		return ReefSoundFitness(ResultAgentStatus.Transform);
	if (AgentParams.bUseE2Fitness)
		return ReefGoalFitnessE2(ResultAgentStatus.Transform.GetLocation());
	return ReefGoalFitness(ResultAgentStatus.Transform.GetLocation());
}

void ALarvaAgent::EvaluateLarvaPerformance(const FLarvaAgentStatus& ResultAgentStatus) const
{
	const float Fitness = ComputeFitness(ResultAgentStatus);
	// Interactive (thread-per-agent) path: hop to the game thread to broadcast the result. The batch
	// path does NOT use this — it calls ComputeFitness directly and collects results synchronously
	// after a ParallelFor, avoiding both the per-agent game-thread task and per-agent OS threads.
	AsyncTask(ENamedThreads::GameThread, [this, Fitness]()
	{
		OnSimulationFinishedEvent.Broadcast(Fitness, Genome);
	});
}

#pragma region FitnessFunctions
float ALarvaAgent::BasicDistanceFitness(const FVector& AgentLocation) const
{
	// The higher the distance from the agent start position, the higher the fitness
	return FVector::Dist(AgentLocation, AgentParams.StartLocation);
}

float ALarvaAgent::BasicGoalDistFitness(const FVector& AgentLocation) const
{
	// Calculate the distance from the goal
	float DistanceFromGoal = FVector::Dist(AgentLocation, FVector { 0.0f, 0.0f, 0.0f });
	
	// The smaller the distance, the higher the fitness
	// Avoid division by zero by adding a small epsilon value
	const float Epsilon = 1e-6f;
	return (1.0f / (DistanceFromGoal + Epsilon)) * 1000.0f;
}

float ALarvaAgent::NearestReefDistFitness(const FVector& AgentLocation) const
{
	float SmallestDistance = 10000000.0f;
	for (const auto& Cell : GetGrid()->GetBulkData().Data)
	{
		if (!Cell.Data.bIsReefCell) continue;
		
		auto Distance = FVector::Dist(Cell.WorldPosition, AgentLocation);
		if(Distance < SmallestDistance)
			SmallestDistance = Distance;
	}
	const float Epsilon = 1e-6f;
	float InvertedDistance  = 1.0f / (SmallestDistance + Epsilon); 
	float DistanceScore = InvertedDistance * 500.0f; 
	
	return DistanceScore;
}

void ALarvaAgent::CalculateReefExplorationBonus(const FVector& AgentLocation, const FVector& LastLocation)
{
	auto CellAtLastLocation = GetGrid()->GetCellAtPoint(LastLocation);
	auto CellAtAgentLocation = GetGrid()->GetCellAtPoint(AgentLocation);
	
	// Reward for moving towards a higher gradient	
	float PreviousCCA = CellAtLastLocation.Data.ReefData.CCA;
	float PreviousAlteromonas = CellAtLastLocation.Data.ReefData.Alteromonas;
	
	float CurrentCCA = CellAtAgentLocation.Data.ReefData.CCA;
	float CurrentAlteromonas = CellAtAgentLocation.Data.ReefData.Alteromonas;

	float CCAGradient = CurrentCCA - PreviousCCA;
	float AlteromonasGradient = CurrentAlteromonas - PreviousAlteromonas;
	
	if(CCAGradient > 0.0f)
	{
		if(CurrentCCA > MaxCCAFound)
			MaxCCAFound = CurrentCCA;
	}
	
	if(AlteromonasGradient > 0.0f)
	{
		if(CurrentAlteromonas > MaxAlteromonasFound)
			MaxAlteromonasFound = CurrentAlteromonas;
	}
}
// E1 (CCA settlement) fitness.
float ALarvaAgent::ReefGoalFitness(const FVector& AgentLocation) const
{
	bool bEndsInBounds = UDataGridUtils::IsInBounds(GetGrid()->GetDataConfig(), AgentLocation);
	float AreaQualityScore = 0.0f;

	auto CurrentCell = GetGrid()->GetCellAtPoint(AgentLocation);
	
	if(bEndsInBounds)
	{
		auto CCA = CurrentCell.Data.ReefData.CCA;
		auto Alteromonas = CurrentCell.Data.ReefData.Alteromonas;
		
		AreaQualityScore = (CCA + Alteromonas * 10.f) * 10.0f;
		if (CurrentCell.Data.bIsReefCell)
			AreaQualityScore *= 50.0f;
	}
	
	float NearestDistance = bEndsInBounds ? NearestReefDistFitness(AgentLocation) : 0.f;  

	float SettleScore;
	if(IsSettled())
	{
		if(bEndsInBounds)
		{
			if (CurrentCell.Data.bIsReefCell)
			{
				SettleScore = 2500.0f; // Ultimate goal
			}
			else
			{
				SettleScore = -100.0f;
			}
		}
		else
		{
			SettleScore = -30.0f;
		}
	}
	else
	{
		SettleScore = -30.0f;
	}
	
	float Fitness = NearestDistance + AreaQualityScore + SettleScore; // + ExplorationBonus;
	return Fitness;
}

float ALarvaAgent::ReefGoalFitnessE2(const FVector& AgentLocation) const
{
	// Tay et al. 2011 measured larval VERTICAL POSITION in a tall column over time (net downward
	// migration into the lower column), NOT settlement-substrate choice. So E2 rewards net downward
	// migration from the release point and does NOT force terminal settlement (settling is neutral,
	// as in E3 — a larva that settles simply freezes its position, which is still scored by how far
	// down it got). The downward drive is provided biologically by the age-gated geotactic bias
	// (see FAgentInitParams); this fitness selects controllers that express it. The resulting
	// depth-zone distribution is compared to Tay in validation. NOTE: single-population training
	// cannot reproduce Tay's inter-species vertical spread (documented limitation), and a purely
	// downward reward risks over-accumulation at the very bottom — check the validation distribution.
	const bool bEndsInBounds = UDataGridUtils::IsInBounds(GetGrid()->GetDataConfig(), AgentLocation);
	if (!bEndsInBounds) return -30.f; // left the column entirely

	// Tay mechanism (see FAgentInitParams / findings): reward AGE-APPROPRIATE vertical position,
	// not maximal depth. A larva that reached its own (heterogeneous) competency age is rewarded
	// for having migrated DOWN; a larva that never became competent this run is rewarded for
	// staying UP near the release point. This stops the controller from being selected to
	// turbo-dive (which produced ~96% bottom vs Tay's ~70%) and preserves a not-yet-competent top
	// fraction -> bimodal top/bottom with an empty middle, as Tay observed. Z is up (grid origin Z
	// = bottom); Age = last sim step reached. FIRST-CUT formula: calibrate the CompetencyAge range
	// against Tay's time course (1.9/59.8/69.9% bottom @ day 3.5/10.4/15.7).
	if (Age >= AgentParams.SettlementCompetencyAge)
		return AgentParams.StartLocation.Z - AgentLocation.Z; // competent -> reward downward migration
	return AgentLocation.Z;                                    // never competent -> reward staying high
}

float ALarvaAgent::ReefSoundFitness(const FTransform& AgentTransform) const
{
	if (SoundSources.Num() == 0) return -1000.f;
	
	FVector SoundClusterCenter = FVector::ZeroVector;
	int32 ValidSoundSources = 0;
	for (const AActor* SoundSource : SoundSources)
	{
		if (!SoundSource) continue;
		SoundClusterCenter += SoundSource->GetActorLocation();
		ValidSoundSources++;
	}
	if (ValidSoundSources == 0) return -1000.f;
	SoundClusterCenter /= static_cast<float>(ValidSoundSources);
	
	auto CurrentCell = GetGrid()->GetCellAtPoint(AgentTransform.GetLocation());
	
	const float Epsilon = 1e-6f;
	const float InitialDistance = FMath::Max(FVector::Dist(AgentParams.StartLocation, SoundClusterCenter), Epsilon);
	const float CurrentDistance = FVector::Dist(AgentTransform.GetLocation(), SoundClusterCenter);
	const float DistanceScore = (1.0f - (CurrentDistance / InitialDistance)) * 5000.0f;
	
	// Reward proximity to the acoustic field generated by the whole speaker cluster.
	const float ScaledParticleMotion = FMath::Pow(CurrentCell.Data.WaterData.ParticleMotion, 2.0f);
	const float ParticleMotionScore = ScaledParticleMotion * 5000.0f;
	const float MovementScore = (InitialDistance - CurrentDistance) * 500.0f;
	
	return DistanceScore + ParticleMotionScore + MovementScore;
}
#pragma endregion	

