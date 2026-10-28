#!/bin/sh
# Builds the AeroClicker bridge mod without Gradle/Loom.
#
# The mod reaches Minecraft entirely through reflection (see StateExport.java), so it needs no
# Minecraft jar and no remapping: the only classpath entry is the Fabric loader jar, and the
# resulting jar drops straight into any 26.1.2 / 26.2 / 26.3 instance's mods/ folder.
#
#   ./tools/mc-bridge/build.sh [output-dir]
#
# Overrides: FABRIC_LOADER (fabric-loader-*.jar), JAVAC.
set -e

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
out_dir=${1:-"$here/build"}
javac=${JAVAC:-javac}
jar=${JAR:-$(dirname "$javac")/jar}
command -v "$jar" >/dev/null 2>&1 || jar=jar

loader=${FABRIC_LOADER:-$(ls -1 "$HOME/.local/share/PrismLauncher/libraries/net/fabricmc/fabric-loader"/*/fabric-loader-*.jar 2>/dev/null | sort -V | tail -n 1)}
[ -f "$loader" ] || { echo "build.sh: no fabric-loader jar (set FABRIC_LOADER)" >&2; exit 1; }

# Java 21 bytecode is the floor Fabric 26.x runs on, even though the game itself is built for 25.
rm -rf "$out_dir"
mkdir -p "$out_dir/classes"
"$javac" --release 21 -cp "$loader" -d "$out_dir/classes" $(find "$here/src" -name '*.java')
cp "$here/resources/fabric.mod.json" "$here/../../LICENSE" "$out_dir/classes/"
"$jar" --create --file "$out_dir/aeroclicker-bridge-1.0.0.jar" -C "$out_dir/classes" .
rm -rf "$out_dir/classes"
echo "$out_dir/aeroclicker-bridge-1.0.0.jar"
