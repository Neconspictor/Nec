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

#include <zCOptionExtended.h>
#include "ObjectManager.h"
#include <fstream> 
#include "api/g2/zcoption.h"
#include "api/g2/zcworld.h"
#include "HookManager.h"
#include "oCMobContainer.h"
#include "Util.h"
#include "api/g2/zcparser.h"
#include <ocgameExtended.h>
#include <Logger.h>
#include <set>
#include <zCPar_SymbolTable.h>
#include <api/g2/oCObjectFactory.h>
#include <api/g2/oCItemExtended.h>
#include <Constants.h>
#include <DII.h>

using namespace std;
using namespace constants;

ObjectManager::ObjectManager() = default;
std::unique_ptr<ObjectManager> ObjectManager::mInstance = std::make_unique<ObjectManager>();

bool ObjectManager::addProxy(const zSTRING & sourceInstance, const zSTRING & targetInstance)
{
	//We only add a proxy if sourceInstance isn't proxied yet
	auto sourceAlreadyThereIT = mProxiesNames.find(sourceInstance);
	if (sourceAlreadyThereIT != mProxiesNames.end()) {
		mLogStream << __FUNCSIG__ << ": Cannot add proxy (" << sourceInstance.ToChar() << ", " << targetInstance.ToChar()
			<< ") since their already exists another proxy path: ("
			<< sourceAlreadyThereIT->first.ToChar() << ", " << sourceAlreadyThereIT->second.ToChar() << ")" << std::endl;

		util::logWarning(&mLogStream);
		return false;
	}

	auto* parser = zCParser::GetParser();

	// Note: source instance id cannot be a resolved proxy anymore.
	const auto sourceInstanceID = parser->GetIndex(sourceInstance);

	// Note: target instance id could be a resolved proxy!
	auto targetInstanceID = parser->GetIndex(targetInstance);
	{
		auto it = mUnresolvedNamesToInstances.find(targetInstance);
		if (it != mUnresolvedNamesToInstances.end()) {
			targetInstanceID = it->second;
		}
	}

	// We don't want to proxy invalid parser symbol indices
	if (sourceInstanceID == -1) {
		mLogStream << __FUNCSIG__ << ": Cannot add proxy (" << sourceInstance.ToChar() << ", " << targetInstance.ToChar()
			<< ") since source instance index is not a valid parser symbol index!" << std::endl;
		util::logWarning(&mLogStream);
		return false;
	}


	if (targetInstanceID == -1) {
		mLogStream << __FUNCSIG__ << ": Cannot add proxy (" << sourceInstance.ToChar() << ", " << targetInstance.ToChar()
			<< ") since target instance index is not a valid parser symbol index!" << std::endl;
		util::logWarning(&mLogStream);
		return false;
	}
	
	// We don't want self proxying
	if (targetInstanceID == sourceInstanceID) {
		mLogStream << __FUNCSIG__ << ": Cannot add proxy (" << sourceInstance.ToChar() << ", " << targetInstance.ToChar()
			<< ") since self proxying isn't allowed!" << std::endl;
		util::logWarning(&mLogStream);
		return false;
	}

	//check, that we do not add a circle in proxying
	auto it = mProxies.find(targetInstanceID);
	while (it != mProxies.end()) {
		auto target = it->second;
		if (target == sourceInstanceID) {
			mLogStream << __FUNCSIG__ << ": Cannot add proxy (" << sourceInstance.ToChar() << ", " << targetInstance.ToChar() 
				<< ") since it would result into circle proxying!" << std::endl;
			util::logWarning(&mLogStream);
			return false;
		}
		it = mProxies.find(target);
	}

	mProxiesNames.insert({sourceInstance, targetInstance});
	
	if (sourceInstanceID != -1) {
		mUnresolvedNamesToInstances.insert({ sourceInstance, sourceInstanceID });
		mProxies.insert({ sourceInstanceID, targetInstanceID });
	}

	return true;
}

bool ObjectManager::isProxied(const zSTRING& instanceName) const
{
	return mProxiesNames.find(instanceName.ToChar()) != mProxiesNames.end();
}

int ObjectManager::getUnProxiedInstanceID(const zSTRING& instanceName) const
{
	if (!isProxied(instanceName)) return zCParser::GetParser()->GetIndex(instanceName);
	return mUnresolvedNamesToInstances.at(instanceName.ToChar());
}

void ObjectManager::removeProxy(const zSTRING& sourceInstance)
{
	auto it = mUnresolvedNamesToInstances.find(sourceInstance);
	if (it == mUnresolvedNamesToInstances.end()) return;
	auto sourceInstanceID = it->second;

	mProxies.erase(sourceInstanceID);
	mProxiesNames.erase(sourceInstance);
	mUnresolvedNamesToInstances.erase(sourceInstance);
}

int ObjectManager::resolveProxying(int instanceID) const
{
	int resolvedInstanceID = instanceID;
	auto it = mProxies.find(instanceID);
	while (it != mProxies.end()) {
		auto target = it->second;
		resolvedInstanceID = target;
		it = mProxies.find(target);
	}

	return resolvedInstanceID;
}

const zSTRING& ObjectManager::resolveProxying(const zSTRING& symbolName) const
{
	const auto* resolvedName = &symbolName;
	auto it = mProxiesNames.find(resolvedName);
	while (it != mProxiesNames.end()) {
		const auto& target = it->second;
		resolvedName =& target;
		it = mProxiesNames.find(target);
	}

	return *resolvedName;
}


