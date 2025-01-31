/*////////////////////////////////////////////////////////////////////////////

This file is part of neclib.

Copyright � 2015-2020 David Goeth

All Rights reserved.

THE WORK (AS DEFINED BELOW) IS PROVIDED
UNDER THE TERMS OF THIS CREATIVE COMMONS
PUBLIC LICENSE ("CCPL" OR "LICENSE").
THE WORK IS PROTECTED BY COPYRIGHT AND/OR
OTHER APPLICABLE LAW. ANY USE OF THE WORK
OTHER THAN AS AUTHORIZED UNDER THIS LICENSE
OR COPYRIGHT LAW IS PROHIBITED.

BY EXERCISING ANY RIGHTS TO THE WORK PROVIDED
HERE, YOU ACCEPT AND AGREE TO BE BOUND BY THE
TERMS OF THIS LICENSE. TO THE EXTENT THIS
LICENSE MAY BE CONSIDERED TO BE A CONTRACT,
THE LICENSOR GRANTS YOU THE RIGHTS CONTAINED
HERE IN CONSIDERATION OF YOUR ACCEPTANCE OF
SUCH TERMS AND CONDITIONS.

Full license at http://creativecommons.org/licenses/by-nc/3.0/legalcode

/////////////////////////////////////////////////////////////////////////////**/



#include <DII.h>
#include <ObjectManager.h>
#include <Util.h>
#include <HookManager.h>
#include <Windows.h>
#include <api/g2/zcworld.h>
#include <api/g2/ocgame.h>
#include <api/g2/zcparser.h>
#include <Logger.h>
#include <api/g2/ocnpc.h>
#include <api/g2/ocnpcinventory.h>
#include <Levitation.h>
#include <functional>
#include <Constants.h>
#include <api\g2\ocobjectfactory.h>

using namespace constants;

const std::string DII::SAVE_ITEM_FILE_EXT = ".SAV";
const std::string DII::SAVE_ITEM_INSTANCES  = "DII_INSTANCES";
const std::string DII::FILE_PATERN = "DII_*";

bool DII::showExtendedDebugInfo = false;


typedef void ( __thiscall* LoadSavegame )(void*, int, int); 
LoadSavegame loadSavegame;
typedef void ( __thiscall* WriteSavegame )(void*, int, int); 
WriteSavegame writeSavegame;
typedef int ( __thiscall* OCItemGetValue )(void*); 
OCItemGetValue oCItemGetValue;
typedef zCPar_Symbol* (__thiscall* ZCPar_SymbolTableGetSymbol)(void* pThis, int index); 
ZCPar_SymbolTableGetSymbol zCPar_SymbolTableGetSymbol;
typedef zCPar_Symbol* (__thiscall* ZCPar_SymbolTableGetSymbolString)(void* pThis, zSTRING const & symbolNaame); 
ZCPar_SymbolTableGetSymbolString zCPar_SymbolTableGetSymbolString;
typedef int (__thiscall* ZCPar_SymbolTableGetIndex)(void* pThis, zSTRING const & symbolNaame); 
ZCPar_SymbolTableGetIndex zCPar_SymbolTableGetIndex;
typedef int (__thiscall* ZCParserGetIndex)(void* pThis, zSTRING const & symbolNaame); 
ZCParserGetIndex zCParserGetIndex;
typedef int (__thiscall* CreateInstance)(void* pThis, int instanceId, void* source);
CreateInstance createInstance;
typedef void (__thiscall* OCGameLoadGame)(void* pThis, int, zSTRING const &);
OCGameLoadGame oCGameLoadGame;
typedef void ( __thiscall* OCGameLoadWorld )(void*, int, zSTRING const &); 
OCGameLoadWorld oCGameLoadWorld;


OCItemInsertEffect DII::oCItemInsertEffect = (OCItemInsertEffect)0x00712C40;

