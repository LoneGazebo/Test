// CustomMods.cpp
#include "CvGameCoreDLLPCH.h"

// must be included after all other headers
#include "LintFree.h"

CustomMods gCustomMods;

CustomMods::CustomMods() :
	m_bInit(false)
{
}


// I would rather have all of this shit here (where it need never be read more than once) to make the calling of events cleaner in the main code
int CustomMods::eventHook(const char* szName, const char* p, ...) {
	CvLuaArgsHandle args;

	va_list vl;
	va_start(vl, p);

	for (const char* it = p; *it; ++it) {
		if (*it == 'b') {
			// It's a boolean
			args->Push(va_arg(vl, bool));
		} else {
			// Assume it's an int
			args->Push(va_arg(vl, int));
		}
	}

	va_end(vl);

	return eventHook(szName, args);
}

int CustomMods::eventTestAll(const char* szName, const char* p, ...) {
	CvLuaArgsHandle args;

	va_list vl;
	va_start(vl, p);

	for (const char* it = p; *it; ++it) {
		if (*it == 'b') {
			// It's a boolean
			args->Push(va_arg(vl, bool));
		} else {
			// Assume it's an int
			args->Push(va_arg(vl, int));
		}
	}

	va_end(vl);

	return eventTestAll(szName, args);
}

int CustomMods::eventTestAny(const char* szName, const char* p, ...) {
	CvLuaArgsHandle args;

	va_list vl;
	va_start(vl, p);

	for (const char* it = p; *it; ++it) {
		if (*it == 'b') {
			// It's a boolean
			args->Push(va_arg(vl, bool));
		} else {
			// Assume it's an int
			args->Push(va_arg(vl, int));
		}
	}

	va_end(vl);

	return eventTestAny(szName, args);
}

int CustomMods::eventAccumulator(int &iValue, const char* szName, const char* p, ...) {
	CvLuaArgsHandle args;

	va_list vl;
	va_start(vl, p);

	for (const char* it = p; *it; ++it) {
		if (*it == 'b') {
			// It's a boolean
			args->Push(va_arg(vl, bool));
		} else {
			// Assume it's an int
			args->Push(va_arg(vl, int));
		}
	}

	va_end(vl);

	return eventAccumulator(iValue, szName, args);
}

int CustomMods::eventHook(const char* szName, CvLuaArgsHandle &args) {
	ICvEngineScriptSystem1* pkScriptSystem = gDLL->GetScriptSystem();
	if (pkScriptSystem) {
		bool bResult;
		if (LuaSupport::CallHook(pkScriptSystem, szName, args.get(), bResult)) {
			return GAMEEVENTRETURN_HOOK;
		}
	}

	return GAMEEVENTRETURN_NONE;
}

int CustomMods::eventTestAll(const char* szName, CvLuaArgsHandle &args) {
	ICvEngineScriptSystem1* pkScriptSystem = gDLL->GetScriptSystem();
	if (pkScriptSystem) {
		bool bResult = false;
		if (LuaSupport::CallTestAll(pkScriptSystem, szName, args.get(), bResult)) {
			if (bResult) {
				return GAMEEVENTRETURN_TRUE;
			} else {
				return GAMEEVENTRETURN_FALSE;
			}
		}
	}

	return GAMEEVENTRETURN_NONE;
}

int CustomMods::eventTestAny(const char* szName, CvLuaArgsHandle &args) {
	ICvEngineScriptSystem1* pkScriptSystem = gDLL->GetScriptSystem();
	if (pkScriptSystem) {
		bool bResult = false;
		if (LuaSupport::CallTestAny(pkScriptSystem, szName, args.get(), bResult)) {
			if (bResult) {
				return GAMEEVENTRETURN_TRUE;
			} else {
				return GAMEEVENTRETURN_FALSE;
			}
		}
	}

	return GAMEEVENTRETURN_NONE;
}

