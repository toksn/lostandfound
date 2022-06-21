// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SpecialMovementComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class LOSTANDFOUND_API USpecialMovementComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	USpecialMovementComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	
	// try to start wallrunning an a wall that was hit
	void tryWallrun(const FHitResult& wallHit);

	void ResetJump(int new_jump_count);
	void Jump();

	float mRightAxis;
	float mForwardAxis;

private:
	class ACharacter* owner;
	class UCharacterMovementComponent* move;

	enum EWallrunSide
	{
		WR_LEFT,
		WR_RIGHT
	};
	enum EWallrunEndReason
	{
		USER_JUMP,		/* user jumped off */
		USER_STOP,		/* user stopped the wallrun input */
		FALL_OFF,		/* user fell off */
		HIT_GROUND		/* user hit the ground */
	};

	float mDefaultGravityScale;
	float mDefaultAirControl;
	float mDefaultMaxWalkSpeed;
	bool mIsWallrunning = false;
	FVector mWallrunDir;
	EWallrunSide mWallrunSide;
	bool clawIntoWall;
	float clawZTargetVelo;
	float clawSpeed;
	float clawTime;

	FVector calcWallrunDir(FVector wallNormal, EWallrunSide side);
	EWallrunSide findWallrunSide(FVector wallNormal);
	FVector calcLaunchVelocity() const;
	bool surfaceIsWallrunPossible(FVector surfaceNormal) const;
	bool isWallrunInputPressed() const;
	void setHorizontalVelocity(FVector2D vel);
	FVector2D getHorizontalVelocity();
	void clampHorizontalVelocity();
	bool checkSideForWall(FHitResult& hit, EWallrunSide side, FVector forwardDirection, bool debug = false);

	// start to wallclaw, claw duration ~= 1 / speed (seconds)
	void startWallClaw(float speed, float targetZVelocity);
	void endWallClaw();

	void startWallrun(FVector wallNormal);
	void endWallrun(EWallrunEndReason endReason);
	void updateWallrun(float time);
};
