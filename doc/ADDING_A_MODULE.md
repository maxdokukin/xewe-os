IF MODULE CAN BE DISABLED, YOU SHOULD ADD THIS ON TOP OF EVERY NON-CORE FUNCTION
if (is_disabled()) return;
why? because other modules may attemt to call the disabled modules functions. return at the beginning of the function to avoid logical issues.

- this can and should be automated with a script as a part of the build.sh
- 