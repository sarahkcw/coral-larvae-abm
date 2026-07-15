#pragma once
#include <set>
#include "CoreMinimal.h"
#include "GenomeFunctions.h"
#include "Components/ActorComponent.h"
#include "AgentBrainComponent.generated.h"

UCLASS(ClassGroup=(Custom))
class CORAL_LARVAE_ABM_API UAgentBrainComponent : public UActorComponent
{
	GENERATED_BODY()
	
public:	
	UAgentBrainComponent();

	static FNeuralNet CreateWiringForGenome(const FGenome& Genome, int NumNeurons = 1);
	static FString GetStringForGenome(FGenome Genome);
	static FString PrintDotGraph(FNeuralNet Net);
	
private:
	static void MakeRenumberedConnectionList(TArray<FGene>& Connections, const FGenome& Genome, const unsigned NumNeurons);
	static void MakeNodeList(FNodeMap& NodeMap, const TArray<FGene>& Connections);
	static void RemoveUnusedNeurons(FNodeMap& NodeMap, TArray<FGene>& Connections);

	static FNeuralNet CreateNeuralNet(TArray<FGene>& Connections, FNodeMap& NodeMap);
	
	static bool CanReachAction(int NeuronIdx, const TArray<FGene>& Connections, std::set<int>& Visited);
	static void RemoveConnectionsToNeuron(TArray<FGene>& Connections, FNodeMap& NodeMap, const uint16_t NeuronIdx);
};