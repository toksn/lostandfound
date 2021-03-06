// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "SpecialMovementComponent.h"
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"

#define IGNORE_SELF_COLLISION_PARAM FCollisionQueryParams(FName(TEXT("KnockTraceSingle")), true, owner)
#define CAPSULE_RADIUS owner->GetCapsuleComponent()->GetScaledCapsuleRadius()
#define WALLRUN_REPLACEMENT CAPSULE_RADIUS * 1.2f

// Sets default values for this component's properties
USpecialMovementComponent::USpecialMovementComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void USpecialMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	mState = ESpecialMovementState::NONE;

	mMaxWallrunInnerAngle = FMath::Clamp(mMaxWallrunInnerAngle, 45.0f, 180.0f);
	mMaxWallrunInnerAngle = FMath::Clamp(mMaxWallrunOuterAngle, 45.0f, 180.0f);
	mMaxWallrunStartAngle = FMath::Clamp(mMaxWallrunStartAngle, 0.0f, 90.0f);

	// slowmo wallrun for testing
	// mWallrunSpeed *= 0.1f;
	// mWallrunGravity = 0.005f;

	// turn on debugging
	// mDebugJump = true;
	// mDebugSlide = true;
	// mDebugWallrun = true;
}

void USpecialMovementComponent::Init(ACharacter* parent, UCameraComponent* parentCamera, USpringArmComponent* parentCameraSpringArm) {
	owner = parent;
	move = owner->GetCharacterMovement();
	cameraStick = parentCameraSpringArm;
	camera = parentCamera;

	// get default values to reset after wallrun
	mDefaultGravityScale = move->GravityScale;
	mDefaultMaxWalkSpeed = move->MaxWalkSpeed;
	mDefaultAirControl = move->AirControl;

	// get default values to reset after slide
	mDefaultGroundFriction = move->GroundFriction;
	mDefaultBrakingDecelerationWalking = move->BrakingDecelerationWalking;
	mDefaultMaxWalkSpeedCrouched = move->MaxWalkSpeedCrouched;
}

// Called every frame
void USpecialMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (isWallrunning()) {
		updateWallrun(DeltaTime);
	}
	else if (mState == ESpecialMovementState::SLIDE) {
		updateSlide(DeltaTime);
	}
}

void USpecialMovementComponent::OnLanded(const FHitResult& Hit)
{
	ResetJump(0);
	resetWallrunPrevention();
	mJumpMidAirAllowed = false;
}

void USpecialMovementComponent::ResetJump(int new_jump_count)
{
	owner->JumpCurrentCount = FMath::Clamp(new_jump_count, 0, owner->JumpMaxCount);
}

void USpecialMovementComponent::Jump()
{
	FVector launchVelo = FVector::ZeroVector;
	if (isWallrunning()) {
		// jump off wall
		launchVelo = calcLaunchVelocity(false);
		endWallrun(EWallrunEndReason::USER_JUMP);
	}
	else if (owner->JumpCurrentCount < owner->JumpMaxCount) {
		if (move->IsFalling() == false || mJumpMidAirAllowed) {
			owner->JumpCurrentCount++;
			launchVelo = calcLaunchVelocity(true);
		}
	}

	if (launchVelo != FVector::ZeroVector) {
		owner->LaunchCharacter(launchVelo, false, true);
	}
}

void USpecialMovementComponent::Slide()
{
	startSlide();
}

void USpecialMovementComponent::clampHorizontalVelocity(FVector & velocity, float const maxSpeed) const
{
	FVector2D vel(velocity);
	if (vel.Length() > maxSpeed) {
		float speedFactor = maxSpeed / vel.Length();
		velocity.X = vel.X * speedFactor;
		velocity.Y = vel.Y * speedFactor;
	}
}

bool USpecialMovementComponent::checkDirectionForWall(FHitResult& hit, FVector const & origin, FVector direction)
{
	// check for the current wall next to the character with a single trace line
	float const traceLength = owner->GetCapsuleComponent()->GetCollisionShape().Capsule.Radius * 2.0f; // TODO: make member variable and expose to BP
	direction.Normalize();
	direction *= traceLength;

	if (mDebugWallrun) {
		DrawDebugLine(GetWorld(), origin, origin + direction, FColor::Red, false, 40.0f, 0U, 5.0f);
	}

	return GetWorld()->LineTraceSingleByChannel(hit, origin, origin + direction, ECollisionChannel::ECC_WorldStatic, IGNORE_SELF_COLLISION_PARAM);
}

