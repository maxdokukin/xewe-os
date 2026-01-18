How to add a nem module:

Say you are adding a new module: ModuleName.

First, depending on whether is build more for hardware of software interaction, create new folded and .cpp .h files in the scr/Modules/<Software|Hardware>/ModuleName

For example, if is mopre of a Software module, create these files:
/src/Modules/Hardware/ModuleName/ModuleName.cpp
/src/Modules/Hardware/ModuleName/ModuleName.h
/src/Modules/Hardware/ModuleName/README.md (is good to have)

Now /src_templates is a good starting point; copy paste from 
/src_templates/ModuleTemplate.h -> ModuleName.h
/src_templates/ModuleTemplate.cpp -> ModuleName.cpp

Your ModuleName will be inheriting from Module. So, you only need to define a constructor. Other functions are optional for you. That said, make sure that if you have custom code when overriding, you still call parent method.

Ex: void ModuleName::disable (const bool verbose, const bool do_restart) {
    // do your custom routines here
    Module::disable(verbose, do_restart);
}

Constructor:
IN the constructor, you can define what happens when the module is beeing created. I don't recommend running any functions here, as there is begin routing logic that will be discussed later.

ModuleName::ModuleName(SystemController& controller)
      : Module(controller,
               /* module_name         */ "",
               /* module_description  */ "",
               /* nvs_key             */ "",
               /* requires_init_setup */ false,
               /* can_be_disabled     */ false,
               /* has_cli_cmds        */ false)
{}

Here, you should name your module, givit is some description, and set nvs key, ideally around 3 characters.

Now, booleans:
- requires_init_setup: this is a flag to run begin_routines_init(); it will only happen once, upon the firs boot after the software is uploaded. So in case you need to cofigure something once, that flag should be true. Ex: wifi requires init setup to select a network and enter a password.
- can_be_disabled: if module can be enabled/disabled
- has_cli_cmds: if your  module has cli cmds; if this is set to true, by default the module will support status and reset commands; if the can_be_disabled flag is set to true, then the module will also support enable disable commands by default.

In the bnody of constructor you can define your custom CLI cmds:
Each Command has these properties:
struct Command {
    string                      name;
    string                      description;
    string                      sample_usage;
    size_t                      arg_count;
    command_function_t          function;
};
Example:
commands_storage.push_back({
    "add",
    "Add a button mapping: <pin> \"<$cmd ...>\" [pullup|pulldown] [on_press|on_release|on_change] [debounce_ms]",
    std::string("Sample Use: $") + lower(module_name) + " add 9 \"$system reboot\" pullup on_press 50",
    5,
    [this](std::string_view args){ button_add_cli(args); }
});

You can refer to existing modules constructors to see how it's done.

Begin Logic:
There are 4 begin functions, which may sound like a lot; but every sinngle one of them has a purpose.

begin_routines_required: happens every time on boot
begin_routines_init: happens on first boot, or after $enable
begin_routines_regular: happens on regular boot
begin_routines_common: happens at the end of the boot

![begin_flow.png](../static/media/resources/readme/begin_flow.png)

Chances are, you won't need all four. But its good to have options for the flexibility.
NOTE: even if the module is disabled, begin will still be called. This is done because other modules may refer to disabled module. This would avoid null pointers otherwise.

Loop:
Here you can add all the code that you need to execute routinely. Blocking functions will affect all other modules, so maybe don't use delay().

Custom functions:
All funcitons that you develop for the module should have important consideration. If your module can be disabled, you should check if it is iat the beginning of your custom functions, and return. Alike calling begin() for disabled modules, your custom functions may be called by others, so make sure to include one liner if (is_disabled()) return;

void ModuleName::custom_function () {
    // make sure to have this, otherwise if other modules call it when disabled, this will lead to undesired bugs.
    if (is_disabled()) return;
    
    // do your custom routines here
}

That's it on module delevopment practicies; NOw let's go to the integration part


Integrating New Module
Very straightforward and mechanical

in scr/SystemController/SystemController.h:
add #include "../Modules/<Hardware|Software>/ModuleName/ModuleName.h"
update constexpr size_t MODULE_COUNT by one.

in SystemController::public:
add variable ModuleName module_name;

in scr/SystemController/SystemController.cpp:
in SystemController::SystemController()
add ", module_name(*this)"
add "modules[MODULE_COUNT - 1] = &module_name;"

in SystemController::begin()
if your module depends on other modules (ex: WebServer requires Wifi)
then add this before calling module begin 
module_name.add_requirement (module_required);
then add module_name.begin (ModuleNameConfig {});

note that your dependency setting and begin should be before 

"// should be initialized last to collect all cmds
command_parser.begin        (CommandParserConfig(modules, MODULE_COUNT));"

That's it, now your custom module is fullly integrated with others.


