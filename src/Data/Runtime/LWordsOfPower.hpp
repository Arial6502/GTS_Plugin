#pragma once

namespace RuntimeData {

	struct WordsOfPower : iListable <RE::TESWordOfPower> {

		//The single word behind the Tiny Calamity shout. Knowing it is what lets an actor use the shout.
		Entry(GTSWordTinyCalamity, GTSP, 0x5ED063);
	};
}
