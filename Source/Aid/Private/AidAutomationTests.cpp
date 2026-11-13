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

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAidUnplacedNotesTest,
    "Aid.Data.Authoring.UnplacedNotesStayOutOfViewport",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAidUnplacedNotesTest::RunTest(const FString& Parameters)
{
    UAidDocument* Doc = NewObject<UAidDocument>(GetTransientPackage());
    const FGuid Id = Doc->AddUnplacedNode(FText::FromString(TEXT("Pending puzzle note")));
    const FAidNode* Note = Doc->FindNode(Id);
    TestNotNull(TEXT("Unplaced note exists in the document"), Note);
    if (!Note) return false;
    TestEqual(TEXT("Unplaced anchor persisted in the data model"), Note->Anchor.Type, EAidAnchorType::Unplaced);
    TestEqual(TEXT("Unplaced note never renders"), UAidDocument::GetNoteVisibility(*Note, FVector::ZeroVector, FVector::ZeroVector), static_cast<uint8>(0));
    TestTrue(TEXT("An unplaced note is valid authored content"), Doc->ValidateGraph().IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAidUnplacedConnectionsTest,
    "Aid.Data.Visibility.UnplacedEndpointSuppressesConnection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAidUnplacedConnectionsTest::RunTest(const FString& Parameters)
{
    FAidNode A, B;
    A.ViewDistance = 2000.f;
    B.ViewDistance = 2000.f;
    B.Anchor.Type = EAidAnchorType::Unplaced;
    FAidConnection Edge;
    const FVector Camera = FVector::ZeroVector;
    const FVector Near(500., 0., 0.), Far(10000., 0., 0.);
    TestFalse(TEXT("No false line to world origin for unplaced target"),
        UAidDocument::IsConnectionVisible(Edge, A, B, Camera, Near, FVector::ZeroVector));
    B.Anchor.Type = EAidAnchorType::World;
    TestTrue(TEXT("Once placed, far target can be traced from visible source"),
        UAidDocument::IsConnectionVisible(Edge, A, B, Camera, Near, Far));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAidDocumentOrganizationTest,
    "Aid.Data.Organization.DocumentCategoryAndLevelAssociation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAidDocumentOrganizationTest::RunTest(const FString& Parameters)
{
    UAidDocument* Doc = NewObject<UAidDocument>(GetTransientPackage());
    TestTrue(TEXT("Default category is ungrouped"), Doc->DocumentCategory.IsNone());
    Doc->DocumentCategory = TEXT("QA");
    TestEqual(TEXT("Document category changes without changing notes"), Doc->DocumentCategory, FName(TEXT("QA")));
    TestTrue(TEXT("Empty document has no level associations"), !Doc->IsAssociatedWithLevel(TEXT("/Game/TestMap")));
    const UAidProjectSettings* Settings = GetDefault<UAidProjectSettings>();
    TestTrue(TEXT("Project provides document category suggestions"), Settings->DocumentCategories.Contains(FName(TEXT("QA"))));
    return true;
}

#endif

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAidUnorderedSingleConnectionTest,
    "Aid.Data.Graph.OneConnectionPerUnorderedPair",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAidUnorderedSingleConnectionTest::RunTest(const FString& Parameters)
{
    UAidDocument* Doc = NewObject<UAidDocument>(GetTransientPackage());
    const FGuid A = Doc->AddNode(FText::FromString(TEXT("Door")), FVector::ZeroVector);
    const FGuid B = Doc->AddNode(FText::FromString(TEXT("Switch")), FVector(50., 0., 0.));
    const FGuid C = Doc->AddNode(FText::FromString(TEXT("Power")), FVector(100., 0., 0.));
    TestTrue(TEXT("A-B initially absent"), !Doc->HasConnectionBetween(A, B));
    TestTrue(TEXT("First A-B connection succeeds"), Doc->AddConnection(A, B, FText::FromString(TEXT("Requires"))).IsValid());
    TestTrue(TEXT("A-B recognized in both directions"), Doc->HasConnectionBetween(B, A));
    TestFalse(TEXT("Second A-B connection blocked"), Doc->AddConnection(A, B, FText::GetEmpty()).IsValid());
    TestFalse(TEXT("Reverse B-A connection blocked"), Doc->AddConnection(B, A, FText::GetEmpty()).IsValid());
    TestEqual(TEXT("Rejected additions do not mutate array"), Doc->Connections.Num(), 1);
    TestTrue(TEXT("A-C is a distinct pair"), Doc->AddConnection(A, C, FText::GetEmpty()).IsValid());
    TestEqual(TEXT("Only two unique note pairs exist"), Doc->Connections.Num(), 2);
    TestTrue(TEXT("Valid graph"), Doc->ValidateGraph().IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAidImportedDuplicateDetectionTest,
    "Aid.Data.Graph.DuplicatePairValidation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAidImportedDuplicateDetectionTest::RunTest(const FString& Parameters)
{
    UAidDocument* Doc = NewObject<UAidDocument>(GetTransientPackage());
    const FGuid A = Doc->AddNode(FText::FromString(TEXT("One")), FVector::ZeroVector);
    const FGuid B = Doc->AddNode(FText::FromString(TEXT("Two")), FVector(100., 0., 0.));
    TestTrue(TEXT("Existing edge created"), Doc->AddConnection(A, B, FText::GetEmpty()).IsValid());
    FAidConnection Reversed = Doc->Connections[0];
    Reversed.Id = FGuid::NewGuid();
    Reversed.SourceNode = B;
    Reversed.TargetNode = A;
    Doc->Connections.Add(Reversed); // Simulates old/externally-authored duplicate asset.
    TestTrue(TEXT("Validator reports legacy duplicate unordered pair"),
        Doc->ValidateGraph().ContainsByPredicate([](const FString& Error) { return Error.Contains(TEXT("duplicates")); }));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAidRetargetPreservesIdentityTest,
    "Aid.Data.Graph.ConnectionRetargeting",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAidRetargetPreservesIdentityTest::RunTest(const FString& Parameters)
{
    UAidDocument* Doc = NewObject<UAidDocument>(GetTransientPackage());
    const FGuid A = Doc->AddUnplacedNode(FText::FromString(TEXT("A")));
    const FGuid B = Doc->AddUnplacedNode(FText::FromString(TEXT("B")));
    const FGuid C = Doc->AddUnplacedNode(FText::FromString(TEXT("C")));
    const FGuid D = Doc->AddUnplacedNode(FText::FromString(TEXT("D")));
    const FGuid AB = Doc->AddConnection(A, B, FText::FromString(TEXT("Retarget me")));
    const FGuid AC = Doc->AddConnection(A, C, FText::FromString(TEXT("Existing")));
    TestTrue(TEXT("Connections created"), AB.IsValid() && AC.IsValid());
    TestFalse(TEXT("Retarget fails for duplicate unordered pair"), Doc->RetargetConnection(AB, C));
    TestFalse(TEXT("Self-connections cannot be created by retargeting"), Doc->RetargetConnection(AB, A));
    TestFalse(TEXT("Unknown targets rejected"), Doc->RetargetConnection(AB, FGuid::NewGuid()));
    TestTrue(TEXT("Valid destination accepted"), Doc->RetargetConnection(AB, D));
    const FAidConnection* Moved = Doc->Connections.FindByPredicate([AB](const FAidConnection& E){ return E.Id == AB; });
    TestNotNull(TEXT("Original connection ID retained"), Moved);
    if (Moved)
    {
        TestEqual(TEXT("Retargeted endpoint"), Moved->TargetNode, D);
        TestEqual(TEXT("Label retained"), Moved->Label.ToString(), FString(TEXT("Retarget me")));
    }
    TestTrue(TEXT("Resulting graph valid"), Doc->ValidateGraph().IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAidDerivedColorAndCustomLabelTest,
    "Aid.Data.Presets.SingleBaseColorAndCustomFooter",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAidDerivedColorAndCustomLabelTest::RunTest(const FString& Parameters)
{
    const UAidProjectSettings* Settings = GetDefault<UAidProjectSettings>();
    FAidNode Note;
    Note.Category = NAME_None;
    TestEqual(TEXT("Default is permanent option"), Settings->GetNotePresetLabel(Note), FString(TEXT("Default")));
    Note.Category = TEXT("Design");
    TestEqual(TEXT("Named preset labels appear"), Settings->GetNotePresetLabel(Note), FString(TEXT("Design")));
    Note.bCustomAppearance = true;
    Note.CustomFooterText = FText::FromString(TEXT("Custom intent"));
    Note.Style.ApplyBaseColor(FLinearColor(.25f,.66f,.45f,1.f));
    TestEqual(TEXT("Custom mode overrides preset name"), Settings->GetNotePresetLabel(Note), FString(TEXT("Custom")));
    TestEqual(TEXT("Custom footer is used"), Settings->GetNoteFooterLabel(Note), FString(TEXT("Custom intent")));
    TestTrue(TEXT("Body derived darker than header"),
        FAidCardStyle::GetBodyColor(Note.Style.HeaderBackgroundColor).G < Note.Style.HeaderBackgroundColor.G);
    TestTrue(TEXT("Footer darker than body"),
        FAidCardStyle::GetFooterColor(Note.Style.HeaderBackgroundColor).G <
        FAidCardStyle::GetBodyColor(Note.Style.HeaderBackgroundColor).G);
    FAidConnection Edge;
    TestEqual(TEXT("Default relationship permanent"), Settings->GetConnectionPresetLabel(Edge), FString(TEXT("Default")));
    Edge.bCustomAppearance = true;
    Edge.CustomFooterText = FText::FromString(TEXT("Custom relation"));
    TestEqual(TEXT("Custom connection footer"), Settings->GetConnectionFooterLabel(Edge), FString(TEXT("Custom relation")));
    return true;
}

#endif