void DII::hookModule()
{
	checkDII_USER_DATA_ClassFormat();

	loadSavegame = (LoadSavegame) (LOAD_SAVEGAME_ADDRESS);
	writeSavegame = (WriteSavegame) (WRITE_SAVEGAME_ADDRESS);
	oCItemGetValue = (OCItemGetValue) (OCITEM_GET_VALUE_ADDRESS);
	createInstance = (CreateInstance) (ZCPARSER_CREATE_INSTANCE);
	oCGameLoadGame = (OCGameLoadGame) OCGAME_LOAD_GAME_ADDRESS;

	zCParserGetIndex = (ZCParserGetIndex)ZCPARSER_GETINDEX;
	zCPar_SymbolTableGetIndex = (ZCPar_SymbolTableGetIndex) ZCPAR_SYMBOL_TABLE_GETINDEX;
	zCPar_SymbolTableGetSymbol = (ZCPar_SymbolTableGetSymbol)ZCPAR_SYMBOL_TABLE_GETSYMBOL;
	zCPar_SymbolTableGetSymbolString = (ZCPar_SymbolTableGetSymbolString)ZCPAR_SYMBOL_TABLE_GETSYMBOL_STRING;


		//0x006521E0

	HookManager* hookManager = HookManager::getHookManager();


	hookManager->addFunctionHook((LPVOID*)&loadSavegame, loadSavegameHook, mModuleDesc);
	hookManager->addFunctionHook((LPVOID*)&writeSavegame, writeSavegameHook, mModuleDesc);
	hookManager->addFunctionHook((LPVOID*)&oCItemGetValue, oCItemGetValueHook, mModuleDesc);
	

	hookManager->addFunctionHook((LPVOID*)&createInstance, createInstanceHook, mModuleDesc);
	hookManager->addFunctionHook((LPVOID*)&oCGameLoadGame, oCGameLoadGameHook, mModuleDesc);
	
	hookManager->addFunctionHook((LPVOID*)&zCParserGetIndex, zCParserGetIndexHook, mModuleDesc);
	hookManager->addFunctionHook((LPVOID*)&zCPar_SymbolTableGetIndex, zCPar_SymbolTableGetIndexHook, mModuleDesc);
	hookManager->addFunctionHook((LPVOID*)&zCPar_SymbolTableGetSymbol, zCPar_SymbolTableGetSymbolHook, mModuleDesc);
	hookManager->addFunctionHook((LPVOID*)&zCPar_SymbolTableGetSymbolString, zCPar_SymbolTableGetSymbolStringHook, mModuleDesc);
}

void DII::unHookModule()
{
	HookManager* hookManager = HookManager::getHookManager();
	hookManager->removeFunctionHook((LPVOID*)&loadSavegame, loadSavegameHook, mModuleDesc);
	hookManager->removeFunctionHook((LPVOID*)&writeSavegame, writeSavegameHook, mModuleDesc);
	hookManager->removeFunctionHook((LPVOID*)&oCItemGetValue, oCItemGetValueHook, mModuleDesc);

	hookManager->removeFunctionHook((LPVOID*)&createInstance, createInstanceHook, mModuleDesc);
	hookManager->removeFunctionHook((LPVOID*)&oCGameLoadGame, oCGameLoadGameHook, mModuleDesc);

	hookManager->removeFunctionHook((LPVOID*)&zCParserGetIndex, zCParserGetIndexHook, mModuleDesc);
	hookManager->removeFunctionHook((LPVOID*)&zCPar_SymbolTableGetIndex, zCPar_SymbolTableGetIndexHook, mModuleDesc);
	hookManager->removeFunctionHook((LPVOID*)&zCPar_SymbolTableGetSymbol, zCPar_SymbolTableGetSymbolHook, mModuleDesc);
	hookManager->removeFunctionHook((LPVOID*)&zCPar_SymbolTableGetSymbolString, zCPar_SymbolTableGetSymbolStringHook, mModuleDesc);
};