zCListSort<oCItem>* ObjectManager::getInvItemByInstanceId(oCNpcInventory* inventory, int instanceIdParserSymbolIndex)
{
	inventory->UnpackCategory();
	ObjectManager* manager = ObjectManager::getObjectManager();
	zCListSort<oCItem>* list = reinterpret_cast<zCListSort<oCItem>*>(inventory->inventory_data);
	while (list != NULL) {
		oCItem* item = list->GetData();
		if (item != NULL && manager->getInstanceId(*item) == instanceIdParserSymbolIndex)
		{
			return list;
		}
		list = list->GetNext();
	}

	return NULL;
}

int ObjectManager::getSlotNumber(oCNpcInventory* inventory, oCItem* item)
{
	int itemNumber = inventory->GetNumItemsInCategory();
	for (int i = 0; i < itemNumber; ++i)
	{
		oCItem* slotItem = inventory->GetItem(i);
		if (slotItem == item)
		{
			return i;
		}
	}

	//item isn't in the inventory
	return -1;
}

oCItem* ObjectManager::searchItemInInvbyInstanzValue(oCNpcInventory* inventory, int searchValue)
{
	inventory->UnpackCategory();
	int itemNumber = inventory->GetNumItemsInCategory();
	for (int i = 0; i < itemNumber; ++i)
	{
		oCItem* slotItem = inventory->GetItem(i);
		if (slotItem->instanz == searchValue)
		{
			return slotItem;
		}
	}

	//no item was found
	return NULL;
}


g2ext_extended::zCPar_SymbolTable* ObjectManager::zCParserGetSymbolTable(void* parser)
{
	BYTE* pointer = ((BYTE*)parser) + 0x10;
	return (g2ext_extended::zCPar_SymbolTable*)pointer;
}


ObjectManager* ObjectManager::getObjectManager() {
	return mInstance.get();
}

void ObjectManager::release()
{
	mInstance.reset();
}


int ObjectManager::createNewInstanceId(oCItem* item, const zSTRING& instanceName) {
	if (item == NULL) {
		mLogStream << "ObjectManager::createNewInstanceId: item is null!" << std::endl;
		util::logWarning(&mLogStream);
		return 0;
	}

	//Create new zCPar_Symbol for the newly created instance and register it to gothic.
	zCParser* parser = zCParser::GetParser();

	int prototypeInstanceParserSymbolID = getInstanceId(*item);
	zCPar_Symbol* prototypeSymbol = parser->GetSymbol(prototypeInstanceParserSymbolID);


	if (prototypeSymbol == NULL) {
		mLogStream << "ObjectManager::createNewInstanceId: item parser symbol is null!" << std::endl;
		util::logWarning(&mLogStream);
		return 0;
	}



	int* indexCount = getParserInstanceCount();
	int instanceParserSymbolID = *indexCount;

	if (!createNewInstanceWithoutExistingId(item, instanceParserSymbolID, instanceName)) {
		mLogStream << "ObjectManager::createNewInstanceId: Couldn't create new instance '" << instanceName.ToChar() << "'" << endl;
		return 0;
	}

	return instanceParserSymbolID;
};

void ObjectManager::deleteDII(int parserSymbolIndex)
{

	//Not supported for now;
	return;
	if (!isDynamicInstance(parserSymbolIndex)) {
		throw std::invalid_argument("ObjectManager::deleteDII: parserSymbolIndex isn't referring to a valid DII!");
	}

	//For now we leave the symbol as it is
	// make the symbol unfindable 
	auto* parser = zCParser::GetParser();
	auto* symbol = parser->GetSymbol(parserSymbolIndex);
	
	//Check that the symbol cannot be found anymore
	//auto* testSymbol = parser->GetSymbol(parserSymbolIndex);

	const auto& name = symbol->name;

	auto* dii = mNewInstanceMap[parserSymbolIndex].get();
	dii->setDoNotStore(true);

	// remove the DII
	auto deleteCount = mNewInstanceMap.erase(parserSymbolIndex);
	mNewInstanceToSymbolMap.erase(parserSymbolIndex);
	mNameToSymbolMap.erase(name);
	mNameToInstanceMap.erase(name);

	// Note: We expect that exactly one entry was deleted.
	if (deleteCount != 1) {
		mLogStream << "ObjectManager::deleteDII: Assertion error: deleteCount is '" << deleteCount << "' but we expected 1" << endl;
		util::logFatal(&mLogStream);
	}
}

void ObjectManager::registerInstance(int instanceIdParserSymbolIndex, std::unique_ptr<DynInstance> item) {
	// allow no reassignments
	auto it = mNewInstanceMap.find(instanceIdParserSymbolIndex);
	if (it != mNewInstanceMap.end())
	{
		mLogStream << "Instance id already exists! Nothing will be created." << endl;
		util::debug(&mLogStream, Logger::Warning);
		return;
	}

	mNewInstanceMap.insert(std::make_pair<>(instanceIdParserSymbolIndex, std::move(item)));
	
}

void ObjectManager::releaseInstances() {
	//release all allocated parser symbols and update parser symbol table
	int allocatedSize = mNewInstanceMap.size();
	int* symTableSize = getParserInstanceCount();
	*symTableSize = *symTableSize - allocatedSize;
	mLogStream << "ObjectManager::releaseInstances(): allocatedSize = " << allocatedSize << endl;
	util::logAlways(&mLogStream);
	mLogStream << "ObjectManager::releaseInstances(): symTableSize = " << *symTableSize << endl;
	util::logAlways(&mLogStream);

	// clear all data structures
	mNewInstanceMap.clear();
	mNewInstanceToSymbolMap.clear();
	mNameToInstanceMap.clear();
	mNameToSymbolMap.clear();

	mProxiesNames.clear();
	mProxies.clear();
	mUnresolvedNamesToInstances.clear();
};

