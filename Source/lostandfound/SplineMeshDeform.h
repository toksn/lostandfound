// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UObject/NoExportTypes.h"

#include "SplineMeshDeform.generated.h"

class USplineMeshComponent;

UCLASS()
class LOSTANDFOUND_API ASplineMeshDeform : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ASplineMeshDeform();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Spline)
	class USplineComponent* m_spline;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Spline)
	UStaticMesh* m_splineMesh;

	// splinemeshaxis?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	TEnumAsByte<EAxis::Type> mLengthAxis = EAxis::X;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	TEnumAsByte<ECollisionEnabled::Type> mCollisionEnabled = ECollisionEnabled::QueryAndPhysics;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	TEnumAsByte<ECollisionChannel> mCollisionChannel = ECollisionChannel::ECC_WorldStatic;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	float mMeshScale_Y = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	float mMeshScale_Z = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	bool mSplineAtTop = false;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	virtual void OnConstruction(const FTransform& Transform) override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

private:

	void constructSplineMeshes();
};
