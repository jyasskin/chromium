#!/bin/sh
#
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
#@ This script builds a Debian Wheezy sysroot for building Google Chrome.
#@
#@  Generally this script is invoked as:
#@  sysroot-creator-debian.wheezy.sh <mode> <args>*
#@  Available modes are shown below.
#@
#@ List of modes:

######################################################################
# Config
######################################################################

set -o nounset
set -o errexit

readonly SCRIPT_DIR=$(dirname $0)

# This is where the staging sysroot is.
readonly INSTALL_ROOT_AMD64=$(pwd)/debian_wheezy_amd64_staging
readonly INSTALL_ROOT_I386=$(pwd)/debian_wheezy_i386_staging
readonly INSTALL_ROOT_ARM=$(pwd)/debian_wheezy_arm_staging

readonly REQUIRED_TOOLS="wget"

######################################################################
# Package Config
######################################################################

# This is where we get all the debian packages from.
readonly DEBIAN_REPO=http://http.us.debian.org/debian
readonly REPO_BASEDIR="${DEBIAN_REPO}/dists/wheezy"

readonly RELEASE_FILE="Release"
readonly RELEASE_FILE_GPG="Release.gpg"
readonly RELEASE_LIST="${REPO_BASEDIR}/${RELEASE_FILE}"
readonly RELEASE_LIST_GPG="${REPO_BASEDIR}/${RELEASE_FILE_GPG}"
readonly PACKAGE_FILE_AMD64="main/binary-amd64/Packages.bz2"
readonly PACKAGE_FILE_I386="main/binary-i386/Packages.bz2"
readonly PACKAGE_FILE_ARM="main/binary-armhf/Packages.bz2"
readonly PACKAGE_LIST_AMD64="${REPO_BASEDIR}/${PACKAGE_FILE_AMD64}"
readonly PACKAGE_LIST_I386="${REPO_BASEDIR}/${PACKAGE_FILE_I386}"
readonly PACKAGE_LIST_ARM="${REPO_BASEDIR}/${PACKAGE_FILE_ARM}"

