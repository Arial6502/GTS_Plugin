Scriptname GTSProxy extends Quest

; Contains Functions Called By The DLL

Actor Property PlayerRef Auto
Faction Property VampirePCFaction Auto
GlobalVariable Property PlayerIsVampire Auto
PlayerVampireQuestScript Property VampireScript Auto
Perk Property GTSPerkTinyCalamity Auto
Shout Property GTSShoutTinyCalamity Auto
WordOfPower Property GTSWordCatastrophe1 Auto

Function Proxy_SatisfyVampire()

    If PlayerRef.IsInFaction(VampirePCFaction) || PlayerIsVampire.GetValue() == 1.0
        VampireScript.VampireFeed()
    endif
	
endfunction



Function Proxy_AddCalamityShout()
    PlayerRef.AddShout(GTSShoutTinyCalamity)
    Game.TeachWord(GTSWordCatastrophe1)
    Game.UnlockWord(GTSWordCatastrophe1)
endfunction


Function Proxy_RemoveCalamityShout()
    PlayerRef.RemoveShout(GTSShoutTinyCalamity)
endfunction

