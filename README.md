# Ultimate Hacking Keyboard firmware with extended macro support

This is a fork of [UHK firmware](https://github.com/UltimateHackingKeyboard/firmware). This custom firmware provides extended macro action support. Namely, we allow a set of simple commands to be used in text macro actions. Commands are denoted by a single dolar sign  as a prefix. Currently, only oneliners are supported (since we need to respect action indexing of macro player).

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

## Reference manual

The following grammar is supported:

    BODY = $COMMAND
    COMMAND = CONDITION COMMAND
    COMMAND = delayUntilRelease
    COMMAND = switchLayer {fn|mouse|mod|base|last|previous}
    COMMAND = switchKeymap {<abbrev>|last}
    COMMAND = break
    COMMAND = errorStatus
    COMMAND = reportError <custom text>
    CONDITION = ifDoubletap | ifNotDoubletap

- `ifDoubletap/ifNotDoubletap` - is true if previous played macro had the same index and finished at most 250ms ago

- `switchLayer` toggles layer. We keep a stack of size 5, which can be used for nested toggling and/or holds.

  - `last` will toggle last layer toggled via this command and push it onto stack

  - `previous` will pop the stack

- `switchKeymap` will toggle the keymap by its abbreviation. Last will toggle last keymap toggled via this command.

- `delayUntilRelease` sleeps the macro until its activation key is released. Can be used to set action on key release. This is set to at least 50ms in order to prevent debouncing issues.

- `break` will end playback of the current macro

- `errorStatus` will "type" content of error status buffer (256 chars) on the keyboard. Mainly for debug purposes.

- `reportError <custom text>` will append <custom text> to the error report buffer, if there is enough space for that

## Known issues

- Layers can be untoggled only via macro or "toggle" feature. The combined hold/doubletap will *not* release layer toggle.  

- Macros are not recursive. 

## Contributing

If you wish to add some functionality, preferably fork the repo, implement it and post PR. Alternatively, feel free to fire tickets with feature requests... 

## Adding new features

See `macros.c`, namely `processCommandAction(...)`.
