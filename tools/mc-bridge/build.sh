#!/bin/sh
# Builds the AeroClicker bridge mod without Gradle/Loom.
#
# The mod reaches Minecraft entirely through reflection (see StateExport.java), so it needs no
# Minecraft jar and no remapping: the only classpath entry is the Fabric loader jar, and the
# resulting jar drops straight into any 26.1.2 / 26.2 / 26.3 instance's mods/ folder.
#
#   ./tools/mc-bridge/build.sh [output-dir]
#
# Overrides: FABRIC_LOADER (fabric-loader-*.jar), JAVAC, JAVA (runtime to use when javac is
# not installed; see the fallback below).
set -e

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
out_dir=${1:-"$here/build"}
javac=${JAVAC:-javac}
jar=${JAR:-$(dirname "$javac")/jar}
command -v "$jar" >/dev/null 2>&1 || jar=jar

# Some distributions install the JDK's modules but only the java/keytool launchers, so javac and
# jar are missing even though jdk.compiler and jdk.jartool are there (Void Linux is one). Run the
# compiler and the jar tool through those modules instead, so the jar can still be rebuilt.
if ! command -v "$javac" >/dev/null 2>&1; then
    jdk_java=${JAVA:-}
    [ -n "$jdk_java" ] || [ -z "$JAVA_HOME" ] || jdk_java=$JAVA_HOME/bin/java
    jdk_java=${jdk_java:-java}
    [ -x "$jdk_java" ] || { echo "build.sh: no javac, and JAVA=$jdk_java is not executable" >&2; exit 1; }
    "$jdk_java" -m jdk.compiler/com.sun.tools.javac.Main --version >/dev/null 2>&1 || {
        echo "build.sh: $jdk_java has no compiler (jdk.compiler) - install a JDK 21+ or set" >&2
        echo "           JAVA=/path/to/jdk/bin/java" >&2
        exit 1
    }
    shim=$(mktemp -d)
    trap 'rm -rf "$shim"' EXIT
    printf '#!/bin/sh\nexec "%s" -m jdk.compiler/com.sun.tools.javac.Main "$@"\n' "$jdk_java" > "$shim/javac"
    printf '#!/bin/sh\nexec "%s" -m jdk.jartool/sun.tools.jar.Main "$@"\n' "$jdk_java" > "$shim/jar"
    chmod +x "$shim/javac" "$shim/jar"
    javac=$shim/javac
    jar=$shim/jar
fi

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
