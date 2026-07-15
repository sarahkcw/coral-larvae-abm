#include "DataGrid/DataGridVisualizer.h"
#include "DrawDebugHelpers.h"
#include "DataGrid/DataGrid.h"
#include "../../Public/DataGrid/DataGridUtils.h"
#include "GameFramework/Actor.h"

UDataGridVisualizer::UDataGridVisualizer(): DebugControlActor(nullptr), DataGrid(nullptr), Owner(nullptr)
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UDataGridVisualizer::Init()
{
    Owner = GetOwner();
    DataGrid = Owner->FindComponentByClass<UDataGrid>();
    
    const FVector GridCenter = DataGrid->GetDataConfig().ChunkWorldOrigin + DataGrid->GetDataConfig().LocalBounds / 2.f;
    const FVector GridExtent = DataGrid->GetDataConfig().LocalBounds / 2.f;	
    DrawDebugBox(GetWorld(), GridCenter, GridExtent, FColor::White, false, 5000.f, 0, GridThickness);
}

void UDataGridVisualizer::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    
    if (!Owner || !DataGrid)
    {
        UE_LOG(LogTemp, Error, TEXT("Debug component was not properly initialized!"));
        return;
    }
    
    if (DebugControlActor == nullptr)
        return;
    
    if (DrawDebug)
        DebugGrid();
    
    if (DrawReefCells)
        DebugReefCells();

    if (DrawInvalidCells)
        DebugInvalidCells();
    
    if (DrawCCAConcentration)
        DebugCCA();
    
    if (DrawAlteromonas)
        DebugAlteromonas();
    
    if (DrawSoundIntensity)
        DebugParticleMotion();

    if (DrawLightIntensity)
        DebugLightIntensity();

    if (DrawPressure)
        DebugPressure();
        
}

void UDataGridVisualizer::DebugGrid() const
{
    auto Grid = DataGrid->GetCellDataSphereAround(DebugControlActor->GetActorLocation(), DebugControlActor->GetRadius());
    
    const auto DataConfig = Grid.Config;
    
    for (int32 i = 0; i < Grid.Data.Num(); i++)
    {
        auto [WaterData, LightData, ReefData, bIsReef] = Grid.Data[i].Data;

        // Draw cell border
        if (DrawCellBorder)
        {
            FVector Box = FVector(DataConfig.CellEdgeLength / 2); 
            DrawDebugBox(GetWorld(), Grid.Data[i].WorldPosition, Box, FColor::White, false, -1.f, 0, GridThickness / 2.f);
        }

        // Draw current per cell
        if (DrawCurrent)
        {
            const auto CurrentMagnitude = WaterData.Current.Size() * 2;
            auto CurrentDirection = WaterData.Current.GetSafeNormal() * DataConfig.CellEdgeLength * WaterData.Current.Size();
            DrawDebugLine(GetWorld(), Grid.Data[i].WorldPosition - CurrentDirection / 2.f,
                Grid.Data[i].WorldPosition + CurrentDirection / 2.f,
                FColor::Blue, false,-1, 0, CurrentMagnitude);
        }
    }
}

void UDataGridVisualizer::DebugReefCells() const
{
    auto Grid = DataGrid->GetCellDataSphereAround(DebugControlActor->GetActorLocation(), DebugControlActor->GetRadius());
    for(auto [Index, WorldPosition, Data, bInvalid] : Grid.Data)
    {
        if (Data.bIsReefCell)
        {
            FVector Box = FVector(Grid.Config.CellEdgeLength / 2);
            DrawDebugBox(GetWorld(), WorldPosition, Box, FColor::Emerald, false, -1.f, 0, GridThickness / 2.f);
        }
    }
}

void UDataGridVisualizer::DebugInvalidCells() const
{
    auto Grid = DataGrid->GetCellDataSphereAround(DebugControlActor->GetActorLocation(), DebugControlActor->GetRadius());
    for(auto [Index, WorldPosition, Data, bInvalid] : Grid.Data)
    {
        if (bInvalid)
        {
            FVector Box = FVector(Grid.Config.CellEdgeLength / 2);
            DrawDebugBox(GetWorld(), WorldPosition, Box, FColor::Black, false, -1.f, 0, GridThickness / 2.f);
        }
    }
}

