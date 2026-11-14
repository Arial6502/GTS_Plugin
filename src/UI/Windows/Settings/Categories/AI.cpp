#include "UI/Windows/Settings/Categories/AI.hpp"

#include "UI/Core/ImUtil.hpp"

#include "UI/Controls/CheckBox.hpp"
#include "UI/Controls/CollapsingTabHeader.hpp"
#include "UI/Controls/Misc.hpp"
#include "UI/Controls/Slider.hpp"
#include "UI/Controls/ResetButton.hpp"

#include "Config/Config.hpp"

#include "UI/Controls/Text.hpp"

namespace {

    using namespace GTS;

    PSString WeightHelp =
        "These race each other every interval, so the number is a weight and not a percentage.\n"
        "The bracket shows the share each one currently buys. Raising every weight changes nothing;\n"
        "what matters is their size relative to each other and to Do Nothing.\n\n"
        "A roll also skips anything the game refuses at that moment, so the share is a ceiling.";

    PSString SharedIdleHelp =
        "Weight of doing nothing this interval. One value shared by every list on this tab, so it\n"
        "competes in all of them.";

    PSString StartHelp = "Weight of starting this, against the other ways of starting.";

    //----------------------------------------------------------------------------------------------
    // Tab: Grabs
    //----------------------------------------------------------------------------------------------

    void DrawTab_Grabs() {

        auto& Grab = Config::AI.Grab;

        ImUtil_Unique {
            PSString T0 = "Enable grab actions.";
            PSString T1 = "Set the chance for the actor to grab someone if possible.";
            PSString T2 = "How often the actor picks something to do with whoever is in her hand.";

            ImGuiEx::CheckBox("Enable Action", &Grab.bEnableAction, T0);
            ImGuiEx::ResetSettings("Reset", "Restore every grab setting to its default.", Grab);

            ImGui::BeginDisabled(!Grab.bEnableAction);
            ImGuiEx::SliderF("Start Chance", &Grab.fProbability, 1.0f, 100.0f, T1, "%.0f%%");
            ImGuiEx::SliderF("Action Interval", &Grab.fInterval, 1.0f, 10.0f, T2, "Every %.1f Second(s)");
            ImGui::EndDisabled();
        }

        ImGui::BeginDisabled(!Grab.bEnableAction);

        ImGuiEx::HelpText("How these numbers work", WeightHelp);

        ImUtil_Unique {
            ImGuiEx::SeperatorH();
            ImGui::Text("In Hand");

            const float total = ImGuiEx::WeightTotal({
                Grab.fIdleProb, Grab.fThrowProb, Grab.fVoreProb, Grab.fCrushProb,
                Grab.fReleaseProb, Grab.fUnstoreProb, Grab.fGrabPlayStartProb, Grab.fCleavageProb,
            });

            ImGuiEx::SliderWeight("Do Nothing", &Grab.fIdleProb, total, SharedIdleHelp);
            ImGuiEx::SliderWeight("Throw", &Grab.fThrowProb, total, "Throw the held actor.");
            ImGuiEx::SliderWeight("Vore", &Grab.fVoreProb, total, "Eat the held actor.");
            ImGuiEx::SliderWeight("Crush", &Grab.fCrushProb, total, "Crush the held actor in hand.");
            ImGuiEx::SliderWeight("Release", &Grab.fReleaseProb, total, "Put the held actor down.");
            ImGuiEx::SliderWeight("Unstore", &Grab.fUnstoreProb, total, "Take a stored actor back into hand.");
            ImGuiEx::SliderWeight("Start Grab-Play", &Grab.fGrabPlayStartProb, total, "Move to the grab-play list below.");
            ImGuiEx::SliderWeight("Move To Cleavage", &Grab.fCleavageProb, total, "Move to the cleavage list below.");
        }

        ImUtil_Unique {
            ImGuiEx::SeperatorH();
            ImGui::Text("Grab-Play");

            const float total = ImGuiEx::WeightTotal({
                Grab.fIdleProb, Grab.fGrabPlayHeavyCrushProb, Grab.fGrabPlayVoreProb, Grab.fGrabPlayKissProb,
                Grab.fGrabPlayKissVoreProb, Grab.fGrabPlayPokeProb, Grab.fGrabPlayFlickProb,
                Grab.fGrabPlaySandwichProb, Grab.fGrabPlayGrindStartProb, Grab.fGrabPlayGrindStopProb,
                Grab.fGrabPlayExitProb,
            });

            ImGuiEx::SliderWeight("Do Nothing", &Grab.fIdleProb, total, SharedIdleHelp);
            ImGuiEx::SliderWeight("Heavy Crush", &Grab.fGrabPlayHeavyCrushProb, total, "Crush them during grab-play.");
            ImGuiEx::SliderWeight("Vore", &Grab.fGrabPlayVoreProb, total, "Eat them during grab-play.");
            ImGuiEx::SliderWeight("Kiss", &Grab.fGrabPlayKissProb, total, "Kiss the held actor.");
            ImGuiEx::SliderWeight("Kiss-Vore", &Grab.fGrabPlayKissVoreProb, total, "Eat them while kissing.");
            ImGuiEx::SliderWeight("Poke", &Grab.fGrabPlayPokeProb, total, "Poke the held actor.");
            ImGuiEx::SliderWeight("Flick", &Grab.fGrabPlayFlickProb, total, "Flick the held actor.");
            ImGuiEx::SliderWeight("Sandwich", &Grab.fGrabPlaySandwichProb, total, "Sandwich them between both hands.");
            ImGuiEx::SliderWeight("Start Grinding", &Grab.fGrabPlayGrindStartProb, total, "Begin grinding.");
            ImGuiEx::SliderWeight("Stop Grinding", &Grab.fGrabPlayGrindStopProb, total, "Stop grinding.");
            ImGuiEx::SliderWeight("Exit", &Grab.fGrabPlayExitProb, total, "Leave grab-play and go back to simply holding them.");
        }

        ImUtil_Unique {
            ImGuiEx::SeperatorH();
            ImGui::Text("Cleavage");

            const float total = ImGuiEx::WeightTotal({
                Grab.fIdleProb, Grab.fCleavageAttackProb, Grab.fCleavageHeavyProb, Grab.fCleavageAbsorbProb,
                Grab.fCleavageVoreProb, Grab.fCleavageSuffocateProb, Grab.fCleavageSufoStopProb,
                Grab.fStrangleChance, Grab.fCleavageDotStopProb, Grab.fCleavageStopProb,
            });

            ImGuiEx::SliderWeight("Do Nothing", &Grab.fIdleProb, total, SharedIdleHelp);
            ImGuiEx::SliderWeight("Light Crush", &Grab.fCleavageAttackProb, total, "A light crush attack.");
            ImGuiEx::SliderWeight("Heavy Crush", &Grab.fCleavageHeavyProb, total, "A heavy crush attack.");
            ImGuiEx::SliderWeight("Absorb", &Grab.fCleavageAbsorbProb, total, "Absorb the held actor.");
            ImGuiEx::SliderWeight("Vore", &Grab.fCleavageVoreProb, total, "Eat the held actor.");
            ImGuiEx::SliderWeight("Suffocate", &Grab.fCleavageSuffocateProb, total, "Begin suffocating them.");
            ImGuiEx::SliderWeight("Stop Suffocating", &Grab.fCleavageSufoStopProb, total, "Go back to simply holding them.");
            ImGuiEx::SliderWeight("Strangle", &Grab.fStrangleChance, total, "Begin strangling, which damages them over time.");
            ImGuiEx::SliderWeight("Stop Strangling", &Grab.fCleavageDotStopProb, total, "Stop strangling them.");
            ImGuiEx::SliderWeight("Exit", &Grab.fCleavageStopProb, total, "Take them back out of the cleavage.");
        }

        ImGui::EndDisabled();
    }

