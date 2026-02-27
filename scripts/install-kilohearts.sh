#!/usr/bin/env bash
#
# install-kilohearts.sh — Install Kilohearts VST plugins on Linux via Wine + yabridge
#
# Usage:
#   chmod +x install-kilohearts.sh
#   ./install-kilohearts.sh
#
# What this does:
#   1. Installs Wine Staging (from WineHQ repos on Debian/Ubuntu)
#   2. Installs yabridge (Windows VST bridge for Linux)
#   3. Downloads & runs the Kilohearts Windows installer under Wine
#   4. Configures yabridge so your Linux DAW can find the plugins
#
# Tested on: Debian 13, Ubuntu 24.04+
# Requires: sudo access, internet connection
#
set -euo pipefail

YABRIDGE_VERSION="5.1.1"
YABRIDGE_URL="https://github.com/robbert-vdh/yabridge/releases/download/${YABRIDGE_VERSION}/yabridge-${YABRIDGE_VERSION}.tar.gz"
KILOHEARTS_INSTALLER_URL="https://kilohearts.com/data/install/_/win"
KILOHEARTS_INSTALLER="$HOME/Downloads/KiloheartsInstaller.exe"

# Wine >= 9.22 breaks mouse coordinates in yabridge-bridged plugin UIs.
# Wine MR !6569 refactored ConfigureNotify handling; the plugin window's screen
# position is ignored, so Win32 ScreenToClient() produces offset coordinates.
# yabridge issue: https://github.com/robbert-vdh/yabridge/issues/382
# yabridge fix: PR #405 (merged into new-wine10-embedding branch, not yet released)
# Pin to 9.21 until a yabridge release includes the fix.
WINE_VERSION="9.21"

# ─── Helpers ──────────────────────────────────────────────────────────────────

info()  { printf '\033[1;34m[INFO]\033[0m  %s\n' "$*"; }
ok()    { printf '\033[1;32m[ OK ]\033[0m  %s\n' "$*"; }
warn()  { printf '\033[1;33m[WARN]\033[0m  %s\n' "$*"; }
fail()  { printf '\033[1;31m[FAIL]\033[0m  %s\n' "$*"; exit 1; }

command_exists() { command -v "$1" &>/dev/null; }

detect_distro() {
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        echo "${ID}"
    else
        echo "unknown"
    fi
}

detect_distro_family() {
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        # ID_LIKE covers derivatives (e.g. Pop!_OS -> "ubuntu debian")
        echo "${ID_LIKE:-$ID}"
    else
        echo "unknown"
    fi
}

is_debian_family() {
    local family
    family="$(detect_distro_family)"
    [[ "$family" == *debian* ]] || [[ "$family" == *ubuntu* ]] || [[ "$(detect_distro)" == "debian" ]] || [[ "$(detect_distro)" == "ubuntu" ]]
}

is_fedora_family() {
    local family
    family="$(detect_distro_family)"
    [[ "$family" == *fedora* ]] || [[ "$(detect_distro)" == "fedora" ]]
}

is_arch_family() {
    local family
    family="$(detect_distro_family)"
    [[ "$family" == *arch* ]] || [[ "$(detect_distro)" == "arch" ]]
}

# ─── Step 1: Install Wine Staging ────────────────────────────────────────────