bool ObjectManager::assignInstanceId(oCItem* item, int instanceIdParserSymbolIndex){
	auto it = mNewInstanceMap.find(instanceIdParserSymbolIndex);
	if (it == mNewInstanceMap.end())
	{
		mLogStream<< "ObjectManager::assignInstanceId: instance id wasn't found: " << instanceIdParserSymbolIndex << std::endl;
		util::debug(&mLogStream, Logger::Warning);
		return false;
	}
	
	setDynInstanceId(item, instanceIdParserSymbolIndex);

	if(!initByNewInstanceId(item))
	{
		mLogStream << "ObjectManager::assignInstanceId: Item Initialisation failed!" << std::endl;
		util::debug(&mLogStream, Logger::Warning);
		return false;
	};
	return true;
}

void ObjectManager::oCItemOperatorDelete(oCItem* item)
{
	XCALL(0x007144A0);
}

bool ObjectManager::assignInstanceId2(oCItem* item, int instanceIdParserSymbolIndex)
{
	//TODO: get rid of the copy item!!!
	oCItem* copy = oCObjectFactory::GetFactory()->CreateItem(instanceIdParserSymbolIndex);

	if (!copy) return false;
	
	item->idx = copy->idx;
	item->name = copy->name;
	item->nameID = copy->nameID;
	item->hp = copy->hp;
	item->hp_max = copy->hp_max;
	item->mainflags = copy->mainflags;
	item->flags = copy->flags;
	item->weight = copy->weight;
	item->value = copy->value;

	// -- weapons		
	item->damageType = copy->damageType;
	item->damageTotal = copy->damageTotal;

	item->damage[0] = copy->damage[0];
	item->damage[1] = copy->damage[1];
	item->damage[2] = copy->damage[2];
	item->damage[3] = copy->damage[3];
	item->damage[4] = copy->damage[4];
	item->damage[5] = copy->damage[5];
	item->damage[6] = copy->damage[6];
	item->damage[7] = copy->damage[7];

	// -- armor 
	item->wear = copy->wear;
	item->protection[0] = copy->protection[0];
	item->protection[1] = copy->protection[1];
	item->protection[2] = copy->protection[2];
	item->protection[3] = copy->protection[3];
	item->protection[4] = copy->protection[4];
	item->protection[5] = copy->protection[5];
	item->protection[6] = copy->protection[6];
	item->protection[7] = copy->protection[7];

	// -- food
	item->nutrition = copy->nutrition;

	// -- use attributes
	item->cond_atr[0] = copy->cond_atr[0];
	item->cond_atr[1] = copy->cond_atr[1];
	item->cond_atr[2] = copy->cond_atr[2];

	item->cond_value[0] = copy->cond_value[0];
	item->cond_value[1] = copy->cond_value[1];
	item->cond_value[2] = copy->cond_value[2];

	// -- attributes that will be changed on equip
	item->change_atr[0] = copy->change_atr[0];
	item->change_atr[1] = copy->change_atr[1];
	item->change_atr[2] = copy->change_atr[2];

	item->change_value[0] = copy->change_value[0];
	item->change_value[1] = copy->change_value[1];
	item->change_value[2] = copy->change_value[2];

	// -- parser functions
	item->magic = copy->magic;
	item->on_equip = copy->on_equip;
	item->on_unequip = copy->on_unequip;

	item->on_state[0] = copy->on_state[0];
	item->on_state[1] = copy->on_state[1];
	item->on_state[2] = copy->on_state[2];
	item->on_state[3] = copy->on_state[3];

	// -- owner									
	item->owner = copy->owner;			//	owner: npc instance
	item->ownerGuild = copy->ownerGuild;		//	owner: guild
	item->disguiseGuild = copy->disguiseGuild;

	// -- visual
	item->visual = copy->visual;

	// -- change of mesh on equip
	item->visual_change = copy->visual_change;	//	ASC file
	item->effect = copy->effect;			//  Effect instance

	item->visual_skin = copy->visual_skin;

	item->scemeName = copy->scemeName;
	item->material = copy->material;
	item->munition = copy->munition;		//	Instance of ammunition

	item->spell = copy->spell;
	item->range = copy->range;

	item->mag_circle = copy->mag_circle;

	item->description = copy->description;
	item->text[0] = copy->text[0];
	item->text[1] = copy->text[1];
	item->text[2] = copy->text[2];
	item->text[3] = copy->text[3];
	item->text[4] = copy->text[4];
	item->text[5] = copy->text[5];

	item->count[0] = copy->count[0];
	item->count[1] = copy->count[1];
	item->count[2] = copy->count[2];
	item->count[3] = copy->count[3];
	item->count[4] = copy->count[4];
	item->count[5] = copy->count[5];

	// -- inventory presentation
	item->inv_zbias = copy->inv_zbias;								//  far plane (how far the item goes into the room by the z-axis)
	item->inv_rotx = copy->inv_rotx;								//  rotation around x-axis (in degrees)
	item->inv_roty = copy->inv_roty;								//  rotation around y-axis (in degrees)
	item->inv_rotz = copy->inv_rotz;								//  rotation around z-axis (in degrees)
	item->inv_animate = copy->inv_animate;							//  rotate the item
																// Skip instanz field!!!
																//item->instanz=instanz;						//int Symbolindex
	item->c_manipulation = copy->c_manipulation;					//int ?
	item->last_manipulation = copy->last_manipulation;				//zREAL ?
	item->magic_value = copy->magic_value;					//int ?
	
	//item->effectVob = effectVob;						//oCVisualFX*
	item->next = copy->next;
	int address = reinterpret_cast<int>(item);
	address += 0x330;
	int* instance = reinterpret_cast<int*>(address);

	//Get current symbol index and set it as the item's instance id
	*instance = instanceIdParserSymbolIndex;

	//insert effect finally
	//oCItemSaveRemoveEffect(item);
	//oCItemSaveInsertEffect(item);

	//TODO: Verify!!!
	DII::DII_DeleteItem(copy);

	return true;
};