int CustomMods::eventAccumulator(int &iValue, const char* szName, CvLuaArgsHandle &args) {
	ICvEngineScriptSystem1* pkScriptSystem = gDLL->GetScriptSystem();
	if (pkScriptSystem) {
		if (LuaSupport::CallAccumulator(pkScriptSystem, szName, args.get(), iValue)) {
			return GAMEEVENTRETURN_TRUE;
		}
	}

	return GAMEEVENTRETURN_NONE;
}


// Update CustomModOptions table from references in CustomModPostDefines
// Based on code in CvDllDatabaseUtility::PerformDatabasePostProcessing()
void CustomMods::prefetchCache() {
	Database::Connection* db = GC.GetGameDatabase();
	db->BeginTransaction();

	Database::Results kInsert;
	db->Execute(kInsert, "INSERT OR REPLACE INTO CustomModOptions(Name, Value) VALUES(?, ?)");

	Database::Results kPostDefines;
	db->SelectAll(kPostDefines, "CustomModPostDefines");
	while (kPostDefines.Step()) {
		Database::Results kLookup;
		char szSQL[512];
		sprintf_s(szSQL, "select ROWID from %s where Type = '%s' LIMIT 1", kPostDefines.GetText("Table"), kPostDefines.GetText("Type"));

		if (db->Execute(kLookup, szSQL)) {
			if (kLookup.Step()) {
				kInsert.Bind(1, kPostDefines.GetText("Name"));
				kInsert.Bind(2, kLookup.GetInt(0));
				kInsert.Step();
				kInsert.Reset();
			}
		}
	}

	db->EndTransaction();
}

void CustomMods::preloadCache() {
	prefetchCache();

	(void) getOption("EVENTS_CIRCUMNAVIGATION");
}

void CustomMods::reloadCache() {
	m_bInit = false;

	preloadCache();
}

int CustomMods::getOption(const char* szOption, int defValue) {
	return getOption(string(szOption), defValue);
}

