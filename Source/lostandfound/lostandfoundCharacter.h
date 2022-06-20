// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "lostandfoundCharacter.generated.h"

UCLASS(config=Game)
class AlostandfoundCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class UCameraComponent* FollowCamera;
public:
	AlostandfoundCharacter();

	/** Base turn rate, in deg/sec. Other scaling may affect final turn rate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Input)
	float TurnRateGamepad;

protected:

	/** Called for forwards/backward input */
	void MoveForward(float Value);

	/** Called for side to side input */
	void MoveRight(float Value);

	/** 
	 * Called via input to turn at a given rate. 
	 * @param Rate	This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	 */
	void TurnAtRate(float Rate);

	/**
	 * Called via input to turn look up/down at a given rate. 
	 * @param Rate	This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	 */
	void LookUpAtRate(float Rate);

	/** Handler for when a touch input begins. */
	void TouchStarted(ETouchIndex::Type FingerIndex, FVector Location);

	/** Handler for when a touch input stops. */
	void TouchStopped(ETouchIndex::Type FingerIndex, FVector Location);

	void OnLanded(const FHitResult& Hit);
	void Tick(float time);

	void BeginPlay();

protected:
	// APawn interface
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	// End of APawn interface

	void Jump() override;
	void StopJumping() override;

	void ResetJump(int new_jump_count);

	UFUNCTION()
	void OnPlayerHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit);

	
	float mDefaultMaxWalkSpeed;

	// Wallrunning section
	enum EWallrunSide
	{
		WR_LEFT,
		WR_RIGHT
	};
	enum EWallrunEndReason
	{
		USER_JUMP,
		USER_STOP,
		FALL_OFF,
		HIT_GROUND
	};

	float mDefaultGravityScale;
	float mDefaultAirControl;
	bool mIsWallrunning = false;
	FVector mWallrunDir;
	float mRightAxis;
	float mForwardAxis;
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
	// Wallrunning section end


	#define IGNORE_SELF_COLLISION_PARAM FCollisionQueryParams(FName(TEXT("KnockTraceSingle")), true, this)


public:
	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
};

