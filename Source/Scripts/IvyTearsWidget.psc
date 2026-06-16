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
	Return true
EndFunction

Event OnWidgetReset()
	Parent.OnWidgetReset()
	PublishIndex()
EndEvent

Function PublishIndex()
	If (TearsWidgetIndex != None)
		TearsWidgetIndex.SetValueInt(WidgetID)
	EndIf
	If (TearsWidgetReady != None)
		TearsWidgetReady.SetValueInt(1)
	EndIf
EndFunction