#include "Experiments/ResultAnalysisFunctions.h"
#include "DataGrid/DataGridUtils.h"
//#include "Eigen/Dense"
#include <cmath>
#include <limits>
#include <numeric>


void UResultAnalysisFunctions::SaveGenomesToFile(TArray<FGenomeFitnessPair> GenomeFitnessPairs, const FString& FilePath)
{
	FString OutputString;
	for (const auto& GenomeFitnessPair : GenomeFitnessPairs)
	{
		FString GenomeString = UAgentBrainComponent::GetStringForGenome(GenomeFitnessPair.Genome);
		OutputString += FString::Printf(TEXT("%s; %f\n"), *GenomeString, GenomeFitnessPair.FitnessScore);
	}

	FFileHelper::SaveStringToFile(OutputString, *FilePath);
}

TArray<FGenomeFitnessPair> UResultAnalysisFunctions::LoadGenomesFromFile(const FString& FilePath)
{
	TArray<FGenomeFitnessPair> GenomeFitnessPairs;
	FString FileContent;

	if (FFileHelper::LoadFileToString(FileContent, *FilePath))
	{
		TArray<FString> Lines;
		FileContent.ParseIntoArrayLines(Lines);

		for (int32 i = 0; i < Lines.Num(); i += 2) // Process two lines at a time
		{
			if (i + 1 < Lines.Num()) // Ensure there is a corresponding fitness line
			{
				FString GenomeString = Lines[i]; // First line is the genome string
				FString FitnessString = Lines[i + 1]; // Second line is the fitness value

				// Remove leading and trailing whitespace
				FitnessString = FitnessString.TrimStartAndEnd();

				// Remove leading semicolon, if present
				if (FitnessString.StartsWith(TEXT(";")))
				{
					FitnessString = FitnessString.RightChop(1);
				}

				// Convert the genome string back into an FGenome
				FGenome Genome = UGenomeFunctions::GetGenomeFromString(GenomeString);

				// Convert the fitness string to a float
				float Fitness = FCString::Atof(*FitnessString);

				// Create a pair and add it to the array
				FGenomeFitnessPair GenomeFitnessPair;
				GenomeFitnessPair.Genome = Genome;
				GenomeFitnessPair.FitnessScore = Fitness;
				GenomeFitnessPairs.Add(GenomeFitnessPair);
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("Mismatched genome and fitness lines in the file."));
			}
		}
	}

	return GenomeFitnessPairs;
}

float UResultAnalysisFunctions::NearestNeighborDistances(TArray<FVector>& Positions)
{
	TArray<float> Distances;
	for (int i = 0; i < Positions.Num(); i++)
	{
		float MinDistance = FLT_MAX;
		for (int j = 0; j < Positions.Num(); j++)
		{
			if (i == j) continue;
			float Distance = FVector::Dist(Positions[i], Positions[j]);
			if (Distance < MinDistance)
			{
				MinDistance = Distance;
			}
		}
		Distances.Add(MinDistance);
	}

	float Sum = 0;
	for (auto Distance : Distances)
	{
		Sum += Distance;
	}
	return Sum / Distances.Num();
}

float UResultAnalysisFunctions::ClarkeAndEvansR(TArray<FVector>& Positions, float Area)
{
	float NND = NearestNeighborDistances(Positions);
	float DensityOfLarvae = Positions.Num() / Area;
	float ExpectedNND = 0.5 * sqrt(DensityOfLarvae);
	float R = NND / ExpectedNND;
	return R;
}

float UResultAnalysisFunctions::MonteCarloForSignificanceTesting(const FDataChunk& DataChunk,
                                                                 TArray<TArray<FVector>>& AllRunPositions,
                                                                 TArray<float>& Areas)
{
	int NumSimulations = 999;
	int NumLarvae = 100;
	float TotalPValue = 0.0;
	// Loop over all runs (each run has its own positions and area)
	for (int run = 0; run < AllRunPositions.Num(); run++)
	{
		TArray<FVector>& Positions = AllRunPositions[run]; // Positions for this run
		float Area = Areas[run]; // Area for this run

		// Calculate observed R for this run
		float ObservedR = ClarkeAndEvansR(Positions, Area);
		int Count = 0;

		// Monte Carlo simulations for this run
		for (int i = 0; i < NumSimulations; i++)
		{
			auto RandomPositions = ShufflePositionsOnTiles(DataChunk, NumLarvae);
			float RandomR = ClarkeAndEvansR(RandomPositions, Area);

			if (RandomR >= ObservedR)
			{
				Count++;
			}
		}

		// Calculate p-value for this run
		float PValue = (float)Count / (float)NumSimulations;
		TotalPValue += PValue;
	}

	// Return the average p-value across all runs
	return TotalPValue / AllRunPositions.Num();
}