bool DII::DII_AddProxy(const zSTRING& sourceInstanceName, const zSTRING& targetInstanceName)
{
	return ObjectManager::getObjectManager()->addProxy(sourceInstanceName, targetInstanceName);
}

void DII::DII_ApplyInstanceChangesToAll(const zSTRING& instanceName)
{
	mLogStream << __FUNCTION__ << ": " << instanceName.ToChar() << std::endl;
	util::debug(mLogStream);

	auto* manager = ObjectManager::getObjectManager();
	auto* parser = zCParser::GetParser();
	auto symbolIndex = parser->GetIndex(instanceName);
	if (!manager->isDynamicInstance(symbolIndex)) {
		mLogStream << __FUNCTION__ << ": " << instanceName.ToChar() << " is not a  DII" << std::endl;
		util::logWarning(mLogStream);
		return;
	}

	ItemUpdater::UpdateItemData params = { symbolIndex, symbolIndex };
	manager->callForAllItems(ItemUpdater::updateItem, NULL, &params);
}

void DII::DII_RemoveProxy(const zSTRING& sourceInstanceName)
{
	ObjectManager::getObjectManager()->removeProxy(sourceInstanceName);
}


oCItem* __cdecl DII::DII_CreateNewItem(int instanceId) // Func void DII_CreateNewItem(var C_Item item, VAR INT instanceId)
{

	oCItem* item = oCObjectFactory::GetFactory()->CreateItem(instanceId);

	mLogStream << "DII_CreateNewItem: " << instanceId;
	util::debug(mLogStream);

	return item;
}

void __cdecl DII::DII_DeleteDII(int parserSymbolIndex)
{
	auto* manager = ObjectManager::getObjectManager();

	try {
		manager->deleteDII(parserSymbolIndex);
	}
	catch (const std::invalid_argument & e) {
		mLogStream << e.what() << std::endl;
		util::logWarning(mLogStream);
	}
}

void DII::DII_DeleteItem(oCItem* item)
{
	if (item == NULL) return;

	int* refCtr = (int*)((BYTE*)item + 0x4);
	mLogStream << "DII_DeleteItem:  refCtr: " << *refCtr;
	util::debug(mLogStream);

	if (*refCtr >= 0)
	{
		oCGame::GetGame()->GetGameWorld()->RemoveVob(item);
	}

	ObjectManager::oCItemOperatorDelete(item);
}


int DII::DII_CreateNewInstanceInt(oCItem* item) //Func int DII_CreateNewInstance(var C_Item item)
{
	if (item == NULL) { return NULL; }

	mLogStream << __FUNCTION__ << ": Param: " << item->name.ToChar();
	util::debug(mLogStream);

	// Create new instance with item
	ObjectManager* manager = ObjectManager::getObjectManager();


	int parserSymbolIndex = 0;

	while (!parserSymbolIndex) {
#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max

		const std::string instanceName = "DII_*" + std::to_string(util::generateRandom(std::numeric_limits<int>::min(), std::numeric_limits<int>::max())) + "*";

#pragma pop_macro("max")
#pragma pop_macro("min")

		parserSymbolIndex = manager->createNewInstanceId(item, instanceName);
		if (!parserSymbolIndex) {
			mLogStream << __FUNCTION__ << ": Couldn't create new instance '" << instanceName << "'" << std::endl;
			util::logWarning(mLogStream);
		}
	}

	mLogStream << __FUNCTION__ << ": key = " << parserSymbolIndex << std::endl;
	util::debug(mLogStream);
	return parserSymbolIndex;
}

zSTRING* DII::DII_CreateNewInstance(oCItem* item)
{
	const auto parserSymbolIndex = DII_CreateNewInstanceInt(item);
	auto* symbol = zCParser::GetParser()->GetSymbol(parserSymbolIndex);
	return &symbol->name;
}