bool ObjectManager::initByNewInstanceId(oCItem* item) {
	int instanceId = getDynInstanceId(item);
	if (instanceId == NULL) return false;
	DynInstance* instanceItem = getInstanceItem(instanceId);
	instanceItem->init(item, getDynInstanceId(item));
	return true;
};

/*
* @Return: The new instance created for the given item. If no instance was created previously,
NULL will be returned.
*/
int ObjectManager::getDynInstanceId(oCItem* item){
	if (item == NULL) return NULL;
	int instanceIdParserSymbolIndex = getInstanceId(*item);

	auto it = mNewInstanceMap.find(instanceIdParserSymbolIndex);
	if (it == mNewInstanceMap.end()){return NULL;}
	return instanceIdParserSymbolIndex;
}

void ObjectManager::setDynInstanceId(oCItem* item, int instanceIdParserSymbolIndex){

	auto it = mNewInstanceMap.find(instanceIdParserSymbolIndex);
	if (it != mNewInstanceMap.end())
	{
		setInstanceId(item, instanceIdParserSymbolIndex);
	} else
	{
		mLogStream << "ObjectManager::setInstanceId: parameter id has no assigned index. Nothing will be done." << endl;
		util::debug(&mLogStream, Logger::Warning);
	}
};

DynInstance* ObjectManager::getInstanceItem(int instanceIdParserSymbolIndex){

	auto it = mNewInstanceMap.find(instanceIdParserSymbolIndex);
	if (it == mNewInstanceMap.end())
	{
		return NULL;		
	}
	return (*it).second.get();
}; 


void ObjectManager::saveNewInstances(char* directoryPath, char* filename) {
	std::string dir (directoryPath);
	if (!util::existsDir(dir)) {
		if(!util::makePath(dir)) {
			return;
		}
	}

	string fullpath = dir + string(filename);
	ofstream ofs(const_cast<char*>(fullpath.c_str()));

	try {
		ofs.exceptions(std::ios::failbit | std::ios::badbit);

		std::vector<DynInstance*> instances;
		for (auto& pair : mNewInstanceMap) {
			auto* instance = pair.second.get();
			
			if (!instance->getDoNotStore())
				instances.push_back(instance);
		}

		// Save instance items
		ofs << instances.size() << '\n';
		for (auto* instance : instances) {
			instance->serialize(ofs);
			//oa << storeItem;
			ofs << '\n';
		}

		// Save proxies
		auto* parser = zCParser::GetParser();
		ofs << mProxiesNames.size() << '\n';
		for (auto& pair : mProxiesNames) {
			util::writezSTRING(ofs, pair.first);
			ofs << ' ';
			util::writezSTRING(ofs, pair.second);
			ofs << '\n';
		}
	}
	catch (const std::exception & e) {
		mLogStream << "exception msg: " << e.what() << std::endl;
		util::logAlways(&mLogStream);

		mLogStream << __FUNCSIG__ << ": Couldn't process " << fullpath << std::endl;
		util::logFatal(&mLogStream);
	}
}


void ObjectManager::loadNewInstances(char* filename) {
	ifstream ifs(filename);
	if (ifs.fail()) {
		return;
	}

	try {

		ifs.exceptions(std::ios::failbit | std::ios::badbit);

		// archive and stream closed when destructors are called
		size_t size = 0;
		std::stringstream ss;
		std::string line, token;
		getline(ifs, line);
		ss << line;
		getline(ss, token);
		size = atoi(token.c_str());

		for (size_t i = 0; i != size; ++i) {
			ss.str("");
			ss.clear();
			getline(ifs, line);
			ss << line;
			auto instance = std::make_unique<DynInstance>();

			//ifs >> storeItem;
			instance->deserialize(&ss);
			//DynInstance* item = new DynInstance(*storeItem);

			ParserInfo info;
			info.newSymbolName = instance->getSymbolName().ToChar();
			info.oldSymbolName = instance->getPrototypeSymbolName().ToChar();
			info.bitfield = instance->getParserSymbolBitfield();
			info.container = instance.get();

			auto id = createParserSymbol(info);
			registerInstance(id, std::move(instance));
		}


		auto* parser = zCParser::GetParser();
		size_t proxiesSize = 0;
		getline(ifs, line);
		ss.str("");
		ss.clear();
		ss << line;
		getline(ss, token);
		proxiesSize = atoi(token.c_str());

		for (size_t i = 0; i != proxiesSize; ++i) {
			ss.str("");
			ss.clear();
			getline(ifs, line);
			ss << line;
			std::string sourceInstanceName;
			std::string targetInstanceName;
			util::readAndTrim(&ss, sourceInstanceName);
			util::readAndTrim(&ss, targetInstanceName);


			zSTRING source(sourceInstanceName.c_str());
			zSTRING target(targetInstanceName.c_str());

			auto* symbol = createNewInstanceSymbol(parser->GetIndex(source), parser->GetSymbol(target), sourceInstanceName.c_str());
			if (symbol) {
				addSymbolToSymbolTable(symbol);
			}
			if (!addProxy(source, target)) {
				mLogStream << __FUNCSIG__ << ": Couldn't load proxy (" << sourceInstanceName << ", " << targetInstanceName << ")" << std::endl;
				util::logFatal(&mLogStream);
			}
		}
	}
	catch (const std::exception& e) {
		mLogStream << "exception msg: " << e.what() << std::endl;
		util::logAlways(&mLogStream);

		mLogStream << __FUNCSIG__ << ": Couldn't process " << filename << std::endl;
		util::logFatal(&mLogStream);
	}

};