FGLMResult UResultAnalysisFunctions::BinomialGeneralizedLinearModels(const TArray<float>& SubstrateAreas,
                                                                     const TArray<int>& SettlementResults)
{
	// Convert TArray to Eigen vectors
	/*Eigen::VectorXf eigenSubstrateAreas = ConvertTArrayToEigen(SubstrateAreas); // This will now work correctly
	Eigen::VectorXi eigenSettlementResults = ConvertTArrayToEigen(SettlementResults); // This will now work correctly

	const int n = eigenSubstrateAreas.size();

	// Design matrix X (n x 2) -> first column is 1s (intercept), second column is SubstrateAreas
	Eigen::MatrixXf X(n, 2);
	X.col(0) = Eigen::VectorXf::Ones(n); // Intercept term (all ones)
	X.col(1) = eigenSubstrateAreas; // Substrate area term

	// Initialize Beta (coefficients): two coefficients (Intercept and Slope)
	Eigen::VectorXf beta = Eigen::VectorXf::Zero(2); // Beta(0) and Beta(1)

	// Maximum iterations for convergence
	const int maxIterations = 100;
	const float tolerance = 1e-6;

	Eigen::MatrixXf W; // Weight matrix

	// IRLS iteration
	for (int iter = 0; iter < maxIterations; ++iter)
	{
		// Step 1: Compute predicted probabilities p using current beta
		Eigen::VectorXf linearPredictor = X * beta; // X * beta -> linear term
		Eigen::VectorXf p = (1.0 / (1.0 + (-linearPredictor.array()).exp())).matrix(); // Logistic function

		// Step 2: Compute diagonal weight matrix W (based on p)
		Eigen::VectorXf w = (p.array() * (1.0 - p.array())).matrix(); // w = p * (1 - p)
		W = w.asDiagonal();

		// Step 3: Compute the gradient and Hessian for beta update
		Eigen::VectorXf z = linearPredictor.array() + (eigenSettlementResults.cast<float>().array() - p.array()) / w.
			array(); // Cast eigenSettlementResults to float
		Eigen::MatrixXf XtWX = X.transpose() * W * X;
		Eigen::VectorXf XtWz = X.transpose() * W * z;

		// Step 4: Update beta coefficients
		Eigen::VectorXf beta_new = XtWX.ldlt().solve(XtWz); // Solve (X^T W X) beta = X^T W z

		// Check for convergence
		if ((beta_new - beta).norm() < tolerance)
		{
			beta = beta_new;
			break;
		}
		beta = beta_new;
	}

	// Step 5: Compute standard errors of coefficients (inverse of XtWX diagonal)
	Eigen::MatrixXf XtWX = X.transpose() * W * X;
	Eigen::MatrixXf covMatrix = XtWX.inverse(); // Covariance matrix of the coefficients
	float stdErrorIntercept = std::sqrt(covMatrix(0, 0));
	float stdErrorSlope = std::sqrt(covMatrix(1, 1));

	// Step 6: P-value calculation using normal CDF approximation
	float zValue = beta(1) / stdErrorSlope;
	float pValue = CalculatePValue(zValue);

	// Return the result*/
	FGLMResult Result;
	//Result.Intercept = beta(0); // Intercept term Beta(0)
	//Result.Slope = beta(1); // Slope term Beta(1)
	//Result.StdError = stdErrorSlope; // Standard error of Beta(1)
	//Result.PValue = zValue; // P-value for Beta(1)
	return Result;
}

int UResultAnalysisFunctions::Aggregation(TArray<FVector>& Positions, float LarvalVolume)
{
	// radius of a sphere with the same volume as the larva
	const float Radius = 50 * 0.07;
	float LarvalSize = 2 * Radius;
	int DirectContacts = 0;
	int TotalPairs = 0;

	for (int i = 0; i < Positions.Num(); i++)
	{
		for (int j = i + 1; j < Positions.Num(); j++)
		{
			float Distance = FVector::Dist(Positions[i], Positions[j]);

			if (Distance < LarvalSize)  // Check for direct contact
			{
				DirectContacts++;
			}

			TotalPairs++;
		}
	}
	// Define as binary: 1 = aggregated, 0 = not aggregated
	return (DirectContacts > 0) ? 1 : 0;
}