int DII::DII_CreateNewInstanceStr(oCItem* item, const zSTRING& instanceName)
{
	if (!item) {
		return false;
	}

	std::string instanceNameStr = instanceName.ToChar();
	std::transform(instanceNameStr.begin(), instanceNameStr.end(), instanceNameStr.begin(), std::toupper);


	mLogStream << "Param: " << item->name.ToChar();
	util::debug(mLogStream);

	// Create new instance with item
	ObjectManager* manager = ObjectManager::getObjectManager();

	int parserSymbolIndex = manager->createNewInstanceId(item, instanceNameStr);
	if (!parserSymbolIndex) {
		mLogStream << __FUNCTION__ << ": Couldn't create new instance '" << instanceNameStr << "'" << std::endl;
		util::logWarning(mLogStream);
	}


	int index = manager->getDynInstanceId(item);
	mLogStream << __FUNCTION__ << "Index: " << index << std::endl;
	util::debug(mLogStream);

	mLogStream << __FUNCTION__ << ": parser symbol index for instance = " << parserSymbolIndex << std::endl;
	util::debug(mLogStream);
	return parserSymbolIndex != 0;
}

int DII::DII_IsDynamic(oCItem* item) // Func DII_IsDynamic(VAR C_ITEM item)
{
	if (item == NULL) { return FALSE; }

	bool modified = ObjectManager::getObjectManager()->isAssignedToDII(item);
	if (modified)
	{
		return TRUE;
	}
	return FALSE;
}

int DII::DII_IsInstanceDynamic(int parserSymbolIndex)
{
	bool modified = ObjectManager::getObjectManager()->isDynamicInstance(parserSymbolIndex);
	return modified;
}

BYTE* DII::DII_GetUserData(int instanceIdParserSymbolIndex) // Func DII_UserData DII_GetUserData(var int instanceId)
{
	ObjectManager* manager = ObjectManager::getObjectManager();

	if (!manager->isDynamicInstance(instanceIdParserSymbolIndex))
	{
		mLogStream << __FUNCTION__ << ": instanceId isn't dynamic" << std::endl;
		util::debug(mLogStream);
		return NULL;
	}

	DynInstance* storeItem = manager->getInstanceItem(instanceIdParserSymbolIndex);
	return storeItem->getUserData();
}



bool DII::DII_UpdateInstance(const zSTRING& instanceName, oCItem* item)
{
	ObjectManager* manager = ObjectManager::getObjectManager();

	auto instanceIdParserSymbolIndex = manager->getUnProxiedInstanceID(instanceName);

	DynInstance* dynInstance = manager->getInstanceItem(instanceIdParserSymbolIndex);

	if (!item) {
		mLogStream << __FUNCTION__ << ": item mustn't be null " << std::endl;
		util::logWarning(mLogStream);
		return false;
	}

	if (!dynInstance)
	{
		mLogStream << __FUNCTION__ << ": dynInstance not found for instance id " << instanceIdParserSymbolIndex << std::endl;
		util::logWarning(mLogStream);
		return false;
	}
	dynInstance->store(*item);

	return true;
}

void DII::DII_AssignInstanceId(oCItem* item, int instanceIdParserSymbolIndex)
{
	ObjectManager* manager = ObjectManager::getObjectManager();
	manager->assignInstanceId2(item, instanceIdParserSymbolIndex);
}

void DII::DII_GetItemByInstanceId(int itemParserSymbolIndex, int instanceIdParserSymbolIndex)
{
	if (itemParserSymbolIndex <= 0) return;
	zCParser* parser = zCParser::GetParser();
	zCPar_Symbol* symbol = parser->GetSymbol(itemParserSymbolIndex);
	oCItem* item = (oCItem*)symbol->offset;

	// Check if provided instance id is valid
	zCPar_Symbol* instanceSym = parser->GetSymbol(instanceIdParserSymbolIndex);

	if (!instanceSym)
	{
		mLogStream << __FUNCTION__ << ": instanceSym is Null! No item will be searched!" << std::endl;
		util::logWarning(mLogStream);
		return;
	}


	ObjectManager* manager = ObjectManager::getObjectManager();
	item = manager->getItemByInstanceId(instanceIdParserSymbolIndex);

	if (!item)
	{
		mLogStream << __FUNCTION__ << ": item is null!" << std::endl;
		util::logWarning(mLogStream);
	}
	else
	{
		mLogStream << __FUNCTION__ << ": item found..." << std::endl;
		util::debug(mLogStream);
	};

	// update the c_item
	symbol->offset = (int)item;
}