install_wine() {
    if command_exists wine; then
        local wine_ver
        wine_ver="$(wine --version 2>/dev/null || echo 'unknown')"

        # Check if installed version is >= 9.22 (broken mouse coordinates)
        local wine_major wine_minor
        wine_major="$(echo "$wine_ver" | sed -n 's/^wine-\([0-9]*\)\..*/\1/p')"
        wine_minor="$(echo "$wine_ver" | sed -n 's/^wine-[0-9]*\.\([0-9]*\).*/\1/p')"
        if [ -n "$wine_major" ] && [ -n "$wine_minor" ]; then
            if [ "$wine_major" -gt 9 ] || { [ "$wine_major" -eq 9 ] && [ "$wine_minor" -ge 22 ]; }; then
                warn "Wine $wine_ver is installed but >= 9.22 (broken mouse coordinates in plugin UIs)"
                warn "Downgrading to Wine ${WINE_VERSION}..."
            else
                ok "Wine already installed: $wine_ver"
                return 0
            fi
        else
            ok "Wine already installed: $wine_ver"
            return 0
        fi
    else
        info "Installing Wine Staging ${WINE_VERSION}..."
    fi

    if is_debian_family; then
        sudo dpkg --add-architecture i386

        # Add WineHQ GPG key
        sudo mkdir -pm755 /etc/apt/keyrings
        if [ ! -f /etc/apt/keyrings/winehq-archive.key ]; then
            sudo wget -O /etc/apt/keyrings/winehq-archive.key \
                https://dl.winehq.org/wine-builds/winehq.key
        fi

        # Detect codename for sources list
        local codename
        codename="$(. /etc/os-release && echo "${VERSION_CODENAME:-$(echo "$VERSION_ID" | tr -d '.')}")"
        local distro_base
        # Use ubuntu or debian as the base for the repo
        if [[ "$(detect_distro_family)" == *ubuntu* ]] || [[ "$(detect_distro)" == "ubuntu" ]]; then
            distro_base="ubuntu"
        else
            distro_base="debian"
        fi

        local sources_file="/etc/apt/sources.list.d/winehq-${codename}.sources"
        if [ ! -f "$sources_file" ]; then
            sudo tee "$sources_file" > /dev/null <<WINEEOF
Types: deb
URIs: https://dl.winehq.org/wine-builds/${distro_base}/
Suites: ${codename}
Components: main
Architectures: amd64 i386
Signed-By: /etc/apt/keyrings/winehq-archive.key
WINEEOF
        fi

        sudo apt update

        # Pin to exact version to avoid the Wine >= 9.22 mouse coordinate regression
        local ver_suffix="~${codename}-1"
        sudo apt install -y --install-recommends --allow-downgrades \
            "winehq-staging=${WINE_VERSION}${ver_suffix}" \
            "wine-staging=${WINE_VERSION}${ver_suffix}" \
            "wine-staging-amd64=${WINE_VERSION}${ver_suffix}" \
            "wine-staging-i386:i386=${WINE_VERSION}${ver_suffix}"

        # Hold packages so apt upgrade doesn't pull in a broken version
        sudo apt-mark hold winehq-staging wine-staging wine-staging-amd64 wine-staging-i386

    elif is_fedora_family; then
        sudo dnf install -y "winehq-staging-${WINE_VERSION}"

    elif is_arch_family; then
        # On Arch, wine-staging is in the repos — version pinning is harder;
        # user should use downgrade utility or hold the package manually
        warn "Arch: install wine-staging <= 9.21 manually (wine >= 9.22 has mouse coordinate bugs)"
        sudo pacman -S --noconfirm --needed wine-staging winetricks

    else
        fail "Unsupported distro: $(detect_distro). Install Wine Staging ${WINE_VERSION} manually, then re-run this script."
    fi

    ok "Wine Staging installed: $(wine --version)"
}

# ─── Step 2: Initialize Wine prefix ─────────────────────────────────────────

init_wine_prefix() {
    if [ -d "$HOME/.wine/drive_c" ]; then
        ok "Wine prefix already exists"
        return 0
    fi

    info "Initializing Wine prefix (this takes a moment)..."
    WINEARCH=win64 wineboot --init
    # Wait for wineserver to finish
    wineserver --wait
    ok "Wine prefix initialized"
}

# ─── Step 3: Install yabridge ────────────────────────────────────────────────

install_yabridge() {
    if command_exists yabridgectl; then
        ok "yabridge already installed: $(yabridgectl --version 2>/dev/null || echo 'present')"
        return 0
    fi

    info "Installing yabridge ${YABRIDGE_VERSION}..."

    if is_arch_family; then
        sudo pacman -S --noconfirm --needed yabridge
        ok "yabridge installed from repos"
        return 0
    fi

    local tmpdir
    tmpdir="$(mktemp -d)"
    local tarball="${tmpdir}/yabridge-${YABRIDGE_VERSION}.tar.gz"

    info "Downloading yabridge ${YABRIDGE_VERSION}..."
    wget -q --show-progress -O "$tarball" "$YABRIDGE_URL"

    info "Extracting to ~/.local/share/yabridge..."
    mkdir -p "$HOME/.local/share"
    tar -C "$HOME/.local/share" -xzf "$tarball"

    # Symlink yabridgectl into PATH
    mkdir -p "$HOME/.local/bin"
    for bin in yabridgectl; do
        ln -sf "$HOME/.local/share/yabridge/$bin" "$HOME/.local/bin/$bin"
    done

    rm -rf "$tmpdir"

    # Ensure ~/.local/bin is in PATH for this session
    export PATH="$HOME/.local/bin:$PATH"

    if command_exists yabridgectl; then
        ok "yabridge ${YABRIDGE_VERSION} installed"
    else
        fail "yabridge installed but yabridgectl not found in PATH. Add ~/.local/bin to your PATH."
    fi
}

# ─── Step 4: Download Kilohearts installer ───────────────────────────────────

download_kilohearts() {
    mkdir -p "$HOME/Downloads"

    if [ -f "$KILOHEARTS_INSTALLER" ]; then
        ok "Kilohearts installer already downloaded: $KILOHEARTS_INSTALLER"
        return 0
    fi

    info "Downloading Kilohearts installer..."
    if wget -q --show-progress -O "$KILOHEARTS_INSTALLER" "$KILOHEARTS_INSTALLER_URL"; then
        ok "Downloaded to $KILOHEARTS_INSTALLER"
    else
        warn "Automatic download failed."
        echo ""
        echo "  Please download the Windows installer manually from:"
        echo "    https://kilohearts.com/download"
        echo ""
        echo "  Save it as: $KILOHEARTS_INSTALLER"
        echo "  Then re-run this script."
        echo ""
        exit 1
    fi
}

