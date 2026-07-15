#include "Agent/AgentBrainComponent.h"
#include <cassert>
#include <sstream>
#include <string>
#include <iomanip>
#include <set>
#include <stack>
#include "GenomeAnalysisFunctions.h"
#include "Agent/NeuralNetFunctions.h"

UAgentBrainComponent::UAgentBrainComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

FNeuralNet UAgentBrainComponent::CreateWiringForGenome(const FGenome& Genome, const int NumNeurons)
{
	TArray<FGene> Connections;
	FNodeMap NodeMap;
	MakeRenumberedConnectionList(Connections, Genome, NumNeurons);
	MakeNodeList(NodeMap, Connections);
	RemoveUnusedNeurons(NodeMap, Connections);
	return CreateNeuralNet(Connections, NodeMap);
}

FNeuralNet UAgentBrainComponent::CreateNeuralNet(TArray<FGene>& Connections, FNodeMap& NodeMap)
{
	FNeuralNet NeuralNet;
	// Fix numbers of neurons after removing unused neurons
	uint16_t NewNumber = 0;
	for (auto & Node : NodeMap) {
		Node.Value.RemappedNumber = NewNumber++;
	}
	// Renumber the neurons in the connections
	NeuralNet.Connections.Empty();
	for (auto const &Conn : Connections) {
		if (Conn.bTargetType == GNeuron) {
			NeuralNet.Connections.Add(Conn);
			auto &NewConn = NeuralNet.Connections.Last();
			// fix the destination neuron number
			if (const auto* SinkNode = NodeMap.Find(NewConn.TargetIdx)) {
				NewConn.TargetIdx = SinkNode->RemappedNumber;
			}
			// if the source is a neuron, fix its number too
			if (NewConn.bSourceType == GNeuron) {
				if (const auto* SourceNode = NodeMap.Find(NewConn.SourceIdx)) {
					NewConn.SourceIdx = SourceNode->RemappedNumber;
				}
			}
		}
	}
	// Add the connections to actions
	for (auto const &Connection : Connections) {
		if (Connection.bTargetType == GAction) {
			NeuralNet.Connections.Add(Connection);
			auto &newConn = NeuralNet.Connections.Last();
			// if the source is a neuron, fix its number
			if (newConn.bSourceType == GNeuron) {
				if (const auto Node = NodeMap.Find(newConn.SourceIdx))
					newConn.SourceIdx = Node->RemappedNumber;
			}
		}
	}
	// Create the neural node list for the agent
	NeuralNet.Neurons.Empty();
	for (const auto& NeuronEntry : NodeMap) {
		NeuralNet.Neurons.Add( {} );
		NeuralNet.Neurons.Last().bDriven = (NeuronEntry.Value.NumInputsFromSensorsOrOtherNeurons != 0);
		NeuralNet.Neurons.Last().Output = InitialNeuronOutput();
	}
	return NeuralNet;
}

FString UAgentBrainComponent::GetStringForGenome(FGenome Genome)
{
	std::stringstream ss;
	for (size_t i = 0; i < Genome.Genes.Num(); ++i)
	{
		FGene Gene = Genome.Genes[i];
		assert(sizeof(Gene) == 4);
		uint32_t n;
		std::memcpy(&n, &Gene, sizeof(n));
		ss << std::hex << std::setfill('0') << std::setw(8) << n;
		// Add a space between genes, except for the last gene
		if (i < Genome.Genes.Num() - 1)
		{
			ss << " ";
		}
	}
	ss << std::dec << std::endl;
	return FString(ss.str().c_str());
}

FString UAgentBrainComponent::PrintDotGraph(FNeuralNet Net)
{
	std::stringstream ss;
	ss << "digraph NeuralNet {\n";
	ss << "rankdir=TB;\n";
	ss << "node [style=filled];\n";
	for (const auto& Connection : Net.Connections)
	{
		if (Connection.bSourceType == GSensor)
		{
			ss << SensorShortName(static_cast<ESensorType>(Connection.SourceIdx)) << " [fillcolor=coral3];\n";
		}
		else
		{
			ss << "N" << std::to_string(Connection.SourceIdx) << " [fillcolor=cornsilk2];\n";
		}
	}
	for (const auto& Connection : Net.Connections)
	{
		if (Connection.bTargetType == GAction)
		{
			ss << ActionShortName(static_cast<EActionType>(Connection.TargetIdx)) << " [fillcolor=darkseagreen4];\n";
		}
	}
	for (const auto& Connection : Net.Connections)
	{
		if (Connection.bSourceType == GSensor)
		{
			ss << SensorShortName(static_cast<ESensorType>(Connection.SourceIdx));
		}
		else
		{
			ss << "N" << std::to_string(Connection.SourceIdx);
		}
		ss << " -> ";
		if (Connection.bTargetType == GAction)
		{
			ss << ActionShortName(static_cast<EActionType>(Connection.TargetIdx));
		}
		else
		{
			ss << "N" << std::to_string(Connection.TargetIdx);
		}
		auto str = std::to_string(round(Connection.WeightAsFloat() * 100.0) / 100.0);
		//ss << " [label=\"" << str.substr(0, str.length() - 4) << "\"];\n";
		ss << " [label=\"" << str << "\"];\n";
	}
	ss << "}\n";
	return FString(ss.str().c_str());
}