void UDataGridVisualizer::DebugCCA() const
{
    auto Grid = DataGrid->GetCellDataSphereAround(DebugControlActor->GetActorLocation(), DebugControlActor->GetRadius());
    FVector Box = FVector(Grid.Config.CellEdgeLength / 2);
    for (FIndexedCellData Data : Grid.Data)
    {
        auto CCA = Data.Data.ReefData.CCA;
        if(CCA > 0.0f)
        {
            FColor Color = FColor::MakeRedToGreenColorFromScalar(Data.Data.ReefData.CCA);
            DrawDebugBox(GetWorld(), Data.WorldPosition, Box, Color, false, -1.f, 0, GridThickness / 2.f);
        }
    }
}

void UDataGridVisualizer::DebugAlteromonas() const
{
    auto Grid = DataGrid->GetCellDataSphereAround(DebugControlActor->GetActorLocation(), DebugControlActor->GetRadius());
    FVector Box = FVector(Grid.Config.CellEdgeLength / 2);
    for (FIndexedCellData Data : Grid.Data)
    {
        if(Data.Data.ReefData.Alteromonas > 0.0f)
        {
            FColor Color = FColor::Black;
            DrawDebugBox(GetWorld(), Data.WorldPosition, Box, Color, false, -1.f, 0, GridThickness / 2.f);
        }
    }
}

void UDataGridVisualizer::DebugParticleMotion() const
{
    auto Grid = DataGrid->GetCellDataSphereAround(DebugControlActor->GetActorLocation(), DebugControlActor->GetRadius());
    FVector Box = FVector(Grid.Config.CellEdgeLength / 2);
    for (FIndexedCellData Data : Grid.Data)
    {
        auto ParticleMotion = Data.Data.WaterData.ParticleMotion;
        if(ParticleMotion > 0.0f)
        {
            FColor Color = FColor::MakeRedToGreenColorFromScalar(FMath::Clamp(ParticleMotion, 0.f, 1.f));
            DrawDebugBox(GetWorld(), Data.WorldPosition, Box, Color, false, -1.f, 0, GridThickness / 2.f);
        }
    }
}

void UDataGridVisualizer::DebugLightIntensity() const
{
    auto Grid = DataGrid->GetCellDataSphereAround(DebugControlActor->GetActorLocation(), DebugControlActor->GetRadius());
    const FVector Box = FVector(Grid.Config.CellEdgeLength / 2);
    for (const FIndexedCellData& Data : Grid.Data)
    {
        const float LightIntensity = FMath::Clamp(Data.Data.LightData.LightIntensity, 0.0f, 1.0f);
        if (LightIntensity > 0.0f)
        {
            const FColor Color = FColor::MakeRedToGreenColorFromScalar(LightIntensity);
            DrawDebugBox(GetWorld(), Data.WorldPosition, Box, Color, false, -1.f, 0, GridThickness / 2.f);
        }
    }
}

void UDataGridVisualizer::DebugPressure() const
{
    auto Grid = DataGrid->GetCellDataSphereAround(DebugControlActor->GetActorLocation(), DebugControlActor->GetRadius());
    const FVector Box = FVector(Grid.Config.CellEdgeLength / 2);
    const float SurfacePressureAtm = 1.0f;
    const float Density = 1025.0f;
    const float Gravity = 9.81f;
    const float PascalToAtmospheres = 101325.0f;
    const float MaxDepthMeters = FMath::Max(
        (Grid.Config.LocalBounds.Z - Grid.Config.CellEdgeLength / 2.0f) / 100.0f,
        0.001f);
    const float MaxPressureAtm = SurfacePressureAtm + (Density * Gravity * MaxDepthMeters) / PascalToAtmospheres;

    for (const FIndexedCellData& Data : Grid.Data)
    {
        const float Pressure = FMath::GetMappedRangeValueClamped(
            FVector2D(SurfacePressureAtm, MaxPressureAtm),
            FVector2D(0.0f, 1.0f),
            Data.Data.WaterData.Pressure);
        const FColor Color = FColor::MakeRedToGreenColorFromScalar(Pressure);
        DrawDebugBox(GetWorld(), Data.WorldPosition, Box, Color, false, -1.f, 0, GridThickness / 2.f);
    }
}
