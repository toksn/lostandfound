// Copyright Epic Games, Inc. All Rights Reserved.

#include "lostandfoundCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"

//////////////////////////////////////////////////////////////////////////
// AlostandfoundCharacter

AlostandfoundCharacter::AlostandfoundCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// set our turn rate for input
	TurnRateGamepad = 50.f;

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f); // ...at this rotation rate

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 1000.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 5000.f;
	GetCharacterMovement()->MaxAcceleration = 5000.f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
}

//////////////////////////////////////////////////////////////////////////
// Input

void AlostandfoundCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// Set up gameplay key bindings
	check(PlayerInputComponent);
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &AlostandfoundCharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &AlostandfoundCharacter::StopJumping);

	PlayerInputComponent->BindAxis("Move Forward / Backward", this, &AlostandfoundCharacter::MoveForward);
	PlayerInputComponent->BindAxis("Move Right / Left", this, &AlostandfoundCharacter::MoveRight);

	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	PlayerInputComponent->BindAxis("Turn Right / Left Mouse", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("Turn Right / Left Gamepad", this, &AlostandfoundCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("Look Up / Down Mouse", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("Look Up / Down Gamepad", this, &AlostandfoundCharacter::LookUpAtRate);

	// handle touch devices
	PlayerInputComponent->BindTouch(IE_Pressed, this, &AlostandfoundCharacter::TouchStarted);
	PlayerInputComponent->BindTouch(IE_Released, this, &AlostandfoundCharacter::TouchStopped);
}

void AlostandfoundCharacter::Jump()
{
	FVector launchVelo;
	if (mIsWallrunning) {
		// jump off wall
		launchVelo = calcLaunchVelocity();
		endWallrun(EWallrunEndReason::USER_JUMP);
	}
	else if (JumpCurrentCount < JumpMaxCount) {
		//if (GetCharacterMovement()->IsFalling()) {
			//PlayAnimMontage()
		//}	
		JumpCurrentCount++;
		launchVelo = calcLaunchVelocity();
	}

	if (launchVelo.Length() > 0) {
		LaunchCharacter(launchVelo, false, true);
	}


	// are we close to an edge?
		// velocity (direction) is at least 20° off the edge direction
			// setMaxWalkSpeed * 2.0f;
			// setVelocity *= 2.0f;
			// startResetMaxWalkSpeedTimer(3 seconds)
				// Interp the maxWalkSpeed back

}

void AlostandfoundCharacter::StopJumping()
{
	Super::StopJumping();
}

void AlostandfoundCharacter::ResetJump(int new_jump_count)
{
	JumpCurrentCount = FMath::Clamp(new_jump_count, 0, JumpMaxCount);
}

void AlostandfoundCharacter::setHorizontalVelocity(FVector2D vel)
{
	GetCharacterMovement()->Velocity.X = vel.X;
	GetCharacterMovement()->Velocity.Y = vel.Y;
}

FVector2D AlostandfoundCharacter::getHorizontalVelocity()
{
	return FVector2D(GetCharacterMovement()->Velocity.X, GetCharacterMovement()->Velocity.Y);
}

void AlostandfoundCharacter::clampHorizontalVelocity()
{
	if (GetCharacterMovement()->IsFalling()) {
		FVector2D vel = getHorizontalVelocity();
		if (vel.Length() > GetCharacterMovement()->MaxWalkSpeed) {
			float speedFactor = vel.Length() / GetCharacterMovement()->MaxWalkSpeed;
			setHorizontalVelocity(vel / speedFactor);
		}
	}
}

bool AlostandfoundCharacter::checkSideForWall(FHitResult& hit, EWallrunSide side, FVector forwardDirection, bool debug)
{
	/* check for the current wall next to the character with a single trace line */
	float const traceLength = GetCapsuleComponent()->GetCollisionShape().Capsule.Radius * 1.5f; // TODO: make member variable and expose to BP
	FVector traceForWall = FVector::CrossProduct(forwardDirection, FVector(0, 0, side == WR_RIGHT ? 1 : -1));
	traceForWall.Normalize();
	traceForWall *= traceLength;

	if (debug) {
		DrawDebugLine(GetWorld(), GetActorLocation(), GetActorLocation() + traceForWall, FColor::Red, false, 40.0f, 0U, 10.0f);
	}

	return GetWorld()->LineTraceSingleByChannel(hit, GetActorLocation(), GetActorLocation() + traceForWall, ECollisionChannel::ECC_Visibility, IGNORE_SELF_COLLISION_PARAM);
}

void AlostandfoundCharacter::startWallClaw(float speed, float targetZVelocity)
{
	clawIntoWall = true;
	clawTime = 0.0f;
	clawZTargetVelo = targetZVelocity;
	clawSpeed = speed;
	GetCharacterMovement()->GravityScale = 0.0f;
}

void AlostandfoundCharacter::endWallClaw()
{
	GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Green, "stopped claw into wall");
	clawIntoWall = false;
	GetCharacterMovement()->GravityScale = 0.25f;
}

void AlostandfoundCharacter::startWallrun(FVector wallNormal)
{
	mWallrunSide = findWallrunSide(wallNormal);
	mWallrunDir = calcWallrunDir(wallNormal, mWallrunSide);
	
	FHitResult hit;
	if (checkSideForWall(hit, mWallrunSide, GetActorForwardVector(), true) == false) {
		/* too straight to begin wallrun */
		return;
	}

	auto cm = GetCharacterMovement();
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

void AlostandfoundCharacter::endWallrun(EWallrunEndReason endReason)
{
	// call endWallClaw before gravity reset because it does manipulate gravity as well
	endWallClaw();

	auto cm = GetCharacterMovement();
	cm->GravityScale = mDefaultGravityScale;
	cm->AirControl = mDefaultAirControl;

	// stop updateWallRun() timer
	mIsWallrunning = false;

	if (endReason == USER_JUMP) {
		ResetJump(0);
	}
}

void AlostandfoundCharacter::updateWallrun(float time)
{
	if (isWallrunInputPressed() == false) {
		endWallrun(USER_STOP);
		return;
	}

	if (GetCharacterMovement()->IsFalling() == false) {
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
	auto cm = GetCharacterMovement();
	float const speed = cm->MaxWalkSpeed;

	DrawDebugLine(GetWorld(), GetActorLocation(), GetActorLocation() + (mWallrunDir * speed), FColor::Blue, false, -1.0f, 0U, 10.0f);
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

void AlostandfoundCharacter::OnPlayerHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit)
{
	if (mIsWallrunning || isWallrunInputPressed() == false || GetCharacterMovement()->IsFalling() == false) {
		return;
	}

	DrawDebugLine(GetWorld(), Hit.ImpactPoint, Hit.ImpactPoint + (Hit.ImpactNormal * 100.0f), FColor::Yellow, false, 40.0f, 0U, 10.0f);

	if (surfaceIsWallrunPossible(Hit.ImpactNormal)) {

		startWallrun(Hit.ImpactNormal);
	}

}

void AlostandfoundCharacter::TouchStarted(ETouchIndex::Type FingerIndex, FVector Location)
{
	Jump();
}

void AlostandfoundCharacter::TouchStopped(ETouchIndex::Type FingerIndex, FVector Location)
{
	StopJumping();
}

void AlostandfoundCharacter::OnLanded(const FHitResult& Hit)
{
	ResetJump(0);
}

void AlostandfoundCharacter::Tick(float time)
{
	Super::Tick(time);

	if (mIsWallrunning) {
		updateWallrun(time);
	}

	clampHorizontalVelocity();
}

void AlostandfoundCharacter::BeginPlay()
{
	Super::BeginPlay();
	// get default values to reset after wallrun
	mDefaultGravityScale = GetCharacterMovement()->GravityScale;
	mDefaultMaxWalkSpeed = GetCharacterMovement()->MaxWalkSpeed;
	mDefaultAirControl = GetCharacterMovement()->AirControl;

	GetCapsuleComponent()->OnComponentHit.AddDynamic(this, &AlostandfoundCharacter::OnPlayerHit);
}

void AlostandfoundCharacter::TurnAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * TurnRateGamepad * GetWorld()->GetDeltaSeconds());
}

void AlostandfoundCharacter::LookUpAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * TurnRateGamepad * GetWorld()->GetDeltaSeconds());
}

