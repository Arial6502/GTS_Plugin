#pragma once
#include "Config/Util/TomlRefl.hpp"

struct SettingsExperimental_t {
	
	float fTest1 = 1.0f;
	float fTest2 = 1.0f;
	float fTest3 = 1.0f;
	float fTest4 = 1.0f;
	float fTest5 = 1.0f;
	float fTest6 = 1.0f;
	float fTest7 = 1.0f;
	float fTest8 = 1.0f;

	int32_t iTest1 = 0;
	int32_t iTest2 = 0;
	int32_t iTest3 = 0;
	int32_t iTest4 = 0;
	int32_t iTest5 = 0;
	int32_t iTest6 = 0;
	int32_t iTest7 = 0;
	int32_t iTest8 = 0;

};
TOML_SERIALIZABLE(SettingsExperimental_t);
TOML_REGISTER_NAME(SettingsExperimental_t, "Experiments");