int CustomMods::getOption(string sOption, int defValue) {
	if (!m_bInit) {
		const char* szBadPrefix = "MOD_";

		Database::Results kResults;
		DB.SelectAll(kResults, MOD_DB_TABLE);

		while (kResults.Step()) {
			const char* szName = kResults.GetText(MOD_DB_COL_NAME);
			const int iDbUpdates = kResults.GetInt(MOD_DB_COL_DBUPDATES);
			int iValue = kResults.GetInt(MOD_DB_COL_VALUE);

			bool bPrefixError = (strncmp(szName, szBadPrefix, strlen(szBadPrefix)) == 0);

			if (iValue && iDbUpdates) {
				Database::Results kUpdates;
				char szQuery[512] = {0};

				// Did the required mods to the database occur?  We'll assume they didn't, unless proven otherwise!
				bool bOK = false;

				sprintf_s(szQuery, "Name='%s' AND Value=1", szName);
				if (DB.SelectWhere(kUpdates, MOD_DB_UPDATES, szQuery)) {
					if (kUpdates.Step()) {
						// BINGO!  We have our updates
						bOK = true;
					} else {
						// All is not lost as there could be BOTH xml and sql updates
						sprintf_s(szQuery, "Name='%s_SQL' AND Value=1", szName);
						if (DB.SelectWhere(kUpdates, MOD_DB_UPDATES, szQuery)) {
							if (kUpdates.Step()) {
								sprintf_s(szQuery, "Name='%s_XML' AND Value=1", szName);
								if (DB.SelectWhere(kUpdates, MOD_DB_UPDATES, szQuery)) {
									if (kUpdates.Step()) {
										// BINGO!  We have BOTH our updates
										bOK = true;
									}
								}
							}
						}
					}
				}

				if (bOK) {
					CUSTOMLOG("%s: %s appears to have the required database updates", (bPrefixError ? "PREFIX ERROR" : "Cache"), szName);
				} else {
					CUSTOMLOG("%s: %s has missing database updates!", (bPrefixError ? "PREFIX ERROR" : "Cache"), szName);
					iValue = 0;
				}
			}

			CUSTOMLOG("%s: %s = %d", (bPrefixError ? "PREFIX ERROR" : "Cache"), szName, iValue);
			m_options[string(szName)] = iValue;
		}

		MOD_OPT_CACHE(GLOBAL_STACKING_RULES);
		MOD_OPT_CACHE(GLOBAL_BREAK_CIVILIAN_1UPT);
		MOD_OPT_CACHE(GLOBAL_BREAK_CIVILIAN_RESTRICTIONS);
		MOD_OPT_CACHE(GLOBAL_LOCAL_GENERALS);
		MOD_OPT_CACHE(GLOBAL_PROMOTION_CLASSES);
		MOD_OPT_CACHE(GLOBAL_PASSABLE_FORTS);
		MOD_OPT_CACHE(GLOBAL_PASSABLE_FORTS_ANY);
		MOD_OPT_CACHE(GLOBAL_ANYTIME_GOODY_GOLD);
		MOD_OPT_CACHE(GLOBAL_CITY_FOREST_BONUS);
		MOD_OPT_CACHE(GLOBAL_CITY_WORKING);
		MOD_OPT_CACHE(GLOBAL_ALPINE_PASSES);
		MOD_OPT_CACHE(GLOBAL_CS_GIFT_SHIPS);
		MOD_OPT_CACHE(GLOBAL_CS_UPGRADES);
		MOD_OPT_CACHE(GLOBAL_CS_RAZE_RARELY);
		MOD_OPT_CACHE(GLOBAL_CS_LIBERATE_AFTER_BUYOUT);
		MOD_OPT_CACHE(GLOBAL_CS_GIFTS);
		MOD_OPT_CACHE(GLOBAL_CS_OVERSEAS_TERRITORY);
		MOD_OPT_CACHE(GLOBAL_CS_NO_ALLIED_SKIRMISHES);
		MOD_OPT_CACHE(GLOBAL_VENICE_KEEPS_RESOURCES);
		MOD_OPT_CACHE(GLOBAL_NO_FOLLOWUP_FROM_CITIES);
		MOD_OPT_CACHE(GLOBAL_CAPTURE_AFTER_ATTACKING);
		MOD_OPT_CACHE(GLOBAL_NO_OCEAN_PLUNDERING);
		MOD_OPT_CACHE(GLOBAL_NO_CONQUERED_SPACESHIPS);
		MOD_OPT_CACHE(GLOBAL_ALLIES_BLOCK_BLOCKADES);
		MOD_OPT_CACHE(GLOBAL_SHORT_EMBARKED_BLOCKADES);
		MOD_OPT_CACHE(GLOBAL_GRATEFUL_SETTLERS);
		MOD_OPT_CACHE(GLOBAL_RELIGIOUS_SETTLERS);
		MOD_OPT_CACHE(GLOBAL_QUICK_ROUTES);
		MOD_OPT_CACHE(GLOBAL_SUBS_UNDER_ICE_IMMUNITY);
		MOD_OPT_CACHE(GLOBAL_PARATROOPS_MOVEMENT);
		MOD_OPT_CACHE(GLOBAL_PARATROOPS_AA_DAMAGE);
		MOD_OPT_CACHE(GLOBAL_NUKES_MELT_ICE); 
		MOD_OPT_CACHE(GLOBAL_GREATWORK_YIELDTYPES); 
		MOD_OPT_CACHE(GLOBAL_EXCLUDE_FROM_GIFTS);
		MOD_OPT_CACHE(GLOBAL_MOVE_AFTER_UPGRADE);
		MOD_OPT_CACHE(GLOBAL_CANNOT_EMBARK);
		
		MOD_OPT_CACHE(DIPLOMACY_TECH_BONUSES);
		MOD_OPT_CACHE(DIPLOMACY_AUTO_DENOUNCE);
		MOD_OPT_CACHE(DIPLOMACY_CITYSTATES);  
		MOD_OPT_CACHE(DIPLOMACY_CITYSTATES_QUESTS); 
		MOD_OPT_CACHE(DIPLOMACY_CITYSTATES_RESOLUTIONS); 
		MOD_OPT_CACHE(DIPLOMACY_CITYSTATES_HURRY);

		MOD_OPT_CACHE(COMMUNITY_PATCH);
		MOD_OPT_CACHE(MOD_BALANCE_CORE);
		MOD_OPT_CACHE(MOD_BALANCE_CORE_YIELDS);
		MOD_OPT_CACHE(MOD_BALANCE_CORE_SPIES);
		MOD_OPT_CACHE(MOD_BALANCE_CORE_MILITARY);
		MOD_OPT_CACHE(MOD_BALANCE_CORE_SETTLER);
		MOD_OPT_CACHE(MOD_BALANCE_CORE_DEALS);
		MOD_OPT_CACHE(MOD_BALANCE_CORE_MINORS);
		MOD_OPT_CACHE(MOD_BALANCE_CORE_DIFFICULTY);

		MOD_OPT_CACHE(DIPLOMACY_CIV4_FEATURES);
		
		MOD_OPT_CACHE(TRAITS_CROSSES_ICE);
		MOD_OPT_CACHE(TRAITS_CITY_WORKING);
		MOD_OPT_CACHE(TRAITS_ANY_BELIEF);

		MOD_OPT_CACHE(POLICIES_CITY_WORKING);

		MOD_OPT_CACHE(TECHS_CITY_WORKING);

		MOD_OPT_CACHE(PROMOTIONS_VARIABLE_RECON);
		MOD_OPT_CACHE(PROMOTIONS_CROSS_MOUNTAINS);
		MOD_OPT_CACHE(PROMOTIONS_CROSS_OCEANS);
		MOD_OPT_CACHE(PROMOTIONS_CROSS_ICE);
		MOD_OPT_CACHE(PROMOTIONS_HALF_MOVE);
		MOD_OPT_CACHE(PROMOTIONS_DEEP_WATER_EMBARKATION);
		MOD_OPT_CACHE(PROMOTIONS_FLAGSHIP);
		MOD_OPT_CACHE(PROMOTIONS_UNIT_NAMING);
		MOD_OPT_CACHE(PROMOTIONS_IMPROVEMENT_BONUS);

		MOD_OPT_CACHE(UI_CITY_PRODUCTION);
		MOD_OPT_CACHE(UI_CITY_EXPANSION);

		MOD_OPT_CACHE(BUILDINGS_PRO_RATA_PURCHASE);
		MOD_OPT_CACHE(BUILDINGS_CITY_WORKING);

		MOD_OPT_CACHE(TRADE_WONDER_RESOURCE_ROUTES);

		MOD_OPT_CACHE(UNITS_LOCAL_WORKERS);
		MOD_OPT_CACHE(UNITS_HOVERING_LAND_ONLY_HEAL);
		MOD_OPT_CACHE(UNITS_HOVERING_COASTAL_ATTACKS);

		MOD_OPT_CACHE(RELIGION_NO_PREFERRENCES);
		MOD_OPT_CACHE(RELIGION_RANDOMISE);
		MOD_OPT_CACHE(RELIGION_CONVERSION_MODIFIERS);
		MOD_OPT_CACHE(RELIGION_KEEP_PROPHET_OVERFLOW);
		MOD_OPT_CACHE(RELIGION_RECURRING_PURCHASE_NOTIFIY);
		MOD_OPT_CACHE(RELIGION_PLOT_YIELDS);
		MOD_OPT_CACHE(RELIGION_LOCAL_RELIGIONS);

		MOD_OPT_CACHE(PROCESS_STOCKPILE);

		MOD_OPT_CACHE(AI_SECONDARY_WORKERS);
		MOD_OPT_CACHE(AI_SECONDARY_SETTLERS);

		MOD_OPT_CACHE(AI_SMART);
		MOD_OPT_CACHE(AI_SMART_DEALS);
		MOD_OPT_CACHE(AI_SMART_GREAT_PEOPLE);
		MOD_OPT_CACHE(AI_SMART_GRAND_STRATEGY);
		MOD_OPT_CACHE(AI_SMART_POLICY_CHOICE);
		MOD_OPT_CACHE(AI_SMART_ARCHAEOLOGISTS);
		MOD_OPT_CACHE(AI_SMART_DISBAND);
		MOD_OPT_CACHE(AI_SMART_UPGRADES);
		MOD_OPT_CACHE(AI_SMART_HEALING);
		MOD_OPT_CACHE(AI_SMART_FLEE_FROM_DANGER);
		MOD_OPT_CACHE(AI_SMART_RANGED_UNITS);
		MOD_OPT_CACHE(AI_SMART_AIR_TACTICS);
		MOD_OPT_CACHE(AI_SMART_MELEE_TACTICS);
		
		MOD_OPT_CACHE(EVENTS_TERRAFORMING);
		MOD_OPT_CACHE(EVENTS_TILE_IMPROVEMENTS);
		MOD_OPT_CACHE(EVENTS_CIRCUMNAVIGATION);
		MOD_OPT_CACHE(EVENTS_NEW_ERA);
		MOD_OPT_CACHE(EVENTS_NW_DISCOVERY);
		MOD_OPT_CACHE(EVENTS_DIPLO_EVENTS);
		MOD_OPT_CACHE(EVENTS_MINORS);
		MOD_OPT_CACHE(EVENTS_GOODY_CHOICE);
		MOD_OPT_CACHE(EVENTS_GOODY_TECH);
		MOD_OPT_CACHE(EVENTS_AI_OVERRIDE_TECH);
		MOD_OPT_CACHE(EVENTS_GREAT_PEOPLE);
		MOD_OPT_CACHE(EVENTS_RELIGION);
		MOD_OPT_CACHE(EVENTS_FOUND_RELIGION);
		MOD_OPT_CACHE(EVENTS_ACQUIRE_BELIEFS);
		MOD_OPT_CACHE(EVENTS_PLOT);
		MOD_OPT_CACHE(EVENTS_CITY);
		MOD_OPT_CACHE(EVENTS_CITY_BORDERS);
		MOD_OPT_CACHE(EVENTS_CITY_RAZING);
		MOD_OPT_CACHE(EVENTS_CITY_BOMBARD);
		MOD_OPT_CACHE(EVENTS_CITY_CONNECTIONS);
		MOD_OPT_CACHE(EVENTS_AREA_RESOURCES);
		MOD_OPT_CACHE(EVENTS_PARADROPS);
		MOD_OPT_CACHE(EVENTS_UNIT_CREATED);
		MOD_OPT_CACHE(EVENTS_UNIT_PREKILL);
		MOD_OPT_CACHE(EVENTS_UNIT_CAPTURE);
		MOD_OPT_CACHE(EVENTS_CAN_MOVE_INTO);
		MOD_OPT_CACHE(EVENTS_UNIT_UPGRADES);
		MOD_OPT_CACHE(EVENTS_UNIT_DATA);
		MOD_OPT_CACHE(EVENTS_WAR_AND_PEACE);
		MOD_OPT_CACHE(EVENTS_RESOLUTIONS);
		MOD_OPT_CACHE(EVENTS_IDEOLOGIES);
		MOD_OPT_CACHE(EVENTS_NUCLEAR_DETONATION);
		MOD_OPT_CACHE(EVENTS_REBASE);
		MOD_OPT_CACHE(EVENTS_COMMAND);
		MOD_OPT_CACHE(EVENTS_CUSTOM_MISSIONS);

		MOD_OPT_CACHE(EVENTS_RED_TURN);
		MOD_OPT_CACHE(EVENTS_RED_COMBAT);
		MOD_OPT_CACHE(EVENTS_RED_COMBAT_MISSION);
		MOD_OPT_CACHE(EVENTS_RED_COMBAT_ABORT);
		MOD_OPT_CACHE(EVENTS_RED_COMBAT_RESULT);
		MOD_OPT_CACHE(EVENTS_RED_COMBAT_ENDED);

		MOD_OPT_CACHE(API_ESPIONAGE);
		MOD_OPT_CACHE(API_TRADEROUTES);
		MOD_OPT_CACHE(API_RELIGION);
		MOD_OPT_CACHE(API_PLOT_BASED_DAMAGE);
		MOD_OPT_CACHE(API_PLOT_YIELDS);
		MOD_OPT_CACHE(API_ACHIEVEMENTS);
		MOD_OPT_CACHE(API_EXTENSIONS);
		MOD_OPT_CACHE(API_LUA_EXTENSIONS);

		MOD_OPT_CACHE(CONFIG_AI_IN_XML);

		MOD_OPT_CACHE(BUGFIX_LUA_CHANGE_VISIBILITY_COUNT);
		MOD_OPT_CACHE(BUGFIX_RELIGIOUS_SPY_PRESSURE);
		MOD_OPT_CACHE(BUGFIX_MOVE_AFTER_PURCHASE);
		MOD_OPT_CACHE(BUGFIX_UNITCLASS_NOT_UNIT);
		MOD_OPT_CACHE(BUGFIX_BUILDINGCLASS_NOT_BUILDING);
		MOD_OPT_CACHE(BUGFIX_FREE_FOOD_BUILDING);
		MOD_OPT_CACHE(BUGFIX_NAVAL_FREE_UNITS);
		MOD_OPT_CACHE(BUGFIX_NAVAL_NEAREST_WATER);
		MOD_OPT_CACHE(BUGFIX_CITY_STACKING);
		MOD_OPT_CACHE(BUGFIX_BARB_CAMP_TERRAINS);
		MOD_OPT_CACHE(BUGFIX_BARB_CAMP_SPAWNING);
		MOD_OPT_CACHE(BUGFIX_BARB_GP_XP);
		MOD_OPT_CACHE(BUGFIX_REMOVE_GHOST_ROUTES);
		MOD_OPT_CACHE(BUGFIX_UNITS_AWAKE_IN_DANGER);
		MOD_OPT_CACHE(BUGFIX_WORKERS_VISIBLE_DANGER);
		MOD_OPT_CACHE(BUGFIX_FEATURE_REMOVAL);
		MOD_OPT_CACHE(BUGFIX_INTERCEPTOR_STRENGTH);
		MOD_OPT_CACHE(BUGFIX_UNIT_POWER_CALC);
		MOD_OPT_CACHE(BUGFIX_UNIT_POWER_BONUS_VS_DOMAIN_ONLY);
		MOD_OPT_CACHE(BUGFIX_UNIT_POWER_NAVAL_CONSISTENCY);
		MOD_OPT_CACHE(BUGFIX_UNIT_PREREQ_PROJECT);
		MOD_OPT_CACHE(BUGFIX_NO_HOVERING_REBELS);
		MOD_OPT_CACHE(BUGFIX_HOVERING_PATHFINDER);
		MOD_OPT_CACHE(BUGFIX_EMBARKING_PATHFINDER);

		m_bInit = true;
	}

	if (m_options.find(sOption) == m_options.end()) {
		return defValue;
	}

	return m_options[sOption];
}

int CustomMods::getCivOption(const char* szCiv, const char* szName, int defValue) {
	return getOption(string(szCiv) + "_" + szName, getOption(szName, defValue));
}