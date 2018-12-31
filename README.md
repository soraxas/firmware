# Ultimate Hacking Keyboard firmware with extended macro engine

This is a fork of [UHK firmware](https://github.com/UltimateHackingKeyboard/firmware). This custom firmware provides extended macro engine. Namely, we allow a set of simple commands to be used in text macro actions. These commands can be used to reach functionality otherwise unavailable via agent.

## Compatibility

This firmware is 100% compatible with the original unmodified agent. All you need is to flash the modified firmware to your UHK. Configurations won't get lost in case you decide to switch back to official firmware, or if you then again flash the modified version too, since config formats were not altered in any way.

## Features

The firmware implements:
- macro commands for (almost?) all basic features of the keyboard otherwise unreachable via agent. 
- macro commands for conditionals, jumps and sync mechanisms 
- some extended configuration options (composite-keystroke delay, sticky modifiers)
- runtime macro recorder implemented on scancode level, for vim-like macro functionality
- ability to run multiple macros at the same time

Some of the usecases which can be achieved via these commands are: 
- ability to mimic secondary roles 
- ability to bind actions to doubletaps 
- ability to bind modifier-chorded actions (i.e., shortucts which consists of arbitrary number of modifiers and at most one other key)
- ability to bind shift and non-shift scancodes independently
- ability to configure custom layer switching logic, including nested layer toggling 
- flow control via goto command
- runtime macros

## Examples
**Note that every command (i.e., every line in the examples) has to be inputted as a separate action!**

For instance, if the following text is pasted as a macro text action, playing the macro will result in toggling of fn layer.
    
    $switchLayer fn
    
Runtime macro recorder example. In this setup, shift+key will start recording (indicated by the "adaptive mode" led), another shit+key will stop recording. Hiting sole key will then replay the macro (e.g., simple repetitive text edit).

    $ifShift recordMacro A
    $ifNotShift playMacro A

Implementation of standard double-tap-locking hold modifier in recursive version could look like:

    $holdLayer fn
    $ifDoubletap switchLayer fn
    
Alternative way to implement the above example would be the following. However, using `holdLayer` for "hold" mechanisms is strongly encouraged due to more elaborate release logic:

    $switchLayer fn
    $delayUntilRelease
    $switchLayer previous
    $ifDoubletap switchLayer fn

Creating double-shift-to-caps may look like:
   
    <press Shift>
    $delayUntilRelease
    <releaseShift>
    $ifNotDoubletap break
    <tap CapsLock>

Smart switch (if tapped, locks layer; if used with a key, acts as a secondary role):

    $holdLayer mouse
    $ifNotInterrupted switchLayer mouse
    

Mapping shift/nonshift scancodes independently:

    $ifShift suppressMods write 4
    $ifNotShift write %
    
Applies the corresponding settings globaly. Namely turns off sticky modifiers (i.e., modifiers of composite keystrokes will apply but will no longer stick) so that composite actions don't affect external mouse. Furthermore, it enables separation of composite keystrokes, and increases update delay - this may be useful if macro playback needs to be slown down or if artificial delays need to be introduced for some reason. (Recommended values are 0,1,0.)

    $setStickyModsEnabled 0 
    $setSplitCompositeKeystroke 1
    $setKeystrokeDelay 10

Postponed secondary role switch. This modification will postpone all other key activations by 250ms. This modification prevents secondary role hiccups on alphabetic keys.

    $suppressKeys delayUntilReleaseMax 250
    $suppressKeys ifNotPlaytime 249 goTo 4
    $holdLayer mouse
    $break
    $suppressKeys write d
    
## Reference manual

The following grammar is supported:

    BODY = $COMMAND
    COMMAND = [CONDITION|MODIFIER]* COMMAND
    COMMAND = delayUntilRelease
    COMMAND = delayUntil <timeout (NUMBER)>
    COMMAND = delayUntilReleaseMax <timeout (NUMBER)>
    COMMAND = switchLayer {LAYERID|previous}
    COMMAND = holdLayer LAYERID
    COMMAND = holdLayerMax LAYERID <time in ms (NUMBER)>
    COMMAND = switchKeymap {<abbrev>|last}
    COMMAND = break
    COMMAND = printStatus
    COMMAND = setStatus <custom text>
    COMMAND = write <custom text>
    COMMAND = goTo <index>
    COMMAND = recordMacroDelay
    COMMAND = {recordMacro|playMacro} <slot identifier (CHAR)>
    COMMAND = {startMouse|stopMouse} {move DIRECTION|scroll DIRECTION|accelerate|decelerate}
    COMMAND = setStickyModsEnabled {0|1}
    COMMAND = setSplitCompositeKeystroke {0|1}
    COMMAND = setKeystrokeDelay <time in ms, at most 250 (NUMBER)>
    COMMAND = setReg <register index (NUMBER)> <value (NUMBER)> 
    CONDITION = ifDoubletap | ifNotDoubletap
    CONDITION = ifInterrupted | ifNotInterrupted
    CONDITION = {ifPlaytime | ifNotPlaytime} <timeout in ms>
    CONDITION = ifShift | ifAlt | ifCtrl | ifGui | ifAnyMod | ifNotShift | ifNotAlt | ifNotCtrl | ifNotGui | ifNotAnyMod
    CONDITION = {ifRegEq | ifNotRegEq} <register index (NUMBER)> <value (NUMBER)>
    MODIFIER = suppressMods
    MODIFIER = suppressKeys
    DIRECTION = {left|right|up|down}
    LAYERID = fn|mouse|mod|base|last
    NUMBER = #NUMBER | [0-9]+
    CHAR = <any ascii char>

- `ifDoubletap/ifNotDoubletap` is true if previous played macro had the same index and finished at most 250ms ago
- `ifInterrupted/ifNotInterrupted` is true if a keystroke action or mouse action was triggered during macro runtime. Allows fake implementation of secondary roles. Also allows interruption of cycles.
- `ifPlaytime/ifNotPlaytime <timeout in ms>` is true if at least `timeout` milliseconds passed since macro was started.
- `ifShift/ifAlt/ifCtrl/ifGui/ifAnyMod/ifNotShift/ifNotAlt/ifNotCtrl/ifNotGui/ifNotAnyMod` is true if either right or left modifier was held in the previous update cycle.
- Layer/Keymap switching:
  - `switchLayer` toggles layer. We keep a stack of limited size, which can be used for nested toggling and/or holds.
    - `last` will toggle last layer toggled via this command and push it onto stack
    - `previous` will pop the stack
  - `holdLayer <layer>` mostly corresponds to the sequence `switchLayer <layer>; delayUntilRelease; switchLayer previous`, except for more elaborate conflict resolution (releasing holds in incorrect order, correct releasing of holds in case layer is locked via `$switchLayer` command.
  - `holdLayerMax <layer> <timeout in ms>` will timeout after <timeout> ms if no action is performed in that time.
  - `switchKeymap` will toggle the keymap by its abbreviation. Last will toggle the last keymap toggled via this command.
- `suppressMods` will supress any modifiers except those applied via macro engine. Can be used to remap shift and nonShift characters independently.
- `suppressKeys` will supress postpone all new key activations. Can be used to mess with timing - e.g., to postpone activation of other keys.
- `delayUntil <timeout>` sleeps the macro until timeout (in ms) is reached.
- `delayUntilRelease` sleeps the macro until its activation key is released. Can be used to set action on key release. 
- `delayUntilReleaseMax <timeout>` same as `delayUntilRelease`, but is also broken when timeout (in ms) is reached.
- `break` will end playback of the current macro
- `printStatus` will "type" content of error status buffer (256 chars) on the keyboard. Mainly for debug purposes.
- `setStatus <custom text>` will append <custom text> to the error report buffer, if there is enough space for that
- `write <custom text>` will type rest of the string. Same as the plain text command. This is just easier to use with conditionals...
- `goTo <int>` will go to action index int. Actions are indexed from zero.
- `startMouse/stopMouse` start/stop corresponding mouse action. E.g., `startMouse move left`
- `recordMacro|playMacro <slot identifier>` targets vim-like macro functionality. Slot identifier is a single character. Usage (e.g.): call `recordMacro a`, do some work, end recording by another `recordMacro a`. Now you can play the actions (i.e., sequence of keyboard reports) back by calling `playMacro a`. Only BasicKeyboard scancodes are available at the moment. These macros are recorded into RAM only. Number of macros is limited by memory (current limit is set to approximately 500 keystrokes (4kb) (maximum is ~1000 if we used all available memory)). If less than 1/4 of dedicated memory is free, oldest macro slot is freed.
- `recordMacroDelay` will measure time until key release (i.e., works like `delayUntilRelease`) and insert delay of that length into the currently recorded macro. This can be used to wait for window manager's reaction etc. 
- `setStickyModsEnabled` globally turns on or off sticky modifiers
- `setSplitCompositeKeystroke {0|1}` If enabled, composite keystrokes (e.g., Ctrl+c sent by a single key) are separated into distinct usb reports. This makes order of keypresses clearly determined. Enabled by default.
- `setKeystrokeDelay <time in ms, at most 250>` will stop event processing for the specified time after every usb report change. May be used to slow down macroes, to insert delay between composite keystrokes. Beware, this does not queue keypresses - if the delay is too long, some keypresses may be skipped entirelly!
- Registers - for the purpose of toggling functionality on and off, we provide 32 numeric registers (namely of type int32_t). 
  - `setReg <register index> <value>` will set register identified by index to value.
  - `{ifRegEq|ifNotRegEq} <register inex> <value>` will test if the value in the register identified by first argument equals second argument.
  - Register values can also be used in place of all numeric arguments by prefixing register index by '#'. E.g., waiting until release or for amount of time defined by reg 1 can be achieved by `$delayUntilReleaseMax #1`

## Error handling

This version of firmware includes basic error handling. If an error is encountered, led display will change to `ERR` and error message written into the status buffer. Error log can be retrieved via the `$printStatus` command. (E.g., focus some text area (for instance, open notepad), and press key with corresponding macro).)

## Known issues/limitations

- Layers can be untoggled only via macro or "toggle" feature. The combined hold/doubletap will *not* release layer toggle (this is bug of the official firmware, waiting for reply from devs).  
- Only one-liners are allowed, due to our need to respect firmware's indexation of actions.
- Global settings and recorded macros are remembered until power cycling only. 

## Contributing

If you wish some functionality, feel free to fire tickets with feature requests. If you wish something already present on the tracker (e.g., in 'idea' tickets), say so in comments. If feel brave, fork the repo, implement the desired functionality and post a PR.

## Adding new features

Practically all high-level functionality of the firmware is implemented in the following files:

- `usb_report_updater.c` - logic of key activation, layer switching, debouncing, etc.. Almost all important stuff is here.
- `layer.c` - some suport for "hold" layer switching (beware, there are two independent layer switching mechanisms and this one is the less important one).
- `keymap.c` - keymap switching functions.
- `macros.c` - the macro engine. We furthermore extend it by `macro_recorder.c`

Our command actions are rooted in `processCommandAction(...)` in `macros.c`.

If you have any questions regarding the code, simply ask (via tickets or email).

## Building the firmware

If you want to try the firmware out, just download the tar in releases and flash it via Agent. 

If you wish to make changes into the source code, please follow the official repository guide. Basically, you will need:

- Clone the repo with `--recursive` flag.
- Build agent in lib/agent (that is, unmodified official agent), via `npm install && npm run build` in repository root. While doing so, you may run into some problems:
  - You may need to install some packages globally (I am afraid I no longer remember which ones).
  - You may need to downgrade npm: `sudo npm install -g n && sudo n 8.12.0`
  - You will need to commit changes made by npm in this repo, otherwise, make-release.js will be faililng later.
- Then you can setup mcuxpressoide according to the official firmware README guide.
- Now you can build and flash firmware either:
  - Via mcuxpressoide (debugging probes are not needed, see official firmware README).
  - Or via running scripts/make-release.js (run by `node make-release.js`) and flashing the resulting tar.bz2 through agent.
  
If you have any problems with the build procedure, please create issue in the official agent repository. I made no changes into the proccedure and I will most likely not be able to help.