void USpecialMovementComponent::startWallClaw(float speed, float targetZVelocity)
{
	mClawIntoWall = true;
	mClawTime = 0.0f;
	mClawZTargetVelo = targetZVelocity;
	mClawSpeed = speed;
	move->GravityScale = 0.0f;
}

void USpecialMovementComponent::endWallClaw()
{
	if (mClawIntoWall) {
		if (mDebugWallrun) {
			GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Green, "stopped claw into wall");
		}
		mClawIntoWall = false;
		move->GravityScale = mWallrunGravity;
	}
}

bool USpecialMovementComponent::isWallrunning(bool considerUp) const
{
	return mState == ESpecialMovementState::WALLRUN_LEFT || mState == ESpecialMovementState::WALLRUN_RIGHT || (considerUp && mState == ESpecialMovementState::WALLRUN_UP);
}

void USpecialMovementComponent::tryWallrun(const FHitResult& wallHit)
{
	if (mWallrunPrevention || isWallrunInputPressed() == false || move->IsFalling() == false) {
		return;
	}

	if (isWallrunning()) {
		if (surfaceIsWallrunPossible(wallHit.ImpactNormal)) {
			if (mDebugWallrun) {
				GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, FString::Printf(TEXT("hit something while wallrunning, check for blocking geometry.")));
			}

			// This wallhit has to get a corrected position because the impact might be on the other side of the player capsule
			FHitResult correctedWallhit = wallHit;
			correctedWallhit.ImpactPoint = mWallImpact + mWallrunDir * CAPSULE_RADIUS * 0.9f;
			double angle = 0.0f;
			if (isValidInnerOuterAngleDiff(move->GetActorLocation(), correctedWallhit, &angle)) {
				addCameraRotation(FRotator(0.0f, angle, 0.0f));

				mWallNormal = wallHit.ImpactNormal;
				mWallImpact = wallHit.ImpactPoint;
				mWallrunDir = calcWallrunDir(mWallNormal, mState);
				return;
			}
			else {
				// end wallrun with angle out of bounds to prevent a wallrunning loop in an inner corner.
				endWallrun(EWallrunEndReason::ANGLE_OUT_OF_BOUNDS);
				return;
			}
		}
	}

	if (mDebugWallrun) {
		DrawDebugLine(GetWorld(), wallHit.ImpactPoint, wallHit.ImpactPoint + (wallHit.ImpactNormal * 100.0f), FColor::Yellow, false, 40.0f, 0U, 5.0f);
	}

	if (surfaceIsWallrunPossible(wallHit.ImpactNormal)) {
		startWallrun(wallHit);
	}
}

void USpecialMovementComponent::startWallrun(const FHitResult& wallHit)
{
	mWallNormal = wallHit.ImpactNormal;
	mWallImpact = wallHit.ImpactPoint;
	auto state = findWallrunSide(mWallNormal);
	mWallrunDir = calcWallrunDir(mWallNormal, state);


	FHitResult hit;
	FVector side = owner->GetActorRightVector();
	if (state == ESpecialMovementState::WALLRUN_LEFT) {
		side *= -1.0f;
	}

	double const angle = calcAngleBetweenVectors(wallHit.ImpactNormal, -side);
	if (mDebugWallrun) {
		GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Purple, FString::Printf(TEXT("start wallrunning, angle: %f"), angle));

	}
	if (angle > mMaxWallrunStartAngle) {
		// too straight to begin wallrun left / right
		// TODO: wallrun up
		return;
	}

	if (switchState(state) == false) {
		return;
	}

	if (mCorrectCamera && cameraStick) {
		cameraStick->bEnableCameraRotationLag = true;
	}

	// use current velocity or maxWalkSpeed for the wallrun
	mWallrunSpeed = FMath::Max(FVector2D(move->Velocity).Length(), move->MaxWalkSpeed);

	move->GravityScale = mWallrunGravity;
	// claw onto the wall by slowing down the sliding when velocity is below gravity level
	if (move->Velocity.Z < move->GetGravityZ()) {
		startWallClaw(2.0f, move->GetGravityZ());
	}
	else {
		mClawIntoWall = false;
		move->Velocity.Z *= 0.35f;
		// consider clawing into the wall with another speed (slower than downwards claw, like ~1.3f)
		// startWallClaw(1.3f, move->GetGravityZ());
	}

	move->AirControl = 0.0f;
	move->bOrientRotationToMovement = false;
}