void ObjectManager::oCItemInitByScript(oCItem* item, int inst, int second)
{
	XCALL(0x00711BD0);
}

list<oCMobContainer*> ObjectManager::getMobContainers() {
	list<oCMobContainer*> containerList;
	zCWorld* world = oCGame::GetGame()->GetWorld();
	zCListSort<zCVob>* vobList = world->GetVobList();
	auto dummy = std::make_unique<oCMobContainer>();

	while(vobList != NULL) {
		zCVob* vob = vobList->GetData();
		vobList = vobList->GetNext();
		if (vob == NULL){
			continue;
		}

		if (strcmp(typeid(*vob).name(), typeid(*dummy).name()) == 0) {
			containerList.push_back(dynamic_cast<oCMobContainer*>(vob));
		}
	}

	return containerList;
};

static void test (void* obj, void* param, oCItem* itm) {
	if (itm == NULL) return;
	ObjectManager* manager = (ObjectManager*)obj;
	set<int>* usedIds = (set<int>*)param;
	int instanceId = manager->getInstanceId(*itm);
	if (manager->isDynamicInstance(instanceId))
	{
		usedIds->insert(instanceId);
	}
}

bool ObjectManager::createNewInstanceWithoutExistingId(oCItem* item, int instanceParserSymbolID, const zSTRING& instanceName)
{
	zCParser* parser = zCParser::GetParser();
	int parentId = getInstanceId(*item);
	zCPar_Symbol* old = parser->GetSymbol(parentId);

	if (old == NULL) {
		std::stringstream mLogStream;
		mLogStream << "ObjectManager::createNewInstanceWithoutExistingId: Couldn't create new instance since parent instance symbol couldn't be found! instanceName = " << instanceName.ToChar() << endl;
		return false;
	}

	int* indexCount = getParserInstanceCount();
	zCPar_Symbol* symbol = createNewInstanceSymbol(instanceParserSymbolID, old, instanceName);

	if (!symbol) {
		std::stringstream mLogStream;
		mLogStream << "ObjectManager::createNewInstanceWithoutExistingId: Couldn't create symbol '" << instanceName.ToChar() << "'" << endl;
		return false;
	}

	if (!addSymbolToSymbolTable(symbol))
	{
		if (*indexCount < instanceParserSymbolID) *indexCount = instanceParserSymbolID;
		if (*indexCount == instanceParserSymbolID) *indexCount += 1;
	};

	DynInstance* instanceItem = createNewInstanceItem(instanceParserSymbolID);
	if (isAssignedToDII(item))
	{
		/*DynInstance* oldStoreItem = getInstanceItem(parentId);
		instanceItem->copyUserData(*oldStoreItem);
		if (instanceBegin > parentId)
		{
			instanceBegin = parentId;
		}*/
	}
	
	const auto& symbolName = symbol->name;
	
	
	instanceItem->store(*item);
	setPrototypeSymbolName(instanceParserSymbolID, old->name.ToChar());

	instanceItem->setParserSymbolBitfield(symbol->bitfield);
	instanceItem->setSymbolName(symbolName.ToChar());

	
	
	mNewInstanceToSymbolMap.insert(pair<int, zCPar_Symbol*>(instanceParserSymbolID, symbol));


	
	
	//transform(symbolName.begin(), symbolName.end(), symbolName.begin(), ::toupper);
	mNameToSymbolMap.insert(pair<zSTRING, zCPar_Symbol*>(symbolName, symbol));
	mNameToInstanceMap.insert(pair<zSTRING, int>(symbolName, instanceParserSymbolID));

	mLogStream << "ObjectManager::createNewInstanceWithoutExistingId(): indexCount = " << *indexCount << endl;
	util::debug(&mLogStream);
	mLogStream << "ObjectManager::createNewInstanceWithoutExistingId(): parser symbol index = " << instanceParserSymbolID << endl;
	util::debug(&mLogStream);

	return true;
}

zCPar_Symbol* ObjectManager::createNewInstanceSymbol(const ParserInfo* old)
{
	zCParser* parser = zCParser::GetParser();
	zCPar_Symbol* ref = parser->GetSymbol(old->oldSymbolName);
	g2ext_extended::zCPar_SymbolTable* currSymbolTable = zCParserGetSymbolTable(parser);
	zCPar_Symbol* result = currSymbolTable->GetSymbol(old->newSymbolName);
	if (result) { return result; }

	zCPar_Symbol* parent = parser->GetSymbol("C_ITEM");

	result = zCPar_Symbol::create();
	result->parent = ref->parent;
	result->bitfield = ref->bitfield;
	result->name = old->newSymbolName;

	// ensure upper case
	result->name = result->name.Upper();

	result->offset = 0;
	result->next = ref->next;
	result->content.data_ptr = 0;//ref->content.data_ptr;
	result->filenr = ref->filenr;
	result->line = ref->line;
	result->line_anz = ref->line_anz;
	result->pos_beg = ref->pos_beg;
	result->pos_anz = ref->pos_anz;

	return result;
}