void DII::DII_ChangeInstanceForAll(const zSTRING& sourceName, const zSTRING& targetName)
{
	ObjectManager* manager = ObjectManager::getObjectManager();

	auto sourceID = manager->getUnProxiedInstanceID(sourceName);
	auto targetID = manager->getUnProxiedInstanceID(targetName);

	ItemUpdater::UpdateItemData params = { sourceID, targetID };
	manager->callForAllItems(ItemUpdater::updateItemInstance, NULL, &params);
}

int DII::oCItemGetValueHook(void* pThis) {
	oCItem* item = static_cast<oCItem*>(pThis);
	ObjectManager* manager = ObjectManager::getObjectManager();
	//if (manager->getDynInstanceId(item) > ObjectManager::INSTANCE_BEGIN) {
	if (manager->isAssignedToDII(item)) {
		return item->value;
	}
	return oCItemGetValue(pThis);
};



zCPar_Symbol* DII::zCPar_SymbolTableGetSymbolHook(void* pThis, int index)
{
	auto* manager = ObjectManager::getObjectManager();
	index = manager->resolveProxying(index);
	
	zCPar_Symbol* result = zCPar_SymbolTableGetSymbol(pThis, index);
	if (result == NULL)
	{
		result = manager->getSymbolByIndex(index);
	}

	return result;
}

zCPar_Symbol* DII::zCPar_SymbolTableGetSymbolStringHook(void* pThis, zSTRING const & symbolName)
{
	auto* manager = ObjectManager::getObjectManager();
	zSTRING resolvedName = manager->resolveProxying(symbolName);
	zCPar_Symbol* result = zCPar_SymbolTableGetSymbolString(pThis, resolvedName);
	if (result == NULL)
	{
		result = manager->getSymbolByName(resolvedName);
	}

	return result;
}

int DII::zCPar_SymbolTableGetIndexHook(void* pThis, zSTRING const& symbolName)
{
	auto* manager = ObjectManager::getObjectManager();
	zSTRING resolvedName = manager->resolveProxying(symbolName);
	int result = zCPar_SymbolTableGetIndex(pThis, resolvedName);
	if (result == -1)
	{
		result = manager->getIndexByName(resolvedName);
	} 

	return result;
}

int DII::zCParserGetIndexHook(void* pThis, zSTRING const& symbolName)
{
	auto* manager = ObjectManager::getObjectManager();
	int result = zCParserGetIndex(pThis, symbolName);
	if (result == -1)
	{
		result = manager->getIndexByName(symbolName);
	}

	return result;
}

DII::DII() :Module("DII")
{
}

