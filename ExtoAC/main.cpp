#include "global.h"
#include <dllentry.h>

Settings settings;
std::unordered_map<uint64_t, long long> lastBreak;
std::unordered_map<uint64_t, long long> multiBreak;
std::unordered_map<uint64_t, std::string> lastMessage;
std::unordered_map<uint64_t, long long> lastMessageTime;
std::unordered_map<short, std::vector<short>> banitems;

class ServerLevel;

std::unordered_map<unsigned long long, bool> ignore_packets;

DEF_LOGGER("EAC");
DEFAULT_SETTINGS(settings);

void dllenter() { Mod::CommandSupport::GetInstance().AddListener(SIG("loaded"), initCommand); }
void dllexit() {}

void PreInit() {
  for (std::string &s : settings.banitemsS) {
    size_t pos = s.find(":");
    short id   = std::stoi(s.substr(0, pos));
    short aux  = std::stoi(s.erase(0, pos + 1));
    banitems[id].push_back(aux);
  }
  Mod::PlayerDatabase::GetInstance().AddListener(SIG("joined"), [](Mod::PlayerEntry const &entry) {
    if (!settings.validateName || direct_access<int>(entry.player, 2104) == 11) return;
    auto &cert = entry.player->getCertificate();
    auto name  = ExtendedCertificate::getIdentityName(cert);
    bool valid = true;
    auto pn    = entry.player->getEntityName();
    int len    = pn.size();
    if (len > 16 || len < 3) { valid = false; }
    for (int i = 0; i < len && valid; i++) {
      char c = pn[i];
      if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == ' ') {
        continue;
      }
      valid = false;
      break;
    }
    if (!valid || pn != name) {
      LOGV("Incorrect username: %s, expected: %s!") % entry.player->getEntityName() % name;
      if (settings.banInvalidName) {
        Mod::Blacklist::GetInstance().Add(
            Mod::Blacklist::XUID{entry.xuid, entry.name}, settings.banString + "incorrect username.", "ExtoAC");
      }
      LocateService<ServerNetworkHandler>()->disconnectClient(entry.netid, 0, "Nick name incorrect!", false);
    }
  });
}

void PostInit() {}

class BlockLegacy {};

class Block {
private:
  void *vtable;

public:
  unsigned short aux;
  WeakPtr<BlockLegacy> legacy;
};

class ItemEnchants {};

class EnchantmentInstance {};

std::unordered_map<char, int> maxLevels;

THook(void *, "??0EnchantmentInstance@@QEAA@W4Type@Enchant@@H@Z", void *self, char type, int level) {
  if (!settings.resetOverEnchantedItems) return original(self, type, level);
  if (maxLevels.count(type)) {
    if (level > maxLevels[type]) maxLevels[type] = level;
  } else
    maxLevels[type] = level;
  return original(self, type, level);
}

THook(
    ItemEnchants *, "?constructItemEnchantsFromUserData@ItemStackBase@@QEBA?AVItemEnchants@@XZ", ItemStackBase *self) {
  if (!settings.resetOverEnchantedItems) return original(self);
  ItemEnchants *ret                         = original(self);
  std::vector<EnchantmentInstance> enchants = CallServerClassMethod<std::vector<EnchantmentInstance>>(
      "?getAllEnchants@ItemEnchants@@QEBA?AV?$vector@VEnchantmentInstance@@V?$allocator@VEnchantmentInstance@@@std@@@"
      "std@@XZ",
      ret);
  for (int i = 0; i < enchants.size(); i += 8) {
    if (dAccess<int>(&enchants[i], 0x4) > maxLevels[dAccess<char>(&enchants[i], 0)]) {
      self->tag.reset();
      return ret;
    }
  }
  return ret;
}

THook(ItemStack *, "?getItem@DispenserBlockActor@@UEBAAEBVItemStack@@H@Z", void *thi, int i) {
  if (!settings.noDispenserEjectBannedItems) return original(thi, i);
  ItemStack *is = original(thi, i);
  if (!is) { return is; }
  if (banitems.count(is->getId())) {
    if (std::find(banitems[is->getId()].begin(), banitems[is->getId()].end(), -1) != banitems[is->getId()].end() ||
        std::find(banitems[is->getId()].begin(), banitems[is->getId()].end(), is->getAuxValue()) !=
            banitems[is->getId()].end()) {
      return new ItemStack();
    }
  }
  return is;
}