bool ObjectManager::addSymbolToSymbolTable(zCPar_Symbol* symbol)
{
	zCParser* parser = zCParser::GetParser();
	int* indexCount = getParserInstanceCount();
	int countBefore = *indexCount;
	g2ext_extended::zCPar_SymbolTable* symbolTable = zCParserGetSymbolTable(parser);
	symbolTable->Insert(symbol);
	mLogStream << "ObjectManager::addSymbolToSymbolTable(): Name = " << symbol->name.ToChar() << endl;
	mLogStream << "ObjectManager::addSymbolToSymbolTable(): Index = " << parser->GetIndex(symbol->name) << endl;
	mLogStream << "ObjectManager::addSymbolToSymbolTable(): countBefore = " << countBefore << endl;
	mLogStream << "ObjectManager::addSymbolToSymbolTable(): index count = " << *indexCount << endl;
	util::debug(&mLogStream);

	// Some Ikarus functions need the correct length of the current symbol table.
	updateIkarusSymbols();

	return countBefore != *indexCount;
}


DynInstance* ObjectManager::createNewInstanceItem(int instanceIdParserSymbolIndex)
{
	DynInstance* result = getInstanceItem(instanceIdParserSymbolIndex);
	if (!result)
	{
		result = new DynInstance();
		mNewInstanceMap.insert(pair<int, DynInstance*>(instanceIdParserSymbolIndex, result));
	}

	return result;
};

void ObjectManager::updateContainerItem(const ObjectManager::ParserInfo* info)
{
	DynInstance* item = info->container;

	zCParser* parser = zCParser::GetParser();
	int newIndex = parser->GetIndex(info->newSymbolName);
	item->setSymbolName(info->newSymbolName);
	item->setParserSymbolBitfield(info->bitfield);
}

void ObjectManager::logSymbolData(zCPar_Symbol* sym)
{
	mLogStream << sym->name.ToChar() << endl;
	util::logInfo(&mLogStream);
	mLogStream << sym->parent->name.ToChar() << endl;
	util::logInfo(&mLogStream);
	mLogStream<< sym->bitfield << endl;
	util::logInfo(&mLogStream);
	mLogStream << sym->filenr << endl;
	util::logInfo(&mLogStream);
	mLogStream << sym->line << endl;
	util::logInfo(&mLogStream);
	mLogStream << sym->line_anz << endl;
	util::logInfo(&mLogStream);
	mLogStream << sym->next << endl;
	util::logInfo(&mLogStream);
	mLogStream << sym->offset << endl;
	util::logInfo(&mLogStream);
	mLogStream << sym->pos_beg << endl;
	util::logInfo(&mLogStream);
	mLogStream << sym->pos_anz << endl;
	util::logInfo(&mLogStream);
};

int ObjectManager::createParserSymbol(const ParserInfo& info)
{
	zCPar_Symbol* symbol = createNewInstanceSymbol(&info);
	zCParser* parser = zCParser::GetParser();
	if (!addSymbolToSymbolTable(symbol))
	{
		mLogStream << "ObjectManager::createParserSymbols: Adding wasn't successful: " << symbol->name.ToChar() << endl;
		g2ext_extended::zCPar_SymbolTable* currSymbolTable = zCParserGetSymbolTable(parser);
		int index = currSymbolTable->GetIndex(info.newSymbolName);
		int* symTableSize = getParserInstanceCount();
		mLogStream << "index: " << index << endl;
		mLogStream << "symTableSize: " << *symTableSize << endl;
		util::debug(&mLogStream, Logger::Warning);
		if (index >= *symTableSize)
		{
			*symTableSize = index + 1;
			updateIkarusSymbols();
			mLogStream << "ObjectManager::createParserSymbols(): resized symbol table. symTableSize = " << *symTableSize << endl;
			util::logInfo(&mLogStream);
		}
	}

	int newInstanceId = parser->GetIndex(symbol->name);

	updateContainerItem(&info);
	mNewInstanceToSymbolMap.insert(pair<int, zCPar_Symbol*>(newInstanceId, symbol));
	const auto& symbolName = symbol->name;
	mNameToSymbolMap.insert(pair<zSTRING, zCPar_Symbol*>(symbolName, symbol));
	mNameToInstanceMap.insert(pair<zSTRING, int>(symbolName, newInstanceId));

	return newInstanceId;
}

zCPar_Symbol* ObjectManager::createNewInstanceSymbol(int instanceParserSymbolID, zCPar_Symbol* prototype, const zSTRING& source) const
{

	zSTRING name = source;
	name = name.Upper();

	zCPar_Symbol* symbol;
	zCParser* parser = zCParser::GetParser();
	//zCPar_Symbol* ref = parser->GetSymbol(parser->GetIndex("ItMw_1h_vlk_dagger"));

	symbol = parser->GetSymbol(instanceParserSymbolID);

	// Symbol already exists?
	if (symbol) {
		stringstream mLogStream;
		mLogStream << "ObjectManager::createNewSymbol: Symbol for instance id '" << instanceParserSymbolID << "' already exists!" << endl;
		util::logWarning(&mLogStream);
		return nullptr;
	}

	//We have to check that the symbol name doesn't refer to an existing symbol
	if (parser->GetSymbol(name)) {
		stringstream mLogStream;
		mLogStream << "ObjectManager::createNewSymbol: Symbol with name '" << name.ToChar() << "' already exists!" << endl;
		util::logWarning(&mLogStream);
		return nullptr;
	}

	symbol = zCPar_Symbol::create();

	symbol->name = name;

	if (prototype) {
		symbol->parent = prototype->parent;
		symbol->bitfield = prototype->bitfield;
	}
	else {
		symbol->parent = nullptr;
		symbol->bitfield = 0;
	}
	symbol->offset = 0;
	symbol->content.data_ptr = 0;

	return symbol;
};