    //----------------------------------------------------------------------------------------------
    // Tab: Hugs
    //----------------------------------------------------------------------------------------------

    void DrawTab_Hugs() {

        auto& Hugs = Config::AI.Hugs;

        static const std::string HugsInfo = fmt::format(
            fmt::runtime(
                "Hugs have following conditions to occur:\n"
                "- Size Difference must be greater than x{:.2f}\n"
                "- Size Difference must be less than x2.5\n\n"
                "AI Hugs can happen only if these two conditions are met.\n"
                "Maximal Size Difference can be increased by Hug perks."
            ),
            GTS::Action_Hug
        );

        ImUtil_Unique {
            PSString T0 = "Enable hug actions.";
            PSString T1 = "Set the chance for a hug action to be started.";
            PSString T2 = "How often the actor picks something to do with whoever she is hugging.";

            ImGuiEx::HelpText("Why aren't hugs starting", HugsInfo.c_str());
            ImGuiEx::CheckBox("Enable Action", &Hugs.bEnableAction, T0);
            ImGuiEx::ResetSettings("Reset", "Restore every hug setting to its default.", Hugs);

            ImGui::BeginDisabled(!Hugs.bEnableAction);
            ImGuiEx::SliderF("Start Chance", &Hugs.fProbability, 1.0f, 100.0f, T1, "%.0f%%");
            ImGuiEx::SliderF("Action Interval", &Hugs.fInterval, 1.0f, 10.0f, T2, "Every %.1f Second(s)");
            ImGui::EndDisabled();
        }

        ImGui::BeginDisabled(!Hugs.bEnableAction);

        ImGuiEx::HelpText("How these numbers work", WeightHelp);

        ImUtil_Unique {
            ImGuiEx::SeperatorH();
            ImGui::Text("While Hugging");

            const float total = ImGuiEx::WeightTotal({
                Hugs.fIdleProb, Hugs.fHealProb, Hugs.fShrinkProb, Hugs.fFriendlyShrinkProb,
                Hugs.fKillProb, Hugs.fReleaseProb,
            });

            ImGuiEx::SliderWeight("Do Nothing", &Hugs.fIdleProb, total,
                "Weight of doing nothing this interval.");
            ImGuiEx::SliderWeight("Heal", &Hugs.fHealProb, total,
                "Heal the hugged actor. Only ever chosen for a follower or the player.");
            ImGuiEx::SliderWeight("Shrink", &Hugs.fShrinkProb, total,
                "Shrink a hostile. Friendlies use the row below instead.");
            ImGuiEx::SliderWeight("Shrink Friendly", &Hugs.fFriendlyShrinkProb, total,
                "Shrink a follower or the player.");
            ImGuiEx::SliderWeight("Crush", &Hugs.fKillProb, total,
                "Crush the hugged actor. Gated by the two toggles below.");
            ImGuiEx::SliderWeight("Release", &Hugs.fReleaseProb, total,
                "Let the hugged actor go.");
        }

        ImUtil_Unique {
            ImGuiEx::SeperatorH();
            ImGui::Text("Conditions");

            PSString T0 = "Allow the crush row above to be chosen against followers and the player.";
            PSString T1 = "Allow the crush row above to be chosen against friendly, non hostile NPCs.";
            PSString T2 = "Let a follower or the player go once there is no size left to take.\n"
                          "Everyone else is always let go at that point.";

            ImGuiEx::CheckBox("Allow Crushing (Followers & Player)", &Hugs.bKillFollowersOrPlayer, T0);
            ImGuiEx::CheckBox("Allow Crushing (Friendly NPCs)", &Hugs.bKillFriendlies, T1);
            ImGuiEx::CheckBox("Stop When Too Small (Followers & Player)", &Hugs.bStopIfCantShrink, T2);
        }

        ImGui::EndDisabled();
    }

