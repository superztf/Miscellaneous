
#include "EastingDistanceCurve.h"
#include "Animation/AnimSequence.h"
// #include "AnimationBlueprintLibrary.h"
#include "EngineLogs.h"
#include "MotionExtractorTypes.h"
#include "MotionExtractorUtilities.h"
#include "Editor/AnimationBlueprintLibrary/Public/AnimationBlueprintLibrary.h"

// TODO: This logic works decently for simple clips but it should be reworked to be more robust:
//  * It could detect pivot points by change in direction.
//  * It should also account for clips that have multiple stop/pivot points.
//  * It should handle distance traveled for the ends of looping animations.
void UEastingDistanceCurve::OnApply_Implementation(UAnimSequence* Animation)
{
	if (Animation == nullptr)
	{
		UE_LOG(LogAnimation, Error, TEXT("Easting DistanceCurveModifier failed. Reason: Invalid Animation"));
		return;
	}

	if (!Animation->HasRootMotion())
	{
		UE_LOG(LogAnimation, Error, TEXT("Easting DistanceCurveModifier failed. Reason: Root motion is disabled on the animation (%s)"), *GetNameSafe(Animation));
		return;
	}
	USkeleton* Skeleton = Animation->GetSkeleton();
	if (Skeleton == nullptr)
	{
		UE_LOG(LogAnimation, Error, TEXT("Easting DistanceCurveModifier failed. Reason: Animation with invalid Skeleton. Animation: %s"),
			*GetNameSafe(Animation));
		return;
	}
	FMemMark Mark(FMemStack::Get());

	TGuardValue<bool> ForceRootLockGuard(Animation->bForceRootLock, false);
	
	const int32 BoneIndex = Skeleton->GetReferenceSkeleton().FindBoneIndex(BoneName);

	TArray<FBoneIndexType> RequiredBones;
	RequiredBones.Add(IntCastChecked<FBoneIndexType>(BoneIndex));

	Skeleton->GetReferenceSkeleton().EnsureParentsExistAndSort(RequiredBones);
	FBoneContainer BoneContainer(RequiredBones, UE::Anim::ECurveFilterMode::DisallowAll, *Skeleton);
	const FCompactPoseBoneIndex CompactPoseBoneIndex = BoneContainer.MakeCompactPoseIndex(FMeshPoseBoneIndex(BoneIndex));

	// const bool bMetaDataCurve = false;
	//ZTF UAnimationBlueprintLibrary::AddCurve(Animation, CurveName, ERawCurveTrackTypes::RCT_Float, bMetaDataCurve);

	const float AnimLength = Animation->GetPlayLength();
	float SampleInterval;
	int32 NumSteps;
	float TimeOfMinSpeed;

	if(bStopAtEnd)
	{ 
		TimeOfMinSpeed = AnimLength;
	}
	else
	{
		// Perform a high resolution search to find the sample point with minimum speed.
		
		TimeOfMinSpeed = 0.f;
		float MinSpeedSq = FMath::Square(StopSpeedThreshold);

		FTransform TempLastBoneTransform = UMotionExtractorUtilityLibrary::ExtractBoneTransform(Animation, BoneContainer, CompactPoseBoneIndex, 0, true);

		
		SampleInterval = 1.f / 120.f;
		NumSteps = AnimLength / SampleInterval;
		for (int32 Step = 1; Step < NumSteps; ++Step)
		{
			const float Time = Step * SampleInterval;
			FTransform TempCurrentBoneTransform = UMotionExtractorUtilityLibrary::ExtractBoneTransform(Animation, BoneContainer, CompactPoseBoneIndex, Time, true);
			const FVector DeltaLocation = TempCurrentBoneTransform.GetTranslation() - TempLastBoneTransform.GetTranslation();
			const float DeltaDistanceSeq = CalculateMagnitudeSq(DeltaLocation, Axis);
			const float RootMotionSpeedSq = DeltaDistanceSeq / SampleInterval;

			if (RootMotionSpeedSq < MinSpeedSq)
			{
				MinSpeedSq = RootMotionSpeedSq;
				TimeOfMinSpeed = Time;
			}
			TempLastBoneTransform = TempCurrentBoneTransform;
		}
	}


	const FTransform MinSpeedBoneTransform = UMotionExtractorUtilityLibrary::ExtractBoneTransform(Animation, BoneContainer, CompactPoseBoneIndex, TimeOfMinSpeed, true);
	TArray<FRichCurveKey> CurveKeys;
	SampleInterval = 1.f / SampleRate;
	NumSteps = FMath::CeilToInt(AnimLength / SampleInterval);
	float Time = 0.0f;
	for (int32 Step = 0; Step <= NumSteps && Time < AnimLength; ++Step)
	{
		Time = FMath::Min(Step * SampleInterval, AnimLength);

		FTransform BoneTransform = UMotionExtractorUtilityLibrary::ExtractBoneTransform(Animation, BoneContainer, CompactPoseBoneIndex, Time, true);
		
		// Assume that during any time before the stop/pivot point, the animation is approaching that point.
		// TODO: This works for clips that are broken into starts/stops/pivots, but needs to be rethought for more complex clips.
		const float ValueSign = (Time < TimeOfMinSpeed) ? -1.0f : 1.0f;


		const FVector RootMotionTranslation = BoneTransform.GetTranslation() - MinSpeedBoneTransform.GetTranslation();
		const float FinalValue = ValueSign * CalculateMagnitude(RootMotionTranslation, Axis);

		FRichCurveKey& Key = CurveKeys.AddDefaulted_GetRef();
		Key.Time = Time;
		Key.Value = FinalValue;
		
	}


	IAnimationDataController& Controller = Animation->GetController();
	Controller.OpenBracket(FText::FromString(TEXT("Easting Distance Curve Modifier")));
	const FAnimationCurveIdentifier CurveId = UAnimationCurveIdentifierExtensions::GetCurveIdentifier(Animation->GetSkeleton(), CurveName, ERawCurveTrackTypes::RCT_Float);
	if(CurveKeys.Num() && Controller.AddCurve(CurveId))
	{
		Controller.SetCurveKeys(CurveId, CurveKeys);
	}
	Controller.CloseBracket();
}