void UAgentBrainComponent::MakeRenumberedConnectionList(TArray<FGene>& Connections, const FGenome& Genome, const unsigned NumNeurons)
{
	assert(NumNeurons > 0);
	Connections.Empty();
	for (const auto& Gene : Genome.Genes)
	{
		Connections.Add(Gene);
		auto& ModifiedGene = Connections.Last();

		// Adjust source and target indices based on their types
		if (ModifiedGene.bSourceType == GNeuron)  
			ModifiedGene.SourceIdx %= NumNeurons;
		else  // SENSOR
			ModifiedGene.SourceIdx %= ESensorType::NUM_SENSORS;

		if (ModifiedGene.bTargetType == GNeuron)  
			ModifiedGene.TargetIdx %= NumNeurons;
		else  // ACTION
			ModifiedGene.TargetIdx %= EActionType::NUM_ACTIONS;
	}
}

void UAgentBrainComponent::MakeNodeList(FNodeMap& NodeMap, const TArray<FGene>& Connections)
{
	NodeMap.Empty();
	for (const auto &Connection : Connections)
	{
		if (Connection.bTargetType == GNeuron)
		{
			FNeuralNode& TargetNode = NodeMap.FindOrAdd(Connection.TargetIdx);
			if (Connection.SourceIdx == Connection.TargetIdx && Connection.bSourceType == GNeuron) {
				TargetNode.NumSelfInputs++;  // Correctly handles self-loops
			} else {
				TargetNode.NumInputsFromSensorsOrOtherNeurons++;  // Input from another neuron or a sensor
			}
		}
		if (Connection.bSourceType == GNeuron) {
			FNeuralNode& SourceNode = NodeMap.FindOrAdd(Connection.SourceIdx);
			if (Connection.bTargetType == GNeuron || Connection.bTargetType == GSensor) {
				SourceNode.NumOutputs++;  // Ensure only neuron-to-neuron or neuron-to-sensor outputs are counted
			}
		}
	}	
}

void UAgentBrainComponent::RemoveUnusedNeurons(FNodeMap& NodeMap, TArray<FGene>& Connections)
{
	bool bMadeChanges = false;
	TArray<uint16_t> NeuronsToRemove;
	for (auto It = NodeMap.CreateConstIterator(); It; ++It) {

		std::set<int> VisitedNeurons;
		bool IsValid = CanReachAction(It.Key(), Connections, VisitedNeurons);
		if (!IsValid)
		{
			NeuronsToRemove.Add(It.Key());
			bMadeChanges = true;
		}
	}
	for (uint16_t NeuronIdx : NeuronsToRemove) {
		RemoveConnectionsToNeuron(Connections, NodeMap, NeuronIdx);
		NodeMap.Remove(NeuronIdx);
	}
	if (bMadeChanges) {
		RemoveUnusedNeurons(NodeMap, Connections);
	}
}

bool UAgentBrainComponent::CanReachAction(int NeuronIdx, const TArray<FGene>& Connections, std::set<int>& Visited)
{
	std::stack<int> Stack;
	Stack.push(NeuronIdx);
	while (!Stack.empty())
	{
		int current = Stack.top();
		Stack.pop();
		if (Visited.find(current) != Visited.end()) continue;
		
		Visited.insert(current);
		for (const auto& Connection : Connections)
		{
			if (Connection.bSourceType == GNeuron && Connection.SourceIdx == current)
			{
				if (Connection.bTargetType == GAction) 
					return true; // Found a path to an action
				if (Connection.bTargetType == GNeuron) 
					Stack.push(Connection.TargetIdx);
			}
		}
	}
	return false;
}

// During the culling process we remove any neuron that have no output and all feeding connections to that neuron
void UAgentBrainComponent::RemoveConnectionsToNeuron(TArray<FGene>& Connections, FNodeMap& NodeMap, const uint16_t NeuronIdx)
{
	for(int32 i = 0 ; i < Connections.Num(); i++)
	{
		FGene& Connection = Connections[i];
		if (Connection.bTargetType == GNeuron && Connection.TargetIdx == NeuronIdx)
		{
			FNeuralNode* SourceNode = NodeMap.Find(Connection.SourceIdx); // What is going into the node
			FNeuralNode* TargetNode = NodeMap.Find(Connection.TargetIdx); // Looking at this node
			// Self-inputs
			if(Connection.SourceIdx == Connection.TargetIdx)
			{
				TargetNode->NumSelfInputs--;
				TargetNode->NumOutputs--;
			}
			else // Neuron with no output
			{
				TargetNode->NumInputsFromSensorsOrOtherNeurons--;
				if(SourceNode)
					SourceNode->NumOutputs--;
			}
			
			Connections.RemoveAt(i);
			i--;
		}
	}
}