TArray<int> UResultAnalysisFunctions::CalcAggregationResults()
{
	const FString FilePath = FPaths::ProjectContentDir() / TEXT("Evolution/positions.txt");
	TArray<int> AggregationResults;
	TArray<FString> FileLines;
	// Step 1: Read the text file into an array of lines
	if (!FFileHelper::LoadFileToStringArray(FileLines, *FilePath))
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to load the file!"));
		return AggregationResults; // Return empty array if file reading fails
	}

	// Step 2: Process each line (each line is one run)
	for (const FString& Line : FileLines)
	{
		// Split the line into individual position components (assuming space-separated X, Y, Z values)
		TArray<FString> PositionComponents;
		Line.ParseIntoArray(PositionComponents, TEXT("\t"), true);

		TArray<FVector> Positions;

		// Step 3: Parse the position components into FVector (three values per position)
		for (int i = 0; i < PositionComponents.Num(); i += 3)
		{
			if (i + 2 < PositionComponents.Num())
			{
				float X = FCString::Atof(*PositionComponents[i]);
				float Y = FCString::Atof(*PositionComponents[i + 1]);
				float Z = FCString::Atof(*PositionComponents[i + 2]);

				Positions.Add(FVector(X, Y, Z)); // Add parsed FVector to Positions array
			}
		}

		// Step 4: Calculate aggregation for this run and add result to AggregationResults
		float LarvalVolume = 0.07f;  // Replace with actual volume if necessary
		int AggregationResult = Aggregation(Positions, LarvalVolume);
		AggregationResults.Add(AggregationResult);
	}
	
	return AggregationResults;
}

FGLMResult UResultAnalysisFunctions::BinomialGLMForAggregation(const TArray<float>& PatchSizes,
                                                               const TArray<int>& AggregationResults)
{
	// Convert TArray to Eigen vectors
    /*Eigen::VectorXf eigenPatchSizes = ConvertTArrayToEigen(PatchSizes);
    Eigen::VectorXi eigenAggregationResults = ConvertTArrayToEigen(AggregationResults);

    const int n = eigenPatchSizes.size();

    // Design matrix X (n x 2) -> first column is 1s (intercept), second column is PatchSizes
    Eigen::MatrixXf X(n, 2);
    X.col(0) = Eigen::VectorXf::Ones(n); // Intercept term (all ones)
    X.col(1) = eigenPatchSizes; // Patch size term

    // Initialize Beta (coefficients): two coefficients (Intercept and Slope)
    Eigen::VectorXf beta = Eigen::VectorXf::Zero(2);

    // Maximum iterations and tolerance for convergence
    const int maxIterations = 100;
    const float tolerance = 1e-6;

    Eigen::MatrixXf W; // Weight matrix

    // IRLS iteration
    for (int iter = 0; iter < maxIterations; ++iter)
    {
        // Step 1: Compute predicted probabilities p using current beta
        Eigen::VectorXf linearPredictor = X * beta;
        Eigen::VectorXf p = (1.0 / (1.0 + (-linearPredictor.array()).exp())).matrix();

        // Step 2: Compute diagonal weight matrix W (based on p)
        Eigen::VectorXf w = (p.array() * (1.0 - p.array())).matrix();
        W = w.asDiagonal();

        // Step 3: Compute the gradient and Hessian for beta update
        Eigen::VectorXf z = linearPredictor.array() + (eigenAggregationResults.cast<float>().array() - p.array()) / w.array();
        Eigen::MatrixXf XtWX = X.transpose() * W * X;
        Eigen::VectorXf XtWz = X.transpose() * W * z;

        // Step 4: Update beta coefficients
        Eigen::VectorXf beta_new = XtWX.ldlt().solve(XtWz);

        // Check for convergence
        if ((beta_new - beta).norm() < tolerance)
        {
            beta = beta_new;
            break;
        }
        beta = beta_new;
    }

    // Step 5: Compute standard errors of coefficients
    Eigen::MatrixXf XtWX = X.transpose() * W * X;
    Eigen::MatrixXf covMatrix = XtWX.inverse();
    float stdErrorIntercept = std::sqrt(covMatrix(0, 0));
    float stdErrorSlope = std::sqrt(covMatrix(1, 1));

    // Step 6: P-value calculation using normal CDF approximation
    float zValue = beta(1) / stdErrorSlope;
    //float pValue = 2 * (1 - normalCDF(fabs(zValue))); // Two-tailed p-value
*/
    // Return the result
    FGLMResult Result;
    //Result.Intercept = beta(0);
    //Result.Slope = beta(1);
    //Result.StdError = stdErrorSlope;
    //Result.PValue = zValue;
    return Result;
}