void ObjectManager::setInstanceId(oCItem* item, int instanceParserSymbolID) {
	int address = reinterpret_cast<int>(item);
	address += 0x330;
	int* instance = reinterpret_cast<int*>(address);
	*instance = instanceParserSymbolID;

}

/*
* Provides the instance id, the given item currently as.
*/
int ObjectManager::getInstanceId(oCItem& item) {
	int address = reinterpret_cast<int>(&item);
	address += 0x330;
	int* instance = reinterpret_cast<int*>(address);
	return *instance;

}


void ObjectManager::setPrototypeSymbolName(int instanceParserSymbolID, const zSTRING& protoInstanceSymbolName) {
	auto instanceIT = mNewInstanceMap.find(instanceParserSymbolID);
	if (instanceIT == mNewInstanceMap.end()) {
		std::stringstream mLogStream;
		mLogStream << "ObjectManager::setParentInstanceId: instanceID not registered: " << instanceParserSymbolID << endl;
		util::logFatal(&mLogStream);
	}

	auto& instanceItem = instanceIT->second;
	instanceItem->setPrototypeSymbolName(protoInstanceSymbolName);
}

/*
*	Provides the instance id the unmodified item had at the time at which 
*	the new instance id instanceId was created for a specific item.
*/
const zSTRING& ObjectManager::getPrototypeSymbolName(int instanceParserSymbolID) {

	auto it = mNewInstanceMap.find(instanceParserSymbolID);
	static zSTRING empty = "";
	if (it == mNewInstanceMap.end()) return empty;
	auto& instanceItem = it->second;
	return instanceItem->getPrototypeSymbolName();
}


std::string ObjectManager::getSaveGameDirectoryPath(int saveGameSlotNumber) {
	zCOption* option = zCOption::GetOption();
	zSTRING& gothicDirPath = option->GetDirString(static_cast<zTOptionPaths>(24));
	zSTRING& saveGamesDirName = option->GetDirString(static_cast<zTOptionPaths>(2));

	std::stringstream ss;
	//write full path to the savegame
	// write right save game folder 
	if (saveGameSlotNumber == 0) //quicksave folder
	{
		ss << gothicDirPath.ToChar() << saveGamesDirName.ToChar() << "quicksave\\";
	} else
	{
		ss << gothicDirPath.ToChar() << saveGamesDirName.ToChar() << "savegame" << saveGameSlotNumber << "\\";
	}
	return ss.str();
};

std::string ObjectManager::getCurrentDirectoryPath() {
	zCOption* option = zCOption::GetOption();
	zSTRING& gothicDirPath = option->GetDirString(static_cast<zTOptionPaths>(24));
	zSTRING& saveGamesDirName = option->GetDirString(static_cast<zTOptionPaths>(2));

	std::stringstream ss;
	//write full path to the savegame
	ss << gothicDirPath.ToChar() << saveGamesDirName.ToChar() << "current\\";
	return ss.str();
};

bool ObjectManager::InitItemWithDynInstance(oCItem* item, int index)
{
	// only init item if an instance id exists
	if (index == NULL)
	{
		mLogStream << "ObjectManager::InitItemWithDynInstance: instanceId wasn't found!" << std::endl;
		util::logWarning(&mLogStream);
		return false;
	}
	return assignInstanceId(item, index);
}

bool ObjectManager::isAssignedToDII(oCItem* item)
{
	int index = getDynInstanceId(item);
	return isDynamicInstance(index);
}

bool ObjectManager::isDynamicInstance(int instanceParserSymbolID)
{
	auto it = mNewInstanceMap.find(instanceParserSymbolID);
	return it != mNewInstanceMap.end();
}

zCPar_Symbol* ObjectManager::getSymbolByIndex(int parserSymbolID)
{
	auto it = mNewInstanceToSymbolMap.find(parserSymbolID);
	if (it == mNewInstanceToSymbolMap.end()) { return NULL; }
	return it->second;
}

zCPar_Symbol* ObjectManager::getSymbolByName(const zSTRING& symbolName)
{
	//auto name = symbolName;
	//name = name.Upper();
	auto it = mNameToSymbolMap.find(symbolName);
	if (it == mNameToSymbolMap.end()) { return NULL; }
	return it->second;
}


int ObjectManager::getIndexByName(const zSTRING& symbolName)
{
	//zSTRING name = symbolName;
	//name = name.Upper();
	auto it = mNameToInstanceMap.find(symbolName);
	if (it == mNameToInstanceMap.end()) { return -1; }
	return it->second;
}

void ObjectManager::updateIkarusSymbols()
{
	mLogStream << "ObjectManager::updateIkarusSymbols: update Ikarus symbols..." << endl;
	util::logInfo(&mLogStream);

	// Some Ikarus functions need the correct length of the current symbol table.
	// MEM_Reinit_Parser() updates all involved references.
		zSTRING arg("MEM_ReinitParser");

	//CallFunc needs uppercase string
	zCParser::GetParser()->CallFunc(arg.Upper());
}

void ObjectManager::callForAllItems(bool(*func)(void* obj, void* param, oCItem*), void* obj, void* param)
{
	callForAllContainerItems(func, obj, param);
	callForAllWorldItems(func, obj, param);
	callForAllNpcItems(func, obj, param);
}