# ─── Step 5: Run Kilohearts installer under Wine ────────────────────────────

install_kilohearts() {
    local vst3_dir="$HOME/.wine/drive_c/Program Files/Common Files/VST3"

    # Check if already installed
    if [ -d "$vst3_dir" ] && find "$vst3_dir" -maxdepth 2 -name '*kilohearts*' -o -name '*Kilohearts*' 2>/dev/null | grep -qi kilo; then
        ok "Kilohearts plugins appear to be already installed"
        return 0
    fi

    info "Running Kilohearts installer under Wine..."
    echo ""
    echo "  ┌─────────────────────────────────────────────────────────────┐"
    echo "  │  The Kilohearts installer window will open.                 │"
    echo "  │                                                             │"
    echo "  │  In the installer:                                          │"
    echo "  │    - Sign in to your Kilohearts account (or create one)     │"
    echo "  │    - Select the plugins you want to install                 │"
    echo "  │    - Keep the default VST3 install path                     │"
    echo "  │    - Complete the installation                              │"
    echo "  │                                                             │"
    echo "  │  The script will continue after you close the installer.    │"
    echo "  └─────────────────────────────────────────────────────────────┘"
    echo ""

    # Disable esync to avoid the known file descriptor leak with Kilohearts
    WINEESYNC=0 wine "$KILOHEARTS_INSTALLER"
    wineserver --wait

    ok "Kilohearts installer finished"
}

# ─── Step 6: Configure yabridge ─────────────────────────────────────────────

configure_yabridge() {
    info "Configuring yabridge plugin directories..."

    local vst3_dir="$HOME/.wine/drive_c/Program Files/Common Files/VST3"
    local vst2_dir="$HOME/.wine/drive_c/Program Files/Steinberg/VstPlugins"

    if [ -d "$vst3_dir" ]; then
        yabridgectl add "$vst3_dir" 2>/dev/null || true
        ok "Added VST3 directory"
    fi

    if [ -d "$vst2_dir" ]; then
        yabridgectl add "$vst2_dir" 2>/dev/null || true
        ok "Added VST2 directory"
    fi

    info "Syncing yabridge (creating Linux plugin bridges)..."
    yabridgectl sync

    ok "yabridge configured"
}

# ─── Step 7: Create esync workaround wrapper ────────────────────────────────

create_esync_wrapper() {
    local wrapper="$HOME/.local/bin/kilohearts-daw"
    if [ -f "$wrapper" ]; then
        ok "esync workaround wrapper already exists: $wrapper"
        return 0
    fi

    info "Creating DAW launcher wrapper (disables esync for Kilohearts compatibility)..."

    cat > "$wrapper" <<'WRAPPER'
#!/usr/bin/env bash
# Wrapper to launch your DAW with esync disabled.
# Kilohearts plugins leak file descriptors with esync enabled.
#
# Usage: kilohearts-daw <your-daw-command> [args...]
#   e.g.: kilohearts-daw ardour8
#         kilohearts-daw reaper
#         kilohearts-daw bitwig-studio

if [ $# -eq 0 ]; then
    echo "Usage: kilohearts-daw <daw-command> [args...]"
    echo "Example: kilohearts-daw ardour8"
    exit 1
fi

export WINEESYNC=0
exec "$@"
WRAPPER
    chmod +x "$wrapper"
    ok "Created $wrapper"
}

# ─── Summary ─────────────────────────────────────────────────────────────────

print_summary() {
    echo ""
    echo "  ┌─────────────────────────────────────────────────────────────┐"
    echo "  │             Kilohearts Installation Complete                │"
    echo "  └─────────────────────────────────────────────────────────────┘"
    echo ""
    echo "  VST3 bridges are in:  ~/.vst3/"
    echo "  Wine prefix:          ~/.wine/"
    echo ""
    echo "  Configure your DAW to scan these directories:"
    echo "    VST3:  ~/.vst3"
    echo ""
    echo "  If you experience crashes or hangs, launch your DAW with:"
    echo "    WINEESYNC=0 your-daw-command"
    echo "  or use the wrapper:"
    echo "    kilohearts-daw your-daw-command"
    echo ""
    echo "  To add more Windows plugins later:"
    echo "    1. Install them:   wine installer.exe"
    echo "    2. Re-sync:        yabridgectl sync"
    echo ""
    echo "  Ensure ~/.local/bin is in your PATH:"
    echo "    export PATH=\"\$HOME/.local/bin:\$PATH\""
    echo ""
}

# ─── Main ────────────────────────────────────────────────────────────────────

main() {
    echo ""
    echo "  ╔═════════════════════════════════════════════════════════════╗"
    echo "  ║     Kilohearts VST Plugin Installer for Linux              ║"
    echo "  ║     (via Wine Staging + yabridge)                          ║"
    echo "  ╚═════════════════════════════════════════════════════════════╝"
    echo ""

    install_wine
    init_wine_prefix
    install_yabridge
    download_kilohearts
    install_kilohearts
    configure_yabridge
    create_esync_wrapper
    print_summary
}

main "$@"