void USpecialMovementComponent::resetWallrunPrevention()
{
	mWallrunPrevention = false;
	GetWorld()->GetTimerManager().ClearTimer(mWallrunPreventTimer);
}

// This activates the wallrun prevention until the timer ran out or the character hit the ground.
// Use negative or 0.0f seconds for no time constraint, thus only reset when hit the ground.
void USpecialMovementComponent::setWallrunPrevention(float timeToPrevent)
{
	mWallrunPrevention = true;
	if (timeToPrevent > 0.0f) {
		GetWorld()->GetTimerManager().SetTimer(mWallrunPreventTimer, this, &USpecialMovementComponent::resetWallrunPrevention, 1.0f);
	}
}

void USpecialMovementComponent::endWallrun(EWallrunEndReason endReason)
{
	if (switchState(ESpecialMovementState::NONE) == false) {
		return;
	}

	// call endWallClaw before gravity reset because it does manipulate gravity as well
	endWallClaw();

	move->GravityScale = mDefaultGravityScale;
	move->AirControl = mDefaultAirControl;
	move->bOrientRotationToMovement = true;

	if (mCorrectCamera && cameraStick) {
		cameraStick->bEnableCameraRotationLag = false;
	}

	mJumpMidAirAllowed = false;

	if (endReason == USER_JUMP) {
		ResetJump(owner->JumpCurrentCount - mRegainJumpsAfterWalljump);
		mJumpMidAirAllowed = true;
		// TODO: this does not feel good, deactivated for now. Do we really need this?
		// setWallrunPrevention(0.05f);
	}
	else if (endReason == ANGLE_OUT_OF_BOUNDS) {
		// prevent next wallrun until we hit the ground or timed out
		setWallrunPrevention(0.5f);
	}
}

void USpecialMovementComponent::updateWallrun(float time)
{
	if (isWallrunInputPressed() == false) {
		endWallrun(USER_STOP);
		return;
	}

	if (move->IsFalling() == false) {
		endWallrun(HIT_GROUND);
		return;
	}

	FHitResult hit;
	if (checkDirectionForWall(hit, move->GetActorLocation(), - mWallNormal) == false) {
		if (mMaxWallrunOuterAngle <= 70.0f) {
			endWallrun(FALL_OFF);
			return;
		}
		else {
			// when sharp outer angles have to be detected
			// check another trace backwards from a position that should lie in front of a sharp outer wall turn
			FVector capsuleRight = move->GetActorLocation() - mWallNormal * WALLRUN_REPLACEMENT * 1.2f;
			if (checkDirectionForWall(hit, capsuleRight, -mWallrunDir) == false) {
				endWallrun(FALL_OFF);
				return;
			}
		}
	}

	double angle = 0.0f;
	if (isValidInnerOuterAngleDiff(move->GetActorLocation(), hit, &angle) == false) {
		// end wallrun with angle out of bounds to prevent a wallrunning loop in an inner corner.
		endWallrun(EWallrunEndReason::ANGLE_OUT_OF_BOUNDS);
		return;
	}
	addCameraRotation(FRotator(0.0f, angle, 0.0f));

	mWallNormal = hit.ImpactNormal;
	mWallImpact = hit.ImpactPoint;
	mWallrunDir = calcWallrunDir(mWallNormal, mState);

	// set velocity according to the wall direction
	if (mDebugWallrun) {
		DrawDebugLine(GetWorld(), owner->GetActorLocation(), owner->GetActorLocation() + (mWallrunDir * mWallrunSpeed), FColor::Blue, false, -1.0f, 0U, 5.0f);
	}
	move->Velocity.X = mWallrunDir.X * mWallrunSpeed;
	move->Velocity.Y = mWallrunDir.Y * mWallrunSpeed;

	// manually calc velocity Z when clawing into the wall
	if (mClawIntoWall) {
		mClawTime += time;
		if (mDebugWallrun) {
			GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Green, FString::Printf(TEXT("mClawTime: %f, velo: %f, targetVelo: %f"), mClawTime, move->Velocity.Z, mClawZTargetVelo));
		}
		move->Velocity.Z = FMath::FInterpTo(move->Velocity.Z, mClawZTargetVelo, mClawTime, mClawSpeed);
		if (move->Velocity.Z == mClawZTargetVelo) {
			endWallClaw();
		}
	}

	// correct wallrun position
	FVector positionCorrection = mWallImpact + mWallNormal * WALLRUN_REPLACEMENT - move->GetActorLocation();
	positionCorrection.Z = 0.0f;

	// manually set rotation based on the wallrun direction
	move->MoveUpdatedComponent(positionCorrection, mWallrunDir.Rotation(), false);
}

