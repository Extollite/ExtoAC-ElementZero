# ExtoAC
-------------

Features
-------------
- Anti horion .give, .enchant, .nbtwrite 
- Anti toolbox give, enchant, nbt editor
- Anti toolbox name overwrite.
- Anti nuker, insta break, illegal haste
- Anti beacon related dupe method.
- Anti exp exploit
- Anti one hit kill exploits
- Anti spam
- Banned word filter
- Banned items
- Redstone slower (make 0 tick farms not usable)
- /check-inv command to check players inventory

Config
-------------
```
ExtoAC:
    enabled: true
    banGive: true # auto ban on .give .enchant
    banItems:
      - 325:3 # ItemId:Meta use ItemId:-1 for all meta values, you can found them here: https://minecraft.gamepedia.com/Bedrock_Edition_data_value
    banWords:
      - elementzero
    sameMsgTime: 3 #seconds between messages 
    banItem: This item is banned!
    stopSpam: Stop spamming!
    banWord: "Word is banned!"
    blockSameMsg: true #Block massages that are 80% common
    kickBreakingTooFast: You are breaking too fast!
    kickGive: Bad packets!
    redstoneSlower: false # turn on redstone slower
    redstoneTick: 1 #ticks between run redstone, 1 means redstone is ticked 1/10 of second, minecraft value is 1/20
    banString: "You are banned: "
    blockExpOrb: true # if set to true it will block exp exploit but also receiving exp from furnace. 
    setInv: true #turn off check-inv set command
    getInv: true #turn off check-inv get command
    validateName: true
    banInvalidName: true
    resetOverEnchantedItems: true #reset items with illegal enchants.
    noDispenserEjectBannedItems: true #dispensers don't throw banned items.
```

Commands
-------------
- `/check-inv <player name> get` override executor's inventory with player's inventory and executor's enderchest with player's inventory
- `/check-inv <player name> set` override player's inventory with executor's inventory and player's enderchest with executor's inventory