    //----------------------------------------------------------------------------------------------
    // Tab: Thighs
    //----------------------------------------------------------------------------------------------

    void DrawTab_Thighs() {

        auto& Crush = Config::AI.ThighCrush;
        auto& Sandwich = Config::AI.ThighSandwich;

        ImUtil_Unique {
            PSString T0 = "Enable thigh crush actions.";
            PSString T1 = "Set the chance for a thigh crush action to be started.";
            PSString T2 = "How often the actor picks something to do while thigh crushing.";

            ImGui::Text("Thigh Crush");
            ImGuiEx::CheckBox("Enable Action", &Crush.bEnableAction, T0);
            ImGuiEx::ResetSettings("Reset", "Restore every thigh crush setting to its default.", Crush);

            ImGui::BeginDisabled(!Crush.bEnableAction);
            {
                ImGuiEx::SliderF("Start Chance", &Crush.fProbability, 1.0f, 100.0f, T1, "%.0f%%");
                ImGuiEx::SliderF("Action Interval", &Crush.fInterval, 1.0f, 10.0f, T2, "Every %.1f Second(s)");

                const float total = ImGuiEx::WeightTotal({ Crush.fIdleProb, Crush.fProbabilityHeavy });

                ImGuiEx::SliderWeight("Do Nothing", &Crush.fIdleProb, total,
                    "Weight of doing nothing this interval.");
                ImGuiEx::SliderWeight("Attack", &Crush.fProbabilityHeavy, total,
                    "Attack whoever is between her thighs.");
            }
            ImGui::EndDisabled();
        }

        ImUtil_Unique {
            PSString T0 = "Enable thigh sandwich actions.";
            PSString T1 = "Set the chance for a thigh sandwich action to be started.";
            PSString T2 = "How often the actor picks something to do while sandwiching someone.";

            ImGuiEx::SeperatorH();
            ImGui::Text("Thigh Sandwich");
            ImGuiEx::CheckBox("Enable Action", &Sandwich.bEnableAction, T0);
            ImGuiEx::ResetSettings("Reset", "Restore every thigh sandwich setting to its default.", Sandwich);

            ImGui::BeginDisabled(!Sandwich.bEnableAction);
            ImGuiEx::SliderF("Start Chance", &Sandwich.fProbability, 1.0f, 100.0f, T1, "%.0f%%");
            ImGuiEx::SliderF("Action Interval", &Sandwich.fInterval, 1.0f, 5.0f, T2, "Every %.1f Second(s)");
            ImGui::EndDisabled();
        }

        ImGui::BeginDisabled(!Sandwich.bEnableAction);

        ImGuiEx::HelpText("How these numbers work", WeightHelp);

        ImUtil_Unique {
            ImGui::Text("Holding");

            const float total = ImGuiEx::WeightTotal({
                Sandwich.fIdleProb, Sandwich.fLightAttackProb, Sandwich.fHeavyAttackPob,
                Sandwich.fEnterButtModeProb, Sandwich.fButtVoreProb, Sandwich.fExitProb,
            });

            ImGuiEx::SliderWeight("Do Nothing", &Sandwich.fIdleProb, total, SharedIdleHelp);
            ImGuiEx::SliderWeight("Light Attack", &Sandwich.fLightAttackProb, total, "A light thigh attack.");
            ImGuiEx::SliderWeight("Heavy Attack", &Sandwich.fHeavyAttackPob, total, "A heavy thigh attack.");
            ImGuiEx::SliderWeight("Enter Butt Mode", &Sandwich.fEnterButtModeProb, total, "Move to the butt mode list below.");
            ImGuiEx::SliderWeight("Butt Vore", &Sandwich.fButtVoreProb, total, "Eat the held actor.");
            ImGuiEx::SliderWeight("Release", &Sandwich.fExitProb, total, "Let the held actor go and end the sandwich.");
        }

        ImUtil_Unique {
            ImGuiEx::SeperatorH();
            ImGui::Text("Butt Mode");

            const float total = ImGuiEx::WeightTotal({
                Sandwich.fIdleProb, Sandwich.fButtLAtkProb, Sandwich.fButtHAtkProb, Sandwich.fButtGrowProb,
                Sandwich.fButtGrindStart, Sandwich.fButtGrindStop, Sandwich.fButtExitProb,
            });

            ImGuiEx::SliderWeight("Do Nothing", &Sandwich.fIdleProb, total, SharedIdleHelp);
            ImGuiEx::SliderWeight("Light Attack", &Sandwich.fButtLAtkProb, total, "A light butt attack.");
            ImGuiEx::SliderWeight("Heavy Attack", &Sandwich.fButtHAtkProb, total, "A heavy butt attack.");
            ImGuiEx::SliderWeight("Grow", &Sandwich.fButtGrowProb, total, "Grow while in butt mode.");
            ImGuiEx::SliderWeight("Start Grinding", &Sandwich.fButtGrindStart, total, "Begin grinding.");
            ImGuiEx::SliderWeight("Stop Grinding", &Sandwich.fButtGrindStop, total, "Stop grinding.");
            ImGuiEx::SliderWeight("Leave Butt Mode", &Sandwich.fButtExitProb, total, "Go back to the holding list above.");
        }

        ImGui::EndDisabled();
    }