void AlostandfoundCharacter::MoveForward(float Value)
{
	mForwardAxis = Value;
	if ((Controller != nullptr) && (Value != 0.0f))
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		AddMovementInput(Direction, Value);
	}
}

void AlostandfoundCharacter::MoveRight(float Value)
{
	mRightAxis = Value;
	if ( (Controller != nullptr) && (Value != 0.0f) )
	{
		// find out which way is right
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);
	
		// get right vector 
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		// add movement in that direction
		AddMovementInput(Direction, Value);
	}
}

FVector AlostandfoundCharacter::calcWallrunDir(FVector wallNormal, EWallrunSide side)
{
	FVector wallrunDir = FVector::CrossProduct(wallNormal, FVector(0, 0, side == WR_RIGHT ? 1 : -1));
	wallrunDir.Normalize();
	return wallrunDir;
}

AlostandfoundCharacter::EWallrunSide AlostandfoundCharacter::findWallrunSide(FVector wallNormal)
{
	if (FVector2D::DotProduct(FVector2D(GetActorRightVector()), FVector2D(wallNormal)) > 0.0f) {
		return WR_RIGHT;
	}
	else {
		return WR_LEFT;
	}
}

FVector AlostandfoundCharacter::calcLaunchVelocity() const
{
	FVector launchDir(0,0,0);
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
	else if (GetCharacterMovement()->IsFalling()) {
		launchDir = mRightAxis * GetActorRightVector() + mForwardAxis * GetActorForwardVector();
	}

	launchDir.Normalize();
	launchDir *= GetCharacterMovement()->JumpZVelocity;

	launchDir.Z = GetCharacterMovement()->JumpZVelocity; // maybe additive would be better?

	DrawDebugLine(GetWorld(), GetActorLocation(), GetActorLocation() + launchDir, FColor::Green, false, 40.0f, 0U, 10.0f);
	GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Red, *launchDir.ToString());

	return launchDir;
}

bool AlostandfoundCharacter::surfaceIsWallrunPossible(FVector surfaceNormal) const
{
	// z < -0.05f is questionable?! i could try to wallrun on slopes that are kinda tilted inwards (looking down)
	if (surfaceNormal.Z < -0.05f || surfaceNormal.Z > GetCharacterMovement()->GetWalkableFloorZ()) {
		return false;
	}
	return true;
}

bool AlostandfoundCharacter::isWallrunInputPressed() const
{
	return true;
	// todo use actual special input key (right mouse button?)
	if (mForwardAxis > 0.0f) {
		switch (mWallrunSide) {
		case WR_LEFT:
			if (mRightAxis < 0.0f) {
				return true;
			}
			break;
		case WR_RIGHT:
			if (mRightAxis > 0.0f) {
				return true;
			}
			break;
		}
	}

	return false;
}