DII::~DII()
{
	ObjectManager::release();
}


 void DII::loadSavegameHook(void* pThis,int saveGameSlotNumber, int b)
{   
	mLogStream << __FUNCTION__ << ": load savegame..." << std::endl;
	util::logInfo(mLogStream);
	loadDynamicInstances(saveGameSlotNumber);
	loadSavegame(pThis, saveGameSlotNumber, b);

	mLogStream << __FUNCTION__ << ": done." << std::endl;
	util::logInfo(mLogStream);
};


 void DII::writeSavegameHook(void* pThis,int saveGameSlotNumber, int b)
{   
	mLogStream << __FUNCTION__" << : save game..." << std::endl;
	util::logInfo(mLogStream);

	ObjectManager* manager = ObjectManager::getObjectManager();

	//finally write the savegame and restore items that are reseted by the savegame writing method
	std::string saveGameDir;
	if (saveGameSlotNumber != -1)
	{
		saveGameDir = manager->getSaveGameDirectoryPath(saveGameSlotNumber);	
	} else
	{
		saveGameDir = manager->getCurrentDirectoryPath();
	}

	std::string currentDir = manager->getCurrentDirectoryPath();

	util::copyContentTo(currentDir,saveGameDir, FILE_PATERN);

	// Write actual savegame
	writeSavegame(pThis, saveGameSlotNumber, b);

	// We save now the dynamic instances
	// It is better to do this after savegame writing, as gothic copies (possibly outdated) dynamic-instance-save-files from the current
	// save directory to the target savegame directory. Would be save the dynamic instances before the savegame-writing, gothic would eventually 
 	// overwrite our stuff!!! 
	std::string saveInstances = SAVE_ITEM_INSTANCES + SAVE_ITEM_FILE_EXT;
	manager->saveNewInstances(const_cast<char*>(saveGameDir.c_str()), const_cast<char*>(saveInstances.c_str()));
	util::copyFileTo(saveGameDir + saveInstances, currentDir + saveInstances);

	mLogStream << __FUNCTION__ << ": done." << std::endl;
	util::logInfo(mLogStream);
};


 int DII::createInstanceHook(void* pThis, int symolIndex, void* source)
{
	 ObjectManager* manager = ObjectManager::getObjectManager();
	 
	 int instanceId = manager->resolveProxying(symolIndex);

	zCPar_Symbol* symbol = zCParser::GetParser()->GetSymbol(instanceId);
	if (symbol == NULL)
	{
		mLogStream << __FUNCTION__ << ": symbol is null! InstanceId: " << instanceId << std::endl;
		util::debug(mLogStream, Logger::Warning);
	}

	// Get the smybol of the item variable and let it point to the 'source' variable (will contain the newly created item!)
	//
 	zCParser* parser = (zCParser*)pThis;
	zCPar_Symbol* itemSymbol = parser->GetSymbol("DII_ITEM");
	 if (itemSymbol != nullptr)
		itemSymbol->offset = (int)source;

	int result = createInstance(pThis, instanceId, source);

	auto isDynamic = manager->isDynamicInstance(instanceId);
	if (isDynamic)
	{
		oCItem* item = (oCItem*)source;
		manager->InitItemWithDynInstance(item, instanceId);	
		result = manager->getDynInstanceId(item);
	}

	return result;
}

void DII::oCGameLoadGameHook(void* pThis, int second, zSTRING const& worldName)
{
	mLogStream << __FUNCTION__  << ": load..."<< std::endl;
	util::logInfo(mLogStream);
	ObjectManager* manager = ObjectManager::getObjectManager();
	manager->releaseInstances();
	oCGameLoadGame(pThis, second, worldName);

	mLogStream << __FUNCTION__ << ": done." << std::endl;
	util::logInfo(mLogStream);
}

zCListSort<oCItem>* DII::getInvItemByInstanceId(oCNpcInventory* inventory, int instanceId)
{
	inventory->UnpackCategory();
	ObjectManager* manager = ObjectManager::getObjectManager();
	zCListSort<oCItem>* list = reinterpret_cast<zCListSort<oCItem>*>(inventory->inventory_data);
	while(list != NULL) {
		oCItem* item = list->GetData();
		if (item != NULL && manager->getInstanceId(*item) == instanceId)
		{
			return list;
		}
		list = list->GetNext();
	}

	return NULL;
};

oCItem* DII::getInvItemByInstanceId2(oCNpcInventory* inventory, int instanceId)
{
	inventory->UnpackCategory();
	int itemNumber = inventory->GetNumItemsInCategory();
	for (int i = 0; i < itemNumber; ++i)
	{
		oCItem* itm = inventory->GetItem(i);
		if (itm != NULL && itm->GetInstance() == instanceId)
		{
			return itm;
		}
	}

	return NULL;
}