TArray<int> UResultAnalysisFunctions::E2Counting(TArray<FVector>& Positions)
{
	TArray<int> CountedPositionsPerOutlet;
	for (int i = 0; i < 6; i++)
		CountedPositionsPerOutlet.Add(0);

	const float Outlet1 = 200.f;
	const float Outlet2 = 160.f;
	const float Outlet3 = 120.f;
	const float Outlet4 = 80.f;
	const float Outlet5 = 40.f;

	for (auto Position : Positions)
	{
		if (Position.Z > Outlet1)
		{
			CountedPositionsPerOutlet[0]++;
		}
		else if (Position.Z > Outlet2)
		{
			CountedPositionsPerOutlet[1]++;
		}
		else if (Position.Z > Outlet3)
		{
			CountedPositionsPerOutlet[2]++;
		}
		else if (Position.Z > Outlet4)
		{
			CountedPositionsPerOutlet[3]++;
		}
		else if (Position.Z > Outlet5)
		{
			CountedPositionsPerOutlet[4]++;
		}
		else if (Position.Z > 0)
		{
			CountedPositionsPerOutlet[5]++;
		}
	}

	return CountedPositionsPerOutlet;
}

TArray<int> UResultAnalysisFunctions::E3CountingSide(TArray<FVector>& Positions)
{
	TArray<int> CountedPositions;
	for (int i = 0; i < 5; i++)
		CountedPositions.Add(0);

	const float Distance1 = 20.f;
	const float Distance2 = 40.f;
	const float Distance3 = 60.f;
	const float Distance4 = 80.f;

	for (auto Position : Positions)
	{
		if (Position.X < Distance1)
		{
			CountedPositions[0]++;
		}
		else if (Position.X < Distance2)
		{
			CountedPositions[1]++;
		}
		else if (Position.X < Distance3)
		{
			CountedPositions[2]++;
		}
		else if (Position.X < Distance4)
		{
			CountedPositions[3]++;
		}
		else
		{
			CountedPositions[4]++;
		}
	}

	return CountedPositions;
}

TArray<int> UResultAnalysisFunctions::E3CountingAbove(TArray<FVector>& Positions)
{
	TArray<int> CountedPositionsPerOutlet;
	for (int i = 0; i < 5; i++)
		CountedPositionsPerOutlet.Add(0);

	const float Height1 = 10.f;
	const float Height2 = 8.f;
	const float Height3 = 6.f;
	const float Height4 = 4.f;
	const float Height5 = 2.f;
	const float Height6 = 0.f;

	for (auto Position : Positions)
	{
		if (Position.Z > Height2)
		{
			CountedPositionsPerOutlet[0]++;
		}
		else if (Position.Z > Height3)
		{
			CountedPositionsPerOutlet[1]++;
		}
		else if (Position.Z > Height4)
		{
			CountedPositionsPerOutlet[2]++;
		}
		else if (Position.Z > Height5)
		{
			CountedPositionsPerOutlet[3]++;
		}
		else
		{
			CountedPositionsPerOutlet[4]++;
		}
	}

	return CountedPositionsPerOutlet;
}

void UResultAnalysisFunctions::Shuffle(TArray<FVector>& Positions)
{
	const int32 LastIndex = Positions.Num() - 1;
	for (int32 i = 0; i <= LastIndex; ++i)
	{
		int32 j = FMath::RandRange(i, LastIndex);
		if (i != j)
		{
			Positions.Swap(i, j);
		}
	}
}

TArray<FVector> UResultAnalysisFunctions::ShufflePositionsOnTiles(const FDataChunk& DataChunk, int NumLarvae)
{
	TArray<FVector> ShuffledReefPositions;
	int N = NumLarvae;
	auto ReefPositions = UDataGridUtils::GetReefCellPositions(DataChunk);

	// Get N random positions on reef cell surfaces (top of cell)
	for (int i = 0; i < N; i++)
	{
		// Get random reef cell
		int RandomIndex = FMath::RandRange(0, ReefPositions.Num() - 1);
		FVector RandomReefCell = ReefPositions[RandomIndex];

		// Get random position on reef cell
		float XCornerMin = RandomReefCell.X - DataChunk.Config.CellEdgeLength / 2;
		float YCornerMin = RandomReefCell.Y - DataChunk.Config.CellEdgeLength / 2;
		float XCornerMax = RandomReefCell.X + DataChunk.Config.CellEdgeLength / 2;
		float YCornerMax = RandomReefCell.Y + DataChunk.Config.CellEdgeLength / 2;

		float RandomX = FMath::RandRange(XCornerMin, XCornerMax);
		float RandomY = FMath::RandRange(YCornerMin, YCornerMax);
		ShuffledReefPositions.Add(FVector(RandomX, RandomY, RandomReefCell.Z));
	}
	return ShuffledReefPositions;
}