    //----------------------------------------------------------------------------------------------
    // Tab: Feet
    //----------------------------------------------------------------------------------------------

    void DrawTab_Feet() {

        auto& Stomp = Config::AI.Stomp;
        auto& Kick = Config::AI.KickSwipe;

        PSString AlwaysOne = "One of these always happens when the action starts, so the shares total 100%%.";

        ImUtil_Unique {
            PSString T0 = "Enable foot stomps when standing or hand stomps when sneaking or crawling.";
            PSString T1 = "Set the chance for a stomp action to be started.";
            PSString T2 = "Chance to grind after an under stomp. This one is a real percentage.";
            PSString T3 = "Chance to grind after a normal stomp. This one is a real percentage.";

            ImGui::Text("Stomps");
            ImGuiEx::CheckBox("Enable Action", &Stomp.bEnableAction, T0);
            ImGuiEx::ResetSettings("Reset", "Restore every stomp setting to its default.", Stomp);

            ImGui::BeginDisabled(!Stomp.bEnableAction);
            {
                ImGuiEx::SliderF("Start Chance", &Stomp.fProbability, 1.0f, 100.0f, T1, "%.0f%%");

                ImGuiEx::HelpText("Which stomp is picked", AlwaysOne);

                const float total = ImGuiEx::WeightTotal({ Stomp.fLightProb, Stomp.fStrongProb, Stomp.fTrampleProb });

                ImGuiEx::SliderWeight("Light Stomp", &Stomp.fLightProb, total, StartHelp);
                ImGuiEx::SliderWeight("Strong Stomp", &Stomp.fStrongProb, total, StartHelp);
                ImGuiEx::SliderWeight("Trample", &Stomp.fTrampleProb, total,
                    "Trample has no crawling or sneaking variant, so it is skipped in those stances.");

                ImGui::Spacing();
                ImGuiEx::SliderF("Grind On Understomp Chance", &Stomp.fUnderstompGrindProbability, 0.0f, 100.0f, T2, "%.0f%%");
                ImGuiEx::SliderF("Grind On Normal Stomp Chance", &Stomp.fStompGrindProbability, 0.0f, 100.0f, T3, "%.0f%%");
            }
            ImGui::EndDisabled();
        }

        ImUtil_Unique {
            PSString T0 = "Enable kicks when standing or hand swipes when sneaking or crawling.";
            PSString T1 = "Set the chance for a kick or swipe action to be started.";

            ImGuiEx::SeperatorH();
            ImGui::Text("Kicks & Swipes");
            ImGuiEx::CheckBox("Enable Action", &Kick.bEnableAction, T0);
            ImGuiEx::ResetSettings("Reset", "Restore every kick and swipe setting to its default.", Kick);

            ImGui::BeginDisabled(!Kick.bEnableAction);
            ImGuiEx::SliderF("Start Chance", &Kick.fProbability, 1.0f, 100.0f, T1, "%.0f%%");
            ImGui::EndDisabled();
        }

        ImGui::BeginDisabled(!Kick.bEnableAction);

        ImGuiEx::HelpText("Which kick is picked",
            "Stance decides the list: standing gives kicks, crawling gives swipes. The two never\n"
            "compete, so each list totals 100%% on its own.");

        ImUtil_Unique {
            ImGui::Spacing();
            ImGui::Text("Standing");

            const float total = ImGuiEx::WeightTotal({ Kick.fKickLightProb, Kick.fKickHeavyProb, Kick.fKickLowProb });

            ImGuiEx::SliderWeight("Light Kick", &Kick.fKickLightProb, total, StartHelp);
            ImGuiEx::SliderWeight("Heavy Kick", &Kick.fKickHeavyProb, total, StartHelp);
            ImGuiEx::SliderWeight("Low Sweep", &Kick.fKickLowProb, total, StartHelp);
        }

        ImUtil_Unique {
            ImGui::Spacing();
            ImGui::Text("Crawling");

            const float total = ImGuiEx::WeightTotal({ Kick.fSwipeLightProb, Kick.fSwipeHeavyProb });

            ImGuiEx::SliderWeight("Light Swipe", &Kick.fSwipeLightProb, total, StartHelp);
            ImGuiEx::SliderWeight("Heavy Swipe", &Kick.fSwipeHeavyProb, total, StartHelp);
        }

        ImGui::EndDisabled();
    }

