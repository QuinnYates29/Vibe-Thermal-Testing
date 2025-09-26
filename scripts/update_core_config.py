import os

local_conf = "./src/core_config.h"
core_conf = "../Core/src/core_config.h"
new_conf = "core_config.h"

new_file = open(new_conf, "w")

# Open core_config.h from the Core
with open(core_conf, "r") as core_file:
    for core_line in core_file:
        core_word_arr = core_line.split()

        # Write a newline if there's nothing there
        if (len(core_word_arr) == 0): 
            new_file.write("\n")
            continue

        # If the pattern of a real definition is matched
        if (core_word_arr[0] == "#define"):
            # Open up the local file to check the config setting there
            found = False
            with open(local_conf, "r") as local_file:
                for local_line in local_file:
                    local_word_arr = local_line.split()

                    # If not three words long it isn't a match
                    if len(local_word_arr) != 3: continue

                    # If it isn't a #define it isn't a match
                    if local_word_arr[0] != "#define": continue

                    # If it's a definition for the same thing as the Core file, save that line
                    if local_word_arr[1] == core_word_arr[1]:
                        new_file.write(local_line)
                        local_file.close()
                        found = True
                        break

                if not found: new_file.write(core_line)
        else: new_file.write(core_line)
    core_file.close()
new_file.close()

os.system(f"mv {new_conf} ./src")