# Sysroot packages: these are the packages needed to build chrome.
# NOTE: When DEBIAN_PACKAGES is modified, the packagelist files must be updated
# by running this script in GeneratePackageList mode.
readonly DEBIAN_PACKAGES="\
  comerr-dev \
  gcc-4.6 \
  krb5-multidev \
  libasound2 \
  libasound2-dev \
  libatk1.0-0 \
  libatk1.0-dev \
  libavahi-client3 \
  libavahi-common3 \
  libc6 \
  libc6-dev \
  libcairo2 \
  libcairo2-dev \
  libcairo-gobject2 \
  libcairo-script-interpreter2 \
  libcap-dev \
  libcap2 \
  libcomerr2 \
  libcups2 \
  libcups2-dev \
  libdbus-1-3 \
  libdbus-1-dev \
  libdbus-glib-1-2 \
  libdrm2 \
  libelf1 \
  libelf-dev \
  libexif12 \
  libexif-dev \
  libexpat1 \
  libexpat1-dev \
  libffi5 \
  libfontconfig1 \
  libfontconfig1-dev \
  libfreetype6 \
  libfreetype6-dev \
  libgcc1 \
  libgcc1 \
  libgconf-2-4 \
  libgconf2-4 \
  libgconf2-dev \
  libgcrypt11 \
  libgcrypt11-dev \
  libgdk-pixbuf2.0-0 \
  libgdk-pixbuf2.0-dev \
  libgl1-mesa-dev \
  libgl1-mesa-glx \
  libglapi-mesa \
  libglib2.0-0 \
  libglib2.0-dev \
  libgnome-keyring0 \
  libgnome-keyring-dev \
  libgnutls26 \
  libgnutls-dev \
  libgnutls-openssl27 \
  libgnutlsxx27 \
  libgomp1 \
  libgpg-error0 \
  libgpg-error-dev \
  libgssapi-krb5-2 \
  libgssrpc4 \
  libgtk2.0-0 \
  libgtk2.0-dev \
  libk5crypto3 \
  libkadm5clnt-mit8 \
  libkadm5srv-mit8 \
  libkdb5-6 \
  libkeyutils1 \
  libkrb5-3 \
  libkrb5-dev \
  libkrb5support0 \
  libnspr4 \
  libnspr4-dev \
  libnss3 \
  libnss3-dev \
  libnss-db \
  liborbit2 \
  libp11-2 \
  libp11-kit0 \
  libpam0g \
  libpam0g-dev \
  libpango1.0-0 \
  libpango1.0-dev \
  libpci3 \
  libpci-dev \
  libpcre3 \
  libpcre3-dev \
  libpcrecpp0 \
  libpixman-1-0 \
  libpixman-1-dev \
  libpng12-0 \
  libpng12-dev \
  libpulse0 \
  libpulse-dev \
  libpulse-mainloop-glib0 \
  libselinux1 \
  libspeechd2 \
  libspeechd-dev \
  libssl1.0.0 \
  libssl-dev \
  libstdc++6 \
  libstdc++6-4.6-dev \
  libtasn1-3 \
  libudev0 \
  libudev-dev \
  libx11-6 \
  libx11-dev \
  libx11-xcb1 \
  libxau6 \
  libxau-dev \
  libxcb1 \
  libxcb1-dev \
  libxcb-glx0 \
  libxcb-render0 \
  libxcb-render0-dev \
  libxcb-shm0 \
  libxcb-shm0-dev \
  libxcomposite1 \
  libxcomposite-dev \
  libxcursor1 \
  libxcursor-dev \
  libxdamage1 \
  libxdamage-dev \
  libxdmcp6 \
  libxext6 \
  libxext-dev \
  libxfixes3 \
  libxfixes-dev \
  libxi6 \
  libxi-dev \
  libxinerama1 \
  libxinerama-dev \
  libxrandr2 \
  libxrandr-dev \
  libxrender1 \
  libxrender-dev \
  libxss1 \
  libxss-dev \
  libxt6 \
  libxt-dev \
  libxtst6 \
  libxtst-dev \
  libxxf86vm1 \
  linux-libc-dev \
  mesa-common-dev \
  speech-dispatcher \
  x11proto-composite-dev \
  x11proto-core-dev \
  x11proto-damage-dev \
  x11proto-fixes-dev \
  x11proto-input-dev \
  x11proto-kb-dev \
  x11proto-randr-dev \
  x11proto-record-dev \
  x11proto-render-dev \
  x11proto-scrnsaver-dev \
  x11proto-xext-dev \
  zlib1g \
  zlib1g-dev"

DEBIAN_PACKAGES_X86="libquadmath0"

readonly DEBIAN_DEP_LIST_AMD64="packagelist.debian.wheezy.amd64"
readonly DEBIAN_DEP_LIST_I386="packagelist.debian.wheezy.i386"
readonly DEBIAN_DEP_LIST_ARM="packagelist.debian.wheezy.arm"

######################################################################
# Helper
######################################################################

Banner() {
  echo "######################################################################"
  echo $*
  echo "######################################################################"
}


SubBanner() {
  echo "......................................................................"
  echo $*
  echo "......................................................................"
}


Usage() {
  egrep "^#@" "$0" | cut --bytes=3-
}


DownloadOrCopy() {
  if [ -f "$2" ] ; then
    echo "$2 already in place"
    return
  fi

  HTTP=0
  echo "$1" | grep -qs ^http:// && HTTP=1
  if [ "$HTTP" = "1" ]; then
    SubBanner "downloading from $1 -> $2"
    wget "$1" -O "$2"
  else
    SubBanner "copying from $1"
    cp "$1" "$2"
  fi
}


SetEnvironmentVariables() {
  ARCH=""
  echo $1 | grep -qs Amd64$ && ARCH=AMD64
  if [ -z "$ARCH" ]; then
    echo $1 | grep -qs I386$ && ARCH=I386
  fi
  if [ -z "$ARCH" ]; then
    echo $1 | grep -qs ARM$ && ARCH=ARM
  fi
  case "$ARCH" in
    ARM)
      INSTALL_ROOT="$INSTALL_ROOT_ARM";
      ;;
    AMD64)
      INSTALL_ROOT="$INSTALL_ROOT_AMD64";
      ;;
    I386)
      INSTALL_ROOT="$INSTALL_ROOT_I386";
      ;;
    *)
      echo "ERROR: Unexpected bad architecture."
      exit 1
      ;;
  esac
}