bool USpecialMovementComponent::switchState(ESpecialMovementState newState)
{
	bool applyChange = true;
	switch (newState) {
	case ESpecialMovementState::WALLRUN_UP:
		applyChange = mState == ESpecialMovementState::NONE ||
			mState == ESpecialMovementState::SLIDE ||
			mState == ESpecialMovementState::WALLRUN_LEFT ||
			mState == ESpecialMovementState::WALLRUN_RIGHT;
		break;

	case ESpecialMovementState::WALLRUN_LEFT:
	case ESpecialMovementState::WALLRUN_RIGHT:
		applyChange = mState == ESpecialMovementState::NONE ||
			mState == ESpecialMovementState::SLIDE ||
			mState == ESpecialMovementState::WALLRUN_UP;
		break;

	case ESpecialMovementState::SLIDE:
		applyChange = mState == ESpecialMovementState::NONE;
		break;

	case ESpecialMovementState::LEDGE_PULL:
		applyChange = mState == ESpecialMovementState::ON_LEDGE;
		break;

	case ESpecialMovementState::ON_LEDGE:
		applyChange = isWallrunning(true) || mState == ESpecialMovementState::NONE;
		break;

	}

	if (applyChange) {
		if (mState == ESpecialMovementState::SLIDE && newState != ESpecialMovementState::NONE) {
			endSlide(EWallrunEndReason::USER_STOP);
		}
		mState = newState;
	}

	return applyChange;
}

FVector USpecialMovementComponent::calcWallrunDir(FVector wallNormal, ESpecialMovementState state)
{
	if (state != ESpecialMovementState::WALLRUN_LEFT && state != ESpecialMovementState::WALLRUN_RIGHT && state != ESpecialMovementState::WALLRUN_UP) {
		return FVector();
	}

	if (state == ESpecialMovementState::WALLRUN_UP) {
		return FVector(0,0,1.0f-wallNormal.Z);
	}

	FVector perpVec(0, 0, 1);
	if (state == ESpecialMovementState::WALLRUN_RIGHT) {
		perpVec.Z = -1.0f;
	}

	FVector wallrunDir = FVector::CrossProduct(wallNormal, perpVec);
	wallrunDir.Normalize();
	return wallrunDir;
}

ESpecialMovementState USpecialMovementComponent::findWallrunSide(FVector wallNormal)
{
	if (FVector2D::DotProduct(FVector2D(owner->GetActorRightVector()), FVector2D(wallNormal)) > 0.0f) {
		return ESpecialMovementState::WALLRUN_LEFT;
	}
	else {
		return ESpecialMovementState::WALLRUN_RIGHT;
	}
}

bool USpecialMovementComponent::canJumpBoost() const
{
	return move->IsFalling() == false && (mState == ESpecialMovementState::NONE || mState == ESpecialMovementState::SLIDE);
}

