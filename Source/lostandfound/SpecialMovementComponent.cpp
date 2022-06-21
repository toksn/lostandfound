// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "SpecialMovementComponent.h"

#define IGNORE_SELF_COLLISION_PARAM FCollisionQueryParams(FName(TEXT("KnockTraceSingle")), true, owner)

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

	owner = Cast<ACharacter>(GetOwner());
	move = owner->GetCharacterMovement();

	// get default values to reset after wallrun
	mDefaultGravityScale = move->GravityScale;
	mDefaultMaxWalkSpeed = move->MaxWalkSpeed;
	mDefaultAirControl = move->AirControl;
}


// Called every frame
void USpecialMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (mIsWallrunning) {
		updateWallrun(DeltaTime);
	}

	clampHorizontalVelocity();
}

void USpecialMovementComponent::ResetJump(int new_jump_count)
{
	owner->JumpCurrentCount = FMath::Clamp(new_jump_count, 0, owner->JumpMaxCount);
}

void USpecialMovementComponent::Jump()
{
	FVector launchVelo;
	if (mIsWallrunning) {
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

bool USpecialMovementComponent::checkSideForWall(FHitResult& hit, EWallrunSide side, FVector forwardDirection, bool debug)
{
	/* check for the current wall next to the character with a single trace line */
	float const traceLength = owner->GetCapsuleComponent()->GetCollisionShape().Capsule.Radius * 1.5f; // TODO: make member variable and expose to BP
	FVector traceForWall = FVector::CrossProduct(forwardDirection, FVector(0, 0, side == WR_RIGHT ? 1 : -1));
	traceForWall.Normalize();
	traceForWall *= traceLength;

	if (debug) {
		DrawDebugLine(GetWorld(), owner->GetActorLocation(), owner->GetActorLocation() + traceForWall, FColor::Red, false, 40.0f, 0U, 10.0f);
	}

	return GetWorld()->LineTraceSingleByChannel(hit, owner->GetActorLocation(), owner->GetActorLocation() + traceForWall, ECollisionChannel::ECC_Visibility, IGNORE_SELF_COLLISION_PARAM);
}

void USpecialMovementComponent::startWallClaw(float speed, float targetZVelocity)
{
	clawIntoWall = true;
	clawTime = 0.0f;
	clawZTargetVelo = targetZVelocity;
	clawSpeed = speed;
	move->GravityScale = 0.0f;
}

void USpecialMovementComponent::endWallClaw()
{
	GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Green, "stopped claw into wall");
	clawIntoWall = false;
	move->GravityScale = 0.25f;
}

void USpecialMovementComponent::tryWallrun(const FHitResult& wallHit)
{
	if (mIsWallrunning || isWallrunInputPressed() == false || move->IsFalling() == false) {
		return;
	}

	DrawDebugLine(GetWorld(), wallHit.ImpactPoint, wallHit.ImpactPoint + (wallHit.ImpactNormal * 100.0f), FColor::Yellow, false, 40.0f, 0U, 10.0f);

	if (surfaceIsWallrunPossible(wallHit.ImpactNormal)) {
		startWallrun(wallHit.ImpactNormal);
	}
}

void USpecialMovementComponent::startWallrun(FVector wallNormal)
{
	mWallrunSide = findWallrunSide(wallNormal);
	mWallrunDir = calcWallrunDir(wallNormal, mWallrunSide);

	FHitResult hit;
	if (checkSideForWall(hit, mWallrunSide, owner->GetActorForwardVector(), true) == false) {
		/* too straight to begin wallrun */
		return;
	}

	auto cm = move;
	cm->GravityScale = 0.25f;
	// claw onto the wall by slowing down the sliding when velocity is below gravity level
	if (cm->Velocity.Z < cm->GetGravityZ()) {
		startWallClaw(2.0f, cm->GetGravityZ());
	}
	else {
		clawIntoWall = false;
		cm->Velocity.Z *= 0.35f;
		// consider clawing into the wall with another speed (slower than downwards claw, like ~1.3f)
		// startWallClaw(1.3f, cm->GetGravityZ());
	}

	cm->AirControl = 0.0f;
	mIsWallrunning = true;
}

void USpecialMovementComponent::endWallrun(EWallrunEndReason endReason)
{
	// call endWallClaw before gravity reset because it does manipulate gravity as well
	endWallClaw();

	auto cm = move;
	cm->GravityScale = mDefaultGravityScale;
	cm->AirControl = mDefaultAirControl;

	// stop updateWallRun() timer
	mIsWallrunning = false;

	if (endReason == USER_JUMP) {
		ResetJump(0);
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
	if (checkSideForWall(hit, mWallrunSide, mWallrunDir, true) == false) {
		endWallrun(FALL_OFF);
		return;
	}

	auto side = findWallrunSide(hit.ImpactNormal);
	if (side != mWallrunSide) {
		endWallrun(FALL_OFF);
		return;
	}
	mWallrunDir = calcWallrunDir(hit.ImpactNormal, side);

	/* set velocity according to the wall direction */
	auto cm = move;
	float const speed = cm->MaxWalkSpeed;

	DrawDebugLine(GetWorld(), owner->GetActorLocation(), owner->GetActorLocation() + (mWallrunDir * speed), FColor::Blue, false, -1.0f, 0U, 10.0f);
	cm->Velocity.X = mWallrunDir.X * speed;
	cm->Velocity.Y = mWallrunDir.Y * speed;

	// manually calc velocity Z when clawing into the wall
	if (clawIntoWall) {
		clawTime += time;
		GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Green, FString::Printf(TEXT("clawTime: %f, velo: %f, targetVelo: %f"), clawTime, cm->Velocity.Z, clawZTargetVelo));
		cm->Velocity.Z = FMath::FInterpTo(cm->Velocity.Z, clawZTargetVelo, clawTime, clawSpeed);
		if (cm->Velocity.Z == clawZTargetVelo) {
			endWallClaw();
		}
	}
}

FVector USpecialMovementComponent::calcWallrunDir(FVector wallNormal, EWallrunSide side)
{
	FVector wallrunDir = FVector::CrossProduct(wallNormal, FVector(0, 0, side == WR_RIGHT ? 1 : -1));
	wallrunDir.Normalize();
	return wallrunDir;
}

USpecialMovementComponent::EWallrunSide USpecialMovementComponent::findWallrunSide(FVector wallNormal)
{
	if (FVector2D::DotProduct(FVector2D(owner->GetActorRightVector()), FVector2D(wallNormal)) > 0.0f) {
		return WR_RIGHT;
	}
	else {
		return WR_LEFT;
	}
}

FVector USpecialMovementComponent::calcLaunchVelocity() const
{
	FVector launchDir(0, 0, 0);
	if (mIsWallrunning) {
		switch (mWallrunSide) {
		case WR_LEFT:
			launchDir = FVector::CrossProduct(FVector(0, 0, -1), mWallrunDir);
			break;
		case WR_RIGHT:
			launchDir = FVector::CrossProduct(FVector(0, 0, 1), mWallrunDir);
			break;
		}
	}
	else if (move->IsFalling()) {
		launchDir = mRightAxis * owner->GetActorRightVector() + mForwardAxis * owner->GetActorForwardVector();
	}

	launchDir.Normalize();
	launchDir *= move->JumpZVelocity;

	launchDir.Z = move->JumpZVelocity; // maybe additive would be better?

	DrawDebugLine(GetWorld(), owner->GetActorLocation(), owner->GetActorLocation() + launchDir, FColor::Green, false, 40.0f, 0U, 10.0f);
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
