// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "SpecialMovementComponent.h"
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"

#define IGNORE_SELF_COLLISION_PARAM FCollisionQueryParams(FName(TEXT("KnockTraceSingle")), true, owner)
#define WALLRUN_REPLACEMENT owner->GetCapsuleComponent()->GetScaledCapsuleRadius() * 1.2f

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

	owner = Cast<ACharacter>(GetOwner());
	move = owner->GetCharacterMovement();

	// get default values to reset after wallrun
	mDefaultGravityScale = move->GravityScale;
	mDefaultMaxWalkSpeed = move->MaxWalkSpeed;
	mDefaultAirControl = move->AirControl;

	mWallrunSpeed = move->MaxWalkSpeed;

	mMaxWallrunInnerAngle = FMath::Clamp(mMaxWallrunInnerAngle, 45.0f, 180.0f);
	mMaxWallrunInnerAngle = FMath::Clamp(mMaxWallrunOuterAngle, 45.0f, 180.0f);
	mMaxWallrunStartAngle = FMath::Clamp(mMaxWallrunStartAngle, 0.0f, 90.0f);

	// slowmo wallrun for testing
	// mWallrunSpeed *= 0.1f;
	// mWallrunGravity = 0.005f;
}


// Called every frame
void USpecialMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (isWallrunning()) {
		updateWallrun(DeltaTime);
	}

	clampHorizontalVelocity();
}

void USpecialMovementComponent::OnLanded(const FHitResult& Hit)
{
	ResetJump(0);
	resetWallrunPrevention();
}

void USpecialMovementComponent::ResetJump(int new_jump_count)
{
	owner->JumpCurrentCount = FMath::Clamp(new_jump_count, 0, owner->JumpMaxCount);
}

void USpecialMovementComponent::Jump()
{
	FVector launchVelo;
	if (isWallrunning()) {
		// jump off wall
		launchVelo = calcLaunchVelocity();
		endWallrun(EWallrunEndReason::USER_JUMP);
	}
	else if (owner->JumpCurrentCount < owner->JumpMaxCount) {
		//if (move->IsFalling()) {
			//PlayAnimMontage()
		//}	
		owner->JumpCurrentCount++;
		launchVelo = calcLaunchVelocity();
	}

	if (launchVelo.Length() > 0) {
		owner->LaunchCharacter(launchVelo, false, true);
	}

	// are we close to an edge?
		// velocity (direction) is at least 20° off the edge direction
			// setMaxWalkSpeed * 2.0f;
			// setVelocity *= 2.0f;
			// startResetMaxWalkSpeedTimer(3 seconds)
				// Interp the maxWalkSpeed back
}

void USpecialMovementComponent::setHorizontalVelocity(FVector2D vel)
{
	move->Velocity.X = vel.X;
	move->Velocity.Y = vel.Y;
}

FVector2D USpecialMovementComponent::getHorizontalVelocity()
{
	return FVector2D(move->Velocity.X, move->Velocity.Y);
}

void USpecialMovementComponent::clampHorizontalVelocity()
{
	if (move->IsFalling()) {
		FVector2D vel = getHorizontalVelocity();
		if (vel.Length() > move->MaxWalkSpeed) {
			float speedFactor = vel.Length() / move->MaxWalkSpeed;
			setHorizontalVelocity(vel / speedFactor);
		}
	}
}