FVector USpecialMovementComponent::calcLaunchVelocity(bool jumpBoostEnabled) const
{
	FVector launchDir(0, 0, 0);
	bool clampVelo = true;

	if (isWallrunning()) {
		switch (mState) {
		case ESpecialMovementState::WALLRUN_LEFT:
		case ESpecialMovementState::WALLRUN_RIGHT:
		case ESpecialMovementState::WALLRUN_UP:
			launchDir = mWallNormal;
			launchDir.Normalize();
			launchDir *= move->JumpZVelocity;
			break;
		}
	}
	else if (move->IsFalling()) {
		// owner->GetActorRightAxis might be in forwardvector direction because bOrientToMovement and viceversa, use camera instead
		launchDir = mRightAxis * camera->GetRightVector() + mForwardAxis * camera->GetForwardVector();
		launchDir.Normalize();
		launchDir *= move->JumpZVelocity;
	}
	else if (jumpBoostEnabled && canJumpBoost()) {
		// Jumpboost: check for a close edge
		FVector movedir = owner->GetActorForwardVector();
		float const length = owner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + move->MaxStepHeight * 2.0f;
		float const tolerance = CAPSULE_RADIUS * 1.5f;
		// capsule radius is needed to offset from the center the actor
		FVector traceDownOrigin = move->GetActorLocation() + movedir * (CAPSULE_RADIUS + tolerance);

		FHitResult hit;
		bool onEdge = false == GetWorld()->LineTraceSingleByChannel(hit, traceDownOrigin, traceDownOrigin - FVector(0.0f, 0.0f, length), ECollisionChannel::ECC_WorldStatic, IGNORE_SELF_COLLISION_PARAM);

		if (mDebugJump) {
			FColor debugCol = onEdge ? FColor::Green : FColor::Red;
			DrawDebugLine(GetWorld(), traceDownOrigin, traceDownOrigin - FVector(0.0f, 0.0f, length), debugCol, false, 100.0f, 0U, 5.0f);
			if (onEdge) {
				GEngine->AddOnScreenDebugMessage(-1, 15.0f, debugCol, FString::Printf(TEXT("JUMP BOOST RECEIVED!")));
			}
		}

		if (onEdge) {
			// boost by multiplier - 1.0f to offset for the current velocity
			launchDir.X = move->Velocity.X * (mJumpBoostMultiplier - 1.0f);
			launchDir.Y = move->Velocity.Y * (mJumpBoostMultiplier - 1.0f);
		}
		clampVelo = false;
	}

	if (clampVelo) {
		// do not gain more than the current horizontal velocity
		FVector launchVelo = move->Velocity + launchDir;
		float const currentHorizontalSpeed = FVector2D(move->Velocity).Length();
		clampHorizontalVelocity(launchVelo, currentHorizontalSpeed);

		launchDir = launchVelo - move->Velocity;
	}

	launchDir.Z = move->JumpZVelocity;

	if (mDebugJump) {
		DrawDebugLine(GetWorld(), owner->GetActorLocation(), owner->GetActorLocation() + launchDir, FColor::Green, false, 40.0f, 0U, 5.0f);
		GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Red, FString::Printf(TEXT("IsFalling % d, launchDir %s"), move->IsFalling(), *launchDir.ToString()));
	}

	return launchDir;
}

bool USpecialMovementComponent::surfaceIsWallrunPossible(FVector surfaceNormal) const
{
	// z < -0.05f is questionable?! i could try to wallrun on slopes that are kinda tilted inwards (looking down)
	if (surfaceNormal.Z < -0.05f || surfaceNormal.Z > move->GetWalkableFloorZ()) {
		return false;
	}
	return true;
}

bool USpecialMovementComponent::isWallrunInputPressed() const
{
	return true;
}

double USpecialMovementComponent::calcAngleBetweenVectors(FVector a, FVector b)
{
	return FMath::Acos(FVector::DotProduct(a, b)) * 180.0f / PI;
}


bool USpecialMovementComponent::isValidInnerOuterAngleDiff(FVector const & origin, const FHitResult& hit, double * angleOut)
{
	FVector const hitdirection = FVector::CrossProduct(hit.ImpactNormal, FVector(0, 0, mState == ESpecialMovementState::WALLRUN_LEFT ? 1 : -1));
	double const angle = calcAngleBetweenVectors(mWallrunDir, hitdirection);

	// inner / outer angle determination taken from: https://stackoverflow.com/questions/12397564/check-if-an-angle-defined-by-3-points-is-inner-or-outer
	// Describe the two angle vectors coming from the center point as a and b. And describe the vector from your center point to the origin as center.
	FVector const center = origin - hit.ImpactPoint;
	FVector const a = -mWallrunDir;
	FVector const b = hitdirection;
	bool const isInnerAngle = (FVector::DotProduct(a + b, center) > 0.0 && FVector::DotProduct(FVector::CrossProduct(a, center), FVector::CrossProduct(b, center)) < 0.0);

	float maxAngle = isInnerAngle? mMaxWallrunInnerAngle : mMaxWallrunOuterAngle;

	if (mDebugWallrun && angle > 0.005f) {
		FColor debugCol = isInnerAngle ? FColor::Yellow : FColor::Green;
		DrawDebugLine(GetWorld(), hit.ImpactPoint + a * 100, hit.ImpactPoint, debugCol, false, 100.0f, 0U, 5.0f);
		DrawDebugLine(GetWorld(), hit.ImpactPoint + b * 100, hit.ImpactPoint, debugCol, false, 100.0f, 0U, 5.0f);
		DrawDebugCrosshairs(GetWorld(), origin, FRotator::ZeroRotator, 30.0f, FColor::Blue, false, 100.0f, 0U);
		GEngine->AddOnScreenDebugMessage(-1, 15.0f, debugCol, FString::Printf(TEXT("angle: %f"), angle));
	}

	if (angleOut) {
		if ((isInnerAngle && mState == ESpecialMovementState::WALLRUN_LEFT) ||
			(isInnerAngle == false && mState == ESpecialMovementState::WALLRUN_RIGHT)) {

			*angleOut = angle;
		}
		else {
			*angleOut = -angle;
		}
	}

	if (angle > maxAngle) {
		return false;
	}

	return true;
}

