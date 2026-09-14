Scriptname DMUITestQuest extends Quest

Bool Property Enabled = True Auto
Int Property IntegerValue = 3 Auto
Float Property FloatValue = 0.5 Auto
String Property TextValue = "Initial value" Auto
Int Property ChoiceValue = 0 Auto
Int Property ActionCount = 0 Auto
String Property LastAction = "None" Auto

Event OnInit()
    ResetFixture()
EndEvent

Function IncrementCounter()
    ActionCount += 1
    LastAction = "Member action"
EndFunction

Function RecordGlobalAction()
    ActionCount += 1
    LastAction = "Global action"
EndFunction

Function SetIntegerValue(Int a_value)
    IntegerValue = a_value
    LastAction = "Integer set to " + a_value
EndFunction

Function ResetFixture()
    Enabled = True
    IntegerValue = 3
    FloatValue = 0.5
    TextValue = "Initial value"
    ChoiceValue = 0
    ActionCount = 0
    LastAction = "None"
EndFunction
