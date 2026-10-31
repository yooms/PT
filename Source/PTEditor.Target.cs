// Copyright NeedsGames. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class PTEditorTarget : TargetRules
{
	public PTEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("PT");
		ExtraModuleNames.Add("PTEditor");
	}
}