void USpecialMovementComponent::addCameraRotation(FRotator const & addRotation)
{
	if (mCorrectCamera == false) {
		return;
	}

	AController* controller = owner->GetController();
	if (controller) {
		FRotator const currentRotation = controller->GetControlRotation();
		controller->SetControlRotation(currentRotation + addRotation);
	}
}

bool USpecialMovementComponent::canSlide()
{
	if (move->IsFalling() || move->CurrentFloor.IsWalkableFloor() == false || mState != ESpecialMovementState::NONE) {
		return false;
	}
	return true;
}

void USpecialMovementComponent::startSlide()
{
	if (canSlide() == false) {
		return;
	}

	if (switchState(ESpecialMovementState::SLIDE) == false) {
		return;
	}

	// addImpulse in the direction of the current floor
	FVector const FloorNormal = move->CurrentFloor.HitResult.ImpactNormal;
	FVector launchInFloorDirection = FVector::CrossProduct(FloorNormal, owner->GetActorRightVector()) * -1.0f;

	launchInFloorDirection.Normalize();
	launchInFloorDirection *= move->MaxWalkSpeed * mSlideForceMultiplier;

	// 1.2f factor for a little tolerance to "feel" better because slide can be activated more reliably
	if (move->Velocity.Length() <= move->MaxWalkSpeed * 1.2f) {
		move->AddImpulse(launchInFloorDirection, true);

		if (mDebugSlide) {
			GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Cyan, FString::Printf(TEXT("started slide dir: %s"), *launchInFloorDirection.ToString()));
			DrawDebugLine(GetWorld(), owner->GetActorLocation(), owner->GetActorLocation() + launchInFloorDirection, FColor::Cyan, false, 100.0f, 0U, 5.0f);
		}
	}
	else if (mDebugSlide) {
		DrawDebugLine(GetWorld(), owner->GetActorLocation(), owner->GetActorLocation() + move->Velocity, FColor::Orange, false, 100.0f, 0U, 5.0f);
	}

	owner->Crouch();
	move->MaxWalkSpeedCrouched = 0.0f;
	move->GroundFriction = 0.0f;
	move->BrakingDecelerationWalking = mSlideDeceleration;
	move->bOrientRotationToMovement = false;

	move->SetPlaneConstraintFromVectors(launchInFloorDirection, FloorNormal);
	move->SetPlaneConstraintEnabled(true);
}

void USpecialMovementComponent::endSlide(EWallrunEndReason endReason)
{
	if (switchState(ESpecialMovementState::NONE) == false) {
		return;
	}

	owner->UnCrouch();
	move->MaxWalkSpeedCrouched = mDefaultMaxWalkSpeedCrouched;
	move->GroundFriction = mDefaultGroundFriction;
	move->BrakingDecelerationWalking = mDefaultBrakingDecelerationWalking;
	move->bOrientRotationToMovement = true;

	move->SetPlaneConstraintEnabled(false);
}

void USpecialMovementComponent::updateSlide(float time)
{
	if (move->IsFalling() || move->CurrentFloor.IsWalkableFloor() == false ) {
		if (mDebugSlide) {
			GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Cyan, FString::Printf(TEXT("stopped slide. reason: in air")));
		}
		endSlide(EWallrunEndReason::FALL_OFF);
	}

	if (move->Velocity.Length() < move->MaxWalkSpeed * 0.9f) {
		if (mDebugSlide) {
			GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Cyan, FString::Printf(TEXT("stopped slide. reason: velocity too low")));
		}
		endSlide(EWallrunEndReason::ANGLE_OUT_OF_BOUNDS);
	}
}
