#include "global.h"

class InventoryCheckCommand : public Command {
public:
  CommandSelector<Player> selector;
  enum class InvKey { Set, Get } key;

  InventoryCheckCommand() {}

  void setInv(Player *target, Player *source) {
    Inventory *invSource =
        CallServerClassMethod<PlayerInventory *>("?getSupplies@Player@@QEAAAEAVPlayerInventory@@XZ", source)->invectory.get();
    Inventory *invTarget =
        CallServerClassMethod<PlayerInventory *>("?getSupplies@Player@@QEAAAEAVPlayerInventory@@XZ", target)->invectory.get();
    for (int i = 0;
         i < CallServerClassMethod<int>("?getContainerSize@FillingContainer@@UEBAHXZ", invSource);
         i++) {
      ItemStack copy(
          CallServerClassMethod<ItemStack const &>("?getItem@FillingContainer@@UEBAAEBVItemStack@@H@Z", invSource, i));
      CallServerClassMethod<void>("?setItem@Inventory@@UEAAXHAEBVItemStack@@@Z", invTarget, i, copy);
    }
    ItemStack copyOff(*CallServerClassMethod<ItemStack*>(
        "?getOffhandSlot@Actor@@QEBAAEBVItemStack@@XZ", source));
    CallServerClassMethod<void>(
        "?setOffhandSlot@ServerPlayer@@UEAAXAEBVItemStack@@@Z", target, copyOff);
    for (int i = 0; i < 4; i++) {
      ItemStack copy(CallServerClassMethod<ItemStack const &>(
          "?getArmor@Actor@@UEBAAEBVItemStack@@W4ArmorSlot@@@Z", source, (ArmorSlot) i));
      CallServerClassMethod<void>("?setArmor@ServerPlayer@@UEAAXW4ArmorSlot@@AEBVItemStack@@@Z", target, (ArmorSlot) i, copy);
    }

    EnderChestContainer *enderSource =
        CallServerClassMethod<EnderChestContainer *>("?getEnderChestContainer@Player@@QEAAPEAVEnderChestContainer@@XZ", source);
    EnderChestContainer *enderTarget =
        CallServerClassMethod<EnderChestContainer *>("?getEnderChestContainer@Player@@QEAAPEAVEnderChestContainer@@XZ", target);
    for (int i = 0; i < CallServerClassMethod<int>("?getContainerSize@FillingContainer@@UEBAHXZ", invSource); i++) {
      ItemStack copy(
          CallServerClassMethod<ItemStack const &>("?getItem@FillingContainer@@UEBAAEBVItemStack@@H@Z", invSource, i));
      CallServerClassMethod<void>("?setItem@Inventory@@UEAAXHAEBVItemStack@@@Z", invTarget, i, copy);
    }
    for (int i = 0; i < CallServerClassMethod<int>("?getContainerSize@FillingContainer@@UEBAHXZ", enderSource); i++) {
      ItemStack copy(
          CallServerClassMethod<ItemStack const &>("?getItem@FillingContainer@@UEBAAEBVItemStack@@H@Z", enderSource, i));
      CallServerClassMethod<void>("?setItemWithForceBalance@FillingContainer@@UEAAXHAEBVItemStack@@_N@Z", enderTarget, i, copy, 0);
    }
    CallServerClassMethod<void>(
        "?sendInventory@ServerPlayer@@UEAAX_N@Z", target, false);
  }

  void execute(CommandOrigin const &origin, CommandOutput &output) {
    if (origin.getOriginType() != CommandOriginType::Player) {
      output.error("commands.generic.error.invalidPlayer", {"/inv"});
      return;
    }
    auto result = selector.results(origin);
    if (result.empty()) {
      output.error("commands.generic.selector.empty");
      return;
    }
    if (result.count() > 1) {
      output.error("commands.generic.selector.empty");
      return;
    }
    auto source = (Player *) origin.getEntity();
    for (auto target : result) {
      switch (key) {
      case InvKey::Set:
        if (!settings.setInv) {
          output.error("Setting inventory is turn off!");
          return;
        }
        setInv(target, source);
        output.success(target->getNameTag() + " now have your inventory and enderchest!");
        break;
      case InvKey::Get:
        if (!settings.getInv) {
          output.error("Getting inventory is turn off!");
          return;
        }
        setInv(source, target);
        output.success("You now have " + target->getNameTag() + "'s inventory and enderchest!");
        break;
      }
    }
  }

  static void setup(CommandRegistry *registry) {
    using namespace commands;
    registry->registerCommand(
        "checkinv", "copy and send player eq", CommandPermissionLevel::GameMasters, CommandFlagCheat, CommandFlagNone);
    addEnum<InvKey>(registry, "inv-check-key", {{"get", InvKey::Get}, {"set", InvKey::Set}});
    registry->registerOverload<InventoryCheckCommand>(
        "checkinv", mandatory(&InventoryCheckCommand::selector, "target"),
        mandatory<CommandParameterDataType::ENUM>(&InventoryCheckCommand::key, "key", "inv-check-key"));
  }
};

void initCommand(CommandRegistry *registry) { InventoryCheckCommand::setup(registry); }