/*Eigen::VectorXf UResultAnalysisFunctions::ConvertTArrayToEigen(const TArray<float>& Array)
{
	Eigen::VectorXf EigenVector(Array.Num());
	for (int i = 0; i < Array.Num(); ++i)
	{
		EigenVector(i) = Array[i];
	}
	return EigenVector;
}*/

/*Eigen::VectorXi UResultAnalysisFunctions::ConvertTArrayToEigen(const TArray<int>& Array)
{
	Eigen::VectorXi EigenVector(Array.Num());
	for (int i = 0; i < Array.Num(); ++i)
	{
		EigenVector(i) = Array[i];
	}
	return EigenVector;
}*/

float UResultAnalysisFunctions::CalculatePValue(float ZValue)
{
	//boost::math::normal dist(0.0, 1.0);  // Standard normal distribution

	//float pValue = 2.0 * (1.0 - boost::math::cdf(dist, std::abs(ZValue)));  // Two-tailed test

	return 0;
}

TArray<TArray<FVector>> UResultAnalysisFunctions::ReadSavedArrayPositions()
{
	const FString FilePath = FPaths::ProjectContentDir() / TEXT("Evolution/positions.txt");
	TArray<TArray<FVector>> AllVectors;

	FString FileContent;

	// Read the entire file into FileContent
	if (FFileHelper::LoadFileToString(FileContent, *FilePath))
	{
		TArray<FString> Lines;
		FileContent.ParseIntoArrayLines(Lines); // Split the file content into lines

		for (const FString& Line : Lines)
		{
			TArray<FVector> VectorsForLine;
			TArray<FString> VectorStrings;

			// Split the line by tabs to separate vectors
			Line.ParseIntoArray(VectorStrings, TEXT("\t"), true);

			for (const FString& VectorString : VectorStrings)
			{
				TArray<FString> Components;

				// Split each vector string by spaces to separate x, y, z components
				VectorString.ParseIntoArray(Components, TEXT(" "), true);

				if (Components.Num() == 3)
				{
					// Convert components to float and create FVector
					float X = FCString::Atof(*Components[0]);
					float Y = FCString::Atof(*Components[1]);
					float Z = FCString::Atof(*Components[2]);
					FVector Vector(X, Y, Z);

					VectorsForLine.Add(Vector);
				}
			}

			AllVectors.Add(VectorsForLine); // Add the vectors for this line to the main array
		}
	}

	return AllVectors;
}

void UResultAnalysisFunctions::ShuffleSettlementResults(TArray<int>& SettlementResults)
{
	const int32 LastIndex = SettlementResults.Num() - 1;
	for (int32 i = 0; i <= LastIndex; ++i)
	{
		int32 j = FMath::RandRange(i, LastIndex);
		if (i != j)
		{
			SettlementResults.Swap(i, j);
		}
	}
}

float UResultAnalysisFunctions::ChiSquareTest()
{
	const FString FilePath = FPaths::ProjectContentDir() / TEXT("Evolution/chisquare.txt");
	TArray<FString> FileLines;
	if (!FFileHelper::LoadFileToStringArray(FileLines, *FilePath))
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to read the file."));
		return 0;
	}

	// Initialize array to store summed counts for each chamber (5 chambers in total).
	TArray<int> ChamberSums;
	for (int i = 0; i < 5; i++)
		ChamberSums.Add(0);


	// Process each line (each line is a run).
	for (const FString& Line : FileLines)
	{
		TArray<FString> Counts;
		Line.ParseIntoArray(Counts, TEXT("\t"), true);

		if (Counts.Num() != 5)
		{
			UE_LOG(LogTemp, Warning, TEXT("Invalid line format. Expected 5 values per line."));
			return 0;
		}

		// Sum up counts for each chamber across all runs.
		for (int32 i = 0; i < 5; ++i)
		{
			ChamberSums[i] += FCString::Atof(*Counts[i]);
		}
	}

	// Total observed counts (sum of all chamber counts).
	float TotalObserved = 0;
	for (float Sum : ChamberSums)
	{
		TotalObserved += Sum;
	}

	float ExpectedCount = TotalObserved / 5.0f; // 5 chambers

	// Chi-square value
	float ChiSquare = 0;

	// Calculate chi-square for each chamber.
	for (float ObservedCount : ChamberSums)
	{
		// Chi-square formula: (O - E)^2 / E
		ChiSquare += FMath::Pow(ObservedCount - ExpectedCount, 2) / ExpectedCount;
	}

	// Degrees of freedom (df) = number of chambers - 1.
	int32 DegreesOfFreedom = 5 - 1;

	// Output the chi-square value and degrees of freedom
	UE_LOG(LogTemp, Log, TEXT("Chi-Square Value: %f"), ChiSquare);
	UE_LOG(LogTemp, Log, TEXT("Degrees of Freedom: %d"), DegreesOfFreedom);

	return ChiSquare;
}

