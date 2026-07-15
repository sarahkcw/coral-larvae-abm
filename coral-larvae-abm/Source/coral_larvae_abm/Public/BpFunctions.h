// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Windows/WindowsPlatformApplicationMisc.h"
#include "SimulationManager.h"
#include "BpFunctions.generated.h"

/**
 * 
 */
UCLASS()
class CORAL_LARVAE_ABM_API UBpFunctions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
	UFUNCTION(BlueprintCallable)
	static void CopyStringToClipboard(FString Val) {
		FPlatformApplicationMisc::ClipboardCopy(*Val);
	}

	UFUNCTION(BlueprintCallable)
	static FString FormatGenerationDataForExcel(int Gen, float AvgFit, float MaxFit, float Diversity, int Settlers)
	{
		// Create the header
		FString Header = TEXT("Generation\tAvgFitness\tMaxFitness\tGeneticDiversity\n");

		// Convert floats to strings with comma as the decimal separator
		FString AvgFitStr = FString::SanitizeFloat(AvgFit).Replace(TEXT("."), TEXT(","));
		FString MaxFitStr = FString::SanitizeFloat(MaxFit).Replace(TEXT("."), TEXT(","));
		FString DiversityStr = FString::SanitizeFloat(Diversity).Replace(TEXT("."), TEXT(","));

		// Format the data line
		FString DataLine = FString::Printf(TEXT("%d\t%s\t%s\t%s\n"), Gen, *AvgFitStr, *MaxFitStr, *DiversityStr);

		// Return the header followed by the data
		return Header + DataLine;
	}

	UFUNCTION(BlueprintCallable)
	static FString FormatGenerationDataForExcelWithSettlers(ASimulationManager* SimulationManager, int Gen, float AvgFit, float MaxFit, float Diversity)
	{
		const int TotalSettlers = SimulationManager ? SimulationManager->LastTotalSettlers : 0;
		const int CorrectSettlers = SimulationManager ? SimulationManager->LastCorrectSettlers : 0;
		const int BoundaryContacts = SimulationManager ? SimulationManager->LastBoundaryContacts : 0;

		FString Header = TEXT("Generation\tAvgFitness\tMaxFitness\tGeneticDiversity\tTotalSettlers\tCorrectSettlers\tBoundaryContacts\n");

		FString AvgFitStr = FString::SanitizeFloat(AvgFit).Replace(TEXT("."), TEXT(","));
		FString MaxFitStr = FString::SanitizeFloat(MaxFit).Replace(TEXT("."), TEXT(","));
		FString DiversityStr = FString::SanitizeFloat(Diversity).Replace(TEXT("."), TEXT(","));

		FString DataLine = FString::Printf(
			TEXT("%d\t%s\t%s\t%s\t%d\t%d\t%d\n"),
			Gen,
			*AvgFitStr,
			*MaxFitStr,
			*DiversityStr,
			TotalSettlers,
			CorrectSettlers,
			BoundaryContacts);

		return Header + DataLine;
	}

	
	UFUNCTION(BlueprintCallable, Category = "DataProcessing")
	static FString AddDataToText(const FString& NewData, const FString& ExistingText)
	{
		FString ResultText;

		// Split the NewData into header and data line
		FString NewHeader;
		FString NewDataLine;

		// Find the first newline character to split header and data
		int32 NewLineIndex;
		if (NewData.FindChar(TEXT('\n'), NewLineIndex))
		{
			NewHeader = NewData.Left(NewLineIndex + 1); // Include the newline character
			NewDataLine = NewData.Mid(NewLineIndex + 1);
		}
		else
		{
			// If no newline found, treat the entire NewData as data line
			NewHeader = TEXT("");
			NewDataLine = NewData;
		}

		// Split the ExistingText into header and data (if it exists)
		FString ExistingHeader;
		FString ExistingData;

		if (ExistingText.FindChar(TEXT('\n'), NewLineIndex))
		{
			ExistingHeader = ExistingText.Left(NewLineIndex + 1); // Include the newline character
			ExistingData = ExistingText.Mid(NewLineIndex + 1);
		}
		else
		{
			// If no newline found, treat the entire ExistingText as data
			ExistingHeader = TEXT("");
			ExistingData = ExistingText;
		}

		// Compare the headers
		if (ExistingHeader.Equals(NewHeader))
		{
			// If headers match, just append the new data line below the existing data
			ResultText = ExistingHeader + NewDataLine + ExistingData;
		}
		else
		{
			// If headers don't match, replace the existing header with the new one
			ResultText = NewHeader + NewDataLine + ExistingData;
		}

		return ResultText;
	}
};
