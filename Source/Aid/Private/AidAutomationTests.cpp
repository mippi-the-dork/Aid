#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AidDocument.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAidDocumentGraphTest,
    "Aid.Data.Graph.AuthoringAndValidation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAidDocumentGraphTest::RunTest(const FString& Parameters)
{
    UAidDocument* Doc = NewObject<UAidDocument>(GetTransientPackage());
    TestTrue(TEXT("Empty document is valid"), Doc->ValidateGraph().IsEmpty());
    const FGuid A = Doc->AddNode(FText::FromString(TEXT("Button")), FVector(100, 0, 0));
    const FGuid B = Doc->AddNode(FText::FromString(TEXT("Door")), FVector(900, 0, 0));
    TestTrue(TEXT("Node IDs are unique and valid"), A.IsValid() && B.IsValid() && A != B);
    TestEqual(TEXT("Node count"), Doc->Nodes.Num(), 2);
    TestEqual(TEXT("Valid connection accepted"), Doc->AddConnection(A, B, FText::FromString(TEXT("Opens"))).IsValid(), true);
    TestEqual(TEXT("Connection count"), Doc->Connections.Num(), 1);
    TestFalse(TEXT("Self-link rejected"), Doc->AddConnection(A, A, FText::GetEmpty()).IsValid());
    TestFalse(TEXT("Missing node rejected"), Doc->AddConnection(FGuid::NewGuid(), B, FText::GetEmpty()).IsValid());
    TestEqual(TEXT("Rejected connections do not mutate document"), Doc->Connections.Num(), 1);
    TestEqual(TEXT("Valid graph has no errors"), Doc->ValidateGraph().Num(), 0);
    Doc->Connections[0].TargetNode = FGuid::NewGuid();
    TestTrue(TEXT("Broken edge is detected"), Doc->ValidateGraph().Num() > 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAidAnchorFallbackTest,
    "Aid.Data.Anchors.WorldAndMissingActorFallback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAidAnchorFallbackTest::RunTest(const FString& Parameters)
{
    FAidAnchor Anchor;
    Anchor.WorldPosition = FVector(123., -45., 678.);
    TestEqual(TEXT("World anchor position"), Anchor.Resolve(nullptr), Anchor.WorldPosition);
    Anchor.Type = EAidAnchorType::ActorTag;
    Anchor.ActorTag = TEXT("UnresolvedQAAnchor");
    TestEqual(TEXT("Missing world retains last location"), Anchor.Resolve(nullptr), Anchor.WorldPosition);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAidInvalidIdentityTest,
    "Aid.Data.Graph.DuplicateIdentity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAidInvalidIdentityTest::RunTest(const FString& Parameters)
{
    UAidDocument* Doc = NewObject<UAidDocument>(GetTransientPackage());
    Doc->AddNode(FText::FromString(TEXT("A")), FVector::ZeroVector);
    Doc->AddNode(FText::FromString(TEXT("B")), FVector::ZeroVector);
    Doc->Nodes[1].Id = Doc->Nodes[0].Id;
    TestTrue(TEXT("Duplicate node GUID is invalid"), Doc->ValidateGraph().Num() > 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAidGraphDeleteTest,
    "Aid.Data.Graph.DeleteNodeRemovesConnections",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAidGraphDeleteTest::RunTest(const FString& Parameters)
{
    UAidDocument* Doc = NewObject<UAidDocument>(GetTransientPackage());
    const FGuid A = Doc->AddNode(FText::FromString(TEXT("Source")), FVector::ZeroVector);
    const FGuid B = Doc->AddNode(FText::FromString(TEXT("Target")), FVector(100, 0, 0));
    Doc->AddConnection(A, B, FText::FromString(TEXT("Activates")));
    TestTrue(TEXT("Existing node deleted"), Doc->RemoveNode(A));
    TestEqual(TEXT("Only target remains"), Doc->Nodes.Num(), 1);
    TestTrue(TEXT("All dependent edges removed"), Doc->Connections.IsEmpty());
    TestTrue(TEXT("Graph remains valid"), Doc->ValidateGraph().IsEmpty());
    TestFalse(TEXT("Missing node delete fails harmlessly"), Doc->RemoveNode(A));
    return true;
}

#endif

#if WITH_DEV_AUTOMATION_TESTS

#include "AidProjectSettings.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAidVisibilityStatesTest,
    "Aid.Data.Visibility.DistanceStates",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAidVisibilityStatesTest::RunTest(const FString& Parameters)
{
    FAidNode Node;
    Node.ViewDistance = 2000.f;
    const FVector Anchor = FVector::ZeroVector;
    TestEqual(TEXT("Beyond 100% hidden"), UAidDocument::GetNoteVisibility(Node, FVector(2001.,0.,0.), Anchor), static_cast<uint8>(0));
    TestEqual(TEXT("100% marker"), UAidDocument::GetNoteVisibility(Node, FVector(2000.,0.,0.), Anchor), static_cast<uint8>(1));
    TestEqual(TEXT("75% marker"), UAidDocument::GetNoteVisibility(Node, FVector(1500.,0.,0.), Anchor), static_cast<uint8>(1));
    TestEqual(TEXT("50% card"), UAidDocument::GetNoteVisibility(Node, FVector(1000.,0.,0.), Anchor), static_cast<uint8>(2));
    TestEqual(TEXT("Near card"), UAidDocument::GetNoteVisibility(Node, FVector::ZeroVector, Anchor), static_cast<uint8>(2));
    Node.bVisible = false;
    TestEqual(TEXT("Manually hidden stays hidden"), UAidDocument::GetNoteVisibility(Node, FVector::ZeroVector, Anchor), static_cast<uint8>(0));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAidConnectionOneVisibleTest,
    "Aid.Data.Visibility.ConnectionOneEndpointVisible",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAidConnectionOneVisibleTest::RunTest(const FString& Parameters)
{
    FAidNode A, B;
    A.ViewDistance = 2000.f;
    B.ViewDistance = 2000.f;
    FAidConnection Edge;
    const FVector Camera = FVector::ZeroVector;
    const FVector Near(400., 0., 0.);
    const FVector Far(10000., 0., 0.);
    TestTrue(TEXT("One visible endpoint keeps entire connection visible"),
        UAidDocument::IsConnectionVisible(Edge, A, B, Camera, Near, Far));
    TestFalse(TEXT("Both endpoints beyond view distance hide connection"),
        UAidDocument::IsConnectionVisible(Edge, A, B, Camera, Far, Far + FVector(5000.,0.,0.)));
    B.bVisible = false;
    TestTrue(TEXT("Manually hidden target does not kill visible source connection"),
        UAidDocument::IsConnectionVisible(Edge, A, B, Camera, Near, Far));
    Edge.bVisible = false;
    TestFalse(TEXT("Manual edge hide wins"), UAidDocument::IsConnectionVisible(Edge, A, B, Camera, Near, Far));
    Edge.bVisible = true;
    Edge.ViewDistance = 100.f;
    TestFalse(TEXT("Optional connection distance overrides visibility"),
        UAidDocument::IsConnectionVisible(Edge, A, B, Camera, FVector(800.,0.,0.), Far));
    Edge.ViewDistance = 0.f;
    TestTrue(TEXT("Zero connection distance means unlimited"),
        UAidDocument::IsConnectionVisible(Edge, A, B, Camera, Near, Far));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAidCategoriesPresetsTest,
    "Aid.Data.Presets.CategoryAndRelationship",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAidCategoriesPresetsTest::RunTest(const FString& Parameters)
{
    const UAidProjectSettings* Settings = GetDefault<UAidProjectSettings>();
    TestNotNull(TEXT("Project settings registered"), Settings);
    if (!Settings) return false;
    TestNotNull(TEXT("Design category"), Settings->FindCategory(TEXT("Design")));
    TestNotNull(TEXT("References relationship"), Settings->FindRelationship(TEXT("References")));
    TestTrue(TEXT("Auto border brighter than original"),
        FAidCardStyle().GetBorderColor().G > FAidCardStyle().HeaderBackgroundColor.G);
    UAidDocument* Document = NewObject<UAidDocument>(GetTransientPackage());
    Document->AssociatedLevels.Add(TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Maps/AidMap.AidMap"))));
    TestTrue(TEXT("Association matches map path"), Document->IsAssociatedWithLevel(TEXT("/Game/Maps/AidMap")));
    TestFalse(TEXT("Other map doesn't match"), Document->IsAssociatedWithLevel(TEXT("/Game/Maps/Elsewhere")));
    return true;
}

#endif
