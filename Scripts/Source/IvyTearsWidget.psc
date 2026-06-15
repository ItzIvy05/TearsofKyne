Scriptname IvyTearsWidget extends SKI_WidgetBase

GlobalVariable Property TearsWidgetIndex Auto
GlobalVariable Property TearsWidgetReady Auto

String Function GetWidgetSource()
	Return "TearsStatus.swf"
EndFunction

String Function GetWidgetType()
	Return "IvyTearsWidget"
EndFunction

Bool Function IsExtending()
	; The base class string-comparison check misfires for this script;
	; we ARE extending SKI_WidgetBase, so report it directly.
	Return true
EndFunction

Event OnInit()
	Parent.OnInit()
	RegisterForModEvent("SKIWF_widgetManagerReady", "OnWidgetManagerReady")
EndEvent

Event OnGameReload()
	Parent.OnGameReload()
	RegisterForModEvent("SKIWF_widgetManagerReady", "OnWidgetManagerReady")
EndEvent

Event OnWidgetReset()
	Parent.OnWidgetReset()
	PublishIndex()
EndEvent

Event OnWidgetLoad()
	Parent.OnWidgetLoad()
	PublishIndex()
EndEvent

Function PublishIndex()
	Debug.Trace("[IvyTears] PublishIndex: WidgetID=" + WidgetID + " WidgetRoot=" + WidgetRoot)
	If (TearsWidgetIndex != None)
		TearsWidgetIndex.SetValueInt(WidgetID)
	EndIf
	If (TearsWidgetReady != None)
		TearsWidgetReady.SetValueInt(1)
	EndIf
EndFunction