void UEastingDistanceCurve::OnRevert_Implementation(UAnimSequence* Animation)
{
	constexpr bool bRemoveNameFromSkeleton = false;
	UAnimationBlueprintLibrary::RemoveCurve(Animation, CurveName, bRemoveNameFromSkeleton);
}

float UEastingDistanceCurve::CalculateMagnitude(const FVector& Vector, EMotionExtractor_Axis Axis)
{
	switch (Axis)
	{
	case EMotionExtractor_Axis::X:		return FMath::Abs(Vector.X);
	case EMotionExtractor_Axis::Y:		return FMath::Abs(Vector.Y);
	case EMotionExtractor_Axis::Z:		return FMath::Abs(Vector.Z);
	default: return FMath::Sqrt(CalculateMagnitudeSq(Vector, Axis));
	}
}

float UEastingDistanceCurve::CalculateMagnitudeSq(const FVector& Vector, EMotionExtractor_Axis Axis)
{
	switch (Axis)
	{
	case EMotionExtractor_Axis::X:		return FMath::Square(FMath::Abs(Vector.X));
	case EMotionExtractor_Axis::Y:		return FMath::Square(FMath::Abs(Vector.Y));
	case EMotionExtractor_Axis::Z:		return FMath::Square(FMath::Abs(Vector.Z));
	case EMotionExtractor_Axis::XY:		return Vector.X * Vector.X + Vector.Y * Vector.Y;
	case EMotionExtractor_Axis::XZ:		return Vector.X * Vector.X + Vector.Z * Vector.Z;
	case EMotionExtractor_Axis::YZ:		return Vector.Y * Vector.Y + Vector.Z * Vector.Z;
	case EMotionExtractor_Axis::XYZ:		return Vector.X * Vector.X + Vector.Y * Vector.Y + Vector.Z * Vector.Z;
	default: check(false); break;
	}

	return 0.f;
}
