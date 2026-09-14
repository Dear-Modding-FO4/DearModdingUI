Scriptname DMUITestFunctions Hidden

Function IncrementCounter() Global
    DMUITestQuest fixtureQuest = Game.GetFormFromFile(0x00000800, "DMUITests.esp") as DMUITestQuest
    If fixtureQuest
        fixtureQuest.RecordGlobalAction()
    Else
        Debug.Trace("DMUITests: could not resolve DMUITestQuest from DMUITests.esp|800", 2)
    EndIf
EndFunction