    //----------------------------------------------------------------------------------------------
    // Tab: Butt Crush
    //----------------------------------------------------------------------------------------------

    void DrawTab_ButtCrush() {

        auto& Butt = Config::AI.ButtCrush;

        ImUtil_Unique {
            PSString T0 = "Enable butt crush actions.";
            PSString T1 = "Set the chance for a butt crush action to be started.";
            PSString T2 = "Chance to start the targeted version rather than the fast one.\n"
                          "0%% always starts fast, 100%% always targeted. The actor's stance still decides\n"
                          "whether that plays as a butt crush or a breast crush.";
            PSString T3 = "How often the actor picks something to do during a targeted crush.";

            ImGuiEx::CheckBox("Enable Action", &Butt.bEnableAction, T0);
            ImGuiEx::ResetSettings("Reset", "Restore every butt crush setting to its default.", Butt);

            ImGui::BeginDisabled(!Butt.bEnableAction);
            {
                ImGuiEx::SliderF("Start Chance", &Butt.fProbability, 1.0f, 100.0f, T1, "%.0f%%");
                ImGuiEx::SliderF("Targeted Crush Chance", &Butt.fButtCrushTypeProb, 0.0f, 100.0f, T2, "%.0f%%");
                ImGuiEx::SliderF("Action Interval", &Butt.fInterval, 1.0f, 10.0f, T3, "Every %.1f Second(s)");

                ImGuiEx::HelpText("How these numbers work", WeightHelp);

                const float total = ImGuiEx::WeightTotal({ Butt.fIdleProb, Butt.fGrowProb, Butt.fCrushProb });

                ImGuiEx::SliderWeight("Do Nothing", &Butt.fIdleProb, total,
                    "Weight of doing nothing this interval.");
                ImGuiEx::SliderWeight("Grow", &Butt.fGrowProb, total,
                    "Grow before crushing. Keep this high if you want a slow build up.");
                ImGuiEx::SliderWeight("Crush", &Butt.fCrushProb, total,
                    "Perform the crush. The game raises this internally as she grows, so a low value\n"
                    "still ends in a crush eventually.");
            }
            ImGui::EndDisabled();
        }
    }

    //----------------------------------------------------------------------------------------------
    // Tab: Simple actions
    //----------------------------------------------------------------------------------------------

