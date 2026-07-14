;BEGIN FRAGMENT CODE - Do not edit anything between this and the end comment
;NEXT FRAGMENT INDEX 1
Scriptname IvyInnFillWaterskin Extends TopicInfo Hidden

;BEGIN FRAGMENT Fragment_0
Function Fragment_0(ObjectReference akSpeakerRef)
Actor akSpeaker = akSpeakerRef as Actor
;BEGIN CODE

    Actor player = Game.GetPlayer()

    FillBottles(player, IvyEmptyMeadBottle, IvyBottledWaterMead)
    FillBottles(player, IvyEmptyWineBottle, IvyBottledWaterWine)
    FillBottles(player, IvyEmptySujammaBottle, IvyBottledWaterSujamma)

    FillWaterskins(player)

;END CODE
EndFunction
;END FRAGMENT

;END FRAGMENT CODE - Do not edit anything between this and the begin comment

MiscObject Property Gold001 Auto
GlobalVariable Property IvyCostGlobal Auto
Potion Property IvyWaterBottle01 Auto
Potion Property IvyWaterBottle Auto
Potion Property IvyWaterBottle02 Auto
Potion Property IvyWaterBottle03 Auto
Potion Property IvyEmptyMeadBottle Auto
Potion Property IvyEmptyWineBottle Auto
Potion Property IvyEmptySujammaBottle Auto
Potion Property IvyBottledWaterMead Auto
Potion Property IvyBottledWaterWine Auto
Potion Property IvyBottledWaterSujamma Auto

Function FillBottles(Actor player, Potion emptyBottle, Potion filledBottle)
    if !emptyBottle || !filledBottle
        return
    endif

    int count = player.GetItemCount(emptyBottle)
    if count > 0
        player.RemoveItem(emptyBottle, count)
        player.AddItem(filledBottle, count)
    endif
EndFunction

Function FillWaterskins(Actor player)
    if !IvyWaterBottle01 || !IvyWaterBottle || !IvyWaterBottle02 || !IvyWaterBottle03
        return
    endif

    int countBase = player.GetItemCount(IvyWaterBottle)
    int count02 = player.GetItemCount(IvyWaterBottle02)
    int count03 = player.GetItemCount(IvyWaterBottle03)

    int total = countBase + count02 + count03

    if total <= 0
        return
    endif

    if countBase > 0
        player.RemoveItem(IvyWaterBottle, countBase)
    endif

    if count02 > 0
        player.RemoveItem(IvyWaterBottle02, count02)
    endif

    if count03 > 0
        player.RemoveItem(IvyWaterBottle03, count03)
    endif

    player.AddItem(IvyWaterBottle01, total)

    int iAmount = IvyCostGlobal.GetValue() as int
    player.RemoveItem(Gold001, iAmount)
EndFunction