std::string DII::getClearedWorldName(zSTRING const & worldName) {
	std::string text (const_cast<char*>(const_cast<zSTRING &>(worldName).ToChar()));
	std::vector<std::string> splits;
	util::split(splits, text, '/');
	text = splits.back();
	splits.clear();
	util::split(splits, text, '\\');
	text = splits.back();
	splits.clear();
	util::split(splits, text, '.');
	//now only two elements are in splits (the file name and the file type (*.ZEN))
	text = splits.front();
	return text;
}

void DII::loadDynamicInstances(int saveGameSlotNumber)
{
	mLogStream << __FUNCTION__ << ": load dii instances..." << std::endl;
	util::logInfo(mLogStream);
	ObjectManager* manager = ObjectManager::getObjectManager();
	manager->releaseInstances();
	std::string instances = SAVE_ITEM_INSTANCES + SAVE_ITEM_FILE_EXT;
	std::string saveGameDir = manager->getSaveGameDirectoryPath(saveGameSlotNumber);
	std::string fileName = saveGameDir + instances;
	manager->loadNewInstances((char*)fileName.c_str());
	mLogStream << __FUNCTION__ << ": done." << std::endl;
	util::logInfo(mLogStream);
}

void DII::checkDII_USER_DATA_ClassFormat()
{
	constexpr int zCPar_Symbol_bitfield_type = ((1 << 4) - 1) << 12;
	constexpr int zPAR_TYPE_CLASS = 4 << 12;
	constexpr int zPAR_TYPE_INT = 2 << 12;
	constexpr int zPAR_TYPE_STRING = 3 << 12;
	constexpr int zCPar_Symbol_bitfield_ele = ((1 << 12) - 1) << 0;

	// Retrieve the class symbol
	auto* parser = zCParser::GetParser();
	auto* symbol = parser->GetSymbol(DII_USER_DATA_CLASS_NAME);

	// assert that the symbol is valid
	if (!symbol) {
		mLogStream << __FUNCTION__ << ": " << DII_USER_DATA_CLASS_NAME << " is not defined" << std::endl;
		util::logFatal(mLogStream);
		return;
	}

	// assert that the symbol points to a class

	auto type = symbol->bitfield & zCPar_Symbol_bitfield_type;
	if (type != zPAR_TYPE_CLASS) {
		mLogStream << __FUNCTION__ << ": Expected " << DII_USER_DATA_CLASS_NAME << " to be a class." << std::endl;
		util::logFatal(mLogStream);
		return;
	}

	// check that class size matches the expected size
	auto* manager = ObjectManager::getObjectManager();
	const auto classSize = symbol->offset;
	const auto integerAmount = DII_UserData::getIntAmount();
	const auto stringAmount = DII_UserData::getStringAmount();
	const auto expectedSize = integerAmount * sizeof(int) + stringAmount * sizeof(zSTRINGSerialized);
	if (classSize != expectedSize) {
		mLogStream << __FUNCTION__ << ": class size of " << DII_USER_DATA_CLASS_NAME << " is " << classSize << " but " << expectedSize << " is expected" << std::endl;
		util::logFatal(mLogStream);
		return;
	}



	
	// Are we done?
	if (classSize == 0) return; 



	//check that all integers are defined before the strings.

	// Note: if symbol was loaded from dat file, the next pointer is null!
	// In this case we have to skip without any checking.
	if (!symbol->next) return;

	auto processedSize = 0;
	bool processStrings = false;

	while (processedSize < classSize) {
		symbol = symbol->next;
		if (!symbol) {
			mLogStream << __FUNCTION__ << ": " << DII_USER_DATA_CLASS_NAME << " has an unexpected  next symbol (null)(???)" << std::endl;
			util::logFatal(mLogStream);
			return;
		}

		int type = symbol->bitfield & zCPar_Symbol_bitfield_type;

		if (type != zPAR_TYPE_INT && type != zPAR_TYPE_STRING) {
			mLogStream << __FUNCTION__ << ": " << DII_USER_DATA_CLASS_NAME << ": only int and string members are supported for the class format." << std::endl;
			util::logFatal(mLogStream);
			return;
		}

		//check that all integers are defined before the strings come.
		if (type == zPAR_TYPE_STRING && !processStrings) {
			processStrings = true;
			if (processedSize != integerAmount * sizeof(int)) {
				mLogStream << __FUNCTION__ << ": " << DII_USER_DATA_CLASS_NAME << ": all integer variables have to be declared before the string variables!" << std::endl;
				util::logFatal(mLogStream);
				return;
			}
		}

		// get the element count
		int count = symbol->bitfield & zCPar_Symbol_bitfield_ele;
		if (count <= 0) count = 1;

		auto typeSize = processStrings ? sizeof(zSTRINGSerialized) : sizeof(int);

		// update offset
		processedSize = symbol->offset + count * typeSize;
	}

	// check the class size a second time for safety
	if (processedSize != classSize) {
		mLogStream << __FUNCTION__ << ": class size of " << DII_USER_DATA_CLASS_NAME << " is " << classSize << " but " << processedSize << " was calculated. Fix that bug!" << std::endl;
		util::logFatal(mLogStream);
		return;
	}

}


