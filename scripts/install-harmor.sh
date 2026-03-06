#!/usr/bin/env bash
#
# install-harmor.sh — Install Image-Line Harmor VST on Linux via Wine + yabridge
#
# Usage:
#   chmod +x install-harmor.sh
#   ./install-harmor.sh
#
# What this does:
#   1. Installs Wine Staging (from WineHQ repos on Debian/Ubuntu)
#   2. Installs yabridge (Windows VST bridge for Linux)
#   3. Runs the Harmor Windows installer under Wine
#   4. Configures yabridge so your Linux DAW can find the plugin
#
# Prerequisites:
#   Download the Harmor installer from your Image-Line account:
#     https://www.image-line.com/fl-studio/plugins/harmor/
#   Save it to ~/Downloads/ (the script will look for it there)
#
# Tested on: Debian 13, Ubuntu 24.04+
# Requires: sudo access, internet connection
#
set -euo pipefail

YABRIDGE_VERSION="5.1.1"
YABRIDGE_URL="https://github.com/robbert-vdh/yabridge/releases/download/${YABRIDGE_VERSION}/yabridge-${YABRIDGE_VERSION}.tar.gz"

# The Harmor installer must be downloaded manually from Image-Line's website.
# The script will search ~/Downloads/ for it.
HARMOR_INSTALLER=""

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

# ─── Step 4: Locate Harmor installer ─────────────────────────────────────────

locate_harmor_installer() {
    # Search ~/Downloads/ for likely Harmor installer filenames
    local search_dir="$HOME/Downloads"
    local candidates=()

    if [ -d "$search_dir" ]; then
        while IFS= read -r -d '' f; do
            candidates+=("$f")
        done < <(find "$search_dir" -maxdepth 1 -iname '*harmor*' -iname '*.exe' -print0 2>/dev/null)
    fi

    if [ ${#candidates[@]} -eq 1 ]; then
        HARMOR_INSTALLER="${candidates[0]}"
        ok "Found Harmor installer: $HARMOR_INSTALLER"
        return 0
    elif [ ${#candidates[@]} -gt 1 ]; then
        echo ""
        echo "  Multiple Harmor installers found in ~/Downloads/:"
        local i=1
        for f in "${candidates[@]}"; do
            echo "    $i) $(basename "$f")"
            ((i++))
        done
        echo ""
        read -rp "  Select installer [1-${#candidates[@]}]: " choice
        if [[ "$choice" =~ ^[0-9]+$ ]] && [ "$choice" -ge 1 ] && [ "$choice" -le ${#candidates[@]} ]; then
            HARMOR_INSTALLER="${candidates[$((choice - 1))]}"
            ok "Selected: $HARMOR_INSTALLER"
            return 0
        else
            fail "Invalid selection."
        fi
    fi

    # Not found — prompt the user
    echo ""
    echo "  ┌─────────────────────────────────────────────────────────────┐"
    echo "  │  Harmor installer not found in ~/Downloads/                 │"
    echo "  │                                                             │"
    echo "  │  Please download it from your Image-Line account:           │"
    echo "  │    https://www.image-line.com/fl-studio/plugins/harmor/     │"
    echo "  │                                                             │"
    echo "  │  Or from your Image-Line account downloads page:            │"
    echo "  │    https://support.image-line.com/member/profile.php        │"
    echo "  │                                                             │"
    echo "  │  Save the .exe installer to ~/Downloads/ and re-run         │"
    echo "  │  this script, or enter the full path below.                 │"
    echo "  └─────────────────────────────────────────────────────────────┘"
    echo ""
    read -rp "  Path to Harmor installer .exe (or press Enter to abort): " user_path

    if [ -n "$user_path" ] && [ -f "$user_path" ]; then
        HARMOR_INSTALLER="$user_path"
        ok "Using installer: $HARMOR_INSTALLER"
        return 0
    fi

    fail "No Harmor installer provided. Download it and re-run this script."
}

# ─── Step 5: Run Harmor installer under Wine ────────────────────────────────

install_harmor() {
    local vst3_dir="$HOME/.wine/drive_c/Program Files/Common Files/VST3"
    local vst2_dir="$HOME/.wine/drive_c/Program Files/Image-Line"

    # Check if already installed
    local found=false
    if [ -d "$vst3_dir" ] && find "$vst3_dir" -maxdepth 2 \( -iname '*harmor*' -o -iname '*image*line*' \) -print -quit 2>/dev/null | grep -qi .; then
        found=true
    fi
    if [ -d "$vst2_dir" ] && find "$vst2_dir" -maxdepth 3 -iname '*harmor*' -print -quit 2>/dev/null | grep -qi .; then
        found=true
    fi

    if $found; then
        ok "Harmor appears to be already installed"
        return 0
    fi

    info "Running Harmor installer under Wine..."
    echo ""
    echo "  ┌─────────────────────────────────────────────────────────────┐"
    echo "  │  The Harmor installer window will open.                     │"
    echo "  │                                                             │"
    echo "  │  In the installer:                                          │"
    echo "  │    - Accept the license agreement                           │"
    echo "  │    - Keep the default install paths                         │"
    echo "  │    - Make sure VST plugin option is checked                 │"
    echo "  │    - Complete the installation                              │"
    echo "  │                                                             │"
    echo "  │  The script will continue after you close the installer.    │"
    echo "  └─────────────────────────────────────────────────────────────┘"
    echo ""

    wine "$HARMOR_INSTALLER"
    wineserver --wait

    ok "Harmor installer finished"
}

# ─── Step 6: Configure yabridge ─────────────────────────────────────────────

configure_yabridge() {
    info "Configuring yabridge plugin directories..."

    local vst3_dir="$HOME/.wine/drive_c/Program Files/Common Files/VST3"
    local vst2_dir="$HOME/.wine/drive_c/Program Files/Steinberg/VstPlugins"
    local il_shared="$HOME/.wine/drive_c/Program Files/Image-Line/Shared/VST"
    local il_vst="$HOME/.wine/drive_c/Program Files/Image-Line/FL Studio/Plugins/VST"

    for dir in "$vst3_dir" "$vst2_dir" "$il_shared" "$il_vst"; do
        if [ -d "$dir" ]; then
            yabridgectl add "$dir" 2>/dev/null || true
            ok "Added plugin directory: $dir"
        fi
    done

    info "Syncing yabridge (creating Linux plugin bridges)..."
    yabridgectl sync

    ok "yabridge configured"
}

# ─── Summary ─────────────────────────────────────────────────────────────────

print_summary() {
    echo ""
    echo "  ┌─────────────────────────────────────────────────────────────┐"
    echo "  │              Harmor Installation Complete                    │"
    echo "  └─────────────────────────────────────────────────────────────┘"
    echo ""
    echo "  VST bridges are in:   ~/.vst3/ and ~/.vst/"
    echo "  Wine prefix:          ~/.wine/"
    echo ""
    echo "  Configure your DAW to scan these directories:"
    echo "    VST3:  ~/.vst3"
    echo "    VST2:  ~/.vst"
    echo ""
    echo "  Harmor requires an Image-Line account for activation."
    echo "  On first load in your DAW, you may need to unlock it via"
    echo "  the Image-Line license dialog that appears in Wine."
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
    echo "  ║     Image-Line Harmor VST Installer for Linux              ║"
    echo "  ║     (via Wine Staging + yabridge)                          ║"
    echo "  ╚═════════════════════════════════════════════════════════════╝"
    echo ""

    install_wine
    init_wine_prefix
    install_yabridge
    locate_harmor_installer
    install_harmor
    configure_yabridge
    print_summary
}

main "$@"
