"""Keep the linker map path relative on Windows with non-ASCII project paths."""
import os

Import("env")

if os.name == "nt":
    relative_map = os.path.relpath(
        os.path.join(env.subst("$BUILD_DIR"), env.subst("$PROGNAME") + ".map"),
        env.subst("$PROJECT_DIR"),
    ).replace("\\", "/")
    env.Replace(LINKFLAGS=[
        '-Wl,-Map="' + relative_map + '"' if str(flag).startswith("-Wl,-Map=") else flag
        for flag in env["LINKFLAGS"]
    ])
