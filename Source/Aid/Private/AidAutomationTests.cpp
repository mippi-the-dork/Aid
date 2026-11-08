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

#endif