bool DII::ItemUpdater::updateItem(void* obj, void* param, oCItem* itm)
{
	if (itm == NULL) return false;

	ItemUpdater::UpdateItemData* params = (ItemUpdater::UpdateItemData*)param;
	ObjectManager* manager = ObjectManager::getObjectManager();
	int id = manager->getInstanceId(*itm);;
	if (id == params->expectedInstanceID)
	{
		//int refCtr = *(int*)((BYTE*)itm + 0x4);

		constexpr int ITEM_ACTIVE = 0x40000000;
		constexpr int ITEM_DROPPED = 1 << 10;
		constexpr int ITEM_NFOCUS = 1 << 23;
		bool isActive = itm->flags & ITEM_ACTIVE;
		bool dropped = itm->flags & ITEM_DROPPED;
		bool nfocus = itm->flags & ITEM_NFOCUS;

		bool isInWorld = manager->isItemInWorld(itm);
		int flags = itm->flags;

		auto resolvedID = manager->resolveProxying(params->newInstanceID);
		bool notSameInstance = manager->resolveProxying(params->newInstanceID) != id;

		if (notSameInstance) {
			mLogStream << __FUNCTION__ << ": setting another instance Id causes item effects not to vanish!" << std::endl;
			util::logWarning(mLogStream);
		}

		manager->setInstanceId(itm, resolvedID);
		manager->oCItemSaveRemoveEffect(itm);
		itm->InitByScript(resolvedID, itm->amount);

		if (isActive) itm->flags |= ITEM_ACTIVE;
		if (dropped) itm->flags |= ITEM_DROPPED;
		if (nfocus) itm->flags |= ITEM_NFOCUS;

		manager->oCItemSaveRemoveEffect(itm);

		if (isInWorld && !isActive)
			manager->oCItemSaveInsertEffect(itm);

		return true;
	}

	return false;
}

bool DII::ItemUpdater::updateItemInstance(void* obj, void* param, oCItem* itm)
{
	if (itm == NULL) return false;

	ItemUpdater::UpdateItemData* params = (ItemUpdater::UpdateItemData*)param;
	ObjectManager* manager = ObjectManager::getObjectManager();
	int id = manager->getInstanceId(*itm);;
	if (id == params->expectedInstanceID)
	{
		auto resolvedID = manager->resolveProxying(params->newInstanceID);
		manager->setInstanceId(itm, resolvedID);
		return true;
	}

	return false;
}