bool USpecialMovementComponent::checkDirectionForWall(FHitResult& hit, FVector const & origin, FVector direction, bool debug)
{
	/* check for the current wall next to the character with a single trace line */
	float const traceLength = owner->GetCapsuleComponent()->GetCollisionShape().Capsule.Radius * 2.0f; // TODO: make member variable and expose to BP
	direction.Normalize();
	direction *= traceLength;

	if (debug) {
		DrawDebugLine(GetWorld(), origin, origin + direction, FColor::Red, false, 40.0f, 0U, 5.0f);
	}

	return GetWorld()->LineTraceSingleByChannel(hit, origin, origin + direction, ECollisionChannel::ECC_Visibility, IGNORE_SELF_COLLISION_PARAM);
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
		GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Green, "stopped claw into wall");
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
			GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, FString::Printf(TEXT("hit something while wallrunning, check for blocking geometry.")));

			// This wallhit has to get a corrected position because the impact might be on the other side of the player capsule
			FHitResult correctedWallhit = wallHit;
			correctedWallhit.ImpactPoint = mWallImpact + mWallrunDir * owner->GetCapsuleComponent()->GetScaledCapsuleRadius() * 0.9f;
			if (isValidInnerOuterAngleDiff(move->GetActorLocation(), correctedWallhit)) {
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

	DrawDebugLine(GetWorld(), wallHit.ImpactPoint, wallHit.ImpactPoint + (wallHit.ImpactNormal * 100.0f), FColor::Yellow, false, 40.0f, 0U, 5.0f);

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
	GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Purple, FString::Printf(TEXT("start wallrunning, angle: %f"), angle));
	if (angle > mMaxWallrunStartAngle) {
		// too straight to begin wallrun left / right
		// TODO: wallrun up
		return;
	}

	if (switchState(state) == false) {
		return;
	}

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

	if (endReason == USER_JUMP) {
		ResetJump(owner->JumpCurrentCount - mRegainJumpsAfterWalljump);
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
	if (checkDirectionForWall(hit, move->GetActorLocation(), - mWallNormal, true) == false) {
		if (mMaxWallrunOuterAngle <= 70.0f) {
			endWallrun(FALL_OFF);
			return;
		}
		else {
			// when sharp outer angles have to be detected
			// check another trace backwards from a position that should lie in front of a sharp outer wall turn
			FVector capsuleRight = move->GetActorLocation() - mWallNormal * WALLRUN_REPLACEMENT * 1.2f;
			if (checkDirectionForWall(hit, capsuleRight, -mWallrunDir, true) == false) {
				endWallrun(FALL_OFF);
				return;
			}
		}
	}

	if (isValidInnerOuterAngleDiff(move->GetActorLocation(), hit) == false) {
		// end wallrun with angle out of bounds to prevent a wallrunning loop in an inner corner.
		endWallrun(EWallrunEndReason::ANGLE_OUT_OF_BOUNDS);
		return;
	}

	mWallNormal = hit.ImpactNormal;
	mWallImpact = hit.ImpactPoint;
	mWallrunDir = calcWallrunDir(mWallNormal, mState);

	// set velocity according to the wall direction
	DrawDebugLine(GetWorld(), owner->GetActorLocation(), owner->GetActorLocation() + (mWallrunDir * mWallrunSpeed), FColor::Blue, false, -1.0f, 0U, 5.0f);
	move->Velocity.X = mWallrunDir.X * mWallrunSpeed;
	move->Velocity.Y = mWallrunDir.Y * mWallrunSpeed;

	// manually calc velocity Z when clawing into the wall
	if (mClawIntoWall) {
		mClawTime += time;
		GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Green, FString::Printf(TEXT("mClawTime: %f, velo: %f, targetVelo: %f"), mClawTime, move->Velocity.Z, mClawZTargetVelo));
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
	mState = newState;
	return true;
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

FVector USpecialMovementComponent::calcLaunchVelocity() const
{
	FVector launchDir(0, 0, 0);
	if (isWallrunning()) {
		switch (mState) {
		case ESpecialMovementState::WALLRUN_LEFT:
		case ESpecialMovementState::WALLRUN_RIGHT:
		case ESpecialMovementState::WALLRUN_UP:
			launchDir = mWallNormal;
			break;
		}
	}
	else if (move->IsFalling()) {
		launchDir = mRightAxis * owner->GetActorRightVector() + mForwardAxis * owner->GetActorForwardVector();
	}

	launchDir.Normalize();
	launchDir *= move->JumpZVelocity;

	launchDir.Z = move->JumpZVelocity; // maybe additive would be better?

	DrawDebugLine(GetWorld(), owner->GetActorLocation(), owner->GetActorLocation() + launchDir, FColor::Green, false, 40.0f, 0U, 5.0f);
	GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Red, *launchDir.ToString());

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


bool USpecialMovementComponent::isValidInnerOuterAngleDiff(FVector const & origin, const FHitResult& hit)
{
	FVector const hitdirection = FVector::CrossProduct(hit.ImpactNormal, FVector(0, 0, mState == ESpecialMovementState::WALLRUN_LEFT ? 1 : -1));
	double const angle = calcAngleBetweenVectors(mWallrunDir, hitdirection);

	// inner / outer angle determination taken from: https://stackoverflow.com/questions/12397564/check-if-an-angle-defined-by-3-points-is-inner-or-outer
	// Describe the two angle vectors coming from the center point as a and b. And describe the vector from your center point to the origin as center.
	FVector const center = origin - hit.ImpactPoint;
	FVector const a = -mWallrunDir;
	FVector const b = hitdirection;
	bool const isInnerAngle = (FVector::DotProduct(a + b, center) > 0.0 && FVector::DotProduct(FVector::CrossProduct(a, center), FVector::CrossProduct(b, center)) < 0.0);

	FColor debugCol = FColor::Green;
	float maxAngle = mMaxWallrunOuterAngle;
	if (isInnerAngle) {
		debugCol = FColor::Yellow;
		maxAngle = mMaxWallrunInnerAngle;
	}

	if (angle > 0.005f) {
		DrawDebugLine(GetWorld(), hit.ImpactPoint + a * 100, hit.ImpactPoint, debugCol, false, 100.0f, 0U, 5.0f);
		DrawDebugLine(GetWorld(), hit.ImpactPoint + b * 100, hit.ImpactPoint, debugCol, false, 100.0f, 0U, 5.0f);
		DrawDebugCrosshairs(GetWorld(), origin, FRotator::ZeroRotator, 30.0f, FColor::Blue, false, 100.0f, 0U);
		GEngine->AddOnScreenDebugMessage(-1, 15.0f, debugCol, FString::Printf(TEXT("angle: %f"), angle));
	}

	if (angle > maxAngle) {
		return false;
	}
	return true;
}
