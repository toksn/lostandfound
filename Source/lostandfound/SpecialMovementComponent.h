// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SpecialMovementComponent.generated.h"

UENUM(BlueprintType)
enum class ESpecialMovementState : uint8
{
	NONE			UMETA(DisplayName = "None"),
	WALLRUN_LEFT	UMETA(DisplayName = "Wallrun Left"),
	WALLRUN_RIGHT	UMETA(DisplayName = "Wallrun Right"),
	WALLRUN_UP		UMETA(DisplayName = "Wallrun Up"),
	SLIDE			UMETA(DisplayName = "Slide"),
	ON_LEDGE		UMETA(DisplayName = "On Ledge"),
	LEDGE_PULL		UMETA(DisplayName = "Ledge Pull")
};


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

	void OnLanded(const FHitResult& Hit);
	void Jump();

	float mRightAxis;
	float mForwardAxis;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	float mWallrunGravity = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	int mRegainJumpsAfterWalljump = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Status)
	ESpecialMovementState mState;

	/** Maximum inner angle to keep wallrunning, in degrees. Range 0.0f to 180.0f */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	float mMaxWallrunInnerAngle = 70.0f;

	/** Maximum angle between character direction and wall to start wallrunning, in degrees. Range 0.0f to 90.0f*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	float mMaxWallrunStartAngle = 45.0f;

private:
	class ACharacter* owner;
	class UCharacterMovementComponent* move;

	enum EWallrunEndReason
	{
		USER_JUMP,		/* user jumped off */
		USER_STOP,		/* user stopped the wallrun input */
		FALL_OFF,		/* user fell off */
		HIT_GROUND,		/* user hit the ground */
		ANGLE_OUT_OF_BOUNDS	/* angle to keep wallrunning was exceeded */
	};

	float mDefaultGravityScale;
	float mDefaultAirControl;
	float mDefaultMaxWalkSpeed;

	FVector mWallrunDir;
	FVector mWallNormal;
	FVector mWallImpact;
	float mWallrunSpeed;

	bool mClawIntoWall;
	float mClawZTargetVelo;
	float mClawSpeed;
	float mClawTime;

	bool mWallrunPrevention = false;
	FTimerHandle mWallrunPreventTimer;
	UFUNCTION()
	void resetWallrunPrevention();
	void setWallrunPrevention(float timeToPrevent);

	void ResetJump(int new_jump_count);

	ESpecialMovementState findWallrunSide(FVector wallNormal);
	bool checkDirectionForWall(FHitResult& hit, FVector direction, bool debug = false);
	FVector calcWallrunDir(FVector wallNormal, ESpecialMovementState state);
	FVector calcLaunchVelocity() const;

	bool surfaceIsWallrunPossible(FVector surfaceNormal) const;
	bool isWallrunInputPressed() const;

	void setHorizontalVelocity(FVector2D vel);
	FVector2D getHorizontalVelocity();
	void clampHorizontalVelocity();

	// start to wallclaw, claw duration ~= 1 / speed (seconds)
	void startWallClaw(float speed, float targetZVelocity);
	void endWallClaw();

	bool isWallrunning(bool considerUp = false) const;
	void startWallrun(const FHitResult& wallHit);
	void endWallrun(EWallrunEndReason endReason);
	void updateWallrun(float time);

	double calcAngleBetweenVectors(FVector a, FVector b);

	bool switchState(ESpecialMovementState newState);
};
