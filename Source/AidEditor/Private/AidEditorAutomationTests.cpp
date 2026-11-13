#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AidDocument.h"
#include "AidDocumentFactory.h"
#include "AssetDefinition_AidDocument.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAidDedicatedAssetFactoryTest,
    "Aid.Editor.Assets.DedicatedDocumentFactory",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAidDedicatedAssetFactoryTest::RunTest(const FString& Parameters)
{
    UAidDocumentFactory* Factory = NewObject<UAidDocumentFactory>(GetTransientPackage());
    TestTrue(TEXT("Factory creates a dedicated document type"), Factory->SupportedClass == UAidDocument::StaticClass());
    UObject* Asset = Factory->FactoryCreateNew(UAidDocument::StaticClass(), GetTransientPackage(),
        MakeUniqueObjectName(GetTransientPackage(), UAidDocument::StaticClass(), TEXT("TestAidDoc")),
        RF_Transient, nullptr, nullptr);
    UAidDocument* Document = Cast<UAidDocument>(Asset);
    TestNotNull(TEXT("Factory creates AidDocument asset"), Document);
    if (Document)
    {
        TestTrue(TEXT("New document starts valid"), Document->ValidateGraph().IsEmpty());
    }
    UAssetDefinition_AidDocument* Definition = NewObject<UAssetDefinition_AidDocument>(GetTransientPackage());
    TestTrue(TEXT("Aid has dedicated asset identity"), Definition->GetAssetClass().Get() == UAidDocument::StaticClass());
    return true;
}

#endif

#if WITH_DEV_AUTOMATION_TESTS

#include "AidEditorSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAidDeferredPlacementTest,
    "Aid.Editor.Authoring.StagedPlacementConfirmAndCancel",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAidDeferredPlacementTest::RunTest(const FString& Parameters)
{
    UAidDocument* Document = NewObject<UAidDocument>(GetTransientPackage());
    UAidEditorSubsystem* Editor = NewObject<UAidEditorSubsystem>(GetTransientPackage());
    Editor->ActiveDocument = Document;
    const FGuid Id = Editor->AddUnplacedNote();
    TestTrue(TEXT("New note identity is valid"), Id.IsValid());
    Editor->PlacementNodeId = Id;
    Editor->bPlaceNoteOnClick = true;
    Editor->bPlaceOnActor = false;
    Editor->StagePlacement(FVector(100., 200., 300.), nullptr);
    const FAidNode* Note = Document->FindNode(Id);
    if (!TestNotNull(TEXT("Document has the staged note"), Note)) return false;
    TestEqual(TEXT("Staging does not commit a location"), Note->Anchor.Type, EAidAnchorType::Unplaced);
    Editor->CancelPendingAction();
    TestFalse(TEXT("Cancel clears active placement"), Editor->bPlaceNoteOnClick);
    TestEqual(TEXT("Cancel leaves document data unplaced"), Document->FindNode(Id)->Anchor.Type, EAidAnchorType::Unplaced);
    Editor->PlacementNodeId = Id;
    Editor->bPlaceNoteOnClick = true;
    Editor->StagePlacement(FVector(123., 456., 789.), nullptr);
    TestTrue(TEXT("Confirmed placement succeeds"), Editor->ConfirmPlacement());
    Note = Document->FindNode(Id);
    TestEqual(TEXT("Confirm changes anchor to world"), Note->Anchor.Type, EAidAnchorType::World);
    TestTrue(TEXT("Confirmed position is committed"), Note->Anchor.WorldPosition.Equals(FVector(123.,456.,789.), .001));
    TestFalse(TEXT("Confirm clears placement mode"), Editor->bPlaceNoteOnClick);
    return true;
}

#endif

#if WITH_DEV_AUTOMATION_TESTS

#include "AidViewportGeometry.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAidHudViewportClipTest,
    "Aid.Editor.Viewport.SegmentClipping",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAidHudViewportClipTest::RunTest(const FString& Parameters)
{
    const FIntPoint ViewSize(1000, 600);
    FVector2D A(-250., 300.), B(1250., 300.);
    TestTrue(TEXT("Crossing segment retained"), AidViewportGeometry::ClipToViewport(A, B, ViewSize));
    TestTrue(TEXT("Start clipped at left viewport edge"), A.Equals(FVector2D(0.,300.), .01));
    TestTrue(TEXT("End clipped at right viewport edge"), B.Equals(FVector2D(1000.,300.), .01));
    A = FVector2D(200.,-100.); B = FVector2D(200.,700.);
    TestTrue(TEXT("Vertical crossing retained"), AidViewportGeometry::ClipToViewport(A, B, ViewSize));
    TestTrue(TEXT("Vertical top clipped"), A.Equals(FVector2D(200.,0.), .01));
    TestTrue(TEXT("Vertical bottom clipped"), B.Equals(FVector2D(200.,600.), .01));
    A = FVector2D(-100.,-100.); B = FVector2D(-50.,-20.);
    TestFalse(TEXT("Off-screen segment rejected"), AidViewportGeometry::ClipToViewport(A, B, ViewSize));
    A = FVector2D(100.,100.); B = FVector2D(700.,300.);
    TestTrue(TEXT("Fully visible segment retained"), AidViewportGeometry::ClipToViewport(A, B, ViewSize));
    TestTrue(TEXT("Fully visible start unchanged"), A.Equals(FVector2D(100.,100.), .01));
    TestTrue(TEXT("Fully visible end unchanged"), B.Equals(FVector2D(700.,300.), .01));
    return true;
}

#endif
