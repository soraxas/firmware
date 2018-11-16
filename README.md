# Ultimate Hacking Keyboard firmware with extended macro support

This is a fork of [UHK firmware](https://github.com/UltimateHackingKeyboard/firmware). This custom firmware provides extended macro action support. Namely, we allow a set of simple commands to be used in text macro actions. Commands are denoted by a single dolar sign  as a prefix. 

## Compatibility

This firmware is 100% compatible with original unmodified agent. All you need is to flash the modified firmware. Configurations won't get lost in case you decide to switch back to official firmware, or if you then again flash the modified version too, since config formats were not altered in any way.

## Featured usecases

The firmware adds macro commands for (almost?) all basic features of the keyboard. Furthermore some conditionals, jumps and sync mechanisms are available.

Some of the usecases which can be achieved via these commands are: 
- ability to mimic secondary roles 
- ability to bind actions to doubletaps 
- custom layer switching logic, including nested layer toggling (however, only one hold layer can be active at a time)
- flow control via goto command

## Example
For instance, if the following text is pasted as a macro text action, playing the macro will result in toggling of fn layer.
    
    $switchLayer fn

Implementation of standard double-tap-locking hold modifier in recursive version could look like (every line as a separate action!):

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

    $switchLayer mouse
    $delayUntilRelease
    $switchLayer previous
    $ifNotInterrupted switchLayer mouse

## Reference manual

The following grammar is supported:

    BODY = $COMMAND
    COMMAND = CONDITION COMMAND
    COMMAND = delayUntilRelease
    COMMAND = switchLayer {fn|mouse|mod|base|last|previous}
    COMMAND = switchKeymap {<abbrev>|last}
    COMMAND = break
    COMMAND = printStatus
    COMMAND = setStatus <custom text>
    COMMAND = goTo <index>
    COMMAND = recordMacro|playMacro
    COMMAND = {startMouse|stopMouse} {move DIRECTION|scroll DIRECTION|accelerate|decelerate}
    DIRECTION = {left|right|up|down}
    CONDITION = ifDoubletap | ifNotDoubletap
    CONDITION = ifInterrupted | ifNotInterrupted

- `ifDoubletap/ifNotDoubletap` is true if previous played macro had the same index and finished at most 250ms ago
- `ifInterrupted/ifNotInterrupted` is true if a keystroke action was triggered during macro runtime. Allows fake implementation of secondary roles. Also allows interruption of cycles.
- `switchLayer` toggles layer. We keep a stack of size 5, which can be used for nested toggling and/or holds.
  - `last` will toggle last layer toggled via this command and push it onto stack
  - `previous` will pop the stack
- `switchKeymap` will toggle the keymap by its abbreviation. Last will toggle the last keymap toggled via this command.
- `delayUntilRelease` sleeps the macro until its activation key is released. Can be used to set action on key release. This is set to at least 50ms in order to prevent debouncing issues.
- `break` will end playback of the current macro
- `printStatus` will "type" content of error status buffer (256 chars) on the keyboard. Mainly for debug purposes.
- `setStatus <custom text>` will append <custom text> to the error report buffer, if there is enough space for that
- `goTo <int>` will go to action index int. Actions are indexed from zero.
- `startMouse/stopMouse` start/stop corresponding action. E.g., `startMouse move left`
- `recordMacro|playMacro` targets vim-like macro functionality. For the time being, only one slot is available. Usage: call `recordMacro`, do some work, end recording by another `recordMacro`. Now you can play the actions back by calling `playMacro`. Only BasicKeyboard scancodes are available at the moment. These macros are recorded into RAM only. 


## Known issues

- Layers can be untoggled only via macro or "toggle" feature. The combined hold/doubletap will *not* release layer toggle (this is bug of the official firmware, waiting for reply from devs).  
- Only one macro at a time can run.
- Only one-liners are allowed, due to our need to respect firmware's indexation of actions.

## Contributing

If you wish to add some functionality, preferably fork the repo, implement it and post PR. Alternatively, feel free to fire tickets with feature requests... 

## Adding new features

See `macros.c`, namely `processCommandAction(...)`.

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
  
If you have any problems with the build procedure, please create issue in the official agent repository. I made no changes into the proccedure and I will not be able to help with them.

## TODO
- allow multiple macros run at the same time
- enable slots for runtime macros
