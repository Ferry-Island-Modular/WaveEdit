#!/bin/sh

# WaveEdit loads its fonts, themes, catalog, logos, and manual relative to the
# working directory. linuxdeploy sources AppRun hooks before launching the
# application, so switch to the bundled resource directory first.
cd "${APPDIR}/usr/share/waveedit" || exit 1