THook(
    void *, "?destroyBlock@GameMode@@UEAA_NAEBVBlockPos@@E@Z", GameMode *mode, BlockPos const &pos, unsigned char unk) {
  Player *player = mode->player;
  if (!player || player->isCreative() || player->getCommandPermissionLevel() > CommandPermissionLevel::Any)
    return original(mode, pos, unk);
  BlockSource *bs = dAccess<BlockSource *>(player, 0x320);
  if (!bs) return original(mode, pos, unk);
  Block const &block =
      CallServerClassMethod<Block const &>("?getBlock@BlockSource@@QEBAAEBVBlock@@AEBVBlockPos@@@Z", bs, pos);
  BlockLegacy *legacy = block.legacy.get();
  int blockId         = dAccess<int>(legacy, 0x10C);
  if (blockId >= 256) { blockId = 256 - blockId - 1; }
  if (blockId == 7) return nullptr;
  float blockSpeed = dAccess<float>(legacy, 228);
  float speed      = 1. / player->getDestroySpeed(block);
  if (speed == 1 && blockSpeed * 1.5 < speed) { speed = blockSpeed * 1.5; }
  auto &db = Mod::PlayerDatabase::GetInstance();
  auto it  = db.Find(player);
  if (!it) return original(mode, pos, unk);
  auto xuid = it->xuid;
  speed     = std::floor(speed * 100) / 100;
  if (blockSpeed == 0) { speed = 0; }
  long long now = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::high_resolution_clock::now().time_since_epoch())
                      .count();
  if (lastBreak[it->xuid] + speed * 500 > now) {
    if (multiBreak[it->xuid] > 7) { multiBreak[it->xuid] = 0; }
    if (multiBreak[it->xuid] < 7) {
      multiBreak[it->xuid]++;
    } else if (multiBreak[it->xuid] == 7) {
      LOGV("%s broke too fast block: %d, xuid: %d") % it->name % blockId % it->xuid;
      LocateService<ServerNetworkHandler>()->disconnectClient(it->netid, 0, settings.kickBreakingTooFast, false);
      multiBreak[it->xuid] = 0;
    }
    lastBreak[it->xuid] = now;
    return nullptr;

  } else {
    lastBreak[it->xuid]  = now;
    multiBreak[it->xuid] = 0;
  }
  return original(mode, pos, unk);
}

THook(
    void *, "?handle@ServerNetworkHandler@@UEAAXAEBVNetworkIdentifier@@AEBVActorFallPacket@@@Z",
    ServerNetworkHandler &snh, NetworkIdentifier const &netid, ActorFallPacket *pk) {
  auto &db = Mod::PlayerDatabase::GetInstance();
  auto it  = db.Find(netid);
  if (!it || it->player->getCommandPermissionLevel() > CommandPermissionLevel::Any) { return original(snh, netid, pk); }
  if (pk->actorId.value != it->player->getRuntimeID().value) { return nullptr; }
  return original(snh, netid, pk);
}

struct ActorEventPacket : Packet {
  ActorRuntimeID actorId;
  char event;
  int data;

  inline ~ActorEventPacket() {}
  MCAPI virtual MinecraftPacketIds getId() const;
  MCAPI virtual std::string getName() const;
  MCAPI virtual void write(BinaryStream &) const;
  MCAPI virtual StreamReadResult read(ReadOnlyBinaryStream &);
};

THook(
    void *, "?handle@ServerNetworkHandler@@UEAAXAEBVNetworkIdentifier@@AEBVActorEventPacket@@@Z",
    ServerNetworkHandler &snh, NetworkIdentifier const &netid, ActorEventPacket *pk) {
  auto &db = Mod::PlayerDatabase::GetInstance();
  auto it  = db.Find(netid);
  if (!it || it->player->getCommandPermissionLevel() > CommandPermissionLevel::Any) { return original(snh, netid, pk); }
  if (pk->event == 3) { return nullptr; }
  return original(snh, netid, pk);
}