Cleanup() {
  echo "Cleaning: $TMP"
  rm -rf "$TMP"
}

# some sanity checks to make sure this script is run from the right place
# with the right tools
SanityCheck() {
  Banner "Sanity Checks"

  if [ "$(basename $(pwd))" != "sysroot_scripts" ] ; then
    echo -n "ERROR: run this script from "
    echo "src/chrome/installer/linux/sysroot_scripts"
    exit 1
  fi

  if ! mkdir -p "${INSTALL_ROOT}" ; then
     echo "ERROR: ${INSTALL_ROOT} can't be created."
    exit 1
  fi

  TMP=$(mktemp -q -t -d debian-wheezy-XXXXXX)
  if [ -z "$TMP" ]; then
    echo "ERROR: temp dir can't be created."
    exit 1
  fi
  trap Cleanup 0

  for tool in ${REQUIRED_TOOLS} ; do
    if ! which ${tool} ; then
      echo "Required binary $tool not found."
      echo "Exiting."
      exit 1
    fi
  done
}


ChangeDirectory() {
  # Change directory to where this script is.
  cd ${SCRIPT_DIR}
}


ClearInstallDir() {
  Banner "Clearing dirs in ${INSTALL_ROOT}"
  rm -rf ${INSTALL_ROOT}/*
}


CreateTarBall() {
  local tarball=$1
  Banner "Creating tar ball ${tarball}"
  tar zcf ${tarball} -C ${INSTALL_ROOT} .
}

CheckBuildSysrootArgs() {
  if [ "$#" -ne "1" ]; then
    echo "ERROR: BuildSysroot commands only take 1 argument"
    exit 1
  fi

  if [ -z "$1" ]; then
    echo "ERROR: tarball name required"
    exit 1
  fi
}

ExtractPackageBz2() {
  bzcat "$1" | egrep '^(Package:|Filename:|SHA256:) ' > "$2"
}

GeneratePackageListAmd64() {
  local output_file="$1"
  local package_list="${TMP}/Packages.wheezy_amd64.bz2"
  local tmp_package_list="${TMP}/Packages.wheezy_amd64"
  DownloadOrCopy "${PACKAGE_LIST_AMD64}" "${package_list}"
  VerifyPackageListing "${PACKAGE_FILE_AMD64}" "${package_list}"
  ExtractPackageBz2 "$package_list" "$tmp_package_list"

  GeneratePackageList "$tmp_package_list" "$output_file" "${DEBIAN_PACKAGES}
  ${DEBIAN_PACKAGES_X86}"
}

GeneratePackageListI386() {
  local output_file="$1"
  local package_list="${TMP}/Packages.wheezy_i386.bz2"
  local tmp_package_list="${TMP}/Packages.wheezy_amd64"
  DownloadOrCopy "${PACKAGE_LIST_I386}" "${package_list}"
  VerifyPackageListing "${PACKAGE_FILE_I386}" "${package_list}"
  ExtractPackageBz2 "$package_list" "$tmp_package_list"

  GeneratePackageList "$tmp_package_list" "$output_file" "${DEBIAN_PACKAGES}
  ${DEBIAN_PACKAGES_X86}"
}

GeneratePackageListARM() {
  local output_file="$1"
  local package_list="${TMP}/Packages.wheezy_arm.bz2"
  local tmp_package_list="${TMP}/Packages.wheezy_arm"
  DownloadOrCopy "${PACKAGE_LIST_ARM}" "${package_list}"
  VerifyPackageListing "${PACKAGE_FILE_ARM}" "${package_list}"
  ExtractPackageBz2 "$package_list" "$tmp_package_list"

  GeneratePackageList "$tmp_package_list" "$output_file" "${DEBIAN_PACKAGES}"
}

StripChecksumsFromPackageList() {
  local package_file="$1"
  sed -i 's/ [a-f0-9]\{64\}$//' "$package_file"
}

VerifyPackageFilesMatch() {
  local downloaded_package_file="$1"
  local stored_package_file="$2"
  diff -u "$downloaded_package_file" "$stored_package_file"
  if [ "$?" -ne "0" ]; then
    echo "ERROR: downloaded package files does not match $2."
    echo "You may need to run UpdatePackageLists."
    exit 1
  fi
}

######################################################################
#
######################################################################

HacksAndPatchesAmd64() {
  Banner "Misc Hacks & Patches"
  # these are linker scripts with absolute pathnames in them
  # which we rewrite here
  lscripts="${INSTALL_ROOT}/usr/lib/x86_64-linux-gnu/libpthread.so \
            ${INSTALL_ROOT}/usr/lib/x86_64-linux-gnu/libc.so"

  #SubBanner "Rewriting Linker Scripts"
  sed -i -e 's|/usr/lib/x86_64-linux-gnu/||g'  ${lscripts}
  sed -i -e 's|/lib/x86_64-linux-gnu/||g' ${lscripts}

  # This is for chrome's ./build/linux/pkg-config-wrapper
  # which overwrites PKG_CONFIG_PATH internally
  SubBanner "Package Configs Symlink"
  mkdir -p ${INSTALL_ROOT}/usr/share
  ln -s ../lib/x86_64-linux-gnu/pkgconfig ${INSTALL_ROOT}/usr/share/pkgconfig

  SubBanner "Adding an additional ld.conf include"
  LD_SO_HACK_CONF="${INSTALL_ROOT}/etc/ld.so.conf.d/zz_hack.conf"
  echo /usr/lib/gcc/x86_64-linux-gnu/4.6 > "$LD_SO_HACK_CONF"
  echo /usr/lib >> "$LD_SO_HACK_CONF"
}


HacksAndPatchesI386() {
  Banner "Misc Hacks & Patches"
  # these are linker scripts with absolute pathnames in them
  # which we rewrite here
  lscripts="${INSTALL_ROOT}/usr/lib/i386-linux-gnu/libpthread.so \
            ${INSTALL_ROOT}/usr/lib/i386-linux-gnu/libc.so"

  #SubBanner "Rewriting Linker Scripts"
  sed -i -e 's|/usr/lib/i386-linux-gnu/||g'  ${lscripts}
  sed -i -e 's|/lib/i386-linux-gnu/||g' ${lscripts}

  # This is for chrome's ./build/linux/pkg-config-wrapper
  # which overwrites PKG_CONFIG_PATH internally
  SubBanner "Package Configs Symlink"
  mkdir -p ${INSTALL_ROOT}/usr/share
  ln -s ../lib/i386-linux-gnu/pkgconfig ${INSTALL_ROOT}/usr/share/pkgconfig

  SubBanner "Adding an additional ld.conf include"
  LD_SO_HACK_CONF="${INSTALL_ROOT}/etc/ld.so.conf.d/zz_hack.conf"
  echo /usr/lib/gcc/i486-linux-gnu/4.6 > "$LD_SO_HACK_CONF"
  echo /usr/lib >> "$LD_SO_HACK_CONF"
}


HacksAndPatchesARM() {
  Banner "Misc Hacks & Patches"
  # these are linker scripts with absolute pathnames in them
  # which we rewrite here
  lscripts="${INSTALL_ROOT}/usr/lib/arm-linux-gnueabihf/libpthread.so \
            ${INSTALL_ROOT}/usr/lib/arm-linux-gnueabihf/libc.so"

  #SubBanner "Rewriting Linker Scripts"
  sed -i -e 's|/usr/lib/arm-linux-gnueabihf/||g' ${lscripts}
  sed -i -e 's|/lib/arm-linux-gnueabihf/||g' ${lscripts}

  # This is for chrome's ./build/linux/pkg-config-wrapper
  # which overwrites PKG_CONFIG_PATH internally
  SubBanner "Package Configs Symlink"
  mkdir -p ${INSTALL_ROOT}/usr/share
  ln -s ../lib/arm-linux-gnueabihf/pkgconfig ${INSTALL_ROOT}/usr/share/pkgconfig
}


InstallIntoSysroot() {
  Banner "Install Libs And Headers Into Jail"

  mkdir -p ${TMP}/debian-packages
  mkdir -p ${INSTALL_ROOT}
  while (( "$#" )); do
    local file="$1"
    local package="${TMP}/debian-packages/${file##*/}"
    shift
    local sha256sum="$1"
    shift
    if [ "${#sha256sum}" -ne "64" ]; then
      echo "Bad sha256sum from package list"
      exit 1
    fi

    Banner "Installing ${file}"
    DownloadOrCopy ${DEBIAN_REPO}/pool/${file} ${package}
    if [ ! -s "${package}" ] ; then
      echo
      echo "ERROR: bad package ${package}"
      exit 1
    fi
    echo "${sha256sum}  ${package}" | sha256sum --quiet -c

    SubBanner "Extracting to ${INSTALL_ROOT}"
    dpkg --fsys-tarfile ${package}\
      | tar -xvf - --exclude=./usr/share -C ${INSTALL_ROOT}
  done
}