    void DrawTab_Simple() {

        ImGuiEx::HelpText("Why these are different",
            "None of these have follow up steps, so nothing competes with them and the number really\n"
            "is a percentage.");

        ImUtil_Unique {
            PSString T0 = "Enable starting a vore action.";
            PSString T1 = "Set the chance for a vore action to be started.";

            ImGui::Text("Vore");
            ImGuiEx::CheckBox("Enable Action", &Config::AI.Vore.bEnableAction, T0);
            ImGuiEx::SliderF("Start Chance", &Config::AI.Vore.fProbability, 1.0f, 100.0f, T1, "%.0f%%",
                !Config::AI.Vore.bEnableAction);
        }

        ImUtil_Unique {
            PSString THelp = "Growing and shrinking need no target, so they start on the spot.\n"
                             "Both are refused when the actor is already at their smallest or largest,\n"
                             "so a high chance here does not mean it happens every time.";

            PSString T0 = "Enable the AI growing itself.";
            PSString T1 = "Set the chance for a growth action to be started.";
            PSString T2 = "Enable the AI shrinking itself.";
            PSString T3 = "Set the chance for a shrink action to be started.";

            ImGuiEx::SeperatorH();
            ImGui::Text("Growth & Shrink");
            ImGuiEx::HelpText("How these work", THelp);

            ImGuiEx::CheckBox("Enable Growth", &Config::AI.Growth.bEnableAction, T0);
            ImGuiEx::SliderF("Growth Chance", &Config::AI.Growth.fProbability, 0.0f, 100.0f, T1, "%.0f%%",
                !Config::AI.Growth.bEnableAction);

            ImGuiEx::CheckBox("Enable Shrink", &Config::AI.Shrink.bEnableAction, T2);
            ImGuiEx::SliderF("Shrink Chance", &Config::AI.Shrink.fProbability, 0.0f, 100.0f, T3, "%.0f%%",
                !Config::AI.Shrink.bEnableAction);
        }

        ImUtil_Unique {
            PSString TCHelp = "Tiny Calamity is a shout effect that is normally player exclusive.\n"
                              "The vanilla AI never uses shouts, so without this an NPC can never turn it on.";

            PSString WCHelp = "Wrathful Calamity is a one shot attack that is normally player exclusive.\n"
                              "- Only against humanoid enemies that are on low health\n"
                              "- And only while Tiny Calamity is active";

            PSString TC = "When Tiny Calamity is active:\n"
                          "- If ON: NPCs shrink the enemy first, then do the size action\n"
                          "- If OFF: NPCs skip the shrink and play the animation right away\n"
                          "Recommended: keep it ON, same size animations look wrong.";

            PSString T0 = "Allow the AI to speak the Tiny Calamity shout itself.";
            PSString T1 = "Set the chance for the AI to shout when it can.\n"
                          "Needs the Tiny Calamity perk, combat, and a size no greater than x2.";
            PSString T2 = "Enable starting a Wrathful Calamity action.";
            PSString T3 = "Set the chance for a Wrathful Calamity action to be started.";

            ImGuiEx::SeperatorH();
            ImGui::Text("Tiny Calamity");
            ImGuiEx::HelpText("What is Tiny Calamity", TCHelp);

            ImGuiEx::CheckBox("Shrink Enemies First", &Config::AI.bCalamityShrinksFirst, TC);
            ImGuiEx::CheckBox("Enable Shouting", &Config::AI.TinyCalamity.bEnableAction, T0);
            ImGuiEx::SliderF("Shout Chance", &Config::AI.TinyCalamity.fProbability, 0.0f, 100.0f, T1, "%.0f%%",
                !Config::AI.TinyCalamity.bEnableAction);

            ImGui::Spacing();
            ImGui::Text("Wrathful Calamity");
            ImGuiEx::HelpText("What is Wrathful Calamity", WCHelp);

            ImGuiEx::CheckBox("Enable Action", &Config::AI.WrathfulCalamity.bEnableAction, T2);
            ImGuiEx::SliderF("Start Chance", &Config::AI.WrathfulCalamity.fProbability, 1.0f, 100.0f, T3, "%.0f%%",
                !Config::AI.WrathfulCalamity.bEnableAction);
        }
    }

    // One table, so the tab list and what each tab draws cannot drift apart.
    struct ActionTab {
        const char* Label;
        void (*Draw)();
    };

    constexpr ActionTab ActionTabs[] = {
        { "Grabs",          &DrawTab_Grabs },
        { "Hugs",           &DrawTab_Hugs },
        { "Thighs",         &DrawTab_Thighs },
        { "Feet",           &DrawTab_Feet },
        { "Butt Crush",     &DrawTab_ButtCrush },
        { "Simple Actions", &DrawTab_Simple },
    };
}

namespace GTS {

	CategoryAI::CategoryAI() {
		m_name = "AI";
	}