THook(
    void *, "?handle@ServerNetworkHandler@@UEAAXAEBVNetworkIdentifier@@AEBVInventoryTransactionPacket@@@Z",
    ServerNetworkHandler &snh, NetworkIdentifier const &netid, InventoryTransactionPacket *pk) {
  auto &db = Mod::PlayerDatabase::GetInstance();
  auto it  = db.Find(netid);
  if (!it || it->player->getCommandPermissionLevel() > CommandPermissionLevel::Any) { return original(snh, netid, pk); }
  if (ignore_packets.count(it->xuid) == 1) return nullptr;
  ComplexInventoryTransaction *transaction                                   = pk->transaction.get();
  InventoryTransaction &data                                                 = transaction->data;
  std::unordered_map<InventorySource, std::vector<InventoryAction>> &actions = data.actions;
  std::string item;
  bool cheated = false;
  for (auto &i : actions) {
    for (auto &j : i.second) {
      if (i.first.type == InventorySourceType::NONIMPLEMENTEDTODO) {
        cheated = true;
        item    = std::to_string(j.from.getId()) + ":" + std::to_string(j.from.getAuxValue());
        break;
      }
      int id = j.to.getId();
      if (banitems.count(id)) {
        if (std::find(banitems[id].begin(), banitems[id].end(), -1) != banitems[id].end() ||
            std::find(banitems[id].begin(), banitems[id].end(), j.to.getAuxValue()) != banitems[id].end()) {
          LOGV("%s has been detected using: %s, xuid: %s") % it->name %
              (std::to_string(j.to.getId()) + ":" + std::to_string(j.to.getAuxValue())) % it->xuid;
          auto packet = TextPacket::createTextPacket<TextPacketType::SystemMessage, false>(settings.banItem);
          it->player->sendNetworkPacket(packet);
          transaction->onTransactionError(*it->player, InventoryTransactionError::Unexcepted);
          return nullptr;
        }
      }
      id = j.from.getId();
      if (banitems.count(id)) {
        if (std::find(banitems[id].begin(), banitems[id].end(), -1) != banitems[id].end() ||
            std::find(banitems[id].begin(), banitems[id].end(), j.from.getAuxValue()) != banitems[id].end()) {
          LOGV("%s has been detected using: %s, xuid: %s") % it->name %
              (std::to_string(j.from.getId()) + ":" + std::to_string(j.from.getAuxValue())) % it->xuid;
          auto packet = TextPacket::createTextPacket<TextPacketType::SystemMessage, false>(settings.banItem);
          it->player->sendNetworkPacket(packet);
          transaction->onTransactionError(*it->player, InventoryTransactionError::Unexcepted);
          return nullptr;
        }
      }
    }
  }
  if (cheated) {
    LOGV("%s has been detected using: give exploit, xuid: %s Cheated item: %s") % it->name % it->xuid % item;
    if (settings.banGive) {
      Mod::Blacklist::GetInstance().Add(
          Mod::Blacklist::XUID{it->xuid, it->name}, settings.banString + settings.kickGive, "ExtoAC");
    }
    LocateService<ServerNetworkHandler>()->disconnectClient(it->netid, 0, settings.kickGive, false);
    transaction->onTransactionError(*it->player, InventoryTransactionError::Unexcepted);
    return nullptr;
  }
  return original(snh, netid, pk);
}

// THook(
//    void *, "?updateCustomBlockEntityTag@Item@@QEBA_NAEAVBlockSource@@AEAVItemStack@@AEAVBlockPos@@@Z", void *sh,
//    BlockSource &bs, ItemStack &is, BlockPos &bp) {
//  return nullptr;
//}

int LCSubStr(std::string X, std::string Y) {
  int m = X.size();
  int n = Y.size();

  int **LCSuff = new int *[m + 1];
  for (int i = 0; i < m + 1; i++) { LCSuff[i] = new int[n + 1]; }
  int result = 0;

  for (int i = 0; i <= m; i++) {
    for (int j = 0; j <= n; j++) {

      if (i == 0 || j == 0)
        LCSuff[i][j] = 0;

      else if (X[i - 1] == Y[j - 1]) {
        LCSuff[i][j] = LCSuff[i - 1][j - 1] + 1;
        result       = std::max(result, LCSuff[i][j]);
      } else
        LCSuff[i][j] = 0;
    }
  }
  for (int i = 0; i < m + 1; i++) { delete LCSuff[i]; }
  delete LCSuff;
  return result;
}