float UResultAnalysisFunctions::Anova()
{
	const FString FilePath = FPaths::ProjectContentDir() / TEXT("Evolution/anova.txt");
	TArray<FString> FileLines;
	if (!FFileHelper::LoadFileToStringArray(FileLines, *FilePath))
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to read the file."));
		return 0;
	}

	// Initialize array to store sums and counts for each vertical chamber (5 chambers in total).
	TArray<float> ChamberSums;
	for (int i = 0; i < 5; i++)
		ChamberSums.Add(0);

	TArray<int32> ChamberCounts; // To count the number of data points for each chamber
	for (int i = 0; i < 5; i++)
		ChamberCounts.Add(0);

	TArray<float> AllData; // Store all values for total variance calculation

	// Process each line (each line is a run).
	for (const FString& Line : FileLines)
	{
		TArray<FString> Counts;
		Line.ParseIntoArray(Counts, TEXT("\t"), true); // Split by tabs

		if (Counts.Num() != 5)
		{
			UE_LOG(LogTemp, Warning, TEXT("Invalid line format. Expected 5 values per line."));
			return 0;
		}

		// Sum up counts for each chamber and store all values.
		for (int32 i = 0; i < 5; ++i)
		{
			float ChamberValue = FCString::Atof(*Counts[i]);
			ChamberSums[i] += ChamberValue;
			ChamberCounts[i]++;
			AllData.Add(ChamberValue);
		}
	}

	// Step 1: Calculate the mean for each chamber
	TArray<float> ChamberMeans;
	ChamberMeans.SetNum(5);
	for (int32 i = 0; i < 5; ++i)
	{
		ChamberMeans[i] = ChamberSums[i] / ChamberCounts[i];
	}

	// Step 2: Calculate the overall mean
	float TotalSum = std::accumulate(AllData.begin(), AllData.end(), 0.0f);
	float TotalMean = TotalSum / AllData.Num();

	// Step 3: Calculate Between-Group Sum of Squares (SSB)
	float SSB = 0;
	for (int32 i = 0; i < 5; ++i)
	{
		SSB += ChamberCounts[i] * FMath::Pow(ChamberMeans[i] - TotalMean, 2);
	}

	// Step 4: Calculate Within-Group Sum of Squares (SSW)
	float SSW = 0;
	int32 TotalDataPoints = 0;
	for (int32 i = 0; i < 5; ++i)
	{
		for (const FString& Line : FileLines)
		{
			TArray<FString> Counts;
			Line.ParseIntoArray(Counts, TEXT("\t"), true);  // Split by tabs
			float ChamberValue = FCString::Atof(*Counts[i]);
			SSW += FMath::Pow(ChamberValue - ChamberMeans[i], 2);
		}
		TotalDataPoints += ChamberCounts[i];
	}

	// Step 5: Calculate degrees of freedom
	int32 df_between = 5 - 1;  // Number of groups - 1
	int32 df_within = TotalDataPoints - 5;  // Total data points - number of groups

	// Step 6: Calculate Mean Squares
	float MSB = SSB / df_between;  // Mean Square Between
	float MSW = SSW / df_within;   // Mean Square Within

	// Step 7: Calculate F-ratio
	float F = MSB / MSW;

	// Output the ANOVA results
	UE_LOG(LogTemp, Log, TEXT("ANOVA F-Ratio: %f"), F);
	UE_LOG(LogTemp, Log, TEXT("Degrees of Freedom (Between): %d"), df_between);
	UE_LOG(LogTemp, Log, TEXT("Degrees of Freedom (Within): %d"), df_within);
	
	return F;
}
