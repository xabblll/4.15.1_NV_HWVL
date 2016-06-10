// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

// Custom serialization version for changes made in Dev-Editor stream
struct CORE_API FEditorObjectVersion
{
	enum Type
	{
		// Before any version changes were made
		BeforeCustomVersionWasAdded = 0,
		// Localizable text gathered and stored in packages is now flagged with a localizable text gathering process version
		GatheredTextProcessVersionFlagging,
		// Fixed several issues with the gathered text cache stored in package headers
		GatheredTextPackageCacheFixes,
		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	// The GUID for this custom version number
	const static FGuid GUID;

private:
	FEditorObjectVersion() {}
};