CleanupJailSymlinks() {
  Banner "Jail symlink cleanup"

  SAVEDPWD=$(pwd)
  cd ${INSTALL_ROOT}
  find lib lib64 usr/lib -type l -printf '%p %l\n' | while read link target; do
    # skip links with non-absolute paths
    echo "${target}" | grep -qs ^/ || continue
    echo "${link}: ${target}"
    case "${link}" in
      usr/lib/gcc/x86_64-linux-gnu/4.*/* | usr/lib/gcc/i486-linux-gnu/4.*/* | \
      usr/lib/gcc/arm-linux-gnueabihf/4.*/*)
        # Relativize the symlink.
        ln -snfv "../../../../..${target}" "${link}"
        ;;
      usr/lib/x86_64-linux-gnu/* | usr/lib/i386-linux-gnu/* | \
      usr/lib/arm-linux-gnueabihf/*)
        # Relativize the symlink.
        ln -snfv "../../..${target}" "${link}"
        ;;
      usr/lib/*)
        # Relativize the symlink.
        ln -snfv "../..${target}" "${link}"
        ;;
      lib64/* | lib/*)
        # Relativize the symlink.
        ln -snfv "..${target}" "${link}"
        ;;
    esac
  done

  find lib lib64 usr/lib -type l -printf '%p %l\n' | while read link target; do
    # Make sure we catch new bad links.
    if [ ! -r "${link}" ]; then
      echo "ERROR: FOUND BAD LINK ${link}"
      ls -l ${link}
      exit 1
    fi
  done
  cd "$SAVEDPWD"
}

#@
#@ BuildSysrootAmd64 <tarball-name>
#@
#@    Build everything and package it
BuildSysrootAmd64() {
  CheckBuildSysrootArgs $@
  ClearInstallDir
  local package_file="$TMP/package_with_sha256sum_amd64"
  GeneratePackageListAmd64 "$package_file"
  local files_and_sha256sums="$(cat ${package_file})"
  StripChecksumsFromPackageList "$package_file"
  VerifyPackageFilesMatch "$package_file" "$DEBIAN_DEP_LIST_AMD64"
  InstallIntoSysroot "${files_and_sha256sums}"
  CleanupJailSymlinks
  HacksAndPatchesAmd64
  CreateTarBall "$1"
}

#@
#@ BuildSysrootI386 <tarball-name>
#@
#@    Build everything and package it
BuildSysrootI386() {
  CheckBuildSysrootArgs $@
  ClearInstallDir
  local package_file="$TMP/package_with_sha256sum_i386"
  GeneratePackageListI386 "$package_file"
  local files_and_sha256sums="$(cat ${package_file})"
  StripChecksumsFromPackageList "$package_file"
  VerifyPackageFilesMatch "$package_file" "$DEBIAN_DEP_LIST_I386"
  InstallIntoSysroot "${files_and_sha256sums}"
  CleanupJailSymlinks
  HacksAndPatchesI386
  CreateTarBall "$1"
}

#@
#@ BuildSysrootARM <tarball-name>
#@
#@    Build everything and package it
BuildSysrootARM() {
  CheckBuildSysrootArgs $@
  ClearInstallDir
  local package_file="$TMP/package_with_sha256sum_arm"
  GeneratePackageListARM "$package_file"
  local files_and_sha256sums="$(cat ${package_file})"
  StripChecksumsFromPackageList "$package_file"
  VerifyPackageFilesMatch "$package_file" "$DEBIAN_DEP_LIST_ARM"
  InstallIntoSysroot "${files_and_sha256sums}"
  CleanupJailSymlinks
  HacksAndPatchesARM
  CreateTarBall "$1"
}

#
# CheckForDebianGPGKeyring
#
#     Make sure the Debian GPG keys exist. Otherwise print a helpful message.
#
CheckForDebianGPGKeyring() {
  if [ ! -e "/usr/share/keyrings/debian-archive-keyring.gpg" ]; then
    echo "Debian GPG keys missing. Install the debian-archive-keyring package."
    exit 1
  fi
}

#
# VerifyPackageListing
#
#     Verifies the downloaded Packages.bz2 file has the right checksums.
#
VerifyPackageListing() {
  local file_path=$1
  local output_file=$2
  local release_file="${TMP}/${RELEASE_FILE}"
  local release_file_gpg="${TMP}/${RELEASE_FILE_GPG}"

  CheckForDebianGPGKeyring

  DownloadOrCopy ${RELEASE_LIST} ${release_file}
  DownloadOrCopy ${RELEASE_LIST_GPG} ${release_file_gpg}
  echo "Verifying: ${release_file} with ${release_file_gpg}"
  gpgv --keyring /usr/share/keyrings/debian-archive-keyring.gpg \
       ${release_file_gpg} ${release_file}

  echo "Verifying: ${output_file}"
  local checksums=$(grep ${file_path} ${release_file} | cut -d " " -f 2)
  local sha256sum=$(echo ${checksums} | cut -d " " -f 3)

  if [ "${#sha256sum}" -ne "64" ]; then
    echo "Bad sha256sum from ${RELEASE_LIST}"
    exit 1
  fi

  echo "${sha256sum}  ${output_file}" | sha256sum --quiet -c
}

#
# GeneratePackageList
#
#     Looks up package names in ${TMP}/Packages and write list of URLs
#     to output file.
#
GeneratePackageList() {
  local input_file="$1"
  local output_file="$2"
  echo "Updating: ${output_file} from ${input_file}"
  /bin/rm -f "${output_file}"
  shift
  shift
  for pkg in $@ ; do
    local pkg_full=$(grep -A 1 " ${pkg}\$" "$input_file" | \
      egrep -o "pool/.*")
    if [ -z "${pkg_full}" ]; then
        echo "ERROR: missing package: $pkg"
        exit 1
    fi
    local pkg_nopool=$(echo "$pkg_full" | sed "s/^pool\///")
    local sha256sum=$(grep -A 4 " ${pkg}\$" "$input_file" | \
      grep ^SHA256: | sed 's/^SHA256: //')
    if [ "${#sha256sum}" -ne "64" ]; then
      echo "Bad sha256sum from Packages"
      exit 1
    fi
    echo $pkg_nopool $sha256sum >> "$output_file"
  done
  # sort -o does an in-place sort of this file
  sort "$output_file" -o "$output_file"
}

#@
#@ UpdatePackageListsAmd64
#@
#@     Regenerate the package lists such that they contain an up-to-date
#@     list of URLs within the Debian archive. (For amd64)
UpdatePackageListsAmd64() {
  GeneratePackageListAmd64 "$DEBIAN_DEP_LIST_AMD64"
  StripChecksumsFromPackageList "$DEBIAN_DEP_LIST_AMD64"
}

#@
#@ UpdatePackageListsI386
#@
#@     Regenerate the package lists such that they contain an up-to-date
#@     list of URLs within the Debian archive. (For i386)
UpdatePackageListsI386() {
  GeneratePackageListI386 "$DEBIAN_DEP_LIST_I386"
  StripChecksumsFromPackageList "$DEBIAN_DEP_LIST_I386"
}

#@
#@ UpdatePackageListsARM
#@
#@     Regenerate the package lists such that they contain an up-to-date
#@     list of URLs within the Debian archive. (For arm)
UpdatePackageListsARM() {
  GeneratePackageListARM "$DEBIAN_DEP_LIST_ARM"
  StripChecksumsFromPackageList "$DEBIAN_DEP_LIST_ARM"
}

if [ $# -eq 0 ] ; then
  echo "ERROR: you must specify a mode on the commandline"
  echo
  Usage
  exit 1
elif [ "$(type -t $1)" != "function" ]; then
  echo "ERROR: unknown function '$1'." >&2
  echo "For help, try:"
  echo "    $0 help"
  exit 1
else
  ChangeDirectory
  SetEnvironmentVariables "$1"
  SanityCheck
  "$@"
fi
