#pragma once
#include <base/playerdb.h>
#include <base/log.h>
#include <base/event.h>
#include <base/base.h>
#include <hook.h>
#include <Actor/Actor.h>
#include <Actor/GameMode.h>
#include <Math/BlockPos.h>
#include <Block/BlockSource.h>
#include <Packet/ActorFallPacket.h>
#include <chrono>
#include <Level/Level.h>
#include <Core/Minecraft.h>
#include <mods/Audit.h>
#include <exception>
#include <Packet/InventoryTransactionPacket.h>
#include <Packet/MobEquipmentPacket.h>
#include <Packet/TextPacket.h>
#include <Item/Item.h>
#include <mods/Blacklist.h>
#include <Net/ServerNetworkHandler.h>
#include <Container/SimpleContainer.h>
#include <RakNet/RakPeer.h>
#include "cpprest/containerstream.h"
#include "cpprest/filestream.h"
#include "cpprest/http_client.h"
#include "cpprest/json.h"
#include "cpprest/producerconsumerstream.h"
#include <iostream>
#include <sstream>
#include <clocale>
#include <Container/PlayerInventory.h>
#include <mods/CommandSupport.h>
#include <yaml.h>
#include <string>
#include <Item/ItemDescriptor.h>
#include <Item/Potion.h>
#include <Core/WeakPtr.h>
#include <variant>
#include <Core/ServerInstance.h>

struct Settings {
  bool banGive        = true;
  bool blockSameMsg   = true;
  bool redstoneSlower = false;
  bool blockExpOrb    = true;
  bool setInv         = true;
  bool getInv         = true;
  bool validateName            = true;
  bool banInvalidName          = true;
  bool resetOverEnchantedItems = true;
  int sameMsgTime     = 1;
  int redstoneTick    = 1;
  std::vector<std::string> banitemsS{"325:3"};
  bool noDispenserEjectBannedItems = true;
  std::vector<std::string> banWords;
  std::string banItem             = "This item is banned!";
  std::string stopSpam            = "Stop spamming!";
  std::string banWord             = "Word is banned!";
  std::string kickBreakingTooFast = "You are breaking too fast!";
  std::string kickGive            = "Give exploit!";
  std::string banString           = "You are banned: ";

  template <typename IO> static inline bool io(IO f, Settings &settings, YAML::Node &node) {
    return f(settings.banGive, node["banGive"]) && f(settings.blockSameMsg, node["blockSameMsg"]) &&
           f(settings.sameMsgTime, node["sameMsgTime"]) && f(settings.banitemsS, node["banItems"]) &&
           f(settings.banWords, node["banWords"]) && f(settings.banItem, node["banItem"]) &&
           f(settings.stopSpam, node["stopSpam"]) && f(settings.banWord, node["banWord"]) &&
           f(settings.kickBreakingTooFast, node["kickBreakingTooFast"]) && f(settings.kickGive, node["kickGive"]) &&
           f(settings.redstoneSlower, node["redstoneSlower"]) && f(settings.redstoneTick, node["redstoneTick"]) &&
           f(settings.banString, node["banString"]) && f(settings.blockExpOrb, node["blockExpOrb"]) &&
           f(settings.setInv, node["setInv"]) && f(settings.getInv, node["getInv"]) &&
           f(settings.validateName, node["validateName"]) && f(settings.banInvalidName, node["banInvalidName"]) &&
           f(settings.resetOverEnchantedItems, node["resetOverEnchantedItems"]) &&
           f(settings.noDispenserEjectBannedItems, node["noDispenserEjectBannedItems"]);
  }
};

extern Settings settings;
extern void initCommand(CommandRegistry *registry);

template <typename T, int off> inline T &dAccess(void *ptr) { return *(T *) (((uintptr_t) ptr) + off); }
template <typename T, int off> inline T const &dAccess(void const *ptr) { return *(T *) (((uintptr_t) ptr) + off); }
template <typename T> inline T &dAccess(void *ptr, uintptr_t off) { return *(T *) (((uintptr_t) ptr) + off); }
template <typename T> inline const T &dAccess(void const *ptr, uintptr_t off) {
  return *(T *) (((uintptr_t) ptr) + off);
}