    void CategoryAI::DrawLeft() {

        ImUtil_Unique {

            PSString T0 = "Globaly Enable/Disable the follower action AI.";

            PSString T1 = "How often a follower stops to consider starting something.\n"
                          "This is the interval between attempts, not a guarantee one happens.";

            PSString T2 = "Allow the AI to target the player.";
            PSString T3 = "Allow the AI to target other followers.";
            PSString T4 = "Allow AI to perform actions only if NPC is hostile to performer.";
            PSString T5 = "Only allow the action AI to be active when the follower is in combat.";

            PSString T6 = "Prevent Followers from using regular attacks based on their size, when they're large.\n"
                          "- The chance to not attack increases with size.\n"
                          "- Works only if enemies are close to Follower";

            PSString T7 = "If on, always prevents Attacks of followers even if there's no enemies nearby";

            PSString T8 = "When an attempt comes around, this is the chance she picks anything at all.\n"
                          "One roll, before the individual actions are weighed against each other.";

            PSString THelp = "Starting an action takes two rolls.\n\n"
                             "First, every few seconds, one chance decides whether to act at all.\n"
                             "Then the actions that are possible right now compete, each by its own weight,\n"
                             "and one of them wins. The right pane holds those weights.\n\n"
                             "So a low Chance To Act makes followers quiet, while the weights decide what\n"
                             "they do once they have decided to do something.";

            if (ImGui::CollapsingHeader("AI Settings", ImUtil::HeaderFlagsDefaultOpen)) {

                ImGuiEx::HelpText("How does this work", THelp);
                ImGuiEx::CheckBox("Enable AI", &Config::AI.bEnableActionAI, T0);

                {
                    ImGui::BeginDisabled(!Config::AI.bEnableActionAI);

                    ImGuiEx::SliderF("Try To Start A New Action", &Config::AI.fMasterTimer, 2.0f, 15.0f, T1, "Every %.1f Seconds");
                    ImGuiEx::SliderF("Chance To Act", &Config::AI.fActChance, 0.0f, 100.0f, T8, "%.0f%%");

                    ImGui::Spacing();

                    ImGuiEx::CheckBox("Target Player", &Config::AI.bAllowPlayer, T2);
                    ImGui::SameLine();
                    ImGuiEx::CheckBox("Target Other Followers", &Config::AI.bAllowFollowers, T3);
                    ImGui::SameLine();
                    ImGuiEx::CheckBox("Target Only Hostiles", &Config::AI.bHostileOnly, T4);

                    ImGuiEx::CheckBox("Enable AI Only During Combat", &Config::AI.bCombatOnly, T5);
                    ImGuiEx::CheckBox("Disable Regular Attacks When Large", &Config::AI.bDisableAttacks, T6);
                    ImGuiEx::CheckBox("Always Disable Regular Attacks When Large", &Config::AI.bAlwaysDisableAttacks, T7, !Config::AI.bDisableAttacks);

                    ImGui::EndDisabled();
                }

                ImGui::Spacing();
            }
        }

        ImUtil_Unique {

            PSString THelp = "When the AI picks a target that is out of reach it walks there first.\n"
                             "While walking it stops attacking, sheathes its weapon and leaves combat,\n"
                             "and it gives up if the walk stops making progress.";

            PSString T0 = "How long a walk to a target may take before the AI gives up on it.";
            PSString T1 = "How long the AI may fail to get any closer before it gives up on the walk.";
            PSString T2 = "How often the AI looks for a closer target while walking.";
            PSString T3 = "Chance the AI sneaks for an action that has sneaking animations.";
            PSString T4 = "Chance the AI crawls for an action that has crawling animations.\n"
                          "Rolled before the sneak chance, so the two do not need to add up to 100.";

            if (ImGui::CollapsingHeader("Approach Settings", ImUtil::HeaderFlagsDefaultOpen)) {

                ImGuiEx::HelpText("What is this", THelp);

                ImGui::BeginDisabled(!Config::AI.bEnableActionAI);

                ImGuiEx::SliderF("Give Up After", &Config::AI.Movement.fApproachTimeout, 2.0f, 60.0f, T0, "%.0f Second(s)");
                ImGuiEx::SliderF("Give Up When Stuck For", &Config::AI.Movement.fApproachStall, 1.0f, 15.0f, T1, "%.1f Second(s)");
                ImGuiEx::SliderF("Look For A Closer Target", &Config::AI.Movement.fRetargetInterval, 0.25f, 5.0f, T2, "Every %.2f Second(s)");

                ImGui::Spacing();

                ImGuiEx::SliderF("Sneak Chance", &Config::AI.Movement.fSneakChance, 0.0f, 100.0f, T3, "%.0f%%");
                ImGuiEx::SliderF("Crawl Chance", &Config::AI.Movement.fCrawlChance, 0.0f, 100.0f, T4, "%.0f%%");

                ImGui::EndDisabled();
                ImGui::Spacing();
            }
        }

        ImUtil_Unique {

            PSString T0 = "Toggle whether Followers should have movement speed further reduced past x1.25 size.\n"
                          "- This toggle makes Follower Sprinting less likely based on size, so Followers will try to menacingly walk instead of sprinting\n"
                          "- Alters SpeedMult Actor Value, can reduce it up to 15\n\n"
                          "- This toggle requires Dynamic Animation Speed toggle in order to work";
            PSString T2 = "Toggle whether Followers should have decreased rotation speed past x1.25 size.\n\n"
                          "- This toggle requires Dynamic Animation Speed toggle in order to work";
            PSString T3 = "Toggle whether Followers should record movement speed of their legs/hands\n"
                          "- If on - followers won't push others away if just standing still\n"
                          "- May affect fps in a negative way since it records data each frame for each follower";
            PSString T4 = "Toggle whether other NPCs should panic when near a GTS.";

            if (ImGui::CollapsingHeader("Misc Settings", ImUtil::HeaderFlagsDefaultOpen)) {
                ImGuiEx::CheckBox("Decrease Movement Speed AV", &Config::AI.bSlowMovementDown, T0);
                ImGuiEx::CheckBox("Decrease Combat Rotation Speed", &Config::AI.bSlowRotationDown, T2);
                ImGuiEx::CheckBox("Record Node Movement Speed Data", &Config::AI.bRecordBoneSpeedData, T3);
                ImGuiEx::CheckBox("Actors Panic", &Config::AI.bPanic, T4);
                ImGui::Spacing();
            }
        }

        ImUtil_Unique {

            PSString THelp = "These were fixed internal values. Changing them alters pacing and targeting for\n"
                             "every follower, so move them in small steps.\n\n"
                             "Distances are measured in reaches rather than units, so they scale with size:\n"
                             "one reach is how far that actor can act from.";

            PSString T0 = "Run while this much ground is still left to cover. Default 4.";
            PSString T1 = "Jog beyond this, walk inside it. Default 1.5.";
            PSString T2 = "Change stance once this much of the walk is left, so she arrives already in it.\n"
                          "Default 20%%.";
            PSString T3 = "Or once she is this close, whichever comes first. Keeps short walks from\n"
                          "changing stance on arrival. Default 2.";
            PSString T4 = "A different target has to be this much closer before she switches to it.\n"
                          "Default 70%%.";
            PSString T5 = "Size difference at which regular attacks start being held back.\n"
                          "Default x2.5.";

            if (ImGui::CollapsingHeader("Advanced Tuning")) {

                ImGuiEx::HelpText("Read this first", THelp);

                ImGui::BeginDisabled(!Config::AI.bEnableActionAI);

                ImGuiEx::SliderF("Run Beyond", &Config::AI.Movement.fRunBeyond, 1.5f, 10.0f, T0, "%.1f Reaches");
                ImGuiEx::SliderF("Jog Beyond", &Config::AI.Movement.fJogBeyond, 0.5f, 6.0f, T1, "%.1f Reaches");

                ImGui::Spacing();

                ImGuiEx::SliderF("Take Stance With", &Config::AI.Movement.fStanceAt, 5.0f, 90.0f, T2, "%.0f%% Of Walk Left");
                ImGuiEx::SliderF("Take Stance Within", &Config::AI.Movement.fStanceFloor, 0.5f, 6.0f, T3, "%.1f Reaches");
                ImGuiEx::SliderF("Switch Target When Closer By", &Config::AI.Movement.fSwitchRatio, 10.0f, 95.0f, T4, "%.0f%% Of Distance");

                ImGui::Spacing();

                ImGuiEx::SliderF("Suppress Attacks Above", &Config::AI.fSuppressSizeThreshold, 1.0f, 10.0f, T5, "x%.1f Size Difference");

                ImGui::EndDisabled();
                ImGui::Spacing();
            }
        }
    }

    void CategoryAI::DrawRight() {

		static ImGuiEx::CollapsingTabHeader ActionHeader = [] {

            std::vector<std::string> labels = {};

            for (const auto& tab : ActionTabs) {
                labels.emplace_back(tab.Label);
            }

            return ImGuiEx::CollapsingTabHeader("AI Action Settings", labels);
        }();

        ActionHeader.SetDisabledState(!Config::AI.bEnableActionAI);

        if (ImGuiEx::BeginCollapsingTabHeader(ActionHeader)) {

            const int active = ActionHeader.GetActiveTab();

            if (active >= 0 && std::cmp_less(active, std::size(ActionTabs))) {
                ActionTabs[active].Draw();
            }
        }

        ImGuiEx::EndCollapsingTabHeader(ActionHeader);
    }
}