THook(
    void *, "?handle@ServerNetworkHandler@@UEAAXAEBVNetworkIdentifier@@AEBVTextPacket@@@Z", ServerNetworkHandler *snh,
    NetworkIdentifier *netid, TextPacket *packet) {
  // std::cout << packet->content << std::endl;
  auto &db = Mod::PlayerDatabase::GetInstance();
  auto it  = db.Find(*netid);
  if (!it) return original(snh, netid, packet);
  std::string displayName = it->name;
  if (it->player->getCommandPermissionLevel() > CommandPermissionLevel::Any) return original(snh, netid, packet);
  std::string contetLow = packet->content;
  std::transform(
      contetLow.begin(), contetLow.end(), contetLow.begin(), [](unsigned char c) { return std::tolower(c); });
  std::string lastMsg = lastMessage[it->xuid];
  if ((settings.blockSameMsg && (LCSubStr(lastMsg, contetLow) / contetLow.size()) > 0.8) ||
      lastMessageTime[it->xuid] + settings.sameMsgTime * 1000 >
          std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::high_resolution_clock::now().time_since_epoch())
              .count()) {
    auto packet = TextPacket::createTextPacket<TextPacketType::SystemMessage, false>(settings.stopSpam);
    it->player->sendNetworkPacket(packet);
    return nullptr;
  }
  for (std::string &s : settings.banWords) {
    if (contetLow.find(s) != std::string::npos) {
      auto packet = TextPacket::createTextPacket<TextPacketType::SystemMessage, false>(settings.banWord);
      it->player->sendNetworkPacket(packet);
      lastMessage[it->xuid]     = contetLow;
      lastMessageTime[it->xuid] = std::chrono::duration_cast<std::chrono::milliseconds>(
                                      std::chrono::high_resolution_clock::now().time_since_epoch())
                                      .count();
      return nullptr;
    }
  }
  lastMessage[it->xuid]     = contetLow;
  lastMessageTime[it->xuid] = std::chrono::duration_cast<std::chrono::milliseconds>(
                                  std::chrono::high_resolution_clock::now().time_since_epoch())
                                  .count();
  return original(snh, netid, packet);
}

THook(
    void *, "?handle@ServerNetworkHandler@@UEAAXAEBVNetworkIdentifier@@AEBVSpawnExperienceOrbPacket@@@Z",
    ServerNetworkHandler *snh, NetworkIdentifier *netid, void *packet) {
  if (settings.blockExpOrb) return nullptr;
  return original(snh, netid, packet);
}

static long redTick = 0;

THook(void *, "?evaluate@CircuitSystem@@QEAAXPEAVBlockSource@@@Z", void *a, void *b) {
  if (!settings.redstoneSlower) return original(a, b);
  redTick++;
  if (redTick % settings.redstoneTick == 0) return original(a, b);
  return nullptr;
}

std::unordered_map<void *, std::vector<ItemStack>> items;

THook(
    void *,
    "?_addResponseSlotInfo@ItemStackRequestActionHandler@@AEAAXAEBUItemStackRequestHandlerSlotInfo@@AEBVItemStack@@@Z",
    void *thi, void *reqSlot, ItemStack *is) {
  ItemStack isNew(*is);
  items[thi].push_back(isNew);
  return original(thi, reqSlot, is);
}

class ContainerManagerModel {
public:
};

THook(
    char, "?_handleTransfer@ItemStackRequestActionHandler@@AEAA_NAEBVItemStackRequestActionTransferBase@@_N11@Z",
    void *thi, void *base, bool b1, bool b2, bool b3) {
  char ret        = original(thi, base, b1, b2, b3);
  Player *player  = dAccess<Player *>(thi, 0x38);
  void *container = dAccess<void *>(player, 0xB60);
  if (container) {
    if ((int) dAccess<char>(container, 0x29) == 13) {
      try {
        if ((int) dAccess<char>(base, 0x8) == 0 && dAccess<bool>(base, 0x10) &&
            dAccess<char>(base, 0x14) != dAccess<char>(base, 0x20) &&
            std::get<SimpleServerNetId<ItemStackNetIdTag, int, 0>>(dAccess<ItemStackNetIdVariant>(base, 0x18)).value ==
                std::get<SimpleServerNetId<ItemStackNetIdTag, int, 0>>(dAccess<ItemStackNetIdVariant>(base, 0x24))
                    .value) {
          return 0;
        }
      } catch (const std::bad_variant_access &e) {
        // ignore
        return 0;
      }
    }
  }

  auto &db = Mod::PlayerDatabase::GetInstance();
  auto it  = db.Find(player);
  if (it->player->getCommandPermissionLevel() > CommandPermissionLevel::Any) return ret;
  if (items.count(thi)) {
    for (ItemStack &is : items[thi]) {
      // std::cout << is.getId() << " " << is.getIdAux() << std::endl;
      id = is.getId();
      if (banitems.count(id)) {
        if (std::find(banitems[id].begin(), banitems[id].end(), -1) != banitems[id].end() ||
            std::find(banitems[id].begin(), banitems[id].end(), is.getAuxValue()) != banitems[id].end()) {
          LOGV("%s has been detected using: %s, xuid: %s") % it->name %
              (std::to_string(is.getId()) + ":" + std::to_string(is.getAuxValue())) % it->xuid;
          auto packet = TextPacket::createTextPacket<TextPacketType::SystemMessage, false>(settings.banItem);
          it->player->sendNetworkPacket(packet);
          items.erase(thi);
          return 0;
        }
      }
    }
  }
  items.erase(thi);
  return ret;
}