void ObjectManager::callForAllNpcItems(bool(*func)(void* obj, void* param, oCItem*), void* obj, void* param)
{
	zCWorld* world = oCGame::GetGame()->GetWorld();
	zCListSort<oCNpc>* npcList = world->GetNpcList();

	const auto slotCount = SlotInfo::getSlotCount();

	while (npcList != NULL) {
		oCNpc* npc = npcList->GetData();

		if (npc == NULL) {
			npcList = npcList->GetNext();
			continue;
		}

		//if (std::string(npc->name->ToChar()) == "Ich") {
		//	bool test = true;
		//}

		oCNpcInventory* inventory = npc->GetInventory();
		if (inventory == NULL) {
			npcList = npcList->GetNext();
			continue;
		}


		inventory->UnpackAllItems();
		auto* inv = &inventory->inv;

		std::list<oCItem*> items = util::create(inv->contents);
		npcList = npcList->GetNext();

		for (auto* item : items)
		{
			func(obj, param, item);
		}

		for (int i = 0; i < slotCount; ++i) {
			auto& slotName = SlotInfo::getSlotName(i);
			auto* vob = oCNpcGetSlotVob(npc, slotName);
			
			if (vob && vob->type == VOB_TYPE_ITEM) {
				oCItem* itm = (oCItem*)vob;
				//zCWorld* world = oCGame::GetGame()->GetWorld();
				//world->AddVob(itm);
				//oCItemSaveRemoveEffect(itm);
				if (func(obj, param, itm)) {
					//oCItemSaveRemoveEffect(itm);
					//oCNpcPutInSlot(npc, slotName, nullptr, 1);
					//oCItemSaveInsertEffect(itm);
					oCItemSaveRemoveEffect(itm);
					oCNpcPutInSlot(npc, slotName, itm, 1);
					oCItemSaveInsertEffect(itm);
				}
			}
			

		}

	}
}

void ObjectManager::callForAllContainerItems(bool(*func)(void *obj, void *param, oCItem *), void * obj, void * param)
{
	auto containerList = getMobContainers();

	for (auto* container : containerList)
	{
		int address = (int)container->containList_next;
		zCListSort<oCItem>* listAddress = reinterpret_cast<zCListSort<oCItem>*>(address);
		zCListSort<oCItem>* list = listAddress;
		std::list<oCItem*> items = util::create(list);
		
		for (auto* item : items)
		{
			func(obj, param, item);
		}
	}
}

void ObjectManager::callForAllWorldItems(bool(*func)(void* obj, void* param, oCItem*), void* obj, void* param)
{
	zCWorld* world = oCGame::GetGame()->GetWorld();
	zCListSort<oCItem>* itemList = world->GetItemList();
	std::list<oCItem*> items = util::create(itemList);

	for (auto* item : items)
	{
		func(obj, param, item);
	}
}

int* ObjectManager::getParserInstanceCount()
{
	zCParser* parser = zCParser::GetParser();
	return (int*)(((BYTE*)parser) + 0x18 + 0x8);
}

bool ObjectManager::isItemInWorld(oCItem* item)
{
	zCWorld* world = oCGame::GetGame()->GetWorld();
	auto* list = world->GetVobList();
	return list->IsInList(item);
}

struct TEST2_PARAMS {
	oCItem* result;
	int instanceID;
};

static bool test2(void* obj, void* param, oCItem* itm) {
	if (itm == NULL) return false;

	TEST2_PARAMS* params = (TEST2_PARAMS*)param;
	if (params->result != NULL) return false;

	if (itm->GetInstance() == params->instanceID)
	{
		params->result = itm;
		//logStream << "ObjectManager::getItemByInstanceId: result found" << endl;
		//util::debug(&logStream);
		return true;
	}

	return false;
}

oCItem* ObjectManager::getItemByInstanceId(int instanceIdParserSymbolIndex)
{
	/*auto func = [&](oCItem* itm) ->void {
		if (itm == NULL) return;
		if (result != NULL) return;
		
		if (itm->GetInstance() == instanceId)
		{
			result = itm;
			logStream << "ObjectManager::getItemByInstanceId: result found" << endl;
			util::debug(&logStream);
		}
	};*/

	TEST2_PARAMS params = {NULL, instanceIdParserSymbolIndex };

	callForAllItems(test2, NULL, &params);
	return params.result;
}

void ObjectManager::oCItemSaveInsertEffect(oCItem* item)
{
	if (item == NULL)
	{
		return;
	}

	//int* ptr = ((int*)item) + 0x340;

	//Has the item already an effect active?
	if (item->effectVob)
	{
		return;
	}

	item->InsertEffect();
}

void ObjectManager::oCItemSaveRemoveEffect(oCItem* item)
{
	if (item == NULL)
	{
		return;
	}

	//int* ptr = ((int*)item) + 0x340;

	//Has the item no effect to remove?
	if (item->effectVob == NULL)
	{
		return;
	}

	item->RemoveEffect();
}

int * ObjectManager::getRefCounter(oCItem * item)
{
	return (int*)((BYTE*)item + 0x4);
}

int SlotInfo::getSlotCount()
{
	auto* symbol = UTIL_GET_SYMBOL_WITH_CHECKS(DII_SLOT_COUNT);
	return symbol->content.data_int;
}

const zSTRING& SlotInfo::getSlotName(int index)
{
	auto* symbol = UTIL_GET_SYMBOL_WITH_CHECKS(DII_SLOTS);
	auto* pSlotName = symbol->content.data_pstring;

	static zSTRING empty;
	if (!pSlotName) {
		return empty;
	}


	return pSlotName[index];
}