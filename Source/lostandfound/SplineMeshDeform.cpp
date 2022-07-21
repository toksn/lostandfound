// Fill out your copyright notice in the Description page of Project Settings.


#include "SplineMeshDeform.h"
#include "Runtime/Engine/Classes/Components/SplineComponent.h"
#include "Runtime/Engine/Classes/Components/SplineMeshComponent.h"

// Sets default values
ASplineMeshDeform::ASplineMeshDeform()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	//PrimaryActorTick.bCanEverTick = true;

	m_spline = CreateDefaultSubobject<USplineComponent>(TEXT("Spline"));
	RootComponent = m_spline;
	//m_spline->SetUnselectedSplineSegmentColor(FLinearColor::Red);
}

void ASplineMeshDeform::constructSplineMeshes() {
	if (m_spline->GetNumberOfSplinePoints() > 1 && m_splineMesh)
	{
		TArray<USceneComponent*> a;
		m_spline->GetChildrenComponents(false, a);
		for (USceneComponent* segment : a)
		{
			// only destroy it when it is a splinemeshcomponent
			if (segment->IsA<USplineMeshComponent>())
				segment->DestroyComponent();
		}

		ESplineCoordinateSpace::Type space = ESplineCoordinateSpace::Local;

		// try to get "length" of staticMesh used:
		FBox bb = m_splineMesh->GetBoundingBox();
		FVector axisLengths = bb.Max - bb.Min;
		//axisLengths *= GetActorScale();

		float fSplineMeshLength = axisLengths.X;
		if (mLengthAxis == EAxis::Type::Y) {
			fSplineMeshLength = axisLengths.Y;
		}
		else if (mLengthAxis == EAxis::Type::Z) {
			fSplineMeshLength = axisLengths.Z;
		}

		fSplineMeshLength = FMath::Abs(fSplineMeshLength);

		GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Red, FString::Printf(TEXT("SplineMeshLength %f"), fSplineMeshLength));
		float splineLen = m_spline->GetSplineLength();
		for (float currentDistance = 0.0; currentDistance < m_spline->GetSplineLength(); currentDistance += fSplineMeshLength)
		{
			// create UsplineMeshComponent
			USplineMeshComponent* segmentMesh = NewObject<USplineMeshComponent>(this);

			segmentMesh->SetStaticMesh(m_splineMesh);
			//segmentMesh->SetRelativeScale3D(GetActorScale());
			segmentMesh->SetStartScale(FVector2D(mMeshScale_Y, mMeshScale_Z));
			segmentMesh->SetEndScale(FVector2D(mMeshScale_Y, mMeshScale_Z));

			segmentMesh->SetMobility(EComponentMobility::Movable);
			segmentMesh->AttachToComponent(m_spline, FAttachmentTransformRules::KeepRelativeTransform);
			segmentMesh->RegisterComponent();

			FVector start, end, start_tangent, end_tangent;
			float endDist = currentDistance + fSplineMeshLength;
			// clamp at the end
			//float endDist = FMath::Min(currentDistance + fSplineMeshLength, m_spline->GetSplineLength());

			start = m_spline->GetLocationAtDistanceAlongSpline(currentDistance, space);
			start_tangent = m_spline->GetTangentAtDistanceAlongSpline(currentDistance, space);

			end = m_spline->GetLocationAtDistanceAlongSpline(endDist, space);
			end_tangent = m_spline->GetTangentAtDistanceAlongSpline(endDist, space);

			if (mSplineAtTop) {
				start.Z -= axisLengths.Z * mMeshScale_Z;
				end.Z -= axisLengths.Z * mMeshScale_Z;
			}

			segmentMesh->SetStartAndEnd(start, start_tangent, end, end_tangent);

			segmentMesh->SetCollisionEnabled(mCollisionEnabled);
			segmentMesh->SetCollisionObjectType(mCollisionChannel);
			// TODO include?
			//segmentMesh->SetCollisionProfileName("");
			//segmentMesh->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Block);
		}
	}
}

// Called when the game starts or when spawned
void ASplineMeshDeform::BeginPlay()
{
	Super::BeginPlay();
}

void ASplineMeshDeform::OnConstruction(const FTransform& Transform)
{
	constructSplineMeshes();
}

// Called every frame
void ASplineMeshDeform::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

